//! Headless chat/agent session API.
//!
//! This crate is deliberately UI-agnostic: it reuses `aish-llm`, `aish-tools`,
//! `aish-session` and the provider configuration, and produces serializable
//! events that a native frontend (QML, AppKit, GTK, ...) can render.
//!
//! The API is intentionally small and queue based. A frontend can either poll
//! [`ChatService::poll_event`] or block in [`ChatService::wait_event`] on a
//! worker thread and marshal the result to its UI thread.

use std::collections::{HashMap, VecDeque};
use std::fs;
use std::path::Path;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{Arc, Condvar, Mutex};
use std::time::{Duration, SystemTime, UNIX_EPOCH};

use aish_core::{ContextToolCall, LlmEvent, LlmEventType, MemoryType};
use aish_llm::{
    ApprovalChoice, ApprovalMemory, ChatMessage, LlmSession, MessageContent,
    PreflightSecurityContext, ToolCall,
};
use aish_session::{SessionContextMessage, SessionStateSnapshot, SessionStore};
use aish_tools::{GlobTool, GrepTool, ReadFileTool, WebFetchTool, WebSearchConfig, WebSearchTool};
use base64::Engine;
use chrono::Utc;
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use tokio::runtime::Runtime;
use uuid::Uuid;

const DEFAULT_SYSTEM_PROMPT: &str = "\
You are UOS AI Libre, a helpful desktop AI assistant running on Linux.\n\
Answer in the user's language. Use GitHub-flavored Markdown.\n\
Be accurate and concise. If you use a tool, explain the result to the user.\n\
When the user attaches an image, inspect the actual image content before answering.\n";

const MAX_IMAGE_BYTES: u64 = 10 * 1024 * 1024;

/// Configuration accepted by [`ChatService::new`].
///
/// It intentionally mirrors the fields aish already owns; a GUI can build this
/// from `~/.config/aish/config.yaml` or from its own settings page.
#[derive(Debug, Clone, Deserialize)]
pub struct ChatConfig {
    pub model: String,
    #[serde(default = "default_api_base")]
    pub api_base: String,
    #[serde(default)]
    pub api_key: String,
    #[serde(default)]
    pub temperature: Option<f64>,
    #[serde(default)]
    pub max_tokens: Option<u32>,
    #[serde(default)]
    pub max_context_tokens: Option<usize>,
    #[serde(default)]
    pub system_prompt: Option<String>,
    /// Optional SQLite database. When set, every turn is persisted in the same
    /// `sessions`/`ai_messages` schema used by the aish CLI.
    #[serde(default)]
    pub session_db_path: Option<String>,
    #[serde(default = "default_true")]
    pub enable_tools: bool,
}

fn default_true() -> bool {
    true
}

fn default_api_base() -> String {
    "https://api.openai.com/v1".to_string()
}

impl ChatConfig {
    pub fn from_json(json: &str) -> Result<Self, String> {
        serde_json::from_str(json).map_err(|error| format!("invalid chat config: {error}"))
    }

    /// Build a chat config from the same `~/.config/aish/config.yaml` (or
    /// `$AISH_CONFIG_DIR/config.yaml`) used by the aish CLI.
    pub fn from_aish_config() -> Result<Self, String> {
        let config = aish_config::ConfigLoader::load(None)
            .map_err(|error| format!("failed to load aish config: {error}"))?;
        Ok(Self {
            model: config.model,
            api_base: config.api_base,
            api_key: config.api_key,
            temperature: Some(config.temperature),
            max_tokens: config.max_tokens,
            max_context_tokens: config.context_token_budget,
            system_prompt: None,
            session_db_path: config.session_db_path.or_else(default_session_db_path),
            enable_tools: true,
        })
    }
}

/// Match `SessionStore::open(None)` so the GUI and CLI share one transcript.
fn default_session_db_path() -> Option<String> {
    if let Ok(base) = std::env::var("XDG_DATA_HOME") {
        if !base.trim().is_empty() {
            return Some(format!("{base}/aish/sessions.db"));
        }
    }
    let home = std::env::var("HOME").ok()?;
    Some(format!("{home}/.local/share/aish/sessions.db"))
}

/// One serializable event emitted by a chat session.
#[derive(Debug, Clone, Serialize)]
pub struct ChatEvent {
    pub session_id: String,
    /// Stable machine-readable event name, for example `content_delta`.
    pub kind: String,
    pub data: Value,
    pub timestamp: f64,
}

impl ChatEvent {
    fn new(session_id: impl Into<String>, kind: impl Into<String>, data: Value) -> Self {
        Self {
            session_id: session_id.into(),
            kind: kind.into(),
            data,
            timestamp: now_timestamp(),
        }
    }

