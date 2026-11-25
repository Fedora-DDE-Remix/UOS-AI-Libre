# aish-cxxbridge

C++/Qt-facing bridge for the vendored `aish-chat` engine. It exposes a small
`BridgeService` with JSON-in/JSON-out methods; QML can wrap it in a `QObject`
singleton and emit Qt signals from `wait_event`/`poll_event`.

This package contains no GUI and no tests. AI-backend smoke tests live in the
sibling `AI后端测试/` directory outside this repository.

## Layout

```text
aish-cxxbridge/
├── Cargo.toml
├── build.rs              # cxx-build glue
├── src/lib.rs            # #[cxx::bridge] + BridgeService
├── build.sh              # cargo build + cxxbridge header generation
└── generated/            # generated at build time (gitignored)
```

The Rust side depends on `../vendor/aish/crates/aish-chat`.

## Build

```bash
cargo install cxxbridge-cmd --version 1.0.205 --locked
./build.sh
```

The result is:

- `target/release/libaish_cxxbridge.a`
- `generated/aish-cxxbridge.h`

A C++ consumer includes `aish-cxxbridge.h` and links the static library. The
static library already contains the cxx C++ glue; do not compile the generated
`.cc` again.

## API

```cpp
auto service = new_service(config_json);
auto created = create_session(*service, model);       // {"ok":true,"session_id":"..."}
auto sent = send_message(*service, session, text, images_json);
while (auto event = wait_event(*service, 1000)) { ... }
approve(*service, request_id, 0);                    // 0 once, 1 remember, 2 reply, 3 deny
cancel_session(*service, session);
auto sessions = list_sessions(*service, 100);        // {"ok":true,"sessions":[...]}
auto resumed = resume_session(*service, session);    // model + serialized transcript
rename_session(*service, session, "New title");
delete_session(*service, session);
auto info = config_info(*service);                   // safe summary; no API key
save_model_config(*service, model, api_base, key);   // writes aish config, reloads service
set_workspace("/path/to/project");                   // process-global, same as aish tools
shutdown(*service);
```

`wait_event` returns an empty string on timeout. Every other response is a JSON
object; errors are `{"ok":false,"error":"..."}`. Events are the `ChatEvent`
shape from `aish-chat`:

```json
{"session_id":"...","kind":"content_delta","data":{"delta":"..."},"timestamp":...}
```

Important event kinds:

- `content_delta`, `reasoning_start`, `reasoning_delta`, `reasoning_end`
- `tool_start`, `tool_end` (includes `duration_ms` and tool `meta`)
- `approval_request` (respond with `approve`)
- `turn_done`, `error`, `cancelled`

## Notes

- The bridge uses JSON strings deliberately so no Rust type crosses FFI and no
  panic can unwind into Qt.
- The Rust runtime is multi-threaded; `wait_event` may be called from a worker
  thread. A Qt wrapper should marshal events to the UI thread with a queued
  signal.
- `aish-chat` shares `~/.config/aish` and the configured `sessions.db`; the
  `session_db_path` field is optional for isolated tests.
- `save_model_config` writes only `model`, `api_base` and `api_key` into the
  raw YAML document (unknown keys survive), then recreates the Rust service so
  the next session uses the new credentials. Active sessions are dropped.
- `set_workspace` changes the process cwd because aish's built-in file/shell
  tools currently use a process-global cwd. Multi-workspace concurrency needs
  per-session cwd support upstream.
- `list_sessions` falls back to the first user message when
  `sessions.state.summary_preview` is empty, so CLI-created sessions appear in
  the GUI history too.
