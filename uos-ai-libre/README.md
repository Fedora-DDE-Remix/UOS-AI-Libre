# UOS AI Libre — Qt Quick frontend

A native, local-first desktop AI client for the vendored
[aish](../../vendor/aish) agent. The UI is pure Qt Quick / QML and does not use
Qt WebEngine, Electron or a browser runtime.

The visual language follows UOS AI 3.3: a soft aurora sidebar, a centered
welcome composer, suggestion cards, streaming Markdown answers, reasoning and
tool cards, a preview panel and a settings overlay. Agent-specific behaviour
(permissions, approvals, tool output, MCP/skills boundaries) follows aish rather
than copying UOS AI internals.

## Architecture

```text
QML pages (Qt Quick only)
        │  app.* / app.chat
        ▼
AppController (Qt application service)     src/appcontroller.cpp
        │  typed properties, commands, signals
        ▼
AishBridge (JSON/cxx boundary)             src/aishbridge.cpp
        │
        ▼
aish-cxxbridge → aish-chat → aish crates  ../../aish-cxxbridge, ../../vendor/aish
```

Rules that keep the UI maintainable:

- QML never includes Rust headers, never calls `cxx` functions, and never sees
  aish's event schema. It only talks to `AppController`.
- `AishBridge` owns the FFI/JSON/panic boundary and event polling.
- `AppController` owns session lifecycle, streaming-to-`ChatModel` adaptation,
  titles, toast/approval signals and settings commands.
- Presentation-only models (`ChatModel`, `MarkdownModel`) never call the agent.
- Visual components are composable and unaware of the backend:
  `Composer`, `AssistantMessage`, `MarkdownView`, `ToolGroupCard`,
  `ReasoningCard`, `ApprovalDialog`, `PreviewPanel`, etc.

## Build

The frontend links the release static library produced by `aish-cxxbridge`.

