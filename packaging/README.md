# Deployment: taking over the UOS AI D-Bus names

Only one process can own `com.deepin.copilot`, `com.iflytek.aiassistant` and
the D-Bus activation files behind them. A package must therefore conflict with
the `uos-ai` package (or `Obsoletes: uos-ai`); the RPM/DEB should own:

```text
/usr/share/dbus-1/services/com.deepin.copilot.service
/usr/share/dbus-1/services/com.iflytek.aiassistant.service
/etc/xdg/autostart/uos-ai-libre-service.desktop
```

On development machines where UOS AI was unpacked manually, use one of the
scripts below.

## User-level takeover (no root)

```bash
packaging/install-user-service.sh --build-dir uos-ai-libre/build --yes
packaging/install-user-service.sh --restore
```

This is the recommended path on the current development machine. It copies the
binary to `~/.local/bin`, writes higher-priority service files to
`~/.local/share/dbus-1/services`, disables the system UOS AI autostart with a
`Hidden=true` user override, and installs a user autostart entry. The original
system `/usr/share/dbus-1/services` files are not modified. D-Bus activation
prefers the user service directory, so screen recorder and Grand Search start
UOS AI Libre on demand. D-Bus activation was verified by killing the running
process and calling com.deepin.copilot.version again.

The application itself now owns the shortcut policy through
`ShortcutService`: on startup it disables UOS AI's
`org.deepin.dde.keybinding.shortcut.app.uos-ai` entry when necessary and
registers a custom `UOS AI Libre` shortcut for `Super+Space` that runs the
current binary with `--chat`. The installer only saves UOS AI's original
hotkey before activation; `--restore` deletes the custom shortcut and restores
that saved hotkey.

## System-wide takeover (sudo)

For a system-wide install, use the script:

```bash
# Build first: ./build.sh
sudo packaging/install-service.sh --prefix /usr

# Put the original UOS AI files back:
sudo packaging/install-service.sh --restore
```

The script:

1. backs up UOS AI's two D-Bus service files and its autostart entry under
   `/var/lib/uos-ai-libre/backup`;
2. renames the UOS AI autostart entry to `*.disabled-by-uos-ai-libre`;
3. stops a running `/usr/bin/uos-ai-assistant`;
4. installs UOS AI Libre and writes activation files whose `Exec` points at the
   selected prefix;
5. reloads the session bus (when run as a user) and asks D-Bus to activate the
   new service.

Run it with `sudo` to install into `/usr`; after that, the next request from
screen recorder, Grand Search or another DDE component activates UOS AI Libre.
When run through `sudo`, the script cannot safely start a user service, so it
leaves activation for the next D-Bus request/login.

The DDE `Super+Space` keybinding is per-user state. A system package cannot
change it from `%post`; either ship a DSG shortcut definition for a new
keybinding entry or run `install-user-service.sh` for each user (it also
registers the custom shortcut and can restore the original).

A staged test that does not touch the system:

```bash
rm -rf /tmp/uos-ai-libre-stage
mkdir -p /tmp/uos-ai-libre-stage/usr/share/dbus-1/services
# Optional: put fake UOS AI files there first.
packaging/install-service.sh --destdir /tmp/uos-ai-libre-stage \
    --build-dir uos-ai-libre/build --yes --no-install
packaging/install-service.sh --destdir /tmp/uos-ai-libre-stage --restore --yes
```

`--dry-run` prints every operation without changing files.

For distribution packages, prefer the package manager's conflict mechanism over
this script. The RPM spec should use `Obsoletes: uos-ai` (and/or
`Conflicts: uos-ai`) so the old service files cannot coexist with ours.
