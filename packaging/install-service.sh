#!/usr/bin/env bash
# Install UOS AI Libre's D-Bus activation files and take the copilot bus names
# away from a manually-installed UOS AI. This is intentionally separate from
# the RPM/DEB packaging path: those packages should Obsolete/Conflict with the
# uos-ai package, but the current test machine has UOS AI unpacked outside the
# package manager.
#
# Usage:
#   sudo packaging/install-service.sh
#   sudo packaging/install-service.sh --prefix /usr --yes
#   sudo packaging/install-service.sh --restore
#
# For a dry staging test:
#   packaging/install-service.sh --destdir /tmp/uos-ai-libre-stage \
#       --build-dir uos-ai-libre/build --yes
#
# The script backs up the original UOS AI files under
# /var/lib/uos-ai-libre/backup and can restore them with --restore.

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

PREFIX=/usr
DESTDIR=
BUILD_DIR="$REPO_DIR/uos-ai-libre/build"
BACKUP_DIR=
DO_INSTALL=1
RESTORE=0
DRY_RUN=0
ASSUME_YES=0

readonly UOS_SERVICES=(
    "com.deepin.copilot.service"
    "com.iflytek.aiassistant.service"
)
readonly UOS_AUTOSTART="uos-ai-assistant.autostart.desktop"
readonly OUR_AUTOSTART="uos-ai-libre-service.desktop"
readonly DISABLED_SUFFIX=".disabled-by-uos-ai-libre"

usage() {
    sed -n '2,24p' "$0" | sed 's/^# \{0,1\}//'
}

