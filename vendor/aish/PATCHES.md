# Vendored aish changes

This directory is a vendored copy of `LFRon/aish-enhanced` (see
`VENDORED_FROM`). The following additive changes are maintained here for the
UOS-AI-Libre bridge. They do not change the behavior of the `aish` CLI/TUI.

## 1. `crates/aish-chat` (new)

Headless chat/agent session API used by `aish-cxxbridge`:

- `ChatService` owns the tokio runtime, SQLite session store, approval broker
  and event queue.
- `ChatSession` wraps `aish-llm::LlmSession`, tool registration, history,
  streaming events, cancellation and persistence into the existing
  `sessions`/`ai_messages` schema.
- Image attachments are encoded as OpenAI-compatible `image_url` blocks.
- Approval callbacks are bridged to `approval_request` events and an
  `approve(request_id, decision)` command.

## 2. `aish-llm`: always stream content deltas when requested

`LlmSession` gained `set_stream_content_deltas(bool)`. The terminal renderer
keeps the legacy behavior (content deltas only for tool-call previews); GUI
sessions enable the flag and receive every `ContentDelta`.

## 3. `aish-llm`: richer `ToolExecutionEnd`

The event data now includes `duration_ms` and the tool result `meta`, so a
frontend can render search sources, HTTP status and other structured results
without reparsing formatted text.

## 4. `aish-chat`: complete turn persistence

`ProcessResult::new_messages` contains tool-loop messages but not the final
assistant text. `ChatSession` appends the final assistant message explicitly
before persisting, so the SQLite transcript round-trips a complete turn.

## Updating the vendored tree

Re-vendor from the fork, then re-apply these changes (or cherry-pick the matching
commits from the fork). Keep `VENDORED_FROM` in sync. The frontend should pin a
known commit; do not float on `main`.

## 5. Shared config and session state

`ChatConfig::from_aish_config()` loads `$AISH_CONFIG_DIR/config.yaml` (or the
XDG default) and resolves the default `~/.local/share/aish/sessions.db` when
the config does not override `session_db_path`. `ChatSession` writes both the
append-only `ai_messages` transcript and the `sessions.state` snapshot
(including `cwd`), so the CLI's resume selector and the GUI see the same
conversation and workspace.

## 6. Reasoning ordering

When the first visible `ContentDelta` arrives, `aish-llm` now emits
`ReasoningEnd` before the content delta and then stops the reasoning segment.
A GUI can collapse the thinking card exactly when the answer begins; if a
provider interleaves more reasoning later, a new `ReasoningStart` segment is
opened.

## 7. Session management APIs for the headless client

`ChatService` gained `list_sessions`, `resume_session`, `delete_session` and
`rename_session`. Resuming rebuilds a live `ChatSession` around the persisted
`ai_messages` transcript (falling back to `sessions.state`
`context_messages_snapshot` when the transcript is empty) without creating a
second session row. It also restores the saved `cwd` when the directory still
exists, exactly like the CLI resume path. `SessionStore` also gained
`get_first_user_message` to give older CLI sessions a useful history title.

## 8. Shared model configuration updates

`ChatService::save_model_settings` edits only `model`, `api_base` and `api_key`
in the raw `config.yaml` document (unknown keys are preserved) through
`ConfigLoader::update_raw`, which writes atomically and preserves file mode.
The bridge recreates the service after saving so the next session uses the new
credentials; active sessions are intentionally dropped.

## 9. Session metadata for the Qt history sidebar

`list_sessions` returns a small JSON-friendly summary (session id, model,
summary, cwd, created/updated timestamps). `config_info` exposes the current
model, API base, database path and whether a key exists, but never the key
itself.

## 10. Headless frontend parity with aish-shell

`aish-chat` is not a second agent runtime. It is the headless frontend for the
same aish core:

- Registers the same tool pool as `aish-shell`: `bash`, `read_file`,
  `write_file`, `edit_file`, `undo_edit`, `ask_user`, `python_exec`, `glob`,
  `grep`, `WebFetch`, `WebSearch`, `enter_plan_mode`, `exit_plan_mode`,
  `memory`, `skill`, `Agent` (subagents), skill registry tools and one MCP
  gateway per enabled server.
- Uses the same `security_policy.yaml` through `BashTool` and the same
  approval callback/remembered approvals.
- Writes tool/security events to the shared `audit_events` table with secret
  redaction.
- Loads the same `skills` directories, `mcp` servers, memory, fallback models
  and API accounts from `config.yaml`.
- Persists `sessions.state.cwd` per conversation and passes that cwd to tool
  execution. GUI workspace switching is therefore a view of aish session
  state, not a separate setting.
- Uses the same SQLite `sessions`/`ai_messages` database. GUI-created and
  CLI-created conversations are interchangeable.
- `ask_user` is exposed as an `interaction_required` event. The current Qt
  bridge auto-cancels it until the GUI question card is added; the tool and
  response API are already in place.
