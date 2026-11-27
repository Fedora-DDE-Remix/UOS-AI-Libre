# Deployment: taking over the UOS AI D-Bus names

Only one process can own `com.deepin.copilot`, `com.iflytek.aiassistant` and
the D-Bus activation files behind them. A package must therefore conflict with
the `uos-ai` package (or `Obsoletes: uos-ai`); the RPM/DEB should own:

```text
/usr/share/dbus-1/services/com.deepin.copilot.service
/usr/share/dbus-1/services/com.iflytek.aiassistant.service
/etc/xdg/autostart/uos-ai-libre-service.desktop
```

On development machines where UOS AI was unpacked manually, use the script:

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
