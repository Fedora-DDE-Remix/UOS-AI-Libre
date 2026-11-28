#!/usr/bin/env bash
# User-level takeover of the UOS AI D-Bus names.
#
# This does not need root and does not modify /usr. It puts a higher-priority
# service file in ~/.local/share/dbus-1/services, disables UOS AI's system
# autostart with a user override (~/.config/autostart/... Hidden=true), and
# installs an autostart entry for UOS AI Libre. This is the practical path on a
# development machine where UOS AI was unpacked manually.
#
# For a system-wide package, use install-service.sh and package-level
# Obsoletes/Conflicts instead.

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="$REPO_DIR/uos-ai-libre/build"
DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
BIN_DIR="$HOME/.local/bin"
SERVICES_DIR="$DATA_HOME/dbus-1/services"
AUTOSTART_DIR="$CONFIG_HOME/autostart"
BACKUP_DIR="$DATA_HOME/uos-ai-libre/backup"
COPY_BINARY=1
RESTORE=0
DRY_RUN=0
ASSUME_YES=0

readonly UOS_AUTOSTART="uos-ai-assistant.autostart.desktop"
readonly OUR_AUTOSTART="uos-ai-libre-service.desktop"
readonly KEYBINDING_SERVICE="org.deepin.dde.Keybinding1"
readonly KEYBINDING_PATH="/org/deepin/dde/Keybinding1"
readonly KEYBINDING_IFACE="org.deepin.dde.Keybinding1"
readonly UOS_AI_SHORTCUT="org.deepin.dde.keybinding.shortcut.app.uos-ai"
readonly CUSTOM_SHORTCUT_NAME="UOS AI Libre"
readonly CUSTOM_ID_FILE="$BACKUP_DIR/keybinding-custom-id"
readonly UOS_HOTKEY_FILE="$BACKUP_DIR/keybinding-uos-ai-hotkeys"

usage() {
    sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'
}

die() {
    printf 'install-user-service: %s\n' "$*" >&2
    exit 1
}

say() {
    printf '%s\n' "$*"
}

run() {
    if (( DRY_RUN )); then
        printf 'would run:'
        printf ' %q' "$@"
        printf '\n'
    else
        "$@"
    fi
}

backup_file() {
    local src="$1"
    local dst="$BACKUP_DIR/${src#/}"

    if [[ -e "$src" && ! -e "$dst" ]]; then
        run mkdir -p "$(dirname "$dst")"
        run cp -a "$src" "$dst"
        say "backed up: $src"
    fi
}

restore_file() {
    local original="$1"
    local dst="$BACKUP_DIR/${original#/}"

    if [[ -e "$dst" ]]; then
        run mkdir -p "$(dirname "$original")"
        run cp -a "$dst" "$original"
        say "restored: $original"
    fi
}

stop_matching() {
    local wanted="$1"
    local require_service="${2:-0}"
    (( DRY_RUN )) && return 0

    local proc exe cmdline
    for proc in /proc/[0-9]*; do
        [[ -r "$proc/exe" ]] || continue
        exe="$(readlink -f "$proc/exe" 2>/dev/null || true)"
        [[ "$exe" == "$wanted" ]] || continue
        cmdline="$(tr '\0' ' ' < "$proc/cmdline" 2>/dev/null || true)"
        if (( require_service )) && [[ "$cmdline" != *" --service"* ]]; then
            continue
        fi
        say "stopping $exe (pid ${proc##*/})"
        kill -TERM "${proc##*/}" 2>/dev/null || true
    done
}

reload_bus() {
    (( DRY_RUN )) && return 0
    [[ -n "${DBUS_SESSION_BUS_ADDRESS:-}" ]] || return 0
    dbus-send --session --dest=org.freedesktop.DBus \
        --type=method_call / org.freedesktop.DBus.ReloadConfig || true
}

keybinding_call() {
    gdbus call --session --dest "$KEYBINDING_SERVICE" \
        --object-path "$KEYBINDING_PATH" \
        --method "$KEYBINDING_IFACE.$1" "${@:2}"
}

keybinding_available() {
    command -v gdbus >/dev/null 2>&1 || return 1
    keybinding_call GetShortcut "$UOS_AI_SHORTCUT" >/dev/null 2>&1
}

backup_uos_hotkeys() {
    [[ -e "$UOS_HOTKEY_FILE" ]] && return 0
    local raw hotkeys
    raw="$(keybinding_call GetShortcut "$UOS_AI_SHORTCUT" 2>/dev/null || true)"
    hotkeys="$(printf '%s' "$raw" | sed -n 's/.*\[\(.*\)\].*/\1/p')"
    if [[ -n "$hotkeys" ]]; then
        run mkdir -p "$BACKUP_DIR"
        if (( ! DRY_RUN )); then
            printf '[%s]\n' "$hotkeys" > "$UOS_HOTKEY_FILE"
        fi
    fi
}

