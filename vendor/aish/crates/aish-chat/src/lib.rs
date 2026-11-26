//! Headless frontend adapter for aish.
//!
//! This crate is deliberately UI-agnostic: it reuses aish's LLM loop, tools,
//! security policy, skills, MCP client, memory, audit store, session database
//! and cwd semantics, and produces serializable events that a native frontend
//! (QML, AppKit, GTK, ...) can render. It is not a second agent runtime.
//!
//! The API is intentionally queue based. A frontend can either poll
//! [`ChatService::poll_event`] or block in [`ChatService::wait_event`] on a
//! worker thread and marshal the result to its UI thread.

use std::collections::{HashMap, VecDeque};
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{Arc, Condvar, Mutex};
use std::time::{Duration, SystemTime, UNIX_EPOCH};

use aish_core::{
    AishError, ContextToolCall, LlmEvent, LlmEventType, MemoryCategory, MemoryScope, MemoryType,
};
use aish_llm::{
    ApiAccount, ApprovalChoice, ApprovalMemory, ChatMessage, LlmSession, MessageContent,
    PreflightSecurityContext, RetryPolicy, RotationState, Tool, ToolCall,
};
use aish_mcp::McpManager;
use aish_memory::{MemoryManager, MemorySource};
use aish_prompts::ProjectInstructionsLimits;
use aish_security::secret::SecretScanner;
use aish_session::{
    AuditQuery, AuditStore, SessionContextMessage, SessionStateSnapshot, SessionStore,
};
use aish_skills::SkillManager;
use aish_tools::ask_user::AskUserResponse;
use aish_tools::bash::BashTool;
use aish_tools::fs::SnapshotStore;
use aish_tools::undo_edit::UndoEditTool;
use aish_tools::{
    AgentTool, AskUserTool, EditFileTool, EnterPlanModeTool, ExitPlanModeTool, GlobTool, GrepTool,
    MemorySearchResult, MemoryTool, PythonTool, ReadFileTool, SkillInfo, SkillInstallTool,
    SkillSearchTool, SkillTool, SkillTrustTool, WebFetchTool, WebSearchConfig, WebSearchTool,
    WriteFileTool,
};
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
const MAX_LIVE_SESSIONS: usize = 8;

/// Configuration accepted by [`ChatService::new`].
///
/// The normal frontend path is [`ChatService::from_aish_config`], which reads
/// `~/.config/aish/config.yaml` and all shared aish state. This struct exists
/// so tests and embedders can pass an isolated configuration without writing
/// a user config file.
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
    /// Skill discovery/registry configuration, shared with the aish CLI.
    #[serde(default)]
    pub skills: aish_config::SkillsConfig,
    /// MCP servers, timeouts and output caps, shared with the aish CLI.
    #[serde(default)]
    pub mcp: aish_config::McpConfig,
    /// Long-term memory configuration, shared with the aish CLI.
    #[serde(default)]
    pub memory: Option<aish_config::MemoryConfig>,
    /// Project-level AGENTS.md discovery, shared with the aish CLI.
    #[serde(default)]
    pub project_instructions: aish_config::model::ProjectInstructionsConfig,
    /// Preferred answer language from the aish config.
    #[serde(default)]
    pub output_language: Option<String>,
    /// Message window used by the CLI prompt builder.
    #[serde(default)]
    pub max_llm_messages: Option<usize>,
    /// Ordered model fallbacks, shared with the aish CLI.
    #[serde(default)]
    pub fallback_models: Vec<String>,
    /// Additional provider credentials for quota rotation.
    #[serde(default)]
    pub api_accounts: Vec<aish_config::ApiAccountConfig>,
    #[serde(default = "default_true")]
    pub fallback_revert_on_cooldown: bool,
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
            skills: config.skills,
            mcp: config.mcp,
            memory: config.memory,
            project_instructions: config.project_instructions,
            output_language: config.output_language,
            max_llm_messages: Some(config.max_llm_messages),
            fallback_models: config.fallback_models,
            api_accounts: config.api_accounts,
            fallback_revert_on_cooldown: config.fallback_revert_on_cooldown,
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
    interactions: Arc<Mutex<HashMap<String, std::sync::mpsc::Sender<AskUserResponse>>>>,
    store: Option<Arc<Mutex<SessionStore>>>,
    audit_store: Option<Arc<AuditStore>>,
    /// aish's tool/runtime state is shared by every session, but only one
    /// foreground turn runs at a time (the same model as the aish CLI's
    /// single active shell). The guard also keeps the process cwd stable for
    /// relative tool paths during a turn.
    turn_lock: Arc<tokio::sync::Mutex<()>>,
    mcp_manager: Option<Arc<McpManager>>,
}

