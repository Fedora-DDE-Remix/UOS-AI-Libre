# UOS AI Libre — Qt Quick frontend

A native, local-first desktop AI client for the vendored
[aish](../../vendor/aish) agent. The UI is pure Qt Quick / QML and does not use
Qt WebEngine, Electron or a browser runtime.

The visual language follows UOS AI 3.3: a soft aurora sidebar, a centered
welcome composer, suggestion cards, streaming Markdown answers, reasoning and
tool cards, a preview panel and a settings overlay. History items expose a
three-dot action menu and inline rename; the application menu keeps the theme
choices in a side submenu. Agent-specific behaviour (permissions, approvals,
tool output, MCP/skills boundaries) follows aish rather than copying UOS AI
internals.

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

The backend is deliberately **aish's frontend, not a second agent**. It
registers aish's full tool pool (bash, file read/write/edit/undo, Python,
glob/grep, web fetch/search, ask_user, plan mode, memory, skills,
Agent/subagents, skill registry and MCP gateways), loads the same
`security_policy.yaml`, writes aish's audit table, and restores the same
`sessions.state.cwd` and context snapshot. The only GUI-owned state is
presentation state (theme, window geometry); permissions, tools, model,
skills, MCP, sessions and workspace/cwd are all aish state.

Rules that keep the UI maintainable:

- QML never includes Rust headers, never calls `cxx` functions, and never sees
  aish's event schema. It only talks to `AppController`.
- `AishBridge` owns the FFI/JSON/panic boundary and event polling.
- `AppController` owns session lifecycle, streaming-to-`ChatModel` adaptation,
  titles, toast/approval signals and settings commands.
- Presentation-only models (`ChatModel`, `MarkdownModel`) never call the agent.
- Visual components are composable and unaware of the backend:
  `Composer`, `AssistantMessage`, `MarkdownView`, `ToolGroupCard`,
  `ReasoningCard`, `ApprovalDialog`, `AskUserDialog`, `PreviewPanel`, etc.
- The composer shows the active aish session cwd and effective security
  policy, not a GUI-owned workspace or permission mode. History rows show the
  same session cwd.
- Settings pages are views over aish state: security policy, registered tool
  catalog, installed skills, MCP manager status, long-term memory and audit
  events. The security page can edit global policy fields and the ordered raw
  `rules:` entries (with a fallback-engine preview); built-in rules are shown
  read-only. The skills page can search registries, install into quarantine,
  review/verify and trust/remove; the MCP page can enable/disable/trust/test,
  add/update/remove servers and edit server options. Connection-level MCP YAML
  and registry file settings remain aish-native.
- The model page is provider-centric: providers come from aish's primary
  endpoint and `api_accounts`, can be added with a custom display name, live
  model discovery, and multiple enabled models per provider. Third-party
  providers support OpenAI-compatible endpoints only; Anthropic-native
  endpoints are shown read-only and must be maintained in `config.yaml`. The
  chat composer lists every enabled OpenAI-compatible model across providers,
  with the model on the left and provider on the right and a search box above
  the list. The provider dialog caches the last fetched catalogue per endpoint,
  so editing a provider shows every model from that fetch with the enabled
  ones checked; long catalogues scroll inside the dialog.
- Assistant turns render reasoning, text segments and tool groups in event
  order: a text segment streamed before a tool call stays above that tool group,
  and later text appears below it. Completed segments are immutable timeline
  entries; only the currently streaming text segment is updated per delta.
- "本会话允许" for WebFetch/WebSearch is tool-wide for the current session, so
  a later fetch on another host does not prompt again. It is listed as a
  `tool` entry on the security page and can be revoked independently.
- The audit page supports free-text filtering across event type, tool, command,
  arguments, decision and rule fields, and a confirmed "清空" action that
  removes aish's `audit_events` rows.
- Each user bubble exposes copy and undo actions on hover. Undo asks for
  confirmation, is disabled while a turn is running, and deletes that user
  message plus every later message from both the live history and
  `ai_messages`. The removed text is put back into the composer. The composer
  sends on Enter and inserts a newline on Shift+Enter or Ctrl+Enter.
- Long conversations load lazily: opening a session materialises only the
  newest transcript window, and "加载更早消息" pages older turns into the same
  model without re-reading the whole conversation into QML.
- QML is compiled at build time by `qmlcachegen`; the bytecode and AOT
  functions are linked into the executable through `qt_add_qml_module`. Both
  build scripts check that the QML cache loader was produced, so a packaged
  binary never falls back to parsing QML from the source tree at startup.
- The sidebar history list defaults to sessions whose saved cwd matches the
  current workspace. The globe button in the header switches to all workspaces,
  sorted by `updated_at`; the list scrollbar sits at the sidebar edge with a
  content inset so it does not touch history rows. The composer's workspace
  row shows `当前工作目录:` plus the full path, elided in the middle when
  needed, and its folder icon aligns with the suggestion-card icons.
- Menus and secondary menus open with a short fade/scale transition from
  their top edge instead of appearing instantly.
- Session-scoped "本会话允许" approvals are listed on the security page and can
  be revoked or cleared. They are never persisted; a policy change invalidates
  them through aish's policy fingerprint.
- Skill/MCP changes refresh the live `skill`/`mcp__*` tools without restarting
  the app. Settings jobs (registry search/install, MCP apply/probe/reload) run
  in the background and report through `settings_job` events.