```bash
cd ../aish-cxxbridge
CXXBRIDGE=/path/to/cxxbridge ./build.sh

cd ../uos-ai-libre
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Or use the convenience script:

```bash
./build.sh
```

Requirements: Qt 6.8 (Quick, Controls 2, Svg), CMake 3.21+, Ninja, a C++17
compiler, Rust stable, and `cxxbridge-cmd` 1.0.205 for the header generator.

Run:

```bash
./build/uos-ai-libre
```

The app reads the same `$AISH_CONFIG_DIR/config.yaml` (or
`~/.config/aish/config.yaml`) and `sessions.db` as the aish CLI. Model changes
made in Settings are written atomically to `config.yaml`; the API key stays in
the aish config and is never exposed to QML for display.

## Tests

- `build/markdown-test` covers the block parser, tables, task lists, CJK soft
  joins, truncation, incremental updates and a 44 KB parse benchmark. It is
  registered with CTest as `markdown-test`.
- Real-provider tests live outside the repository in
  `../../AI后端测试/`. `app_smoke.cpp` drives `AppController` end-to-end and
  verifies streaming, session list/resume/rename/delete and config saving.
- `qmllint` from Qt 6.8 checks every QML file. Remaining warnings concern the
  dynamic `app` context object and `Overlay.overlay` metadata, not runtime
  errors.

### Snapshot mode

The binary has a development-only snapshot mode used for off-screen UI
verification:

```bash
UOS_AI_LIBRE_SNAPSHOT=/tmp/welcome.png \
UOS_AI_LIBRE_SNAPSHOT_PAGE=welcome \
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
AISH_CONFIG_DIR=/tmp/aish-test-config \
./build/uos-ai-libre
```

Pages: `welcome`, `chat`, `settings`, `approval`, `menu`, `composer`, `tasks`,
`knowledge`, `collapsed`. To load a session, set
`UOS_AI_LIBRE_SNAPSHOT_SESSION=<uuid>`. To make a real streaming request, set
`UOS_AI_LIBRE_SNAPSHOT_PROMPT="..."`; the snapshot is taken after the turn
finishes. `UOS_AI_LIBRE_THEME=light|dark` and
`UOS_AI_LIBRE_SETTINGS_INDEX=0..6` are also available.

## Window lifecycle

Closing the window destroys only the QML window. `AppController`, the Rust
agent runtime, SQLite sessions and the tray shell stay alive; a streaming turn
can finish in the background. Relaunching the app is single-instance: the new
process sends `show` over a per-user local socket and exits, and the existing
process creates a fresh window bound to the existing `ChatModel`.

Use the tray or the application menu's **退出 UOS AI Libre** entry to stop the
agent explicitly. Session-manager shutdown (`commitDataRequest` /
`saveStateRequest`) marks the session as ending and forces a clean process exit,
which is allowed during logout/shutdown.

## Wayland, treeland and RHI

The window uses standard `xdg-shell`/Wayland APIs (`startSystemMove`,
`startSystemResize`, frameless client-side decoration), so it does not depend on
X11 or on a treeland-private protocol. It was exercised on a headless wlroots
compositor with:

- `QT_QPA_PLATFORM=wayland QSG_RHI_BACKEND=software`
- `QT_QPA_PLATFORM=wayland QSG_RHI_BACKEND=opengl` (llvmpipe)
- `QT_QPA_PLATFORM=wayland QSG_RHI_BACKEND=vulkan` (lavapipe)

Welcome, Markdown chat, reasoning/tool cards and the settings overlay all
rendered correctly; close-to-background followed by a second launch recreated a
fresh 1280x695 window on the 1280x800 screen. Window size is clamped to the
screen's available geometry while respecting the QML minimum size.

The same build was also exercised on the actual DDE **treeland** session
(`WAYLAND_DISPLAY=treeland.socket`) with the Vulkan RHI and with the OpenGL
path. The tray exported `org.kde.StatusNotifierItem` and a working
`com.canonical.dbusmenu` menu (verified through `busctl`), with the icon
provided as a 22x22 `IconPixmap`; `org.kde.StatusNotifierWatcher` listed the
application. Standard Qt Wayland/xdg-shell is sufficient; no treeland-private
protocol is required.

## Security and limits

- Markdown is rendered as Qt `MarkdownText` block by block; raw HTML is not
  executed, remote images are not loaded, and links are restricted to
  `http`, `https` and `mailto` by `AppController::openExternal`.
- Attachments are only accepted through the picker, screenshot action or
  drag-and-drop; paths are canonicalized, extensions checked and images capped
  at 10 MB by the Rust layer.
- Tool calls still go through aish security policy. The approval dialog is a
  Qt frontend for `approval_request`; stale approvals from a replaced session
  are denied automatically. The dialog auto-denies after 290 seconds.
- Screenshots are written to a `0700` directory with `0600` file permissions
  and cleaned up on startup.
- Workspace switching changes the process cwd, matching aish's current
  process-global tool cwd. Multi-workspace concurrency needs per-session cwd
  support in aish.
- Voice input, IM integration, cloud accounts and an embedded browser are out
  of scope. The mic and browser panel show explanatory toasts.
- Inline math (`$...$`) is not typeset yet; block `$$...$$` renders as a
  readable code-style block.
- Model switching mid-conversation is not wired: a model change in Settings
  resets the current session, and new conversations use the saved model.

## Files

```text
src/aishbridge.*       cxx/JSON boundary, event pump, config/session commands
src/appcontroller.*    application service consumed by QML (sessions, approvals)
src/windowhost.*       disposable QML window; backend survives window close
src/singleinstance.*   per-user local socket, reopen existing instance
src/chatmodel.*        message rows, streaming text/reasoning/tools
src/markdownmodel.*    incremental block parser and diffed list model
src/iconprovider.*     tinted SVG icon provider (whitelisted local resources)
qml/Main.qml           frameless window, navigation, menus, dialogs, toasts
qml/Shell.qml          persistent tray shell and hidden-window notifications
qml/pages/ChatPage.qml welcome/chat composition and session UI state
qml/pages/SettingsPage.qml
qml/components/        reusable visual pieces
qml/Theme.qml          light/dark palette and metrics
resources/             icons, logo, desktop entry
tests/markdown_test.cpp
```

## License

GPL-3.0-or-later. Vendored aish remains Apache-2.0; see
`../../vendor/aish/LICENSE`.