die() {
    printf 'install-service: %s\n' "$*" >&2
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

clean_destdir() {
    local path="$1"
    if [[ -n "$path" ]]; then
        [[ "$path" == /* ]] || die "--destdir must be an absolute path"
        path="${path%/}"
    fi
    printf '%s' "$path"
}

root_path() {
    printf '%s%s' "$DESTDIR" "$1"
}

backup_file() {
    local relative="$1"
    local src
    src="$(root_path "/$relative")"
    local dst="$BACKUP_DIR/$relative"

    if [[ -e "$src" && ! -e "$dst" ]]; then
        run mkdir -p "$(dirname "$dst")"
        run cp -a "$src" "$dst"
        say "backed up: /$relative"
    fi
}

restore_file() {
    local relative="$1"
    local src="$BACKUP_DIR/$relative"
    local dst
    dst="$(root_path "/$relative")"

    if [[ -f "$src" ]]; then
        run mkdir -p "$(dirname "$dst")"
        run cp -a "$src" "$dst"
        say "restored: /$relative"
    fi
}

write_template() {
    local template="$1"
    local destination="$2"
    local tmp
    tmp="$(mktemp)"
    sed "s|@UOS_AI_LIBRE_EXEC@|$PREFIX/bin/uos-ai-libre|g" "$template" > "$tmp"
    run mkdir -p "$(dirname "$destination")"
    run install -m 0644 "$tmp" "$destination"
    rm -f "$tmp"
}

stop_uos_ai() {
    (( DRY_RUN )) && return 0
    [[ -z "$DESTDIR" ]] || return 0

    local pid exe found=0
    for proc in /proc/[0-9]*; do
        [[ -r "$proc/exe" ]] || continue
        exe="$(readlink -f "$proc/exe" 2>/dev/null || true)"
        case "$exe" in
            /usr/bin/uos-ai-assistant|*/uos-ai-assistant)
                pid="${proc##*/}"
                if kill -0 "$pid" 2>/dev/null; then
                    say "stopping UOS AI (pid $pid)"
                    kill -TERM "$pid" 2>/dev/null || true
                    found=1
                fi
                ;;
        esac
    done
    if (( found )); then
        # Give it a moment to release com.deepin.copilot and
        # com.iflytek.aiassistant before D-Bus activation is used again.
        sleep 1
    fi
}

reload_session_bus() {
    (( DRY_RUN )) && return 0
    [[ -z "$DESTDIR" ]] || return 0
    [[ -n "${DBUS_SESSION_BUS_ADDRESS:-}" ]] || return 0
    run dbus-send --session --dest=org.freedesktop.DBus \
        --type=method_call / org.freedesktop.DBus.ReloadConfig || true
}

activate_our_service() {
    (( DRY_RUN )) && return 0
    [[ -z "$DESTDIR" ]] || return 0
    [[ "$(id -u)" -ne 0 ]] || return 0
    [[ -n "${DBUS_SESSION_BUS_ADDRESS:-}" ]] || return 0

    # D-Bus activation makes the user session, not root, own the service.
    run gdbus call --session --dest com.deepin.copilot \
        --object-path /com/deepin/copilot \
        --method com.deepin.copilot.version >/dev/null 2>&1 || true
}

install_service_files() {
    local services_dir
    services_dir="$(root_path "$PREFIX/share/dbus-1/services")"
    local autostart_dir
    autostart_dir="$(root_path "/etc/xdg/autostart")"

    write_template "$REPO_DIR/uos-ai-libre/resources/dbus/com.deepin.copilot.service.in" \
        "$services_dir/com.deepin.copilot.service"
    write_template "$REPO_DIR/uos-ai-libre/resources/dbus/com.iflytek.aiassistant.service.in" \
        "$services_dir/com.iflytek.aiassistant.service"
    write_template "$REPO_DIR/uos-ai-libre/resources/autostart/uos-ai-libre-service.desktop.in" \
        "$autostart_dir/$OUR_AUTOSTART"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)
            [[ $# -ge 2 ]] || die "--prefix needs a value"
            PREFIX="$2"
            shift 2
            ;;
        --destdir)
            [[ $# -ge 2 ]] || die "--destdir needs a value"
            DESTDIR="$2"
            shift 2
            ;;
        --build-dir)
            [[ $# -ge 2 ]] || die "--build-dir needs a value"
            BUILD_DIR="$2"
            shift 2
            ;;
        --backup-dir)
            [[ $# -ge 2 ]] || die "--backup-dir needs a value"
            BACKUP_DIR="$2"
            shift 2
            ;;
        --no-install)
            DO_INSTALL=0
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

DESTDIR="$(clean_destdir "$DESTDIR")"
PREFIX="${PREFIX%/}"
[[ "$PREFIX" == /* ]] || die "--prefix must be absolute"

if [[ -z "$BACKUP_DIR" ]]; then
    BACKUP_DIR="$(root_path "/var/lib/uos-ai-libre/backup")"
fi
[[ "$BACKUP_DIR" == /* ]] || die "--backup-dir must be absolute"

if [[ -z "$DESTDIR" && "$(id -u)" -ne 0 ]]; then
    die "run this with sudo, or use --destdir for a staging test"
fi

if (( RESTORE )); then
    say "Restoring the UOS AI service files from $BACKUP_DIR"
    if (( ! ASSUME_YES && ! DRY_RUN )); then
        read -r -p "Continue? [y/N] " answer
        [[ "$answer" == [yY] ]] || exit 1
    fi

    # Remove our activation/autostart files first.
    run rm -f "$(root_path "/etc/xdg/autostart/$OUR_AUTOSTART")"
    run rm -f "$(root_path "$PREFIX/etc/xdg/autostart/$OUR_AUTOSTART")"
    for service in "${UOS_SERVICES[@]}"; do
        run rm -f "$(root_path "$PREFIX/share/dbus-1/services/$service")"
        restore_file "usr/share/dbus-1/services/$service"
    done

    disabled="$(root_path "/etc/xdg/autostart/${UOS_AUTOSTART}${DISABLED_SUFFIX}")"
    if [[ -f "$disabled" ]]; then
        run mv "$disabled" "$(root_path "/etc/xdg/autostart/$UOS_AUTOSTART")"
        say "restored: /etc/xdg/autostart/$UOS_AUTOSTART"
    else
        restore_file "etc/xdg/autostart/$UOS_AUTOSTART"
    fi

    reload_session_bus
    say "UOS AI files restored. Start UOS AI again if you need it."
    exit 0
fi

[[ -d "$BUILD_DIR" ]] || die "build directory not found: $BUILD_DIR"
[[ -x "$BUILD_DIR/uos-ai-libre" ]] || die "missing built binary: $BUILD_DIR/uos-ai-libre"

if (( ! ASSUME_YES && ! DRY_RUN )); then
    say "This will install UOS AI Libre into ${DESTDIR}${PREFIX},"
    say "back up UOS AI's D-Bus/autostart files, and take over"
    say "com.deepin.copilot + com.iflytek.aiassistant."
    read -r -p "Continue? [y/N] " answer
    [[ "$answer" == [yY] ]] || exit 1
fi

say "backup directory: $BACKUP_DIR"
for service in "${UOS_SERVICES[@]}"; do
    backup_file "usr/share/dbus-1/services/$service"
done
backup_file "etc/xdg/autostart/$UOS_AUTOSTART"

# Disable the old autostart entry in place. Removing it would make --restore
# depend on a second copy; keeping the file lets the user see what happened.
disabled_autostart="$(root_path "/etc/xdg/autostart/${UOS_AUTOSTART}${DISABLED_SUFFIX}")"
autostart="$(root_path "/etc/xdg/autostart/$UOS_AUTOSTART")"
if [[ -f "$autostart" ]]; then
    run mv "$autostart" "$disabled_autostart"
    say "disabled UOS AI autostart: /etc/xdg/autostart/$UOS_AUTOSTART"
fi

stop_uos_ai

if (( DO_INSTALL )); then
    say "installing binary, desktop file and icons..."
    run env DESTDIR="$DESTDIR" cmake --install "$BUILD_DIR" --prefix "$PREFIX"

    # Older builds installed the autostart file under ${prefix}/etc. It is
    # never read there and would leave a second copy behind.
    stale_autostart="$(root_path "$PREFIX/etc/xdg/autostart/$OUR_AUTOSTART")"
    if [[ -e "$stale_autostart" ]]; then
        run rm -f "$stale_autostart"
    fi

    # Older installs used a 256x256 PNG. The branding is now a scalable SVG;
    # remove the old raster file so both icons cannot diverge.
    stale_icon="$(root_path "$PREFIX/share/icons/hicolor/256x256/apps/uos-ai-libre.png")"
    if [[ -e "$stale_icon" ]]; then
        run rm -f "$stale_icon"
    fi
else
    say "skipping binary install (--no-install)"
fi

install_service_files
reload_session_bus
activate_our_service

if (( DRY_RUN )); then
    say "dry run complete; nothing was changed."
else
    say "UOS AI Libre now owns the copilot activation names."
    say "The service starts on the next D-Bus request, or at next login."
    say "Use '$0 --restore' to put the UOS AI files back."
fi