    fn from_llm(session_id: &str, event: LlmEvent) -> Self {
        let kind = match event.event_type {
            LlmEventType::OpStart => "op_start",
            LlmEventType::OpEnd => "op_end",
            LlmEventType::ContextCompactionStart => "context_compaction_start",
            LlmEventType::ContextCompactionEnd => "context_compaction_end",
            LlmEventType::GenerationStart => "generation_start",
            LlmEventType::GenerationEnd => "generation_end",
            LlmEventType::ContentDelta => "content_delta",
            LlmEventType::ReasoningStart => "reasoning_start",
            LlmEventType::ReasoningDelta => "reasoning_delta",
            LlmEventType::ReasoningEnd => "reasoning_end",
            LlmEventType::ToolExecutionStart => "tool_start",
            LlmEventType::ToolExecutionEnd => "tool_end",
            LlmEventType::Error => "error",
            LlmEventType::ToolConfirmationRequired => "tool_confirmation_required",
            LlmEventType::InteractionRequired => "interaction_required",
            LlmEventType::Cancelled => "cancelled",
        };
        Self {
            session_id: session_id.to_string(),
            kind: kind.to_string(),
            data: event.data,
            timestamp: event.timestamp,
        }
    }
}

struct EventQueue {
    events: Mutex<VecDeque<ChatEvent>>,
    available: Condvar,
}

impl EventQueue {
    fn new() -> Self {
        Self {
            events: Mutex::new(VecDeque::new()),
            available: Condvar::new(),
        }
    }

    fn push(&self, event: ChatEvent) {
        if let Ok(mut queue) = self.events.lock() {
            queue.push_back(event);
            self.available.notify_one();
        }
    }

    fn poll(&self) -> Option<ChatEvent> {
        self.events.lock().ok()?.pop_front()
    }

    fn wait(&self, timeout: Duration) -> Option<ChatEvent> {
        let queue = self.events.lock().ok()?;
        let (mut queue, _) = self
            .available
            .wait_timeout_while(queue, timeout, |queue| queue.is_empty())
            .ok()?;
        queue.pop_front()
    }
}

/// A chat/agent service owning one tokio runtime and any number of sessions.
pub struct ChatService {
    config: ChatConfig,
    runtime: Runtime,
    sessions: Mutex<HashMap<String, Arc<ChatSession>>>,
    events: Arc<EventQueue>,
    approvals: Arc<Mutex<HashMap<String, std::sync::mpsc::Sender<ApprovalChoice>>>>,
    store: Option<Arc<Mutex<SessionStore>>>,
}

impl ChatService {
    pub fn new(config: ChatConfig) -> Result<Self, String> {
        let runtime = tokio::runtime::Builder::new_multi_thread()
            .enable_all()
            .build()
            .map_err(|error| format!("failed to start tokio runtime: {error}"))?;

        let store = match config.session_db_path.as_deref() {
            Some(path) if !path.trim().is_empty() => {
                let store = SessionStore::open(Some(Path::new(path)))
                    .map_err(|error| format!("failed to open session database: {error}"))?;
                Some(Arc::new(Mutex::new(store)))
            }
            _ => None,
        };

        Ok(Self {
            config,
            runtime,
            sessions: Mutex::new(HashMap::new()),
            events: Arc::new(EventQueue::new()),
            approvals: Arc::new(Mutex::new(HashMap::new())),
            store,
        })
    }

    pub fn from_json(config_json: &str) -> Result<Self, String> {
        Self::new(ChatConfig::from_json(config_json)?)
    }

    /// Create a service from the shared aish config file.
    pub fn from_aish_config() -> Result<Self, String> {
        Self::new(ChatConfig::from_aish_config()?)
    }

