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
append-only `ai_messages` transcript and the `sessions.state` snapshot, so the
CLI's resume selector and the GUI see the same conversation.

## 6. Reasoning ordering

When the first visible `ContentDelta` arrives, `aish-llm` now emits
`ReasoningEnd` before the content delta and then stops the reasoning segment.
A GUI can collapse the thinking card exactly when the answer begins; if a
provider interleaves more reasoning later, a new `ReasoningStart` segment is
opened.
