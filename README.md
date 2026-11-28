# UOS-AI-Libre

A native, no-WebEngine desktop frontend for the [aish](https://github.com/AI-Shell-Team/aish)
agent engine. The UI is pure Qt Quick / QML and shares aish's configuration,
sessions, tools, MCP/skills and security policy.

## Current state

- `uos-ai-libre/` — Qt 6 / QML desktop client with a frameless UOS-AI-style
  shell, streaming Markdown chat, reasoning/tool cards, approvals, history,
  settings, themes and a preview panel. Closing the window keeps the agent
  runtime alive; relaunching is single-instance and reopens a fresh window.
  Native Wayland/treeland is supported through standard `xdg-shell`, with
  OpenGL, Vulkan and software RHI backends.
- AI FollowAlong: select text to show a 24 px AI icon; click it to open the
  non-draggable action pill (search, explain, summarize, translate, continue,
  expand, correct, polish, Ask AI). The pill is clamped to the screen on all
  edges, click-away hides only the pill, and every action reuses the main
  window with a new aish session. X11 uses PRIMARY + XRecord; Treeland uses
  `wlr-data-control`, `treeland-active-notify` and the `treeland-dde-shell-v1`
  overlay with an explicit XWayland pointer position when available.
- `vendor/aish/` — vendored `LFRon/aish-enhanced` source, including the new
  headless `aish-chat` crate and session-management additions.
- `vendor/aish-cxxbridge/` — Rust `cxx` bridge exposing `ChatService` to C++/Qt.
- `AI后端测试/` — AI-backend smoke tests and real-provider checks. This
  directory is intentionally outside the repository; it is not committed.

## Architecture

```text
QML (Qt Quick, no WebEngine)
        │  app.* / app.chat
        ▼
AppController (Qt application service, no aish internals)
        │
        ▼
AishBridge (JSON/cxx boundary)
        │
        ▼
aish-chat (headless agent API)        [vendored]
        │
        ├─ aish-llm / aish-tools
        ├─ aish-session (SQLite)
        └─ aish-config / aish-security
```

The bridge and service layer share aish configuration and the
`sessions`/`ai_messages` schema. They do not reimplement model providers, tools,
MCP, skills or security policy. QML pages never touch the Rust/cxx layer.

## Build and run

```bash
cd vendor/aish-cxxbridge
./build.sh

cd ../uos-ai-libre
./build.sh
./build/uos-ai-libre
```

See `uos-ai-libre/README.md` for architecture, testing, snapshot mode and
known limits.

## Debian package

Debian packaging lives in `debian/` at the repository root and builds a native
`uos-ai-libre` 1.0.0 binary package:

```bash
sudo apt build-dep .
dpkg-buildpackage -us -uc -b -j14
```

The result is `../uos-ai-libre_1.0.0_amd64.deb`.  The build compiles
`vendor/aish-cxxbridge` first, then the Qt/QML frontend, and installs the
binary, desktop/icon files, D-Bus activation files and the XDG autostart entry.
The package declares `Conflicts`/`Replaces`/`Provides` for `uos-ai`,
`uos-ai-rag`, and `uos-ai-agent`, so installing it replaces all three while
keeping the `com.deepin.copilot` and `com.iflytek.aiassistant` contracts
available.

On Deepin/DDE the package installs a real dde-shell dock applet, matching UOS
AI's original implementation:

```text
/usr/lib/x86_64-linux-gnu/dde-shell/org.deepin.ds.dock.aibar.so
/usr/share/dde-shell/org.deepin.ds.dock.aibar/
```

It reuses UOS AI's `org.deepin.ds.dock.aibar` plugin id and
`AppletDockItem.dockOrder: 26`, so it sits on the right side of the dock,
between the tray (25) and Show Desktop (30). The application suppresses its
StatusNotifierItem on DDE to avoid a duplicate entry. Restart dde-shell or log
out/in after installing:

```bash
systemctl --user restart dde-shell@DDE.service
```

Rust dependencies are pinned by `vendor/aish-cxxbridge/Cargo.lock`.  A clean
build needs either network access to crates.io or a populated Cargo registry
cache; the generated cxx headers are checked in, so the external `cxxbridge`
executable is optional.

## Testing

```bash
cd uos-ai-libre
ctest --test-dir build --output-on-failure
```

Real-provider tests are kept outside the repository in `../AI后端测试/`; they
cover streaming text, image input, tools, approvals, reasoning and the Qt
application service. No API key is stored in this repository.

## License

GPL-3.0-or-later. The vendored aish source remains Apache-2.0; see
`vendor/aish/LICENSE` and `vendor/aish/PATCHES.md`.