restore_uos_hotkeys() {
    if [[ -f "$UOS_HOTKEY_FILE" ]]; then
        keybinding_call ModifyHotkeys "$UOS_AI_SHORTCUT" \
            "$(cat "$UOS_HOTKEY_FILE")" >/dev/null 2>&1 || true
        run rm -f "$UOS_HOTKEY_FILE"
    fi
}

delete_custom_shortcuts() {
    local ids id
    ids="$(keybinding_call SearchShortcuts "$CUSTOM_SHORTCUT_NAME" 2>/dev/null \
        | grep -oE 'org\.deepin\.dde\.keybinding\.shortcut\.custom\.[0-9a-f-]+' \
        | sort -u || true)"
    for id in $ids; do
        keybinding_call DeleteCustomShortcut "$id" >/dev/null 2>&1 || true
    done
}

prepare_keybinding() {
    if ! keybinding_available; then
        say "keybinding service unavailable; the app will retry at startup"
        return 0
    fi
    if (( DRY_RUN )); then
        say "would back up UOS AI's <Super>space for --restore"
        return 0
    fi
    backup_uos_hotkeys
    say "the app will register Super+Space through ShortcutService on startup"
}

restore_keybinding() {
    if ! keybinding_available; then
        return 0
    fi
    if (( DRY_RUN )); then
        say "would remove the UOS AI Libre custom shortcut and restore UOS AI's Super+Space"
        return 0
    fi

    if [[ -f "$CUSTOM_ID_FILE" ]]; then
        keybinding_call DeleteCustomShortcut "$(cat "$CUSTOM_ID_FILE")" \
            >/dev/null 2>&1 || true
        rm -f "$CUSTOM_ID_FILE"
    fi
    delete_custom_shortcuts
    restore_uos_hotkeys
}

write_service_files() {
    local exec_path="$1"
    run mkdir -p "$SERVICES_DIR"

    local service
    for service in com.deepin.copilot.service com.iflytek.aiassistant.service; do
        if [[ -e "$SERVICES_DIR/$service" ]] \
            && ! grep -q 'uos-ai-libre' "$SERVICES_DIR/$service" 2>/dev/null; then
            backup_file "$SERVICES_DIR/$service"
        fi
    done

    if (( DRY_RUN )); then
        say "would write: $SERVICES_DIR/com.deepin.copilot.service"
        say "would write: $SERVICES_DIR/com.iflytek.aiassistant.service"
        return
    fi
    cat > "$SERVICES_DIR/com.deepin.copilot.service" <<EOF
[D-BUS Service]
Name=com.deepin.copilot
Exec=$exec_path --service
EOF
    cat > "$SERVICES_DIR/com.iflytek.aiassistant.service" <<EOF
[D-BUS Service]
Name=com.iflytek.aiassistant
Exec=$exec_path --service
EOF
}

write_autostart_files() {
    local exec_path="$1"
    run mkdir -p "$AUTOSTART_DIR"

    # The user autostart file disables the system-wide UOS AI entry. Do not
    # back up our own previous override as if it were a user original.
    if [[ -e "$AUTOSTART_DIR/$UOS_AUTOSTART" ]] \
        && ! grep -q 'X-UOS-AI-Libre-Managed=true' \
            "$AUTOSTART_DIR/$UOS_AUTOSTART" 2>/dev/null; then
        backup_file "$AUTOSTART_DIR/$UOS_AUTOSTART"
    fi

    if (( DRY_RUN )); then
        say "would write: $AUTOSTART_DIR/$UOS_AUTOSTART (Hidden=true)"
        say "would write: $AUTOSTART_DIR/$OUR_AUTOSTART"
        return
    fi
    cat > "$AUTOSTART_DIR/$UOS_AUTOSTART" <<EOF
[Desktop Entry]
Type=Application
Name=UOS AI (disabled by UOS AI Libre)
Hidden=true
X-UOS-AI-Libre-Managed=true
EOF
    cat > "$AUTOSTART_DIR/$OUR_AUTOSTART" <<EOF
[Desktop Entry]
Type=Application
Name=UOS AI Libre Service
Comment=UOS AI compatible D-Bus service
Exec=$exec_path --service
Icon=uos-ai-libre
Terminal=false
NoDisplay=true
X-GNOME-Autostart-Delay=2
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)
            [[ $# -ge 2 ]] || die "--build-dir needs a value"
            BUILD_DIR="$2"
            shift 2
            ;;
        --no-copy)
            COPY_BINARY=0
            shift
            ;;
        --restore)
            RESTORE=1
            shift
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        --yes|-y)
            ASSUME_YES=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            die "unknown argument: $1 (use --help)"
            ;;
    esac