    /// Create a new session and return its id.
    pub fn create_session(&self, model: Option<&str>) -> Result<String, String> {
        let model = model
            .map(str::trim)
            .filter(|value| !value.is_empty())
            .unwrap_or(&self.config.model)
            .to_string();

        if model.is_empty() {
            return Err("model must not be empty".to_string());
        }

        let session_id = if let Some(store) = &self.store {
            let store = store
                .lock()
                .map_err(|_| "session store lock poisoned".to_string())?;
            store
                .create_session(&model, Some(&self.config.api_base))
                .map_err(|error| format!("failed to create session row: {error}"))?
                .session_uuid
        } else {
            Uuid::new_v4().to_string()
        };

        let mut llm = LlmSession::new(
            &self.config.api_base,
            &self.config.api_key,
            &model,
            self.config.temperature,
            self.config.max_tokens,
        );
        if let Some(tokens) = self.config.max_context_tokens {
            llm.set_max_context_tokens(tokens);
        }
        // A GUI renders tokens as they arrive; the terminal client keeps the
        // legacy "render final text" behaviour.
        llm.set_stream_content_deltas(true);
        if self.config.enable_tools {
            register_default_tools(&mut llm, &self.config, &model);
        }

        let events = self.events.clone();
        let event_session_id = session_id.clone();
        llm.set_event_callback(Arc::new(move |event| {
            events.push(ChatEvent::from_llm(&event_session_id, event));
            None
        }));

        let approvals = self.approvals.clone();
        let approval_events = self.events.clone();
        let approval_session_id = session_id.clone();
        let approval_seq = Arc::new(AtomicU64::new(0));
        llm.set_confirmation_callback(Arc::new(move |ctx: &PreflightSecurityContext| {
            let request_id = format!(
                "{}-approval-{}",
                approval_session_id,
                approval_seq.fetch_add(1, Ordering::Relaxed)
            );
            let (sender, receiver) = std::sync::mpsc::channel();
            if let Ok(mut pending) = approvals.lock() {
                pending.insert(request_id.clone(), sender);
            }
            let (risk, allow, require_confirmation) = match ctx.decision.as_ref() {
                Some(decision) => (
                    decision.level.as_str(),
                    decision.allow,
                    decision.require_confirmation,
                ),
                // WebFetch and a few other tools use the generic confirm
                // callback without a SecurityDecision. Surface a conservative
                // level so the UI still has something to render.
                None => ("MEDIUM", true, true),
            };
            let mode = match ctx.mode {
                aish_llm::SecurityPanelMode::Confirm => "confirm",
                aish_llm::SecurityPanelMode::Blocked => "blocked",
                aish_llm::SecurityPanelMode::Info => "info",
            };
            approval_events.push(ChatEvent::new(
                &approval_session_id,
                "approval_request",
                json!({
                    "request_id": request_id,
                    "tool_name": ctx.tool_name,
                    "target": ctx.target,
                    "message": ctx.message,
                    "mode": mode,
                    "risk": risk,
                    "allow": allow,
                    "require_confirmation": require_confirmation,
                }),
            ));
            match receiver.recv_timeout(Duration::from_secs(300)) {
                Ok(choice) => choice,
                Err(_) => {
                    if let Ok(mut pending) = approvals.lock() {
                        pending.remove(&request_id);
                    }
                    ApprovalChoice::Deny
                }
            }
        }));

        llm.set_approval_memory(Arc::new(parking_lot::Mutex::new(ApprovalMemory::new())));

        let session = Arc::new(ChatSession {
            id: session_id.clone(),
            model,
            system_prompt: self
                .config
                .system_prompt
                .clone()
                .filter(|value| !value.trim().is_empty())
                .unwrap_or_else(|| DEFAULT_SYSTEM_PROMPT.to_string()),
            llm: Arc::new(llm),
            history: Mutex::new(Vec::new()),
            running: AtomicBool::new(false),
            cancelled: AtomicBool::new(false),
            events: self.events.clone(),
            store: self.store.clone(),
            session_uuid: session_id.clone(),
        });

        self.sessions
            .lock()
            .map_err(|_| "session map lock poisoned".to_string())?
            .insert(session_id.clone(), session);

        Ok(session_id)
    }

    pub fn send(&self, session_id: &str, text: &str, images: Vec<String>) -> Result<(), String> {
        let session = self.session(session_id)?;
        if !session.begin_turn() {
            return Err("session is already running a turn".to_string());
        }
        let text = text.to_string();
        let session_for_task = session.clone();
        self.runtime.spawn(async move {
            session_for_task.run_turn(text, images).await;
        });
        Ok(())
    }

    pub fn cancel(&self, session_id: &str) {
        if let Ok(sessions) = self.sessions.lock() {
            if let Some(session) = sessions.get(session_id) {
                session.cancel();
            }
        }
    }

    pub fn approve(&self, request_id: &str, decision: i32) -> bool {
        let choice = match decision {
            0 => ApprovalChoice::Once,
            1 => ApprovalChoice::RememberSession,
            2 => ApprovalChoice::ReplyToAi,
            _ => ApprovalChoice::Deny,
        };
        if let Ok(mut pending) = self.approvals.lock() {
            if let Some(sender) = pending.remove(request_id) {
                return sender.send(choice).is_ok();
            }
        }
        false
    }

    pub fn poll_event(&self) -> Option<ChatEvent> {
        self.events.poll()
    }

