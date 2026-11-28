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