done

[[ -d "$BUILD_DIR" ]] || die "build directory not found: $BUILD_DIR"
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"
[[ -x "$BUILD_DIR/uos-ai-libre" ]] || die "missing built binary: $BUILD_DIR/uos-ai-libre"

EXEC_PATH="$BIN_DIR/uos-ai-libre"
if (( COPY_BINARY )); then
    :
else
    EXEC_PATH="$BUILD_DIR/uos-ai-libre"
fi

if (( RESTORE )); then
    say "Restoring user-level UOS AI Libre service files"
    if (( ! ASSUME_YES && ! DRY_RUN )); then
        read -r -p "Continue? [y/N] " answer
        [[ "$answer" == [yY] ]] || exit 1
    fi

    stop_matching "$BIN_DIR/uos-ai-libre" 1
    stop_matching "$BUILD_DIR/uos-ai-libre" 1
    restore_keybinding

    run rm -f "$SERVICES_DIR/com.deepin.copilot.service"
    run rm -f "$SERVICES_DIR/com.iflytek.aiassistant.service"
    restore_file "$SERVICES_DIR/com.deepin.copilot.service"
    restore_file "$SERVICES_DIR/com.iflytek.aiassistant.service"
    run rm -f "$AUTOSTART_DIR/$OUR_AUTOSTART"
    run rm -f "$DATA_HOME/applications/uos-ai-libre.desktop"
    run rm -f "$DATA_HOME/icons/hicolor/256x256/apps/uos-ai-libre.png"

    if [[ -e "$BACKUP_DIR/${AUTOSTART_DIR#/}/$UOS_AUTOSTART" ]]; then
        restore_file "$AUTOSTART_DIR/$UOS_AUTOSTART"
    else
        run rm -f "$AUTOSTART_DIR/$UOS_AUTOSTART"
    fi
    if (( COPY_BINARY )); then
        run rm -f "$BIN_DIR/uos-ai-libre"
    fi

    reload_bus
    say "User files removed. System UOS AI service files were never changed."
    exit 0
fi

if (( ! ASSUME_YES && ! DRY_RUN )); then
    say "This will install a user-level UOS AI Libre service in:"
    say "  $SERVICES_DIR"
    say "  $AUTOSTART_DIR"
    say "and disable UOS AI's system autostart for this user."
    read -r -p "Continue? [y/N] " answer
    [[ "$answer" == [yY] ]] || exit 1
fi

# Stop the old owner before installing/activating the replacement.
stop_matching "/usr/bin/uos-ai-assistant" 0
stop_matching "$BIN_DIR/uos-ai-libre" 1
stop_matching "$BUILD_DIR/uos-ai-libre" 1

if (( COPY_BINARY )); then
    say "installing binary: $EXEC_PATH"
    run install -Dm755 "$BUILD_DIR/uos-ai-libre" "$EXEC_PATH"
fi
run install -Dm644 "$REPO_DIR/uos-ai-libre/resources/uos-ai-libre.desktop" \
    "$DATA_HOME/applications/uos-ai-libre.desktop"
run install -Dm644 "$REPO_DIR/uos-ai-libre/resources/images/logo.png" \
    "$DATA_HOME/icons/hicolor/256x256/apps/uos-ai-libre.png"

write_service_files "$EXEC_PATH"
write_autostart_files "$EXEC_PATH"
prepare_keybinding
reload_bus

if (( ! DRY_RUN )); then
    # Activation is the real test: the session bus must choose our user-level
    # service file over /usr/share/dbus-1/services/com.deepin.copilot.service.
    gdbus call --session --dest com.deepin.copilot \
        --object-path /com/deepin/copilot \
        --method com.deepin.copilot.version >/dev/null 2>&1 || true
    sleep 0.5
    owner="$(dbus-send --session --dest=org.freedesktop.DBus \
        --type=method_call --print-reply /org/freedesktop/DBus \
        org.freedesktop.DBus.GetConnectionUnixProcessID \
        string:com.deepin.copilot 2>/dev/null | awk '/uint32/ {print $2; exit}')"
    if [[ -n "$owner" ]]; then
        owner_exe="$(readlink -f "/proc/$owner/exe" 2>/dev/null || true)"
        if [[ "$owner_exe" == "$EXEC_PATH" ]]; then
            say "activation OK: $owner_exe (pid $owner)"
        else
            say "warning: com.deepin.copilot is owned by $owner_exe (pid $owner)"
            say "         expected $EXEC_PATH; stop the old process and retry."
        fi
    else
        say "warning: com.deepin.copilot has no owner yet; it will start on the next D-Bus request."
    fi
fi

say "User-level UOS AI Libre service installed."
say "Use '$0 --restore' to remove it."
