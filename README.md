# UOS-AI-Libre

A native, no-WebEngine desktop frontend for the [aish](https://github.com/AI-Shell-Team/aish)
agent engine. The QML UI is intentionally not started yet; this repository
currently contains the vendored aish source and the C++ bridge that will back
the QML frontend.

## Current state

- `vendor/aish/` — vendored `LFRon/aish-enhanced` source, including the new
  headless `aish-chat` crate.
- `aish-cxxbridge/` — Rust `cxx` bridge exposing `ChatService` to C++/Qt.
- `AI后端测试/` — AI-backend smoke tests and real-provider checks. This
  directory is intentionally outside the repository; it is not committed.
- No GUI/QML yet.

## Architecture

```text
QML (Qt Quick, no WebEngine)          [later]
        │
        ▼
aish-cxxbridge (C++/Rust cxx)         [this milestone]
        │
        ▼
aish-chat (headless agent API)        [vendored]
        │
        ├─ aish-llm / aish-tools
        ├─ aish-session (SQLite)
        └─ aish-config / aish-security
```

The bridge shares aish configuration and the `sessions`/`ai_messages` schema.
It does not reimplement model providers, tools, MCP, skills or security policy.

## Build the bridge

```bash
cd aish-cxxbridge
./build.sh
```

This produces `target/release/libaish_cxxbridge.a` and
`generated/aish-cxxbridge.h`. See `aish-cxxbridge/README.md` for the C++ API.

## Testing

Real-provider tests are kept outside the repository:

```text
../AI后端测试/
```

They build against `aish-cxxbridge` and cover streaming text, image input,
tool calls, approvals and reasoning events. No API key is stored in this
repository.

## License

GPL-3.0-or-later. The vendored aish source remains Apache-2.0; see
`vendor/aish/LICENSE` and `vendor/aish/PATCHES.md`.