impl ChatService {
    pub fn new(config: ChatConfig) -> Result<Self, String> {
        let runtime = tokio::runtime::Builder::new_multi_thread()
            .enable_all()
            .build()
            .map_err(|error| format!("failed to start tokio runtime: {error}"))?;

        let session_db_path = config.session_db_path.clone();
        let store = match session_db_path
            .as_deref()
            .filter(|path| !path.trim().is_empty())
        {
            Some(path) => {
                let store = SessionStore::open(Some(Path::new(path)))
                    .map_err(|error| format!("failed to open session database: {error}"))?;
                Some(Arc::new(Mutex::new(store)))
            }
            None => None,
        };
        let audit_store = session_db_path
            .as_deref()
            .filter(|path| !path.trim().is_empty())
            .and_then(|path| AuditStore::open(Some(Path::new(path))).ok())
            .map(Arc::new);

        // MCP is part of aish's normal tool pool. Keep the manager alive for
        // the lifetime of the service so its background drivers survive
        // individual chat turns.
        let (mcp_settings, mcp_warnings) = aish_mcp::resolve_settings(&config.mcp);
        for warning in &mcp_warnings {
            tracing::warn!(%warning, "MCP configuration warning");
        }
        let mcp_manager = if mcp_settings.is_inactive() {
            None
        } else {
            let manager = McpManager::new(mcp_settings);
            manager.start(runtime.handle());
            Some(manager)
        };

        Ok(Self {
            config,
            runtime,
            sessions: Mutex::new(HashMap::new()),
            events: Arc::new(EventQueue::new()),
            approvals: Arc::new(Mutex::new(HashMap::new())),
            interactions: Arc::new(Mutex::new(HashMap::new())),
            store,
            audit_store,
            turn_lock: Arc::new(tokio::sync::Mutex::new(())),
            mcp_manager,
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

        let cwd = std::env::current_dir()
            .map_err(|error| format!("cannot determine current directory: {error}"))?;
        self.create_session_inner(session_id, model, Vec::new(), cwd)
    }

    /// Build a live session around an existing transcript.
    ///
    /// Used both for new sessions (empty history) and for resuming a session
    /// loaded from SQLite (full history, same session id).
    fn create_session_inner(
        &self,
        session_id: String,
        model: String,
        history: Vec<ChatMessage>,
        cwd: PathBuf,
    ) -> Result<String, String> {
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
        // Match the CLI's multi-account quota rotation and model fallback.
        let mut accounts = vec![ApiAccount {
            name: "primary".to_string(),
            api_key: self.config.api_key.clone(),
            api_base: None,
            model: None,
            weight: 1,
            disabled: self.config.api_key.trim().is_empty(),
        }];
        for account in &self.config.api_accounts {
            accounts.push(ApiAccount {
                name: account.name.clone(),
                api_key: account.api_key.clone(),
                api_base: account.api_base.clone(),
                model: account.model.clone(),
                weight: account.weight.max(1),
                disabled: account.disabled,
            });
        }
        if !self.config.fallback_models.is_empty() || accounts.len() > 1 {
            let mut policy = RetryPolicy::default();
            policy.revert_on_cooldown = self.config.fallback_revert_on_cooldown;
            let rotation = RotationState::new(
                model.clone(),
                accounts,
                self.config.fallback_models.clone(),
                policy,
            );
            if rotation.is_active() {
                llm.set_rotation(rotation);
            }
        }
        // A GUI renders tokens as they arrive; the terminal client keeps the
        // legacy "render final text" behaviour.
        llm.set_stream_content_deltas(true);
        if self.config.enable_tools {
            register_session_tools(
                &mut llm,
                &self.config,
                &model,
                &session_id,
                self.events.clone(),
                self.interactions.clone(),
                self.mcp_manager.as_ref(),
            );
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
                    "cwd": std::env::current_dir()
                        .ok()
                        .map(|path| path.to_string_lossy().to_string()),
                    "analysis": ctx.decision.as_ref().map(|decision| &decision.analysis),
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

        // Match the CLI: tool calls and security decisions are written to the
        // shared audit table, with secrets redacted before they reach SQLite.
        if let Some(audit) = &self.audit_store {
            let policy = aish_security::load_policy(None);
            let scanner = SecretScanner::new(&policy.secret_patterns);
            let redactor: Arc<dyn Fn(&str) -> String + Send + Sync> =
                Arc::new(move |text| aish_security::secret::redact_secrets(text, &scanner));
            let host = Arc::new(Mutex::new(Some(host_name())));
            llm.set_audit_context(
                audit.clone() as Arc<dyn aish_core::AuditSink>,
                Some(redactor),
                session_id.clone(),
                std::env::var("USER").ok(),
                Some(host),
            );
        }

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
            history: Mutex::new(history),
            running: AtomicBool::new(false),
            cancelled: AtomicBool::new(false),
            events: self.events.clone(),
            store: self.store.clone(),
            session_uuid: session_id.clone(),
            cwd: Mutex::new(cwd),
            project_instructions: ProjectInstructionsLimits {
                enabled: self.config.project_instructions.enabled,
                max_file_bytes: self.config.project_instructions.max_file_bytes,
                max_total_bytes: self.config.project_instructions.max_total_bytes,
            },
            output_language: self.config.output_language.clone(),
        });
        session.persist_snapshot();

        if let Ok(mut sessions) = self.sessions.lock() {
            sessions.insert(session_id.clone(), session);
            // A QML history sidebar can resume many old conversations over a
            // long session. Keep only a bounded number of idle live sessions;
            // older ones are reloaded from SQLite on demand.
            if sessions.len() > MAX_LIVE_SESSIONS {
                let idle: Vec<String> = sessions
                    .iter()
                    .filter(|(id, session)| {
                        id.as_str() != session_id && !session.running.load(Ordering::SeqCst)
                    })
                    .map(|(id, _)| id.clone())
                    .take(sessions.len() - MAX_LIVE_SESSIONS)
                    .collect();
                for id in idle {
                    sessions.remove(&id);
                }
            }
        } else {
            return Err("session map lock poisoned".to_string());
        }

        Ok(session_id)
    }

    pub fn send(&self, session_id: &str, text: &str, images: Vec<String>) -> Result<(), String> {
        let session = self.session(session_id)?;
        let cwd = session.cwd_path();
        if !cwd.is_dir() {
            return Err(format!("会话目录已不存在：{}", cwd.display()));
        }
        // Match the aish CLI model: one foreground turn owns the process cwd.
        // The guard is acquired before spawning so a second session cannot
        // silently race relative paths.
        let guard = match self.turn_lock.clone().try_lock_owned() {
            Ok(guard) => guard,
            Err(_) => return Err("another aish session is already running".to_string()),
        };
        if !session.begin_turn() {
            return Err("session is already running a turn".to_string());
        }
        let text = text.to_string();
        let session_for_task = session.clone();
        self.runtime.spawn(async move {
            session_for_task.run_turn(text, images, guard).await;
        });
        Ok(())
    }

    /// Change the active workspace. With a session id this updates the same
    /// `sessions.state.cwd` field the aish CLI restores; without one it only
    /// changes the directory inherited by the next new conversation.
    pub fn set_workspace(&self, session_id: Option<&str>, path: &str) -> Result<String, String> {
        let canonical = fs::canonicalize(path)
            .map_err(|error| format!("cannot open workspace {path}: {error}"))?;
        if !canonical.is_dir() {
            return Err(format!(
                "workspace is not a directory: {}",
                canonical.display()
            ));
        }
        if let Some(session_id) = session_id.filter(|id| !id.trim().is_empty()) {
            let session = self.session(session_id)?;
            if session.running.load(Ordering::SeqCst) {
                return Err("cannot change workspace while this session is running".to_string());
            }
            if let Ok(mut cwd) = session.cwd.lock() {
                *cwd = canonical.clone();
            }
            std::env::set_current_dir(&canonical)
                .map_err(|error| format!("cannot switch workspace: {error}"))?;
            session.persist_snapshot();
        } else {
            std::env::set_current_dir(&canonical)
                .map_err(|error| format!("cannot switch workspace: {error}"))?;
        }
        Ok(canonical.to_string_lossy().to_string())
    }

    /// Answer a pending `ask_user` interaction. The GUI can add a question
    /// card later without changing the tool/runtime boundary.
    pub fn respond_interaction(&self, request_id: &str, response_json: &str) -> bool {
        let response: AskUserResponse = match serde_json::from_str::<Value>(response_json) {
            Ok(value) => {
                if value
                    .get("cancelled")
                    .and_then(Value::as_bool)
                    .unwrap_or(false)
                {
                    AskUserResponse::Cancelled
                } else if let Some(text) = value.get("text").and_then(Value::as_str) {
                    AskUserResponse::Text(text.to_string())
                } else if let Some(text) = value.get("answer").and_then(Value::as_str) {
                    AskUserResponse::Text(text.to_string())
                } else {
                    let selected = value.get("selected").unwrap_or(&Value::Null);
                    let selected_value = selected
                        .get("value")
                        .and_then(Value::as_str)
                        .unwrap_or_default();
                    if selected_value.is_empty() {
                        AskUserResponse::Cancelled
                    } else {
                        AskUserResponse::Selected {
                            value: selected_value.to_string(),
                            label: selected
                                .get("label")
                                .and_then(Value::as_str)
                                .unwrap_or(selected_value)
                                .to_string(),
                            description: selected
                                .get("description")
                                .and_then(Value::as_str)
                                .map(str::to_string),
                        }
                    }
                }
            }
            Err(_) => return false,
        };
        if let Ok(mut pending) = self.interactions.lock() {
            if let Some(sender) = pending.remove(request_id) {
                return sender.send(response).is_ok();
            }
        }
        false
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

    /// Tool names registered for a session. Exposed for frontend diagnostics
    /// and tests so the GUI can prove it is using aish's tool pool rather than
    /// a private subset.
    pub fn tool_names(&self, session_id: &str) -> Result<Vec<String>, String> {
        let session = self.session(session_id)?;
        Ok(session
            .llm
            .tool_specs()
            .into_iter()
            .map(|spec| spec.function.name)
            .collect())
    }

    /// Effective aish security policy for the settings UI. The policy file is
    /// the source of truth; this is a read-only projection of it.
    pub fn policy_info(&self) -> Value {
        let policy = aish_security::load_policy(None);
        let path = aish_security::resolve_security_policy_path(None)
            .map(|path| path.to_string_lossy().to_string())
            .unwrap_or_default();
        json!({
            "ok": true,
            "path": path,
            "default_risk_level": policy.default_risk_level.as_str().to_ascii_lowercase(),
            "enable_sandbox": policy.enable_sandbox,
            "sandbox_off_action": policy.sandbox_off_action.as_str().to_ascii_lowercase(),
            "sandbox_timeout_seconds": policy.sandbox_timeout_seconds,
            "sandbox_socket_available": Path::new("/run/aish/sandbox.sock").exists(),
            "input_guard_enabled": policy.input_guard.enabled,
            "audit_enabled": policy.audit_enabled,
            "rules": policy.rules,
            "validation_issues": policy.validation_issues,
        })
    }

    /// Update the aish security globals through the same YAML-preserving
    /// writer the CLI settings panel uses. Rules remain YAML-edited.
    pub fn save_policy(&self, fields: &Value) -> Result<Value, String> {
        let path = aish_security::resolve_security_policy_path(None)
            .ok_or_else(|| "aish security policy path is unavailable".to_string())?;
        let mut updates: Vec<(&str, String)> = Vec::new();
        if let Some(level) = fields
            .get("default_risk_level")
            .and_then(Value::as_str)
            .map(|value| value.trim().to_ascii_lowercase())
        {
            if !matches!(level.as_str(), "low" | "medium" | "high") {
                return Err("invalid default_risk_level".to_string());
            }
            updates.push(("default_risk_level", level));
        }
        if let Some(action) = fields
            .get("sandbox_off_action")
            .and_then(Value::as_str)
            .map(|value| value.trim().to_ascii_lowercase())
        {
            if !matches!(action.as_str(), "allow" | "confirm" | "block") {
                return Err("invalid sandbox_off_action".to_string());
            }
            updates.push(("sandbox_off_action", action));
        }
        if let Some(timeout) = fields
            .get("sandbox_timeout_seconds")
            .and_then(Value::as_f64)
        {
            if !(0.0..=3600.0).contains(&timeout) {
                return Err("invalid sandbox_timeout_seconds".to_string());
            }
            updates.push(("sandbox_timeout_seconds", timeout.to_string()));
        }
        if let Some(enabled) = fields.get("enable_sandbox").and_then(Value::as_bool) {
            updates.push(("enable_sandbox", enabled.to_string()));
        }
        let input_guard = fields.get("input_guard_enabled").and_then(Value::as_bool);
        let update_refs: Vec<(&str, &str)> = updates
            .iter()
            .map(|(field, value)| (*field, value.as_str()))
            .collect();
        aish_security::save_policy_ui_fields(&path, &update_refs, input_guard)
            .map_err(|error| format!("failed to save security policy: {error}"))?;
        // Re-read so malformed values cannot silently fall back to defaults
        // without the UI noticing.
        let _ = aish_security::load_policy(Some(&path));
        Ok(self.policy_info())
    }

    /// Tool descriptions for the read-only tools page. This builds a
    /// non-persisted aish session so the settings page works before the first
    /// user conversation exists.
    pub fn tool_catalog(&self) -> Value {
        let mut llm = LlmSession::new(
            &self.config.api_base,
            &self.config.api_key,
            &self.config.model,
            self.config.temperature,
            self.config.max_tokens,
        );
        if self.config.enable_tools {
            register_session_tools(
                &mut llm,
                &self.config,
                &self.config.model,
                "tool-catalog",
                self.events.clone(),
                self.interactions.clone(),
                self.mcp_manager.as_ref(),
            );
        }
        let mut specs = llm.tool_specs();
        specs.sort_by(|left, right| left.function.name.cmp(&right.function.name));
        let tools: Vec<Value> = specs
            .into_iter()
            .map(|spec| {
                let name = spec.function.name;
                let source = if name.starts_with("mcp__") {
                    "mcp"
                } else if name == "Agent" {
                    "subagent"
                } else if name == "skill" || name.starts_with("skill_") {
                    "skill"
                } else {
                    "aish"
                };
                json!({
                    "name": name,
                    "description": spec.function.description,
                    "source": source,
                })
            })
            .collect();
        json!({"ok": true, "tools": tools})
    }

    /// Installed aish skills, including quarantine/context metadata.
    pub fn skill_catalog(&self) -> Value {
        let mut manager = SkillManager::new();
        let _ = manager.load_all_skills();
        let mut skills = manager.list_skills();
        skills.sort_by(|left, right| left.metadata.name.cmp(&right.metadata.name));
        let skills: Vec<Value> = skills
            .into_iter()
            .map(|skill| {
                json!({
                    "name": skill.metadata.name,
                    "description": skill.metadata.description,
                    "source": skill.source,
                    "file_path": skill.file_path,
                    "base_dir": skill.base_dir,
                    "context": skill.metadata.context,
                    "agent": skill.metadata.agent,
                    "allowed_tools": skill.metadata.allowed_tools,
                    "quarantined": skill.quarantined,
                })
            })
            .collect();
        let roots: Vec<String> = manager
            .get_skill_dirs()
            .into_iter()
            .map(|path| path.to_string_lossy().to_string())
            .collect();
        json!({"ok": true, "skills": skills, "roots": roots})
    }

    /// Trust one quarantined skill through the same aish marker logic as the
    /// `skill_trust` tool.
    pub fn trust_skill(&self, name: &str) -> Result<Value, String> {
        let tool = SkillTrustTool;
        let result = tool.execute(json!({"skill_name": name}));
        if result.ok {
            Ok(json!({"ok": true, "message": result.output}))
        } else {
            Err(result.output)
        }
    }

    /// MCP server/tool status for the connectors page.
    pub fn mcp_status(&self) -> Value {
        let Some(manager) = &self.mcp_manager else {
            return json!({"ok": true, "enabled": false, "servers": []});
        };
        let servers: Vec<Value> = manager
            .snapshots()
            .into_iter()
            .map(|snapshot| {
                let tools: Vec<Value> = snapshot
                    .tools
                    .iter()
                    .map(|tool| {
                        json!({
                            "name": tool.name,
                            "description": tool.description,
                            "side_effects": tool.side_effects,
                            "required": tool.required,
                        })
                    })
                    .collect();
                json!({
                    "name": snapshot.name,
                    "transport": snapshot.transport,
                    "endpoint": snapshot.endpoint,
                    "status": snapshot.status.label(),
                    "status_detail": match &snapshot.status {
                        aish_mcp::ServerStatus::Connected { protocol_version, tool_count } => {
                            json!({"protocol_version": protocol_version, "tool_count": tool_count})
                        }
                        aish_mcp::ServerStatus::Failed { reason } => json!({"reason": reason}),
                        _ => Value::Null,
                    },
                    "trust": snapshot.trust,
                    "declared_side_effects": snapshot.declared_side_effects,
                    "tools": tools,
                })
            })
            .collect();
        json!({"ok": true, "enabled": manager.is_active(), "servers": servers})
    }

    /// Reconnect all configured MCP servers and return refreshed status.
    pub fn mcp_reload(&self) -> Value {
        if let Some(manager) = &self.mcp_manager {
            let _ = self.runtime.block_on(async {
                manager.reload().await;
            });
        }
        self.mcp_status()
    }

    /// Read the shared aish long-term memory store.
    pub fn memory_list(&self, limit: usize) -> Value {
        let manager = match MemoryManager::new(MemoryManager::default_path()) {
            Ok(manager) => manager,
            Err(error) => {
                return json!({"ok": false, "error": error.to_string(), "entries": []});
            }
        };
        let entries: Vec<Value> = manager
            .list()
            .iter()
            .rev()
            .take(limit.clamp(1, 500))
            .map(|entry| {
                json!({
                    "id": entry.id,
                    "content": entry.content,
                    "category": format!("{:?}", entry.category).to_lowercase(),
                    "scope": format!("{:?}", entry.scope).to_lowercase(),
                    "source": entry.source,
                    "created_at": entry.created_at,
                    "expires_at": entry.expires_at,
                    "expired": MemoryManager::is_expired(entry),
                })
            })
            .collect();
        json!({"ok": true, "entries": entries})
    }

    /// Recent audit events from the shared aish audit table.
    pub fn audit_tail(&self, limit: usize) -> Result<Value, String> {
        let Some(store) = &self.audit_store else {
            return Ok(json!({"ok": true, "events": []}));
        };
        let events = store
            .query(&AuditQuery {
                limit: limit.clamp(1, 500),
                ..AuditQuery::default()
            })
            .map_err(|error| format!("failed to query audit events: {error}"))?;
        Ok(json!({"ok": true, "events": events}))
    }

    /// Resume a persisted session and return its model plus transcript.
    ///
    /// The session id is preserved, so continuing the conversation appends to
    /// the same rows the CLI and the GUI already share.
    pub fn resume_session(
        &self,
        session_id: &str,
    ) -> Result<(String, Vec<ChatMessage>, PathBuf, Option<String>), String> {
        if session_id.trim().is_empty() {
            return Err("session id must not be empty".to_string());
        }

        if let Ok(sessions) = self.sessions.lock() {
            if let Some(session) = sessions.get(session_id) {
                let history = session
                    .history
                    .lock()
                    .map(|history| history.clone())
                    .map_err(|_| "session history lock poisoned".to_string())?;
                let cwd = session.cwd_path();
                let warning = activate_cwd(&cwd);
                return Ok((session.model.clone(), history, cwd, warning));
            }
        }

        let (model, history, cwd, warning) = {
            let Some(store) = &self.store else {
                return Err("session database is not configured".to_string());
            };
            let store = store
                .lock()
                .map_err(|_| "session store lock poisoned".to_string())?;
            let record = store
                .get_session(session_id)
                .map_err(|error| format!("failed to load session: {error}"))?
                .ok_or_else(|| format!("session not found: {session_id}"))?;
            let snapshot = record.state_snapshot();
            let messages = store
                .get_ai_messages(session_id)
                .map_err(|error| format!("failed to load transcript: {error}"))?;
            // CLI sessions predate the GUI transcript table: their complete
            // context lives in sessions.state. Fall back to it instead of
            // showing an empty conversation.
            let history: Vec<ChatMessage> = if messages.is_empty() {
                snapshot
                    .context_messages_snapshot
                    .iter()
                    .cloned()
                    .map(from_session_message)
                    .collect()
            } else {
                messages.into_iter().map(from_session_message).collect()
            };

            let fallback = std::env::current_dir()
                .map_err(|error| format!("cannot determine current directory: {error}"))?;
            let (cwd, warning) = match snapshot.cwd.as_deref() {
                Some(saved) if Path::new(saved).is_dir() => (PathBuf::from(saved), None),
                Some(saved) => (
                    fallback,
                    Some(format!("会话保存的目录已不存在：{saved}；已使用当前目录")),
                ),
                None => (fallback, None),
            };
            (record.model, history, cwd, warning)
        };

        let warning = warning.or_else(|| activate_cwd(&cwd));
        self.create_session_inner(
            session_id.to_string(),
            model.clone(),
            history.clone(),
            cwd.clone(),
        )?;
        Ok((model, history, cwd, warning))
    }

    /// List recent sessions for a history sidebar.
    ///
    /// Only metadata is returned; the transcript is loaded on demand by
    /// [`Self::resume_session`].
    pub fn list_sessions(&self, limit: usize) -> Result<Vec<Value>, String> {
        let Some(store) = &self.store else {
            return Ok(Vec::new());
        };
        let store = store
            .lock()
            .map_err(|_| "session store lock poisoned".to_string())?;
        let records = store
            .list_sessions(limit.clamp(1, 500))
            .map_err(|error| format!("failed to list sessions: {error}"))?;
        let mut sessions = Vec::with_capacity(records.len());
        for record in records {
            let snapshot = record.state_snapshot();
            let cwd = snapshot.cwd.clone();
            let summary = snapshot
                .summary_preview
                .clone()
                .or_else(|| {
                    snapshot
                        .context_messages_snapshot
                        .iter()
                        .find(|message| {
                            message.role == "user" && !message.content.starts_with("[Shell]")
                        })
                        .map(|message| message.content.clone())
                })
                .or_else(|| {
                    store
                        .get_first_user_message(&record.session_uuid)
                        .ok()
                        .flatten()
                })
                .map(|text| summarize(text.trim(), 60))
                .filter(|text| !text.is_empty())
                .or_else(|| {
                    cwd.as_deref()
                        .and_then(|path| Path::new(path).file_name())
                        .map(|name| name.to_string_lossy().to_string())
                })
                .unwrap_or_else(|| "未命名会话".to_string());
            let updated_at = snapshot.updated_at.unwrap_or(record.created_at);
            sessions.push(json!({
                "session_id": record.session_uuid,
                "model": record.model,
                "summary": summary,
                "cwd": cwd,
                "created_at": record.created_at.to_rfc3339(),
                "updated_at": updated_at.to_rfc3339(),
            }));
        }
        Ok(sessions)
    }

    /// Rename a session by updating the shared `summary_preview` field.
    pub fn rename_session(&self, session_id: &str, title: &str) -> Result<(), String> {
        let title = title.trim();
        if title.is_empty() {
            return Err("title must not be empty".to_string());
        }
        if title.chars().count() > 120 {
            return Err("title is too long".to_string());
        }
        let Some(store) = &self.store else {
            return Err("session database is not configured".to_string());
        };
        let store = store
            .lock()
            .map_err(|_| "session store lock poisoned".to_string())?;
        let record = store
            .get_session(session_id)
            .map_err(|error| format!("failed to load session: {error}"))?
            .ok_or_else(|| format!("session not found: {session_id}"))?;
        let mut snapshot = record.state_snapshot();
        snapshot.summary_preview = Some(title.to_string());
        if snapshot.updated_at.is_none() {
            snapshot.updated_at = Some(Utc::now());
        }
        store
            .update_session_state(session_id, &snapshot)
            .map_err(|error| format!("failed to rename session: {error}"))
    }

    /// Delete a persisted session. Running sessions are left untouched so a
    /// turn cannot race the delete and recreate orphan transcript rows.
    pub fn delete_session(&self, session_id: &str) -> Result<(), String> {
        if let Ok(sessions) = self.sessions.lock() {
            if let Some(session) = sessions.get(session_id) {
                if session.running.load(Ordering::SeqCst) {
                    return Err("session is still running".to_string());
                }
                session.cancel();
            }
        }
        let Some(store) = &self.store else {
            return Err("session database is not configured".to_string());
        };
        let store = store
            .lock()
            .map_err(|_| "session store lock poisoned".to_string())?;
        store
            .delete_session(session_id)
            .map_err(|error| format!("failed to delete session: {error}"))?;
        if let Ok(mut sessions) = self.sessions.lock() {
            sessions.remove(session_id);
        }
        Ok(())
    }

    /// Safe, non-secret configuration summary for a settings page.
    pub fn config_info(&self) -> Value {
        json!({
            "model": self.config.model,
            "api_base": self.config.api_base,
            "has_api_key": !self.config.api_key.trim().is_empty(),
            "session_db_path": self.config.session_db_path,
            "enable_tools": self.config.enable_tools,
            "config_path": aish_config::ConfigLoader::default_config_path()
                .to_string_lossy()
                .to_string(),
        })
    }

    /// Update the three model fields in the shared aish YAML config.
    ///
    /// The raw-document editor preserves keys the typed config does not model.
    /// An empty `api_key` keeps the existing key instead of erasing it.
    pub fn save_model_settings(
        model: &str,
        api_base: &str,
        api_key: &str,
    ) -> Result<String, String> {
        let model = model.trim();
        let api_base = api_base.trim();
        if model.is_empty() || model.chars().count() > 200 {
            return Err("model name is empty or too long".to_string());
        }
        if model.contains(['\n', '\r', '\0']) || api_base.contains(['\n', '\r', '\0']) {
            return Err("model configuration contains invalid characters".to_string());
        }
        if !(api_base.starts_with("https://") || api_base.starts_with("http://")) {
            return Err("API base must be an http(s) URL".to_string());
        }
        if api_key.chars().count() > 4096 || api_key.contains(['\n', '\r', '\0']) {
            return Err("API key is too long or contains invalid characters".to_string());
        }

        let path = aish_config::ConfigLoader::default_config_path();
        aish_config::ConfigLoader::update_raw(&path, |document| {
            let map = document.as_mapping_mut().ok_or_else(|| {
                AishError::Config(format!("config root is not a mapping: {}", path.display()))
            })?;
            map.insert(
                serde_yaml::Value::String("model".to_string()),
                serde_yaml::Value::String(model.to_string()),
            );
            map.insert(
                serde_yaml::Value::String("api_base".to_string()),
                serde_yaml::Value::String(api_base.to_string()),
            );
            if !api_key.trim().is_empty() {
                map.insert(
                    serde_yaml::Value::String("api_key".to_string()),
                    serde_yaml::Value::String(api_key.to_string()),
                );
            }
            Ok(())
        })
        .map_err(|error| format!("failed to save model configuration: {error}"))?;
        Ok(path.to_string_lossy().to_string())
    }

    pub fn shutdown(&self) {
        if let Ok(sessions) = self.sessions.lock() {
            for session in sessions.values() {
                session.cancel();
            }
        }
        if let Some(manager) = &self.mcp_manager {
            let _ = self.runtime.block_on(async {
                manager.shutdown().await;
            });
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
    /// The aish session cwd, persisted in `sessions.state.cwd` and restored by
    /// `resume_session`. The GUI must not maintain a second workspace state.
    cwd: Mutex<PathBuf>,
    project_instructions: ProjectInstructionsLimits,
    output_language: Option<String>,
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

    fn cwd_path(&self) -> PathBuf {
        self.cwd
            .lock()
            .map(|cwd| cwd.clone())
            .unwrap_or_else(|_| std::env::current_dir().unwrap_or_else(|_| PathBuf::from("/")))
    }

    fn system_prompt_for_turn(&self) -> String {
        let mut prompt = self.system_prompt.clone();
        if let Ok(cwd) = std::env::current_dir() {
            let now = chrono::Local::now();
            prompt.push_str(&format!(
                "\n\n## Runtime Context\n- Current directory: {}\n- Current date/time: {}\n",
                cwd.display(),
                now.format("%Y-%m-%d %H:%M %:z")
            ));
            if self.project_instructions.enabled {
                let state = aish_prompts::discover(&cwd, &self.project_instructions);
                let rendered = state.rendered();
                if !rendered.is_empty() {
                    prompt.push_str("\n");
                    prompt.push_str(&rendered);
                    prompt.push('\n');
                }
            }
        }
        if let Some(language) = self
            .output_language
            .as_deref()
            .map(str::trim)
            .filter(|value| !value.is_empty())
        {
            prompt.push_str("\nAnswer in ");
            prompt.push_str(language);
            prompt.push_str(".\n");
        }
        prompt
    }

    async fn run_turn(
        self: Arc<Self>,
        text: String,
        images: Vec<String>,
        turn_guard: tokio::sync::OwnedMutexGuard<()>,
    ) {
        let _turn_guard = turn_guard;
        if self.cancelled.swap(false, Ordering::SeqCst) {
            self.emit("cancelled", json!({"reason": "cancelled_before_start"}));
            self.running.store(false, Ordering::SeqCst);
            return;
        }

        let cwd = self.cwd_path();
        if let Err(error) = std::env::set_current_dir(&cwd) {
            self.emit(
                "error",
                json!({"message": format!("无法进入会话目录 {}: {error}", cwd.display())}),
            );
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

        let system_prompt = self.system_prompt_for_turn();
        let result = self
            .llm
            .process_input(&user_message, &history, Some(&system_prompt), true)
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
        if let Ok(store) = store.lock() {
            for message in messages {
                let persisted = to_session_message(message);
                if let Err(error) = store.append_ai_message(&self.session_uuid, &persisted) {
                    tracing::warn!(%error, session = %self.session_uuid, "failed to persist chat message");
                }
            }
        }
        self.persist_snapshot();
    }

    /// Persist the complete aish session snapshot (cwd + context) even when
    /// no new message was produced, e.g. after a workspace change.
    fn persist_snapshot(&self) {
        let Some(store) = &self.store else {
            return;
        };
        let Ok(store) = store.lock() else {
            return;
        };
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
            cwd: Some(self.cwd_path().to_string_lossy().to_string()),
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

/// Register the same core tool pool as the aish CLI. The GUI is a frontend;
/// it must not invent a reduced "permission" set of tools. aish's security
/// policy and approval callback govern how those tools may act.
fn register_session_tools(
    llm: &mut LlmSession,
    config: &ChatConfig,
    model: &str,
    session_id: &str,
    events: Arc<EventQueue>,
    interactions: Arc<Mutex<HashMap<String, std::sync::mpsc::Sender<AskUserResponse>>>>,
    mcp_manager: Option<&Arc<McpManager>>,
) {
    let mut bash = BashTool::new();
    bash.set_cancellation_token(llm.cancellation_token_arc());
    llm.register_tool(Box::new(bash));

    let snapshot_store: aish_tools::fs::SharedSnapshotStore =
        Arc::new(Mutex::new(SnapshotStore::new()));
    llm.register_tool(Box::new(ReadFileTool::with_store(snapshot_store.clone())));
    llm.register_tool(Box::new(WriteFileTool::with_store(snapshot_store.clone())));
    llm.register_tool(Box::new(EditFileTool::with_store(snapshot_store.clone())));
    llm.register_tool(Box::new(UndoEditTool::new(snapshot_store)));

    // ask_user is an interaction like approvals: emit an event and wait for
    // the frontend. A future QML question card can call respond_interaction
    // without changing this runtime boundary.
    let interaction_seq = Arc::new(AtomicU64::new(0));
    let interaction_events = events.clone();
    let interaction_session = session_id.to_string();
    llm.register_tool(Box::new(AskUserTool::with_runtime(Arc::new(
        move |request: &aish_tools::ask_user::AskUserRequest| {
            let request_id = format!(
                "{}-interaction-{}",
                interaction_session,
                interaction_seq.fetch_add(1, Ordering::Relaxed)
            );
            let (sender, receiver) = std::sync::mpsc::channel();
            if let Ok(mut pending) = interactions.lock() {
                pending.insert(request_id.clone(), sender);
            }
            let options: Vec<Value> = request
                .options
                .iter()
                .map(|option| {
                    json!({
                        "value": option.value,
                        "label": option.label,
                        "description": option.description,
                        "recommended": option.recommended,
                    })
                })
                .collect();
            interaction_events.push(ChatEvent::new(
                &interaction_session,
                "interaction_required",
                json!({
                    "request_id": request_id,
                    "kind": "ask_user",
                    "title": request.title,
                    "prompt": request.prompt,
                    "options": options,
                    "default": request.default,
                    "placeholder": request.placeholder,
                    "allow_freeform_input": request.allow_freeform_input,
                    "required": request.required,
                    "allow_cancel": request.allow_cancel,
                    "min_length": request.min_length,
                }),
            ));
            match receiver.recv_timeout(Duration::from_secs(300)) {
                Ok(response) => Ok(response),
                Err(_) => {
                    if let Ok(mut pending) = interactions.lock() {
                        pending.remove(&request_id);
                    }
                    Ok(AskUserResponse::Cancelled)
                }
            }
        },
    ))));

    llm.register_tool(Box::new(PythonTool::new()));
    llm.register_tool(Box::new(GlobTool::new()));
    llm.register_tool(Box::new(GrepTool::new()));
    llm.register_tool(Box::new(WebFetchTool::new(
        &config.api_base,
        &config.api_key,
        model,
        config.temperature,
        config.max_tokens,
    )));
    llm.register_tool(Box::new(WebSearchTool::from_config(
        &WebSearchConfig::from_env(),
    )));
    llm.register_tool(Box::new(EnterPlanModeTool::new()));
    llm.register_tool(Box::new(ExitPlanModeTool::new()));

    // Long-term memory uses the shared aish Markdown store.
    let memory_manager: Arc<Mutex<Option<MemoryManager>>> = Arc::new(Mutex::new(
        MemoryManager::new(MemoryManager::default_path()).ok(),
    ));
    let mm_search = memory_manager.clone();
    let mm_store = memory_manager.clone();
    let mm_delete = memory_manager.clone();
    let mm_list = memory_manager.clone();
    let mem_session = session_id.to_string();
    let mem_host = host_name();
    let memory_tool = MemoryTool::new(
        Box::new(move |query, limit| {
            let mut guard = mm_search.lock().unwrap();
            guard
                .as_mut()
                .map(|manager| {
                    manager
                        .recall(query, limit)
                        .into_iter()
                        .map(|entry| MemorySearchResult {
                            id: entry.id as usize,
                            content: entry.content.clone(),
                            category: format!("{:?}", entry.category).to_lowercase(),
                            source: entry.source.clone(),
                            scope: format!("{:?}", entry.scope).to_lowercase(),
                            created_at: entry.created_at.clone(),
                            expires_at: entry.expires_at.clone(),
                            expired: MemoryManager::is_expired(entry),
                        })
                        .collect()
                })
                .unwrap_or_default()
        }),
        Box::new(
            move |content, category, source, importance, scope, ttl_seconds| {
                let mut guard = mm_store.lock().unwrap();
                guard
                    .as_mut()
                    .map(|manager| {
                        let source = MemorySource {
                            label: source.to_string(),
                            session_uuid: Some(mem_session.clone()),
                            host: Some(mem_host.clone()),
                        };
                        manager
                            .store_with_provenance(
                                content,
                                parse_memory_category(category),
                                parse_memory_scope(scope),
                                source,
                                f64::from(importance),
                                ttl_seconds,
                            )
                            .map(|id| id.to_string())
                            .unwrap_or_else(|error| error.to_string())
                    })
                    .unwrap_or_else(|| "memory not available".to_string())
            },
        ),
        Box::new(move |id| {
            let mut guard = mm_delete.lock().unwrap();
            guard
                .as_mut()
                .and_then(|manager| manager.remove(id as i64).ok())
                .unwrap_or(false)
        }),
        Box::new(move |limit| {
            let guard = mm_list.lock().unwrap();
            guard
                .as_ref()
                .map(|manager| {
                    manager
                        .list()
                        .iter()
                        .rev()
                        .take(limit)
                        .map(|entry| MemorySearchResult {
                            id: entry.id as usize,
                            content: entry.content.clone(),
                            category: format!("{:?}", entry.category).to_lowercase(),
                            source: entry.source.clone(),
                            scope: format!("{:?}", entry.scope).to_lowercase(),
                            created_at: entry.created_at.clone(),
                            expires_at: entry.expires_at.clone(),
                            expired: MemoryManager::is_expired(entry),
                        })
                        .collect()
                })
                .unwrap_or_default()
        }),
    );
    llm.register_tool(Box::new(memory_tool));

    // Skills are passive SKILL.md content, discovered from the same aish
    // directories. Loading is per session so newly installed skills appear
    // in the next conversation without a GUI-specific registry.
    let mut skill_manager = SkillManager::new();
    let _ = skill_manager.load_all_skills();
    let snapshot: HashMap<String, SkillInfo> = skill_manager
        .list_skills()
        .iter()
        .filter(|skill| !skill.quarantined)
        .map(|skill| {
            (
                skill.metadata.name.clone(),
                SkillInfo {
                    name: skill.metadata.name.clone(),
                    content: skill.content.clone(),
                    description: skill.metadata.description.clone(),
                    base_dir: skill.base_dir.clone(),
                    context: skill.metadata.context,
                    agent: skill.metadata.agent.clone(),
                    allowed_tools: skill.metadata.allowed_tools.clone(),
                    quarantined: skill.quarantined,
                },
            )
        })
        .collect();
    let names: Vec<String> = snapshot.keys().cloned().collect();
    let lookup_snapshot = snapshot.clone();
    let lookup = Box::new(move |name: &str| lookup_snapshot.get(name).cloned());
    let list = Box::new(move || names.clone());
    llm.register_tool(Box::new(SkillTool::new(lookup, list)));
    // Agent must be registered after skill so general-purpose/troubleshoot
    // can inherit the parent's skill tool, matching aish-shell.
    llm.register_tool(Box::new(AgentTool::new()));

    let registries: Vec<aish_skills::registry::RegistryConfig> = config
        .skills
        .registries
        .iter()
        .map(|registry| aish_skills::registry::RegistryConfig {
            name: registry.name.clone(),
            registry_type: registry.registry_type.clone(),
            enabled: registry.enabled,
            url: registry.url.clone(),
        })
        .collect();
    llm.register_tool(Box::new(SkillSearchTool::new(registries.clone())));
    llm.register_tool(Box::new(SkillInstallTool::new(registries)));
    llm.register_tool(Box::new(SkillTrustTool));

    if let Some(manager) = mcp_manager {
        for tool in aish_mcp::gateway_tools(manager) {
            llm.register_tool(tool);
        }
    }
}

fn parse_memory_category(value: &str) -> MemoryCategory {
    match value.to_lowercase().as_str() {
        "preference" => MemoryCategory::Preference,
        "environment" => MemoryCategory::Environment,
        "solution" => MemoryCategory::Solution,
        "pattern" => MemoryCategory::Pattern,
        _ => MemoryCategory::Other,
    }
}

fn parse_memory_scope(value: &str) -> MemoryScope {
    match value.to_lowercase().as_str() {
        "host" => MemoryScope::Host,
        "project" => MemoryScope::Project,
        _ => MemoryScope::User,
    }
}

fn host_name() -> String {
    std::env::var("HOSTNAME")
        .ok()
        .filter(|value| !value.trim().is_empty())
        .or_else(|| {
            fs::read_to_string("/etc/hostname")
                .ok()
                .map(|value| value.trim().to_string())
                .filter(|value| !value.is_empty())
        })
        .unwrap_or_else(|| "unknown".to_string())
}

fn activate_cwd(cwd: &Path) -> Option<String> {
    if !cwd.is_dir() {
        return Some(format!(
            "会话保存的目录已不存在：{}；已使用当前目录",
            cwd.display()
        ));
    }
    std::env::set_current_dir(cwd)
        .err()
        .map(|error| format!("无法进入会话目录 {}: {error}", cwd.display()))
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