    pub fn wait_event(&self, timeout: Duration) -> Option<ChatEvent> {
        self.events.wait(timeout)
    }

    pub fn history(&self, session_id: &str) -> Result<Vec<ChatMessage>, String> {
        let session = self.session(session_id)?;
        session
            .history
            .lock()
            .map(|history| history.clone())
            .map_err(|_| "session history lock poisoned".to_string())
    }

    pub fn shutdown(&self) {
        if let Ok(sessions) = self.sessions.lock() {
            for session in sessions.values() {
                session.cancel();
            }
        }
    }

    fn session(&self, session_id: &str) -> Result<Arc<ChatSession>, String> {
        self.sessions
            .lock()
            .map_err(|_| "session map lock poisoned".to_string())?
            .get(session_id)
            .cloned()
            .ok_or_else(|| format!("unknown session: {session_id}"))
    }
}

struct ChatSession {
    id: String,
    model: String,
    system_prompt: String,
    llm: Arc<LlmSession>,
    history: Mutex<Vec<ChatMessage>>,
    running: AtomicBool,
    cancelled: AtomicBool,
    events: Arc<EventQueue>,
    store: Option<Arc<Mutex<SessionStore>>>,
    session_uuid: String,
}

impl ChatSession {
    fn begin_turn(&self) -> bool {
        self.cancelled.store(false, Ordering::SeqCst);
        self.running
            .compare_exchange(false, true, Ordering::SeqCst, Ordering::SeqCst)
            .is_ok()
    }

    fn cancel(&self) {
        self.cancelled.store(true, Ordering::SeqCst);
        self.llm.cancellation_token().cancel();
    }

    async fn run_turn(self: Arc<Self>, text: String, images: Vec<String>) {
        if self.cancelled.swap(false, Ordering::SeqCst) {
            self.emit("cancelled", json!({"reason": "cancelled_before_start"}));
            self.running.store(false, Ordering::SeqCst);
            return;
        }

        let image_urls = match images
            .iter()
            .map(|path| encode_image(path))
            .collect::<Result<Vec<_>, _>>()
        {
            Ok(urls) => urls,
            Err(error) => {
                self.emit("error", json!({"message": error}));
                self.running.store(false, Ordering::SeqCst);
                return;
            }
        };

        self.emit(
            "turn_start",
            json!({
                "model": self.model,
                "text": text,
                "image_count": image_urls.len(),
            }),
        );

        let user_message = if image_urls.is_empty() {
            ChatMessage::user(text)
        } else {
            ChatMessage::user_with_images(text, image_urls)
        };

        let history = match self.history.lock() {
            Ok(history) => history.clone(),
            Err(_) => {
                self.emit("error", json!({"message": "session history lock poisoned"}));
                self.running.store(false, Ordering::SeqCst);
                return;
            }
        };

        let result = self
            .llm
            .process_input(&user_message, &history, Some(&self.system_prompt), true)
            .await;

        match result {
            Ok(result) => {
                // `ProcessResult::new_messages` contains tool-loop messages
                // (assistant tool calls + tool results) but not the final
                // assistant text. Persist both so history and SQLite round-trip
                // the complete turn.
                let mut new_messages = Vec::with_capacity(2 + result.new_messages.len());
                new_messages.push(user_message);
                new_messages.extend(result.new_messages.iter().cloned());
                new_messages.push(ChatMessage::assistant(result.text.clone()));

                if let Ok(mut history) = self.history.lock() {
                    history.extend(new_messages.iter().cloned());
                }
                self.persist(&new_messages);

                let stats = self.llm.token_stats();
                self.emit(
                    "turn_done",
                    json!({
                        "text": result.text,
                        "usage": {
                            "input_tokens": stats.total_input,
                            "output_tokens": stats.total_output,
                            "cached_tokens": stats.cached_input,
                            "request_count": stats.request_count,
                            "last_prompt_tokens": stats.last_prompt_tokens,
                        },
                    }),
                );
            }
            Err(error) => {
                let kind = if self.cancelled.load(Ordering::SeqCst) {
                    "cancelled"
                } else {
                    "error"
                };
                self.emit(kind, json!({"message": error.to_string()}));
            }
        }

        self.running.store(false, Ordering::SeqCst);
    }