- QML is grouped by domain. `controls/` holds reusable base controls such as
  `AppButton` and `IconButton`; each page directory owns its skeleton and
  page-private pieces. Variants are expressed as properties (for example
  `AppButton.variant`) rather than deep inheritance chains.

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

For day-to-day development use the incremental debug build:

```bash
./build-debug.sh
./build-debug/uos-ai-libre
ctest --test-dir build-debug --output-on-failure
```

It builds the Rust bridge with Cargo's `dev` profile and the Qt frontend into
`build-debug/` with `CMAKE_BUILD_TYPE=Debug`. Both Cargo and Ninja caches are
reused, so repeated runs only compile changed Rust crates, C++ sources or QML
files. `build-debug/compile_commands.json` is generated for clangd/IDE use.

Requirements: Qt 6.8 (Quick, Controls 2, Svg), CMake 3.21+, Ninja, a C++17
compiler, Rust stable, and `cxxbridge-cmd` 1.0.205 for the header generator.
`build.sh`/`build-debug.sh` look for `cxxbridge` in PATH or
`~/.cargo/bin/cxxbridge`; if it is missing they install the pinned version with
`cargo install cxxbridge-cmd --version 1.0.205 --locked`. No `/tmp` tool copy
is required.

Run:

```bash
./build/uos-ai-libre
```

The app reads the same `$AISH_CONFIG_DIR/config.yaml` (or
`~/.config/aish/config.yaml`), `security_policy.yaml`, skills directories,
MCP configuration and `sessions.db` as the aish CLI. Model changes made in
Settings are written atomically to `config.yaml`; the API key stays in the
aish config and is never exposed to QML for display. Switching a conversation
restores that session's model and `sessions.state.cwd`, and CLI-created
sessions are loaded from their context snapshot when they predate the GUI
transcript table.

## Tests

- `build/markdown-test` covers the block parser, tables, task lists, CJK soft
  joins, truncation, incremental updates and a 44 KB parse benchmark. It is
  registered with CTest as `markdown-test`.
- Real-provider tests live outside the repository in
  `../../AI后端测试/`. `app_smoke.cpp` drives `AppController` end-to-end and
  verifies streaming, session list/resume/rename/delete and config saving.
  `frontend_smoke.cpp` verifies the full aish tool pool, session cwd resume,
  CLI snapshot fallback, shell/write/Agent turns and audit rows;
  `policy_smoke.cpp` verifies policy read/save and the settings data APIs.
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

Pages: `welcome`, `chat`, `settings`, `approval`, `menu`, `theme`, `context`,
`rename`, `composer`, `tasks`, `knowledge`, `collapsed`. To load a session, set
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

Use the tray's **退出 UOS AI Libre** entry or `Ctrl+Q` to stop the agent
explicitly. Session-manager shutdown (`commitDataRequest` /
`saveStateRequest`) marks the session as ending and forces a clean process exit,
which is allowed during logout/shutdown.

## Wayland, treeland and RHI

The scene-graph backend is selected once at startup, before the first
`QQuickWindow` is created:

1. If `QSG_RHI_BACKEND`, `QT_QUICK_BACKEND` or `QT_OPENGL` is set, the
   application leaves the choice to Qt.
2. A real Vulkan physical device is preferred. The probe loads the Vulkan
   loader in-process, enables the same platform surface extension QRhi uses,
   and ignores CPU/llvmpipe/lavapipe/SwiftShader devices as well as devices
   without a graphics queue.
3. The desktop GPU is identified from the primary screen's DRM connector,
   then an internal eDP/LVDS panel, the only card with connected outputs, or
   the boot VGA card. Its index is passed to QRhi through
   `QT_VK_PHYSICAL_DEVICE_INDEX`, so a hybrid system uses the GPU that drives
   the desktop rather than always picking the discrete GPU.
4. If the desktop GPU has no usable Vulkan device, the application falls back
   to OpenGL instead of silently switching to another discrete Vulkan GPU.
5. Otherwise a temporary offscreen OpenGL context is tried.
6. Offscreen/minimal platforms, or systems without either API, use the Qt Quick
   software renderer.

`UOS_AI_LIBRE_RHI=vulkan|opengl|software|auto` can override the backend.
`UOS_AI_LIBRE_RHI_GPU=vendor:device` (hex, for example `1002:1900`) overrides
the detected desktop GPU; `QT_VK_PHYSICAL_DEVICE_INDEX` is respected if set.
The selected backend and device are printed as `RHI backend: ...` on startup. No `vulkaninfo` or other external process is
launched.

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
src/rhibackend-selector.*  startup Vulkan/OpenGL/software RHI selection
src/iconprovider.*     tinted SVG icon provider (whitelisted local resources)
qml/Theme.qml          light/dark palette and metrics (singleton)
qml/controls/          AppButton, IconButton, AppIcon, text fields, menus,
                       spinner, toast, window buttons and the application menu
qml/pages/Main.qml     frameless application window and navigation
qml/pages/Shell.qml    persistent tray shell and hidden-window notifications
qml/pages/PlaceholderPage.qml
qml/pages/chat/        ChatPage skeleton plus composer, messages and tool cards
qml/pages/settings/    SettingsPage and its section placeholders
qml/sidebar/           Sidebar, SidebarItem, HistoryRow, SessionMenu
qml/markdown/          MarkdownView and block delegates
qml/dialogs/           approval, confirm, model and about dialogs
resources/             icons, logo, desktop entry
tests/markdown_test.cpp
```

## License

GPL-3.0-or-later. Vendored aish remains Apache-2.0; see
`../../vendor/aish/LICENSE`.
