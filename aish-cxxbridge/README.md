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
auto resumed = resume_session(*service, session);    // model, transcript, cwd, warning
rename_session(*service, session, "New title");
delete_session(*service, session);
auto info = config_info(*service);                   // safe summary; no API key
auto tools = tool_names(*service, session);          // aish's registered tool pool
auto catalog = tool_catalog(*service);               // names/descriptions/sources
auto policy = policy_info(*service);                 // policy + raw rule_groups + builtin_rules
save_policy(*service, "{\"default_risk_level\":\"medium\"}");
save_policy_rules(*service, rules_json, fingerprint); // atomic + validated raw YAML edit
auto preview = preview_policy_command(*service, "rm -rf /etc/x", draft_rules_json);
auto approvals = session_approvals(*service, session);
revoke_session_approval(*service, session, "command", "localhost", "pwd");
clear_session_approvals(*service, session);
auto skills = skill_catalog(*service);               // installed aish skills
trust_skill(*service, "some-skill");
skill_read(*service, "some-skill", 262144);
skill_verify(*service, "some-skill");
skill_remove(*service, "some-skill");
skill_search_start(*service, "pdf", 20);             // settings_job result
skill_install_start(*service, "registry/slug");      // quarantined settings_job result
refresh_skills_live(*service);                       // re-register `skill` on live sessions
auto mcp = mcp_status(*service);                     // configured + live MCP status
mcp_add(*service, mcp_args_json);
mcp_update(*service, "server", mcp_args_json);
mcp_remove(*service, "server");
mcp_set_master_enabled(*service, true);
mcp_set_enabled(*service, "server", false);
mcp_set_trust(*service, "server", "*", "ask");
mcp_test(*service, "server");                        // settings_job result
mcp_test_draft(*service, mcp_args_json);
refresh_mcp_tools(*service);                         // live gateway tools
mcp_reload(*service);
auto memory = memory_list(*service, 200);
auto audit = audit_tail(*service, 200);
respond_interaction(*service, request_id, "{\"cancelled\":true}");
save_model_config(*service, model, api_base, key);   // writes aish config, reloads service
set_workspace(*service, session, "/path/to/project");// session cwd, persisted in sessions.state
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
- `interaction_required` (ask_user; respond with `respond_interaction`)
- `turn_done`, `error`, `cancelled`

## Notes

- The bridge uses JSON strings deliberately so no Rust type crosses FFI and no
  panic can unwind into Qt.
- The Rust runtime is multi-threaded; `wait_event` may be called from a worker
  thread. A Qt wrapper should marshal events to the UI thread with a queued
  signal.
- `aish-chat` is a headless frontend for aish, not a second agent runtime. It
  registers the same tool pool as `aish-shell` (bash, file read/write/edit/undo,
  python, glob/grep, web fetch/search, ask_user, plan mode, memory, skills,
  Agent/subagents, skill registry and MCP gateways), loads the same
  `security_policy.yaml`, writes the same audit table and uses the same
  SQLite sessions.
- `aish-chat` shares `~/.config/aish` and the configured `sessions.db`; the
  `session_db_path` field is optional for isolated tests.
- `save_model_config` writes only `model`, `api_base` and `api_key` into the
  raw YAML document (unknown keys survive), then recreates the Rust service so
  the next session uses the new credentials. Active sessions are dropped.
- `set_workspace` updates the session's `sessions.state.cwd` and persists the
  snapshot, matching aish CLI's resume behavior. It is not a separate GUI
  workspace setting.
- `resume_session` restores the saved cwd (with a warning when the directory is
  gone) and falls back to `sessions.state.context_messages_snapshot` when the
  `ai_messages` transcript is empty, so CLI-created sessions are visible in the
  GUI.
- `list_sessions` falls back to the first user message or session cwd when
  `sessions.state.summary_preview` is empty, so CLI-created sessions appear in
  the GUI history too.
- `policy_info` returns both the flattened effective rules and `rule_groups`
  (the raw YAML `rules:` sequence) plus `builtin_rules`. `save_policy_rules`
  edits the raw `path:` shape, validates the whole document and writes
  atomically; a stale fingerprint is rejected. `preview_policy_command` uses
  the same fallback engine as aish and never executes the command.
- `session_approvals`/`revoke_session_approval`/`clear_session_approvals`
  expose the in-memory "本会话允许" memory. It is per session, never persisted,
  and invalidated when `security_policy.yaml` changes.
- `skill_search_start`/`skill_install_start` emit `settings_job` events on the
  same event queue. Installs reuse aish's registry install path and remain
  quarantined (`.untrusted`) until `trust_skill`. `skill_read`,
  `skill_verify`, `skill_remove` and `refresh_skills_live` support review and
  live tool refresh.
- `mcp_*` mutations reuse `aish_mcp::add_to_config`/`merge_into_document`, so
  unknown YAML keys survive. `mcp_status` merges the configured servers from
  `config.yaml` with the running manager (configured-but-disabled servers stay
  visible, per-tool trust is included). `refresh_mcp_tools` re-registers the
  gateway tools on live sessions.
- `settings_job` events are not tied to a chat session:
  `{"kind":"settings_job","data":{"job_id","job","ok","data"|"error"}}`.