    fn persist(&self, messages: &[ChatMessage]) {
        let Some(store) = &self.store else {
            return;
        };
        let Ok(store) = store.lock() else {
            return;
        };
        for message in messages {
            let persisted = to_session_message(message);
            if let Err(error) = store.append_ai_message(&self.session_uuid, &persisted) {
                tracing::warn!(%error, session = %self.session_uuid, "failed to persist chat message");
            }
        }

        // Keep the compacted `sessions.state` snapshot in sync with the
        // transcript. The aish CLI uses this snapshot when resuming, while the
        // GUI loads the append-only `ai_messages` table.
        let snapshot_messages: Vec<SessionContextMessage> = self
            .history
            .lock()
            .map(|history| history.iter().map(to_session_message).collect())
            .unwrap_or_default();
        let summary_preview = snapshot_messages
            .iter()
            .find(|message| message.role == "user")
            .map(|message| summarize(&message.content, 80));
        let snapshot = SessionStateSnapshot {
            cwd: std::env::current_dir()
                .ok()
                .map(|path| path.to_string_lossy().to_string()),
            summary_preview,
            context_messages_snapshot: snapshot_messages,
            updated_at: Some(Utc::now()),
        };
        if let Err(error) = store.update_session_state(&self.session_uuid, &snapshot) {
            tracing::warn!(%error, session = %self.session_uuid, "failed to persist session state");
        }
    }

    fn emit(&self, kind: &str, data: Value) {
        self.events
            .push(ChatEvent::new(self.id.clone(), kind, data));
    }
}

fn register_default_tools(llm: &mut LlmSession, config: &ChatConfig, model: &str) {
    llm.register_tool(Box::new(WebSearchTool::from_config(
        &WebSearchConfig::from_env(),
    )));
    llm.register_tool(Box::new(WebFetchTool::new(
        &config.api_base,
        &config.api_key,
        model,
        config.temperature,
        config.max_tokens,
    )));
    llm.register_tool(Box::new(ReadFileTool::new()));
    llm.register_tool(Box::new(GlobTool::new()));
    llm.register_tool(Box::new(GrepTool::new()));
}

fn summarize(text: &str, max_chars: usize) -> String {
    let mut summary: String = text.chars().take(max_chars).collect();
    if text.chars().count() > max_chars {
        summary.push('…');
    }
    summary
}

fn to_session_message(message: &ChatMessage) -> SessionContextMessage {
    let content = message
        .content
        .as_ref()
        .and_then(|content| content.to_text())
        .unwrap_or_default();
    let tool_calls = message.tool_calls.as_ref().map(|calls| {
        calls
            .iter()
            .map(|call| ContextToolCall {
                id: call.id.clone(),
                name: call.name.clone(),
                arguments: call.arguments.clone(),
            })
            .collect()
    });
    SessionContextMessage {
        role: message.role.clone(),
        content,
        memory_type: MemoryType::Llm,
        name: message.name.clone(),
        tool_call_id: message.tool_call_id.clone(),
        tool_calls,
        reasoning_content: message.reasoning_content.clone(),
    }
}

#[allow(dead_code)]
fn from_session_message(message: SessionContextMessage) -> ChatMessage {
    ChatMessage {
        role: message.role,
        content: if message.content.is_empty() {
            None
        } else {
            Some(MessageContent::Text(message.content))
        },
        tool_calls: message.tool_calls.map(|calls| {
            calls
                .into_iter()
                .map(|call| ToolCall {
                    id: call.id,
                    name: call.name,
                    arguments: call.arguments,
                })
                .collect()
        }),
        tool_call_id: message.tool_call_id,
        name: message.name,
        reasoning_content: message.reasoning_content,
        cache_control: None,
    }
}

fn encode_image(path: &str) -> Result<String, String> {
    let metadata =
        fs::metadata(path).map_err(|error| format!("cannot read image {path}: {error}"))?;
    if !metadata.is_file() {
        return Err(format!("image path is not a file: {path}"));
    }
    if metadata.len() > MAX_IMAGE_BYTES {
        return Err(format!(
            "image is too large: {path} ({} bytes, max {MAX_IMAGE_BYTES})",
            metadata.len()
        ));
    }
    let bytes = fs::read(path).map_err(|error| format!("cannot read image {path}: {error}"))?;
    let mime = match Path::new(path)
        .extension()
        .and_then(|extension| extension.to_str())
        .unwrap_or_default()
        .to_ascii_lowercase()
        .as_str()
    {
        "png" => "image/png",
        "jpg" | "jpeg" => "image/jpeg",
        "webp" => "image/webp",
        "gif" => "image/gif",
        "bmp" => "image/bmp",
        other => return Err(format!("unsupported image extension: {other}")),
    };
    Ok(format!(
        "data:{mime};base64,{}",
        base64::engine::general_purpose::STANDARD.encode(bytes)
    ))
}

fn now_timestamp() -> f64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_secs_f64())
        .unwrap_or(0.0)
}
