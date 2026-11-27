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

use std::collections::{HashMap, HashSet, VecDeque};
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, AtomicU32, AtomicU64, Ordering};
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
    /// Cumulative tool-call round ceiling for one turn. 0 means unlimited.
    #[serde(default)]
    pub max_tool_iterations: u32,
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
            max_tool_iterations: config.max_tool_iterations,
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
    /// The MCP runtime is replaceable at runtime: the settings page can add,
    /// remove, enable or disable servers without restarting the process.
    mcp_manager: Mutex<Option<Arc<McpManager>>>,
    /// Serializes MCP config read/modify/write sequences from this frontend.
    mcp_config_lock: Mutex<()>,
    /// Gateway tool names registered into live sessions, so a configuration
    /// change can unregister exactly what it previously added.
    mcp_tool_names: Mutex<HashSet<String>>,
    /// Monotonic id for background settings jobs (skill registry, MCP probe).
    job_seq: AtomicU64,
    /// Runtime-updatable tool-call ceiling (0 = unlimited).
    tool_loop_limit: Arc<AtomicU32>,
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

        let max_tool_iterations = config.max_tool_iterations;
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
            mcp_manager: Mutex::new(mcp_manager),
            mcp_config_lock: Mutex::new(()),
            mcp_tool_names: Mutex::new(HashSet::new()),
            job_seq: AtomicU64::new(1),
            tool_loop_limit: Arc::new(AtomicU32::new(max_tool_iterations)),
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
                self.mcp_manager().as_ref(),
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
        let approval_memory = Arc::new(parking_lot::Mutex::new(ApprovalMemory::new()));
        let callback_memory = approval_memory.clone();
        llm.set_confirmation_callback(Arc::new(move |ctx: &PreflightSecurityContext| {
            let request_id = format!(
                "{}-approval-{}",
                approval_session_id,
                approval_seq.fetch_add(1, Ordering::Relaxed)
            );
            let tool_name = ctx.tool_name.clone();
            let network_tool = matches!(tool_name.as_str(), "WebFetch" | "WebSearch");
            if network_tool && callback_memory.lock().is_tool_allowed(&tool_name) {
                return ApprovalChoice::RememberSession;
            }
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
                Ok(choice) => {
                    if choice == ApprovalChoice::RememberSession && network_tool {
                        callback_memory.lock().remember_tool(&tool_name);
                    }
                    choice
                }
                Err(_) => {
                    if let Ok(mut pending) = approvals.lock() {
                        pending.remove(&request_id);
                    }
                    ApprovalChoice::Deny
                }
            }
        }));

        llm.set_approval_memory(approval_memory);
        // aish-llm evaluates a hard-coded 20-round window. Keep counting the
        // continuing rounds against the user's session ceiling; 0 means no
        // GUI ceiling, which preserves the no-limit default.
        let iteration_count = Arc::new(AtomicU32::new(0));
        let callback_iterations = iteration_count.clone();
        let callback_limit = self.tool_loop_limit.clone();
        llm.set_iteration_limit_callback(Arc::new(move |rounds| {
            let limit = callback_limit.load(Ordering::Relaxed);
            if limit == 0 {
                return true;
            }
            let total = callback_iterations.fetch_add(rounds, Ordering::Relaxed) + rounds;
            total < limit
        }));

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
            iteration_count,
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
    ///
    /// `rule_groups` is the raw `rules:` sequence exactly as the YAML file
    /// stores it. The rule editor must edit this shape, not the flattened
    /// `rules` projection: a typed [`aish_security::PolicyRule`] serializes its
    /// glob as `pattern`, while the loader only accepts `path`.
    pub fn policy_info(&self) -> Value {
        let policy = aish_security::load_policy(None);
        let path = aish_security::resolve_security_policy_path(None);
        let path_string = path
            .as_ref()
            .map(|path| path.to_string_lossy().to_string())
            .unwrap_or_default();
        let (rule_groups, rules_error) = match path.as_deref() {
            Some(path) => match read_raw_policy_rules(path) {
                Ok(rules) => (rules, Value::Null),
                Err(error) => (Vec::new(), Value::String(error)),
            },
            None => (
                Vec::new(),
                Value::String("policy path unavailable".to_string()),
            ),
        };
        json!({
            "ok": true,
            "path": path_string,
            "text_path": path_string,
            "rule_file_fingerprint": aish_security::policy_epoch_fingerprint(),
            "default_risk_level": policy.default_risk_level.as_str().to_ascii_lowercase(),
            "enable_sandbox": policy.enable_sandbox,
            "sandbox_off_action": policy.sandbox_off_action.as_str().to_ascii_lowercase(),
            "sandbox_timeout_seconds": policy.sandbox_timeout_seconds,
            "sandbox_socket_available": Path::new("/run/aish/sandbox.sock").exists(),
            "input_guard_enabled": policy.input_guard.enabled,
            "audit_enabled": policy.audit_enabled,
            "audit_include_commands": policy.audit_include_commands,
            "rules": policy.rules,
            "builtin_rules": aish_security::policy::default_rules(),
            "rule_groups": rule_groups,
            "rules_error": rules_error,
            "invalid_fallback_rules": policy.invalid_fallback_rules,
            "validation_issues": policy.validation_issues,
        })
    }

    /// Replace the raw `rules:` sequence in `security_policy.yaml`.
    ///
    /// The write is atomic and validated before it reaches disk. `expected`
    /// is the policy fingerprint the page loaded; when the file changed
    /// underneath the editor, the save is refused instead of clobbering an
    /// external edit (for example one made by the aish CLI).
    pub fn save_policy_rules(
        &self,
        rules: &Value,
        expected_fingerprint: Option<&str>,
    ) -> Result<Value, String> {
        let path = aish_security::writable_security_policy_path()
            .ok_or_else(|| "aish security policy path is unavailable".to_string())?;
        if let Some(expected) = expected_fingerprint.filter(|value| !value.trim().is_empty()) {
            let actual = aish_security::policy_epoch_fingerprint();
            if actual != expected {
                return Err("安全策略文件已被外部修改，请先刷新后再保存。".to_string());
            }
        }
        let array = rules
            .as_array()
            .ok_or_else(|| "rules must be a JSON array".to_string())?;
        validate_raw_policy_rules(array)?;
        let rules_yaml = serde_yaml::to_value(rules)
            .map_err(|error| format!("failed to encode rules: {error}"))?;
        aish_config::ConfigLoader::update_raw(&path, |document| {
            let root = document
                .as_mapping_mut()
                .ok_or_else(|| AishError::Config("policy document is not a mapping".into()))?;
            root.insert(
                serde_yaml::Value::String("rules".to_string()),
                rules_yaml.clone(),
            );
            let text = serde_yaml::to_string(document).map_err(|error| {
                AishError::Config(format!("failed to serialize policy: {error}"))
            })?;
            aish_security::policy_from_yaml_str(&text).map_err(AishError::Config)?;
            Ok(())
        })
        .map_err(|error| format!("failed to save security policy: {error}"))?;
        Ok(self.policy_info())
    }

    /// Explain how a shell command would be classified by the fallback policy
    /// engine. `draft_rules` lets the rule editor preview unsaved edits.
    ///
    /// This is a fallback-engine preview only: it does not execute the command
    /// and does not run the sandbox. When `enable_sandbox` is true, the live
    /// decision may instead come from the sandbox assessment.
    pub fn preview_policy_command(
        &self,
        command: &str,
        draft_rules: Option<&Value>,
    ) -> Result<Value, String> {
        let policy = if let Some(rules) = draft_rules {
            let path = aish_security::resolve_security_policy_path(None)
                .ok_or_else(|| "aish security policy path is unavailable".to_string())?;
            let text = fs::read_to_string(&path)
                .map_err(|error| format!("failed to read {}: {error}", path.display()))?;
            let mut document: serde_yaml::Value = serde_yaml::from_str(&text)
                .map_err(|error| format!("invalid policy YAML: {error}"))?;
            let root = document
                .as_mapping_mut()
                .ok_or_else(|| "policy document is not a mapping".to_string())?;
            root.insert(
                serde_yaml::Value::String("rules".to_string()),
                serde_yaml::to_value(rules)
                    .map_err(|error| format!("failed to encode rules: {error}"))?,
            );
            let text = serde_yaml::to_string(&document)
                .map_err(|error| format!("failed to serialize policy: {error}"))?;
            aish_security::policy_from_yaml_str(&text)?
        } else {
            aish_security::load_policy(None)
        };
        let default_risk = policy.default_risk_level.as_str().to_ascii_lowercase();
        let sandbox_enabled = policy.enable_sandbox;
        let engine = aish_security::FallbackRuleEngine::new(policy);
        let assessment = engine.assess_disabled_command(command);
        let result = match assessment {
            None => json!({
                "matched": false,
                "risk": default_risk,
                "default_risk": default_risk,
                "reasons": ["no fallback rule matched; the default risk level applies"],
                "matched_paths": [],
                "rule": Value::Null,
            }),
            Some(assessment) => json!({
                "matched": true,
                "risk": assessment.level.as_str().to_ascii_lowercase(),
                "default_risk": default_risk,
                "reasons": assessment.reasons,
                "matched_paths": assessment.matched_paths,
                "rule": {
                    "id": assessment.primary_rule.rule_id,
                    "name": assessment.primary_rule.name,
                    "pattern": assessment.primary_rule.pattern,
                    "risk": assessment.primary_rule.risk.as_str().to_ascii_lowercase(),
                    "reason": assessment.primary_rule.reason,
                    "suggestion": assessment.primary_rule.suggestion,
                    "command_list": assessment.primary_rule.command_list,
                },
            }),
        };
        Ok(json!({
            "ok": true,
            "command": command,
            "sandbox_enabled": sandbox_enabled,
            "fallback_only": true,
            "preview": result,
        }))
    }

    /// Session-scoped approvals ("本会话允许"), as shown by the security page.
    pub fn session_approvals(&self, session_id: &str) -> Result<Value, String> {
        let session = self.session(session_id)?;
        let (commands, targets, tools) = session.llm.approval_entries();
        let items: Vec<Value> = commands
            .into_iter()
            .map(|(host, command)| json!({"kind": "command", "host": host, "value": command}))
            .chain(
                targets
                    .into_iter()
                    .map(|(tool, target)| json!({"kind": "target", "tool": tool, "value": target})),
            )
            .chain(
                tools
                    .into_iter()
                    .map(|tool| json!({"kind": "tool", "tool": tool, "value": "*"})),
            )
            .collect();
        Ok(json!({"ok": true, "items": items}))
    }

    /// Revoke one remembered approval. `kind` is `command`, `target` or `tool`.
    pub fn revoke_session_approval(
        &self,
        session_id: &str,
        kind: &str,
        first: &str,
        second: &str,
    ) -> Result<Value, String> {
        let session = self.session(session_id)?;
        let removed = session.llm.revoke_approval(kind, first, second);
        Ok(json!({"ok": true, "removed": removed}))
    }

    /// Clear every remembered approval for one session (the GUI equivalent of
    /// `/forget-approvals`).
    pub fn clear_session_approvals(&self, session_id: &str) -> Result<Value, String> {
        let session = self.session(session_id)?;
        session.llm.clear_approvals();
        Ok(json!({"ok": true}))
    }

    /// Update the aish security globals through the same YAML-preserving
    /// writer the CLI settings panel uses. The ordered rules have a separate
    /// raw-YAML editor (`save_policy_rules`).
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
                self.mcp_manager().as_ref(),
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
        let install_root = SkillManager::user_skills_root()
            .map(|path| path.to_string_lossy().to_string())
            .unwrap_or_default();
        let skills_config = aish_config::ConfigLoader::load(None)
            .map(|config| config.skills)
            .unwrap_or_else(|_| self.config.skills.clone());
        let registries: Vec<Value> = skills_config
            .registries
            .iter()
            .map(|registry| {
                json!({
                    "name": registry.name,
                    "type": registry.registry_type,
                    "enabled": registry.enabled,
                    "url": registry.url,
                    "default": registry.name == skills_config.default_registry,
                })
            })
            .collect();
        json!({
            "ok": true,
            "skills": skills,
            "roots": roots,
            "install_root": install_root,
            "registries": registries,
            "auto_search": skills_config.auto_search,
            "default_registry": skills_config.default_registry,
        })
    }

    /// Trust one quarantined skill by resolving the skill's actual directory.
    ///
    /// Resolving the loaded skill instead of joining the name onto the skills
    /// root matters for registry installs whose slug differs from the
    /// `name:` in SKILL.md.
    pub fn trust_skill(&self, name: &str) -> Result<Value, String> {
        let skill = load_skill_by_name(name)?;
        if !skill.quarantined {
            return Ok(json!({"ok": true, "message": format!("技能「{name}」已经是信任状态。")}));
        }
        let base = PathBuf::from(&skill.base_dir);
        let root = SkillManager::user_skills_root()
            .ok_or_else(|| "aish skills directory is unavailable".to_string())?;
        if !base.starts_with(&root) {
            return Err(format!(
                "refusing to trust a skill outside the user skills directory: {}",
                base.display()
            ));
        }
        aish_skills::set_skill_trusted(&base, true)
            .map_err(|error| format!("failed to trust skill: {error}"))?;
        self.refresh_skill_tools();
        Ok(json!({
            "ok": true,
            "message": format!("技能「{name}」已信任。"),
        }))
    }

    /// Refresh the `skill` tool of every live session after an install,
    /// trust, removal or hot reload. Re-registering by name replaces the old
    /// snapshot, so no session restart is needed.
    pub fn refresh_skills_live(&self) -> Value {
        self.refresh_skill_tools();
        self.skill_catalog()
    }

    fn refresh_skill_tools(&self) {
        let snapshot = Arc::new(load_skill_snapshot());
        let names: Vec<String> = snapshot.keys().cloned().collect();
        let Ok(sessions) = self.sessions.lock() else {
            return;
        };
        for session in sessions.values() {
            let lookup_snapshot = snapshot.clone();
            let lookup = Box::new(move |name: &str| lookup_snapshot.get(name).cloned());
            let list_names = names.clone();
            let list = Box::new(move || list_names.clone());
            session
                .llm
                .register_tool_live(Box::new(SkillTool::new(lookup, list)));
        }
    }

    /// Read one installed skill's `SKILL.md` for the review dialog.
    pub fn skill_read(&self, name: &str, max_bytes: usize) -> Result<Value, String> {
        let skill = load_skill_by_name(name)?;
        let path = PathBuf::from(&skill.file_path);
        let bytes =
            fs::read(&path).map_err(|error| format!("cannot read {}: {error}", path.display()))?;
        let cap = max_bytes.clamp(1, 512 * 1024);
        let truncated = bytes.len() > cap;
        let slice = &bytes[..bytes.len().min(cap)];
        let content = String::from_utf8_lossy(slice).to_string();
        Ok(json!({
            "ok": true,
            "name": skill.metadata.name,
            "path": skill.file_path,
            "base_dir": skill.base_dir,
            "description": skill.metadata.description,
            "quarantined": skill.quarantined,
            "content": content,
            "truncated": truncated,
        }))
    }

    /// Verify one installed skill with aish's shared verifier (the same checks
    /// as `/skill verify`).
    pub fn skill_verify(&self, name: &str) -> Result<Value, String> {
        let skill = load_skill_by_name(name)?;
        let report = aish_skills::registry::RegistryManager::verify(Path::new(&skill.base_dir));
        let checks: Vec<Value> = report
            .checks
            .iter()
            .map(|check| {
                json!({
                    "label": check.label,
                    "passed": check.passed,
                    "detail": check.detail,
                })
            })
            .collect();
        Ok(json!({
            "ok": true,
            "name": report.skill_name,
            "valid": report.valid,
            "checks": checks,
        }))
    }

    /// Remove a skill from the user skills directory. Built-in and Claude
    /// skills are read-only; the name is rejected if it could escape the root.
    pub fn skill_remove(&self, name: &str) -> Result<Value, String> {
        if aish_skills::registry::is_unsafe_skill_name(name) {
            return Err(format!("unsafe skill name: {name:?}"));
        }
        let root = SkillManager::user_skills_root()
            .ok_or_else(|| "aish skills directory is unavailable".to_string())?;
        let dir = match load_skill_by_name(name) {
            Ok(skill) => PathBuf::from(skill.base_dir),
            // A broken skill (invalid frontmatter) is not in the catalog; fall
            // back to the directory name so it can still be removed.
            Err(_) => root.join(name),
        };
        if !dir.starts_with(&root) {
            return Err(format!(
                "refusing to remove a skill outside the user skills directory: {}",
                dir.display()
            ));
        }
        let metadata =
            fs::symlink_metadata(&dir).map_err(|error| format!("skill not installed: {error}"))?;
        if metadata.file_type().is_symlink() || !metadata.is_dir() {
            return Err("refusing to remove a non-directory or symlinked skill".to_string());
        }
        fs::remove_dir_all(&dir)
            .map_err(|error| format!("failed to remove {}: {error}", dir.display()))?;
        self.refresh_skill_tools();
        Ok(json!({"ok": true, "removed": name, "path": dir.to_string_lossy()}))
    }

    /// Start a registry search in the background. The result arrives as a
    /// `settings_job` event, so a slow/unreachable registry never blocks the
    /// Qt event loop.
    pub fn start_skill_search(&self, query: &str, limit: usize) -> Value {
        let job_id = self.next_job_id();
        let registries = registry_configs_from_config();
        let events = self.events.clone();
        let job = job_id.clone();
        let query = query.to_string();
        let limit = limit.clamp(1, 50);
        self.runtime.spawn_blocking(move || {
            let manager = aish_skills::registry::RegistryManager::from_config(&registries);
            let outcome = manager.search_all_with_errors(&query, limit);
            let results = serde_json::to_value(&outcome.results).unwrap_or(Value::Null);
            let errors: Vec<Value> = outcome
                .errors
                .iter()
                .map(|error| json!({"registry": error.registry, "error": error.error}))
                .collect();
            events.push(ChatEvent::new(
                "settings",
                "settings_job",
                json!({
                    "job_id": job,
                    "job": "skill_search",
                    "ok": true,
                    "data": {
                        "query": query,
                        "results": results,
                        "errors": errors,
                        "succeeded": outcome.succeeded,
                    },
                }),
            ));
        });
        json!({"ok": true, "job_id": job_id})
    }

    /// Start a registry install in the background. The skill is installed into
    /// aish's user skills directory and left quarantined; the frontend must
    /// review it and call `trust_skill` explicitly.
    pub fn start_skill_install(&self, skill_id: &str) -> Value {
        let job_id = self.next_job_id();
        let registries = registry_configs_from_config();
        let events = self.events.clone();
        let job = job_id.clone();
        let skill_id = skill_id.to_string();
        self.runtime.spawn(async move {
            // The install tool already enforces a 180s deadline and runs its
            // blocking download on a worker thread. Serialize installs so two
            // jobs cannot write the same target directory.
            static INSTALL_LOCK: std::sync::OnceLock<tokio::sync::Mutex<()>> =
                std::sync::OnceLock::new();
            let _guard = INSTALL_LOCK
                .get_or_init(|| tokio::sync::Mutex::new(()))
                .lock()
                .await;
            let tool = SkillInstallTool::new(registries);
            let result = tool.execute_async(json!({"skill_id": skill_id})).await;
            let data = json!({
                "skill_id": skill_id,
                "ok": result.ok,
                "output": result.output,
                "meta": result.meta,
            });
            events.push(ChatEvent::new(
                "settings",
                "settings_job",
                if result.ok {
                    json!({"job_id": job, "job": "skill_install", "ok": true, "data": data})
                } else {
                    json!({
                        "job_id": job,
                        "job": "skill_install",
                        "ok": false,
                        "error": data["output"].as_str().unwrap_or("skill install failed"),
                        "data": data,
                    })
                },
            ));
        });
        json!({"ok": true, "job_id": job_id})
    }

    /// MCP server/tool status for the connectors page.
    ///
    /// The configured list comes from `config.yaml`; runtime status, live
    /// tools and per-tool trust are merged from the running manager. A server
    /// that is configured but disabled (or whose runtime has not started yet)
    /// still appears, so the page can enable it.
    pub fn mcp_status(&self) -> Value {
        let config = match aish_config::ConfigLoader::load(None) {
            Ok(config) => config,
            Err(error) => {
                return json!({
                    "ok": false,
                    "error": format!("failed to load aish config: {error}"),
                    "servers": [],
                });
            }
        };
        let (settings, warnings) = aish_mcp::resolve_settings(&config.mcp);
        let manager = self.mcp_manager();
        let mut snapshots: HashMap<String, aish_mcp::ServerSnapshot> = manager
            .as_ref()
            .map(|manager| {
                manager
                    .snapshots()
                    .into_iter()
                    .map(|snapshot| (snapshot.name.clone(), snapshot))
                    .collect()
            })
            .unwrap_or_default();
        let active = manager.as_ref().is_some_and(|manager| manager.is_active());
        let config_path = aish_config::ConfigLoader::default_config_path()
            .to_string_lossy()
            .to_string();

        let servers: Vec<Value> = config
            .mcp
            .servers
            .iter()
            .map(|server| {
                let resolved = settings.server(&server.name);
                let snapshot = snapshots.remove(&server.name);
                let (status, status_detail) = if !config.mcp.enabled || !server.enabled {
                    ("disabled".to_string(), Value::Null)
                } else if let Some(snapshot) = &snapshot {
                    (
                        snapshot.status.label().to_string(),
                        match &snapshot.status {
                            aish_mcp::ServerStatus::Connected {
                                protocol_version,
                                tool_count,
                            } => json!({
                                "protocol_version": protocol_version,
                                "tool_count": tool_count
                            }),
                            aish_mcp::ServerStatus::Failed { reason } => {
                                json!({"reason": reason})
                            }
                            _ => Value::Null,
                        },
                    )
                } else {
                    ("not_started".to_string(), Value::Null)
                };
                let tools: Vec<Value> = snapshot
                    .as_ref()
                    .map(|snapshot| {
                        snapshot
                            .tools
                            .iter()
                            .map(|tool| {
                                let effective = resolved
                                    .map(|resolved| resolved.effective_trust(&tool.name))
                                    .map(|trust| trust.as_str())
                                    .unwrap_or("ask");
                                let override_level = resolved
                                    .and_then(|resolved| resolved.tool_trust.get(&tool.name))
                                    .map(|trust| trust.as_str());
                                json!({
                                    "name": tool.name,
                                    "description": tool.description,
                                    "side_effects": tool.side_effects,
                                    "required": tool.required,
                                    "trust": effective,
                                    "trust_override": override_level,
                                })
                            })
                            .collect()
                    })
                    .unwrap_or_default();
                let env_keys: Vec<&String> = server.env.keys().collect();
                let header_keys: Vec<&String> = server.headers.keys().collect();
                let has_authorization = server
                    .headers
                    .keys()
                    .any(|key| key.eq_ignore_ascii_case("authorization"));
                json!({
                    "name": server.name,
                    "transport": server.transport,
                    "endpoint": resolved
                        .map(|resolved| resolved.endpoint_label())
                        .unwrap_or_default(),
                    "enabled": server.enabled,
                    "configured": true,
                    "status": status,
                    "status_detail": status_detail,
                    "trust": resolved
                        .map(|resolved| resolved.trust.as_str())
                        .unwrap_or_else(|| if server.trust.trim().is_empty() { "ask" } else { server.trust.as_str() }),
                    "declared_side_effects": resolved
                        .map(|resolved| resolved.side_effects.as_str())
                        .unwrap_or_else(|| if server.side_effects.trim().is_empty() { "unknown" } else { server.side_effects.as_str() }),
                    "tools": tools,
                    "command": server.command,
                    "args": server.args,
                    "url": server.url.as_deref().map(aish_mcp::redact::redact_url_for_log),
                    "cwd": server.cwd,
                    "expose": server.expose,
                    "request_timeout_ms": server.request_timeout_ms,
                    "max_output_bytes": server.max_output_bytes,
                    "env_keys": env_keys,
                    "header_keys": header_keys,
                    "has_authorization": has_authorization,
                })
            })
            .collect();

        json!({
            "ok": true,
            "master_enabled": config.mcp.enabled,
            "active": active,
            "config_path": config_path,
            "warnings": warnings,
            "servers": servers,
        })
    }

    /// Reconnect all configured MCP servers through a background job.
    pub fn mcp_reload(&self) -> Value {
        let job_id = self.next_job_id();
        let Some(manager) = self.mcp_manager() else {
            self.emit_settings_job(&job_id, "mcp_reload", Ok(json!({"servers": []})));
            return json!({"ok": true, "job_id": job_id, "done": true});
        };
        let events = self.events.clone();
        let job = job_id.clone();
        self.runtime.spawn(async move {
            manager.reload().await;
            events.push(ChatEvent::new(
                "settings",
                "settings_job",
                json!({"job_id": job, "job": "mcp_reload", "ok": true, "data": {}}),
            ));
        });
        json!({"ok": true, "job_id": job_id})
    }

    /// Refresh the `mcp__*` gateway tools of every live session from the
    /// current manager. Called after an apply-settings job finishes.
    pub fn refresh_mcp_tools(&self) -> Value {
        let manager = self.mcp_manager();
        let new_names: HashSet<String> = manager
            .as_ref()
            .filter(|manager| manager.settings().enabled)
            .map(|manager| {
                manager
                    .settings()
                    .servers
                    .iter()
                    .filter(|server| server.enabled)
                    .map(|server| aish_mcp::gateway_tool_name(&server.name))
                    .collect()
            })
            .unwrap_or_default();
        let old_names = self
            .mcp_tool_names
            .lock()
            .map(|mut names| std::mem::replace(&mut *names, new_names))
            .unwrap_or_default();
        let Ok(sessions) = self.sessions.lock() else {
            return json!({"ok": false, "error": "session map lock poisoned"});
        };
        for session in sessions.values() {
            for name in &old_names {
                session.llm.unregister_tool_live(name);
            }
            if let Some(manager) = manager
                .as_ref()
                .filter(|manager| manager.settings().enabled)
            {
                for tool in aish_mcp::gateway_tools(manager) {
                    session.llm.register_tool_live(tool);
                }
            }
        }
        json!({"ok": true, "sessions": sessions.len()})
    }

    /// Add one MCP server from the settings form. Validation and server
    /// construction use aish's own `/mcp add` helpers, then the config is
    /// written through the raw-document merger so unknown keys survive.
    pub fn mcp_add(&self, args: &Value) -> Result<Value, String> {
        let _guard = self
            .mcp_config_lock
            .lock()
            .map_err(|_| "MCP config lock poisoned".to_string())?;

        let add_args = add_args_from_json(args)?;
        let mut config = aish_config::ConfigLoader::load(None)
            .map_err(|error| format!("failed to load aish config: {error}"))?;
        let entry = aish_mcp::add_to_config(&mut config.mcp, &add_args)?;
        self.persist_mcp_config(&config.mcp)?;
        let (settings, warnings) = aish_mcp::resolve_settings(&config.mcp);
        let job_id = self.spawn_mcp_apply(settings);
        Ok(json!({
            "ok": true,
            "job_id": job_id,
            "server": entry.name,
            "warnings": warnings,
        }))
    }

    /// Update one existing MCP server in place. Fields absent from `args` are
    /// preserved, which matters for credentials the UI never receives back.
    pub fn mcp_update(&self, name: &str, args: &Value) -> Result<Value, String> {
        let _guard = self
            .mcp_config_lock
            .lock()
            .map_err(|_| "MCP config lock poisoned".to_string())?;

        let name = name.trim();
        if name.is_empty() {
            return Err("server name must not be empty".to_string());
        }
        let mut config = aish_config::ConfigLoader::load(None)
            .map_err(|error| format!("failed to load aish config: {error}"))?;
        let index = config
            .mcp
            .servers
            .iter()
            .position(|server| server.name == name)
            .ok_or_else(|| format!("no MCP server named `{name}` is configured"))?;
        let existing = config.mcp.servers[index].clone();
        let updated = update_server_from_json(existing, args)?;
        config.mcp.servers[index] = updated;
        self.persist_mcp_config(&config.mcp)?;
        let (settings, warnings) = aish_mcp::resolve_settings(&config.mcp);
        let job_id = self.spawn_mcp_apply(settings);
        Ok(json!({
            "ok": true,
            "job_id": job_id,
            "server": name,
            "warnings": warnings,
        }))
    }

    /// Remove one MCP server and disconnect it.
    pub fn mcp_remove(&self, name: &str) -> Result<Value, String> {
        let _guard = self
            .mcp_config_lock
            .lock()
            .map_err(|_| "MCP config lock poisoned".to_string())?;

        let mut config = aish_config::ConfigLoader::load(None)
            .map_err(|error| format!("failed to load aish config: {error}"))?;
        aish_mcp::remove_from_config(&mut config.mcp, name)?;
        self.persist_mcp_config(&config.mcp)?;
        let (settings, warnings) = aish_mcp::resolve_settings(&config.mcp);
        let job_id = self.spawn_mcp_apply(settings);
        Ok(json!({
            "ok": true,
            "job_id": job_id,
            "server": name,
            "warnings": warnings,
        }))
    }

    /// Enable or disable the MCP master switch.
    pub fn mcp_set_master_enabled(&self, enabled: bool) -> Result<Value, String> {
        let _guard = self
            .mcp_config_lock
            .lock()
            .map_err(|_| "MCP config lock poisoned".to_string())?;

        let mut config = aish_config::ConfigLoader::load(None)
            .map_err(|error| format!("failed to load aish config: {error}"))?;
        config.mcp.enabled = enabled;
        self.persist_mcp_config(&config.mcp)?;
        let (settings, warnings) = aish_mcp::resolve_settings(&config.mcp);
        let job_id = self.spawn_mcp_apply(settings);
        Ok(json!({
            "ok": true,
            "job_id": job_id,
            "master_enabled": enabled,
            "warnings": warnings,
        }))
    }

    /// Enable or disable one MCP server.
    pub fn mcp_set_enabled(&self, name: &str, enabled: bool) -> Result<Value, String> {
        let _guard = self
            .mcp_config_lock
            .lock()
            .map_err(|_| "MCP config lock poisoned".to_string())?;

        let mut config = aish_config::ConfigLoader::load(None)
            .map_err(|error| format!("failed to load aish config: {error}"))?;
        aish_mcp::set_enabled_in_config(&mut config.mcp, name, enabled)?;
        self.persist_mcp_config(&config.mcp)?;
        let (settings, warnings) = aish_mcp::resolve_settings(&config.mcp);
        let job_id = self.spawn_mcp_apply(settings);
        Ok(json!({
            "ok": true,
            "job_id": job_id,
            "server": name,
            "enabled": enabled,
            "warnings": warnings,
        }))
    }

    /// Set the server-level trust (`tool` = `*`) or one tool's trust.
    pub fn mcp_set_trust(&self, name: &str, tool: &str, level: &str) -> Result<Value, String> {
        let _guard = self
            .mcp_config_lock
            .lock()
            .map_err(|_| "MCP config lock poisoned".to_string())?;

        let level = aish_mcp::TrustLevel::parse(level)
            .ok_or_else(|| format!("unknown trust level `{level}`"))?;
        let mut config = aish_config::ConfigLoader::load(None)
            .map_err(|error| format!("failed to load aish config: {error}"))?;
        aish_mcp::set_trust_in_config(&mut config.mcp, name, tool, level)?;
        self.persist_mcp_config(&config.mcp)?;
        let (settings, warnings) = aish_mcp::resolve_settings(&config.mcp);
        let job_id = self.spawn_mcp_apply(settings);
        Ok(json!({
            "ok": true,
            "job_id": job_id,
            "server": name,
            "tool": tool,
            "trust": level.as_str(),
            "warnings": warnings,
        }))
    }

    /// Probe one configured server without changing the config.
    pub fn start_mcp_test(&self, name: &str) -> Result<Value, String> {
        let config = aish_config::ConfigLoader::load(None)
            .map_err(|error| format!("failed to load aish config: {error}"))?;
        let (settings, _) = aish_mcp::resolve_settings(&config.mcp);
        let server = settings
            .server(name)
            .cloned()
            .ok_or_else(|| format!("no enabled MCP server named `{name}` is configured"))?;
        Ok(self.spawn_mcp_probe(format!("server:{name}"), server))
    }

    /// Probe a server definition from the add/edit form before it is saved.
    pub fn start_mcp_test_draft(&self, args: &Value) -> Result<Value, String> {
        let add_args = add_args_from_json(args)?;
        let config = aish_config::ConfigLoader::load(None)
            .map_err(|error| format!("failed to load aish config: {error}"))?;
        let mut staging = config.mcp.clone();
        let entry = aish_mcp::add_to_config(&mut staging, &add_args)?;
        let (settings, warnings) = aish_mcp::resolve_settings(&staging);
        let server = settings
            .server(&entry.name)
            .cloned()
            .ok_or_else(|| "draft server was not resolved".to_string())?;
        let mut result = self.spawn_mcp_probe(format!("draft:{}", entry.name), server);
        if let Some(object) = result.as_object_mut() {
            object.insert("warnings".to_string(), json!(warnings));
            object.insert("server".to_string(), json!(entry.name));
        }
        Ok(result)
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

    /// Load a session but return only its newest transcript window. The live
    /// session still owns the full history for LLM context; the UI receives a
    /// page so a very long conversation is not materialised into QML at once.
    pub fn resume_session_window(
        &self,
        session_id: &str,
        limit: usize,
    ) -> Result<
        (
            String,
            Vec<ChatMessage>,
            usize,
            usize,
            PathBuf,
            Option<String>,
        ),
        String,
    > {
        let (model, history, cwd, warning) = self.resume_session(session_id)?;
        let total = history.len();
        let (start, _) = transcript_window(&history, total, limit);
        Ok((model, history[start..].to_vec(), total, start, cwd, warning))
    }

    /// Return one older transcript page. Pages begin at a user message so a
    /// tool result is never split from the assistant call that produced it.
    pub fn history_page(
        &self,
        session_id: &str,
        before: usize,
        limit: usize,
    ) -> Result<Value, String> {
        let session = self.session(session_id)?;
        let history = session
            .history
            .lock()
            .map_err(|_| "session history lock poisoned".to_string())?;
        let total = history.len();
        let end = before.min(total);
        let (start, _) = transcript_window(&history, end, limit);
        let page = &history[start..end];
        Ok(json!({
            "ok": true,
            "messages": page,
            "total": total,
            "start": start,
        }))
    }

    /// Remove a user message and everything after it.
    ///
    /// `keep_count` is the absolute transcript index of the user message. The
    /// live history and the append-only `ai_messages` rows are trimmed
    /// together, then the session snapshot is rewritten so a reload cannot
    /// resurrect the undone turns.
    pub fn undo_to_message(&self, session_id: &str, keep_count: usize) -> Result<Value, String> {
        let session = self.session(session_id)?;
        if session.running.load(Ordering::SeqCst) {
            return Err("会话正在运行，无法撤回".to_string());
        }
        let mut history = session
            .history
            .lock()
            .map_err(|_| "session history lock poisoned".to_string())?;
        let total = history.len();
        if keep_count > total {
            return Err(format!("撤回位置超出会话历史：{keep_count} > {total}"));
        }
        if keep_count == total {
            return Ok(json!({
                "ok": true,
                "total": total,
                "removed": 0,
            }));
        }
        if let Some(store) = &session.store {
            let store = store
                .lock()
                .map_err(|_| "session store lock poisoned".to_string())?;
            store
                .truncate_ai_messages(session_id, keep_count)
                .map_err(|error| format!("failed to truncate transcript: {error}"))?;
        }
        history.truncate(keep_count);
        drop(history);
        session.persist_snapshot();
        Ok(json!({
            "ok": true,
            "total": keep_count,
            "removed": total - keep_count,
        }))
    }

    /// Delete all rows from aish's audit table.
    pub fn clear_audit_events(&self) -> Result<Value, String> {
        let Some(store) = &self.store else {
            return Err("session database is not configured".to_string());
        };
        let store = store
            .lock()
            .map_err(|_| "session store lock poisoned".to_string())?;
        let deleted = store
            .clear_audit_events()
            .map_err(|error| format!("failed to clear audit events: {error}"))?;
        Ok(json!({"ok": true, "deleted": deleted}))
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

    /// Current GUI tool-call ceiling. 0 means unlimited.
    pub fn tool_loop_limit(&self) -> Value {
        json!({
            "ok": true,
            "limit": self.tool_loop_limit.load(Ordering::Relaxed),
        })
    }

    /// Persist the tool-call ceiling and apply it to live sessions.
    pub fn save_tool_loop_limit(&self, limit: u32) -> Result<Value, String> {
        if limit > 100_000 {
            return Err("工具循环上限不能超过 100000".to_string());
        }
        let path = aish_config::ConfigLoader::default_config_path();
        aish_config::ConfigLoader::update_raw(&path, |document| {
            let map = document.as_mapping_mut().ok_or_else(|| {
                AishError::Config(format!("config root is not a mapping: {}", path.display()))
            })?;
            map.insert(
                serde_yaml::Value::String("max_tool_iterations".to_string()),
                serde_yaml::Value::Number(serde_yaml::Number::from(limit as u64)),
            );
            Ok(())
        })
        .map_err(|error| format!("failed to save tool loop limit: {error}"))?;
        self.tool_loop_limit.store(limit, Ordering::Relaxed);
        Ok(json!({"ok": true, "limit": limit}))
    }

    /// Update the three model fields in the shared aish YAML config.
    ///
    /// The raw-document editor preserves keys the typed config does not model.
    /// An empty `api_key` keeps the existing key instead of erasing it.
    /// Provider/model endpoints known to aish: the active primary endpoint and
    /// every `api_accounts` entry. Keys are never returned, only `has_key`.
    pub fn provider_info(&self) -> Value {
        let config = match aish_config::ConfigLoader::load(None) {
            Ok(config) => config,
            Err(error) => {
                return json!({
                    "ok": false,
                    "error": format!("failed to load aish config: {error}"),
                    "providers": [],
                });
            }
        };
        let mut providers = vec![json!({
            "name": provider_host_label(&config.api_base),
            "host": provider_host_label(&config.api_base),
            "api_base": config.api_base,
            "model": config.model,
            "protocol": provider_protocol(&config.api_base),
            "active": true,
            "has_key": !config.api_key.trim().is_empty(),
            "account": false,
        })];
        let mut seen = std::collections::HashSet::new();
        seen.insert((config.api_base.clone(), config.api_key.clone()));
        for account in &config.api_accounts {
            let base = account
                .api_base
                .clone()
                .unwrap_or_else(|| config.api_base.clone());
            if !seen.insert((base.clone(), account.api_key.clone())) {
                continue;
            }
            providers.push(json!({
                "name": account.name,
                "host": provider_host_label(&base),
                "api_base": base,
                "model": account.model.clone().unwrap_or_default(),
                "protocol": provider_protocol(&base),
                "active": false,
                "has_key": !account.api_key.trim().is_empty(),
                "account": true,
                "disabled": account.disabled,
            }));
        }
        json!({
            "ok": true,
            "config_path": aish_config::ConfigLoader::default_config_path().to_string_lossy(),
            "providers": providers,
        })
    }

    /// Add or update one provider, optionally switching the active endpoint.
    ///
    /// This mirrors the aish `/model` endpoint switcher: switching away from
    /// the current primary preserves it as a named `api_accounts` entry, and
    /// promoting an existing account removes it from the pool so the same
    /// credential is never both primary and a pool entry. `current_name` is the
    /// frontend's display label for the outgoing primary; when empty the host
    /// is used.
    #[allow(clippy::too_many_arguments)]
    pub fn save_provider_settings(
        name: &str,
        api_base: &str,
        api_key: &str,
        model: &str,
        protocol: &str,
        activate: bool,
        current_name: &str,
    ) -> Result<String, String> {
        let name = name.trim();
        let api_base = api_base.trim();
        let model = model.trim();
        if name.is_empty() || name.chars().count() > 80 {
            return Err("供应商名称不能为空，且不能超过 80 个字符".to_string());
        }
        if model.is_empty() || model.chars().count() > 200 {
            return Err("模型名称不能为空，且不能超过 200 个字符".to_string());
        }
        for value in [name, api_base, model, protocol] {
            if value.contains(['\n', '\r', '\0']) {
                return Err("供应商配置包含非法字符".to_string());
            }
        }
        if api_base.chars().count() > 2048 {
            return Err("API 地址过长".to_string());
        }
        let parsed =
            reqwest::Url::parse(api_base).map_err(|error| format!("API 地址无效：{error}"))?;
        if !matches!(parsed.scheme(), "http" | "https") {
            return Err("API 地址必须是 http(s) URL".to_string());
        }
        validate_provider_protocol(api_base, protocol)?;
        if api_key.chars().count() > 4096 || api_key.contains(['\n', '\r', '\0']) {
            return Err("API Key 过长或包含非法字符".to_string());
        }

        let path = aish_config::ConfigLoader::default_config_path();
        let mut config = aish_config::ConfigLoader::load(None)
            .map_err(|error| format!("failed to load aish config: {error}"))?;
        let mut accounts = config.api_accounts.clone();
        let existing_account = accounts
            .iter()
            .find(|account| {
                account.name == name
                    || account.api_base.as_deref() == Some(api_base)
                        && (api_key.trim().is_empty() || account.api_key == api_key.trim())
            })
            .cloned();
        let effective_key = if api_key.trim().is_empty() {
            if api_base == config.api_base {
                config.api_key.clone()
            } else {
                existing_account
                    .as_ref()
                    .map(|account| account.api_key.clone())
                    .unwrap_or_default()
            }
        } else {
            api_key.trim().to_string()
        };
        if effective_key.chars().count() > 4096 {
            return Err("API Key 过长".to_string());
        }

        if activate {
            let endpoint_changed = api_base != config.api_base || effective_key != config.api_key;
            if endpoint_changed && !config.api_key.trim().is_empty() {
                let outgoing_name = if current_name.trim().is_empty() {
                    provider_host_label(&config.api_base)
                } else {
                    current_name.trim().to_string()
                };
                let already_kept = accounts.iter().any(|account| {
                    account.api_key == config.api_key
                        && account.api_base.as_deref() == Some(config.api_base.as_str())
                });
                if !already_kept {
                    let unique = unique_account_name(&accounts, &outgoing_name);
                    accounts.push(aish_config::ApiAccountConfig {
                        name: unique,
                        api_key: config.api_key.clone(),
                        api_base: Some(config.api_base.clone()),
                        model: (!config.model.trim().is_empty()).then(|| config.model.clone()),
                        weight: 1,
                        disabled: false,
                    });
                }
            }
            // The target becomes primary; do not leave a duplicate pool entry.
            accounts.retain(|account| {
                !(account.api_base.as_deref() == Some(api_base) && account.api_key == effective_key)
            });
            config.model = model.to_string();
            config.api_base = api_base.to_string();
            config.api_key = effective_key;
        } else {
            if api_base == config.api_base && effective_key == config.api_key {
                return Err(
                    "该地址和密钥与当前主供应商相同，请直接在主供应商上切换模型".to_string()
                );
            }
            let existing_index = accounts
                .iter()
                .position(|account| account.name == name)
                .or_else(|| {
                    accounts.iter().position(|account| {
                        account.api_base.as_deref() == Some(api_base)
                            && account.api_key == effective_key
                    })
                });
            if let Some(index) = existing_index {
                let account = &mut accounts[index];
                account.name = name.to_string();
                account.api_key = effective_key;
                account.api_base = Some(api_base.to_string());
                account.model = Some(model.to_string());
                account.disabled = false;
            } else {
                accounts.push(aish_config::ApiAccountConfig {
                    name: name.to_string(),
                    api_key: effective_key,
                    api_base: Some(api_base.to_string()),
                    model: Some(model.to_string()),
                    weight: 1,
                    disabled: false,
                });
            }
        }

        aish_config::ConfigLoader::update_raw(&path, |document| {
            let map = document.as_mapping_mut().ok_or_else(|| {
                AishError::Config(format!("config root is not a mapping: {}", path.display()))
            })?;
            if activate {
                map.insert(
                    serde_yaml::Value::String("model".to_string()),
                    serde_yaml::Value::String(config.model.clone()),
                );
                map.insert(
                    serde_yaml::Value::String("api_base".to_string()),
                    serde_yaml::Value::String(config.api_base.clone()),
                );
                if !config.api_key.trim().is_empty() {
                    map.insert(
                        serde_yaml::Value::String("api_key".to_string()),
                        serde_yaml::Value::String(config.api_key.clone()),
                    );
                }
            }
            let accounts_yaml = serde_yaml::to_value(&accounts).map_err(|error| {
                AishError::Config(format!("failed to encode accounts: {error}"))
            })?;
            map.insert(
                serde_yaml::Value::String("api_accounts".to_string()),
                accounts_yaml,
            );
            Ok(())
        })
        .map_err(|error| format!("failed to save provider configuration: {error}"))?;
        Ok(path.to_string_lossy().to_string())
    }

    /// Remove one named non-primary provider account.
    pub fn delete_provider_account(name: &str) -> Result<String, String> {
        let name = name.trim();
        if name.is_empty() {
            return Err("provider name must not be empty".to_string());
        }
        let path = aish_config::ConfigLoader::default_config_path();
        let mut config = aish_config::ConfigLoader::load(None)
            .map_err(|error| format!("failed to load aish config: {error}"))?;
        let before = config.api_accounts.len();
        config.api_accounts.retain(|account| account.name != name);
        if config.api_accounts.len() == before {
            return Err(format!("no provider account named `{name}` is configured"));
        }
        aish_config::ConfigLoader::update_raw(&path, |document| {
            let map = document.as_mapping_mut().ok_or_else(|| {
                AishError::Config(format!("config root is not a mapping: {}", path.display()))
            })?;
            map.insert(
                serde_yaml::Value::String("api_accounts".to_string()),
                serde_yaml::to_value(&config.api_accounts).map_err(|error| {
                    AishError::Config(format!("failed to encode accounts: {error}"))
                })?,
            );
            Ok(())
        })
        .map_err(|error| format!("failed to delete provider: {error}"))?;
        Ok(path.to_string_lossy().to_string())
    }

    /// Fetch the model catalogue of an OpenAI-compatible endpoint in the
    /// background. Third-party providers intentionally support only this
    /// protocol; Anthropic-native endpoints are rejected during validation.
    pub fn start_model_fetch(&self, api_base: &str, api_key: &str, protocol: &str) -> Value {
        let job_id = self.next_job_id();
        let config = aish_config::ConfigLoader::load(None).ok();
        let mut base = api_base.trim().to_string();
        if base.is_empty() {
            if let Some(config) = &config {
                base = config.api_base.clone();
            }
        }
        let mut key = api_key.trim().to_string();
        if key.is_empty() {
            if let Some(config) = &config {
                if base == config.api_base {
                    key = config.api_key.clone();
                } else if let Some(account) = config
                    .api_accounts
                    .iter()
                    .find(|account| account.api_base.as_deref() == Some(base.as_str()))
                {
                    key = account.api_key.clone();
                }
            }
        }
        let protocol = protocol.trim().to_ascii_lowercase();
        let events = self.events.clone();
        let job = job_id.clone();
        self.runtime.spawn(async move {
            let result = fetch_provider_models(&base, &key, &protocol).await;
            match result {
                Ok(models) => {
                    events.push(ChatEvent::new(
                        "settings",
                        "settings_job",
                        json!({
                            "job_id": job,
                            "job": "model_fetch",
                            "ok": true,
                            "data": {
                                "base": base,
                                "protocol": protocol,
                                "models": models,
                            },
                        }),
                    ));
                }
                Err(error) => {
                    events.push(ChatEvent::new(
                        "settings",
                        "settings_job",
                        json!({
                            "job_id": job,
                            "job": "model_fetch",
                            "ok": false,
                            "error": error,
                            "data": {"base": base, "protocol": protocol},
                        }),
                    ));
                }
            }
        });
        json!({"ok": true, "job_id": job_id})
    }

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
        if let Some(manager) = self.mcp_manager() {
            let _ = self.runtime.block_on(async {
                manager.shutdown().await;
            });
        }
    }

    /// Write only the `mcp:` section back to `config.yaml`, preserving keys the
    /// typed model does not own.
    fn persist_mcp_config(&self, mcp: &aish_config::McpConfig) -> Result<(), String> {
        let path = aish_config::ConfigLoader::default_config_path();
        aish_config::ConfigLoader::update_raw(&path, |document| {
            aish_mcp::commands::merge_into_document(document, mcp).map_err(AishError::Config)
        })
        .map_err(|error| format!("failed to save MCP configuration: {error}"))
    }

    /// Start (or reuse) the MCP runtime, then apply settings in the background.
    fn spawn_mcp_apply(&self, settings: aish_mcp::McpSettings) -> String {
        let job_id = self.next_job_id();
        let mut guard = match self.mcp_manager.lock() {
            Ok(guard) => guard,
            Err(_) => {
                self.emit_settings_job(
                    &job_id,
                    "mcp_apply",
                    Err("MCP manager lock poisoned".to_string()),
                );
                return job_id;
            }
        };
        if guard.is_none() && !settings.is_inactive() {
            let manager = McpManager::new(settings.clone());
            manager.start(self.runtime.handle());
            *guard = Some(manager);
        }
        let manager = guard.clone();
        drop(guard);
        let Some(manager) = manager else {
            self.emit_settings_job(&job_id, "mcp_apply", Ok(json!({"inactive": true})));
            return job_id;
        };
        let events = self.events.clone();
        let job = job_id.clone();
        self.runtime.spawn(async move {
            let outcome = manager.apply_settings(settings).await;
            events.push(ChatEvent::new(
                "settings",
                "settings_job",
                json!({
                    "job_id": job,
                    "job": "mcp_apply",
                    "ok": true,
                    "data": {
                        "added": outcome.added,
                        "removed": outcome.removed,
                        "updated": outcome.updated,
                    },
                }),
            ));
        });
        job_id
    }

    /// Probe one server definition with a bounded connect/list/close cycle.
    fn spawn_mcp_probe(&self, label: String, server: aish_mcp::ServerSettings) -> Value {
        let job_id = self.next_job_id();
        let events = self.events.clone();
        let job = job_id.clone();
        self.runtime.spawn(async move {
            let token = Arc::new(aish_llm::CancellationToken::new());
            let cancel = aish_mcp::CancelListener::new(token.clone());
            let (event_tx, _event_rx) = tokio::sync::mpsc::unbounded_channel();
            let options = aish_mcp::ConnectOptions {
                roots: std::env::current_dir()
                    .ok()
                    .map(|path| vec![path.to_string_lossy().to_string()])
                    .unwrap_or_default(),
                cancel: Some(cancel.clone()),
                events: event_tx,
            };
            let probe = tokio::time::timeout(Duration::from_secs(12), async {
                let connection = match aish_mcp::client::connect(&server, options).await {
                    Ok(connection) => connection,
                    Err(error) => return Err(error.describe()),
                };
                let version = connection.server_info().version.clone();
                let tools = match connection.list_tools(Some(cancel)).await {
                    Ok(tools) => tools,
                    Err(error) => {
                        connection.close().await;
                        return Err(error.describe());
                    }
                };
                connection.close().await;
                Ok((version, tools))
            })
            .await;
            match probe {
                Ok(Ok((version, tools))) => {
                    let tools: Vec<Value> = tools
                        .iter()
                        .map(|tool| {
                            json!({
                                "name": tool.name,
                                "description": tool.description,
                            })
                        })
                        .collect();
                    events.push(ChatEvent::new(
                        "settings",
                        "settings_job",
                        json!({
                            "job_id": job,
                            "job": "mcp_test",
                            "ok": true,
                            "data": {
                                "label": label,
                                "version": version,
                                "tools": tools,
                            },
                        }),
                    ));
                }
                Ok(Err(error)) => {
                    events.push(ChatEvent::new(
                        "settings",
                        "settings_job",
                        json!({
                            "job_id": job,
                            "job": "mcp_test",
                            "ok": false,
                            "error": error,
                            "data": {"label": label},
                        }),
                    ));
                }
                Err(_) => {
                    token.cancel();
                    events.push(ChatEvent::new(
                        "settings",
                        "settings_job",
                        json!({
                            "job_id": job,
                            "job": "mcp_test",
                            "ok": false,
                            "error": "连接超时（12 秒）",
                            "data": {"label": label},
                        }),
                    ));
                }
            }
        });
        json!({"ok": true, "job_id": job_id})
    }

    /// Clone the current MCP runtime handle, if one exists.
    fn mcp_manager(&self) -> Option<Arc<McpManager>> {
        self.mcp_manager
            .lock()
            .ok()
            .and_then(|manager| manager.clone())
    }

    /// Next id for a background settings job.
    fn next_job_id(&self) -> String {
        format!(
            "settings-job-{}",
            self.job_seq.fetch_add(1, Ordering::Relaxed)
        )
    }

    /// Emit one background job result. The frontend routes `settings_job`
    /// events before its normal session filter because they are not tied to a
    /// chat session.
    fn emit_settings_job(&self, job_id: &str, job: &str, result: Result<Value, String>) {
        let data = match result {
            Ok(data) => json!({"job_id": job_id, "job": job, "ok": true, "data": data}),
            Err(error) => {
                json!({"job_id": job_id, "job": job, "ok": false, "error": error})
            }
        };
        self.events
            .push(ChatEvent::new("settings", "settings_job", data));
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
    iteration_count: Arc<AtomicU32>,
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
        self.iteration_count.store(0, Ordering::Relaxed);
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

                let history_len = match self.history.lock() {
                    Ok(mut history) => {
                        history.extend(new_messages.iter().cloned());
                        history.len()
                    }
                    Err(_) => 0,
                };
                self.persist(&new_messages);

                let stats = self.llm.token_stats();
                self.emit(
                    "turn_done",
                    json!({
                        "text": result.text,
                        "history_len": history_len,
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
fn json_string_array(value: &Value, field: &str) -> Result<Vec<String>, String> {
    let array = value
        .as_array()
        .ok_or_else(|| format!("`{field}` must be an array of strings"))?;
    array
        .iter()
        .map(|entry| {
            entry
                .as_str()
                .map(str::to_string)
                .ok_or_else(|| format!("`{field}` must contain only strings"))
        })
        .collect()
}

fn json_pair_map(
    value: &Value,
    field: &str,
) -> Result<std::collections::BTreeMap<String, String>, String> {
    let Some(array) = value.as_array() else {
        return Err(format!(
            "`{field}` must be an array of {{key, value}} objects"
        ));
    };
    let mut map = std::collections::BTreeMap::new();
    for entry in array {
        let key = entry
            .get("key")
            .and_then(Value::as_str)
            .map(str::trim)
            .filter(|key| !key.is_empty())
            .ok_or_else(|| format!("`{field}` entries need a non-empty key"))?;
        let value = entry
            .get("value")
            .and_then(Value::as_str)
            .ok_or_else(|| format!("`{field}` entries need a string value"))?;
        map.insert(key.to_string(), value.to_string());
    }
    Ok(map)
}

fn provider_host(base: &str) -> Option<String> {
    reqwest::Url::parse(base.trim())
        .ok()
        .and_then(|url| url.host_str().map(str::to_ascii_lowercase))
}

fn provider_protocol(base: &str) -> &'static str {
    if provider_host(base).as_deref() == Some("api.anthropic.com") {
        "anthropic"
    } else {
        "openai"
    }
}

fn provider_host_label(base: &str) -> String {
    match provider_host(base).as_deref() {
        Some("api.anthropic.com") => "Anthropic".to_string(),
        Some("api.openai.com") => "OpenAI".to_string(),
        Some("api.deepseek.com") => "DeepSeek".to_string(),
        Some(host) => host.to_string(),
        None => base.trim().to_string(),
    }
}

/// The GUI only supports third-party OpenAI-compatible endpoints. aish still
/// recognizes `api.anthropic.com` for configurations edited by hand, but the
/// provider UI must not silently write an Anthropic endpoint under the
/// OpenAI label.
fn validate_provider_protocol(api_base: &str, protocol: &str) -> Result<(), String> {
    let protocol = protocol.trim().to_ascii_lowercase();
    if !matches!(protocol.as_str(), "openai" | "anthropic") {
        return Err("协议必须是 openai 或 anthropic".to_string());
    }
    let actual = provider_protocol(api_base);
    if protocol == actual {
        return Ok(());
    }
    if actual == "anthropic" {
        Err("Anthropic 内置供应商只支持 https://api.anthropic.com".to_string())
    } else {
        Err("自定义供应商只支持 OpenAI 兼容协议；Anthropic 请使用内置供应商".to_string())
    }
}

fn unique_account_name(accounts: &[aish_config::ApiAccountConfig], label: &str) -> String {
    let label = label.trim();
    let label = if label.is_empty() { "provider" } else { label };
    if !accounts.iter().any(|account| account.name == label) {
        return label.to_string();
    }
    let mut index = 2;
    loop {
        let candidate = format!("{label}-{index}");
        if !accounts.iter().any(|account| account.name == candidate) {
            return candidate;
        }
        index += 1;
    }
}

/// Fetch models for one OpenAI-compatible provider without touching the active
/// chat session. Anthropic-native endpoints are not part of the third-party
/// provider UI.
async fn fetch_provider_models(
    api_base: &str,
    api_key: &str,
    protocol: &str,
) -> Result<Vec<Value>, String> {
    validate_provider_protocol(api_base, protocol)?;
    let base = api_base.trim().trim_end_matches('/');
    if base.is_empty() {
        return Err("API 地址不能为空".to_string());
    }
    if protocol.trim().eq_ignore_ascii_case("anthropic") {
        return fetch_anthropic_models(base, api_key).await;
    }
    let models = aish_llm::fetch_models_from_api(
        base,
        (!api_key.trim().is_empty()).then_some(api_key.trim()),
    )
    .await?;
    let mut seen = HashSet::new();
    let mut result: Vec<Value> = models
        .iter()
        .filter(|model| aish_llm::model_supports_tools(&model.id))
        .filter(|model| seen.insert(model.id.clone()))
        .map(|model| {
            json!({
                "id": model.id,
                "owned_by": model.owned_by,
            })
        })
        .collect();
    result.sort_by(|left, right| {
        left["id"]
            .as_str()
            .unwrap_or_default()
            .cmp(right["id"].as_str().unwrap_or_default())
    });
    result.truncate(300);
    if result.is_empty() {
        return Err("端点返回了空模型列表".to_string());
    }
    Ok(result)
}

async fn fetch_anthropic_models(base: &str, api_key: &str) -> Result<Vec<Value>, String> {
    if api_key.trim().is_empty() {
        return Err("Anthropic 需要 API Key".to_string());
    }
    let base = base.trim_end_matches('/');
    let base = base.strip_suffix("/v1/messages").unwrap_or(base);
    let url = if base.ends_with("/v1") {
        format!("{base}/models")
    } else {
        format!("{base}/v1/models")
    };
    let client = reqwest::Client::builder()
        .timeout(Duration::from_secs(12))
        .build()
        .map_err(|error| format!("failed to build HTTP client: {error}"))?;
    let response = client
        .get(&url)
        .header("x-api-key", api_key.trim())
        .header("anthropic-version", "2023-06-01")
        .send()
        .await
        .map_err(|error| {
            if error.is_timeout() {
                "请求超时（12 秒）".to_string()
            } else if error.is_connect() {
                "连接失败，请检查 API 地址和网络".to_string()
            } else {
                format!("请求失败：{error}")
            }
        })?;
    let status = response.status();
    if !status.is_success() {
        let body = response.text().await.unwrap_or_default();
        let body = aish_mcp::redact::sanitize_diagnostic_text(&body);
        let mut preview: String = body.chars().take(300).collect();
        if body.chars().count() > 300 {
            preview.push_str("...");
        }
        return Err(format!(
            "Anthropic 返回 HTTP {}：{preview}",
            status.as_u16()
        ));
    }
    let body: Value = response
        .json()
        .await
        .map_err(|error| format!("无法解析模型列表：{error}"))?;
    let mut result: Vec<Value> = body
        .get("data")
        .and_then(Value::as_array)
        .map(|items| {
            items
                .iter()
                .filter_map(|item| {
                    let id = item.get("id").and_then(Value::as_str)?;
                    Some(json!({
                        "id": id,
                        "owned_by": item
                            .get("display_name")
                            .and_then(Value::as_str)
                            .unwrap_or("anthropic"),
                    }))
                })
                .collect()
        })
        .unwrap_or_default();
    result.sort_by(|left, right| {
        left["id"]
            .as_str()
            .unwrap_or_default()
            .cmp(right["id"].as_str().unwrap_or_default())
    });
    result.truncate(300);
    if result.is_empty() {
        return Err("Anthropic 返回了空模型列表".to_string());
    }
    Ok(result)
}

fn normalize_mcp_url(raw: &str) -> Option<String> {
    let trimmed = raw.trim();
    if trimmed.is_empty() {
        return None;
    }
    if trimmed.contains("://") {
        Some(trimmed.to_string())
    } else {
        Some(format!("https://{trimmed}"))
    }
}

fn parse_transport(value: &str) -> Result<aish_mcp::TransportKind, String> {
    match value.trim().to_ascii_lowercase().as_str() {
        "stdio" | "" => Ok(aish_mcp::TransportKind::Stdio),
        "http" => Ok(aish_mcp::TransportKind::Http),
        other => Err(format!(
            "unknown transport `{other}` (expected stdio or http)"
        )),
    }
}

/// Build `/mcp add` arguments from the QML form JSON.
fn add_args_from_json(value: &Value) -> Result<aish_mcp::AddArgs, String> {
    let name = value
        .get("name")
        .and_then(Value::as_str)
        .map(str::trim)
        .filter(|name| !name.is_empty())
        .ok_or_else(|| "MCP server name must not be empty".to_string())?;
    let transport = value
        .get("transport")
        .and_then(Value::as_str)
        .map(parse_transport)
        .transpose()?;
    let command = match value.get("command") {
        Some(Value::Array(_)) => json_string_array(value.get("command").unwrap(), "command")?,
        Some(Value::String(command)) if !command.trim().is_empty() => {
            vec![command.trim().to_string()]
        }
        Some(_) => return Err("`command` must be a string or string array".to_string()),
        None => Vec::new(),
    };
    let extra_args = match value.get("args") {
        Some(Value::Array(_)) => json_string_array(value.get("args").unwrap(), "args")?,
        Some(_) => return Err("`args` must be a string array".to_string()),
        None => Vec::new(),
    };
    let mut argv = command;
    argv.extend(extra_args);
    let url = value
        .get("url")
        .and_then(Value::as_str)
        .and_then(normalize_mcp_url);
    let trust = value
        .get("trust")
        .and_then(Value::as_str)
        .map(|level| {
            aish_mcp::TrustLevel::parse(level)
                .ok_or_else(|| format!("unknown trust level `{level}`"))
        })
        .transpose()?;
    let side_effects = value
        .get("side_effects")
        .and_then(Value::as_str)
        .map(|effects| {
            aish_mcp::SideEffects::parse(effects)
                .ok_or_else(|| format!("unknown side effects `{effects}`"))
        })
        .transpose()?;
    let expose = value
        .get("expose")
        .and_then(Value::as_str)
        .filter(|value| !value.trim().is_empty())
        .map(|mode| {
            aish_mcp::ExposeMode::parse(mode).ok_or_else(|| format!("invalid expose mode `{mode}`"))
        })
        .transpose()?;
    let env = match value.get("env") {
        Some(env) => json_pair_map(env, "env")?,
        None => Default::default(),
    };
    let headers = match value.get("headers") {
        Some(headers) => json_pair_map(headers, "headers")?,
        None => Default::default(),
    };
    Ok(aish_mcp::AddArgs {
        name: Some(name.to_string()),
        transport,
        command: argv,
        url,
        token: value
            .get("token")
            .and_then(Value::as_str)
            .map(str::trim)
            .filter(|token| !token.is_empty())
            .map(str::to_string),
        trust,
        side_effects,
        timeout_ms: value.get("request_timeout_ms").and_then(Value::as_u64),
        max_output_bytes: value
            .get("max_output_bytes")
            .and_then(Value::as_u64)
            .map(|value| value as usize),
        expose,
        env: env.into_iter().collect(),
        headers: headers.into_iter().collect(),
    })
}

/// Apply only the fields present in `args` to an existing server entry.
///
/// The name is immutable: renaming would change the `mcp__<name>` tool prefix
/// and lose unknown YAML keys, so callers remove and re-add instead.
fn update_server_from_json(
    mut server: aish_config::McpServerConfig,
    args: &Value,
) -> Result<aish_config::McpServerConfig, String> {
    if let Some(requested) = args.get("name").and_then(Value::as_str) {
        if requested.trim() != server.name {
            return Err("MCP server rename is not supported; remove and add it again".to_string());
        }
    }
    if let Some(enabled) = args.get("enabled").and_then(Value::as_bool) {
        server.enabled = enabled;
    }
    if let Some(transport) = args.get("transport").and_then(Value::as_str) {
        server.transport = parse_transport(transport)?.as_str().to_string();
    }
    if args.get("command").is_some() || args.get("args").is_some() {
        let mut argv = match args.get("command") {
            Some(Value::Array(_)) => json_string_array(args.get("command").unwrap(), "command")?,
            Some(Value::String(command)) if !command.trim().is_empty() => {
                vec![command.trim().to_string()]
            }
            Some(_) => return Err("`command` must be a string or string array".to_string()),
            None => Vec::new(),
        };
        if let Some(extra) = args.get("args") {
            argv.extend(json_string_array(extra, "args")?);
        }
        if argv.is_empty() {
            return Err("stdio transport needs a command".to_string());
        }
        server.command = Some(argv.remove(0));
        server.args = argv;
    }
    if let Some(url) = args.get("url") {
        server.url = url.as_str().and_then(normalize_mcp_url);
    }
    if let Some(env) = args.get("env") {
        server.env = json_pair_map(env, "env")?;
    }
    if let Some(headers) = args.get("headers") {
        server.headers = json_pair_map(headers, "headers")?;
    }
    if let Some(token) = args
        .get("token")
        .and_then(Value::as_str)
        .map(str::trim)
        .filter(|token| !token.is_empty())
    {
        if !server
            .headers
            .keys()
            .any(|key| key.eq_ignore_ascii_case("authorization"))
        {
            server
                .headers
                .insert("Authorization".to_string(), format!("Bearer {token}"));
        }
    }
    if let Some(cwd) = args.get("cwd") {
        server.cwd = cwd
            .as_str()
            .map(str::trim)
            .filter(|cwd| !cwd.is_empty())
            .map(str::to_string);
    }
    if let Some(trust) = args.get("trust").and_then(Value::as_str) {
        let level = aish_mcp::TrustLevel::parse(trust)
            .ok_or_else(|| format!("unknown trust level `{trust}`"))?;
        server.trust = level.as_str().to_string();
    }
    if let Some(effects) = args.get("side_effects").and_then(Value::as_str) {
        let effects = aish_mcp::SideEffects::parse(effects)
            .ok_or_else(|| format!("unknown side effects `{effects}`"))?;
        server.side_effects = effects.as_str().to_string();
    }
    if let Some(expose) = args.get("expose").and_then(Value::as_str) {
        if !expose.trim().is_empty() {
            let mode = aish_mcp::ExposeMode::parse(expose)
                .ok_or_else(|| format!("invalid expose mode `{expose}`"))?;
            server.expose = match mode {
                aish_mcp::ExposeMode::Gateway => "gateway",
                aish_mcp::ExposeMode::Tools => "tools",
            }
            .to_string();
        }
    }
    if let Some(timeout) = args.get("request_timeout_ms") {
        server.request_timeout_ms = timeout.as_u64();
    }
    if let Some(max_output) = args.get("max_output_bytes") {
        server.max_output_bytes = max_output.as_u64().map(|value| value as usize);
    }
    let transport = parse_transport(&server.transport)?;
    match transport {
        aish_mcp::TransportKind::Stdio => {
            if server
                .command
                .as_deref()
                .unwrap_or_default()
                .trim()
                .is_empty()
            {
                return Err("stdio transport needs a command".to_string());
            }
            if server.url.is_some() {
                return Err("stdio transport cannot use a URL".to_string());
            }
        }
        aish_mcp::TransportKind::Http => {
            if server.url.as_deref().unwrap_or_default().trim().is_empty() {
                return Err("http transport needs a URL".to_string());
            }
            if server.command.is_some() || !server.args.is_empty() {
                return Err("http transport cannot use a command".to_string());
            }
        }
    }
    Ok(server)
}

/// Load the full skill record for one installed name (quarantined included).
fn load_skill_by_name(name: &str) -> Result<aish_skills::Skill, String> {
    let mut manager = SkillManager::new();
    manager
        .load_all_skills()
        .map_err(|error| format!("failed to load skills: {error}"))?;
    manager
        .get_skill(name)
        .cloned()
        .ok_or_else(|| format!("skill not found: {name}"))
}

/// Registry adapters from the shared aish config, read fresh so CLI edits are
/// picked up without restarting the frontend.
fn registry_configs_from_config() -> Vec<aish_skills::registry::RegistryConfig> {
    let config = aish_config::ConfigLoader::load(None).unwrap_or_default();
    config
        .skills
        .registries
        .iter()
        .map(|registry| aish_skills::registry::RegistryConfig {
            name: registry.name.clone(),
            registry_type: registry.registry_type.clone(),
            enabled: registry.enabled,
            url: registry.url.clone(),
        })
        .collect()
}

/// Pick a page ending at `end` and beginning at a user turn.
fn transcript_window(history: &[ChatMessage], end: usize, limit: usize) -> (usize, usize) {
    let limit = limit.clamp(1, 200);
    let end = end.min(history.len());
    let mut start = end.saturating_sub(limit);
    while start > 0 && history[start].role != "user" {
        start -= 1;
    }
    (start, end)
}

/// Snapshot every trusted skill into the shape `SkillTool` consumes.
///
/// Quarantined skills are deliberately not advertised: the tool cannot find
/// them, so the model cannot activate unreviewed content.
fn load_skill_snapshot() -> HashMap<String, SkillInfo> {
    let mut manager = SkillManager::new();
    let _ = manager.load_all_skills();
    manager
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
        .collect()
}

/// Read the raw `rules:` sequence from a policy document.
///
/// The editor needs the YAML entry shape (`path`, not the flattened typed
/// `pattern`) so a save cannot silently drop every rule.
fn read_raw_policy_rules(path: &Path) -> Result<Vec<Value>, String> {
    let text = fs::read_to_string(path)
        .map_err(|error| format!("failed to read {}: {error}", path.display()))?;
    let document: serde_yaml::Value =
        serde_yaml::from_str(&text).map_err(|error| format!("invalid policy YAML: {error}"))?;
    let Some(mapping) = document.as_mapping() else {
        return Err("policy document is not a mapping".to_string());
    };
    let Some(rules) = mapping.get(serde_yaml::Value::String("rules".to_string())) else {
        return Ok(Vec::new());
    };
    let Some(sequence) = rules.as_sequence() else {
        return Err("policy `rules` is not a sequence".to_string());
    };
    sequence
        .iter()
        .map(|rule| {
            serde_json::to_value(rule)
                .map_err(|error| format!("failed to encode policy rule: {error}"))
        })
        .collect()
}

/// Validate the shape the YAML loader actually understands.
///
/// The loader silently ignores a rule without `path` or with an invalid
/// `risk`, which is unacceptable for an editor: a typo would remove
/// protection without an error. Reject those shapes before writing.
fn validate_raw_policy_rules(rules: &[Value]) -> Result<(), String> {
    for (index, rule) in rules.iter().enumerate() {
        let position = index + 1;
        let Some(object) = rule.as_object() else {
            return Err(format!("第 {position} 条规则不是对象"));
        };
        let path_ok = match object.get("path") {
            Some(Value::String(value)) => !value.trim().is_empty(),
            Some(Value::Array(values)) => {
                !values.is_empty()
                    && values
                        .iter()
                        .all(|value| value.as_str().is_some_and(|text| !text.trim().is_empty()))
            }
            _ => false,
        };
        if !path_ok {
            return Err(format!(
                "第 {position} 条规则缺少 path（必须是字符串或非空字符串数组）；注意 aish 不接受 pattern 字段"
            ));
        }
        let risk = object
            .get("risk")
            .and_then(Value::as_str)
            .map(|value| value.trim().to_ascii_uppercase())
            .unwrap_or_default();
        if !matches!(risk.as_str(), "LOW" | "MEDIUM" | "HIGH") {
            return Err(format!(
                "第 {position} 条规则的 risk 必须是 LOW、MEDIUM 或 HIGH"
            ));
        }
        for field in ["command_list", "operations", "exclude"] {
            if let Some(value) = object.get(field) {
                let Some(values) = value.as_array() else {
                    return Err(format!("第 {position} 条规则的 {field} 必须是字符串数组"));
                };
                if values.iter().any(|value| value.as_str().is_none()) {
                    return Err(format!("第 {position} 条规则的 {field} 只能包含字符串"));
                }
            }
        }
        for field in [
            "id",
            "name",
            "description",
            "reason",
            "confirm_message",
            "suggestion",
        ] {
            if let Some(value) = object.get(field) {
                if !value.is_null() && value.as_str().is_none() {
                    return Err(format!("第 {position} 条规则的 {field} 必须是字符串"));
                }
            }
        }
    }
    Ok(())
}

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
    let snapshot = load_skill_snapshot();
    let names: Vec<String> = snapshot.keys().cloned().collect();
    let lookup_snapshot = Arc::new(snapshot);
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

#[cfg(test)]
mod transcript_tests {
    use super::*;

    fn message(role: &str) -> ChatMessage {
        ChatMessage {
            role: role.to_string(),
            content: None,
            tool_calls: None,
            tool_call_id: None,
            name: None,
            reasoning_content: None,
            cache_control: None,
        }
    }

    #[test]
    fn transcript_window_starts_at_a_user_turn() {
        // user, assistant+tool result, user, assistant, ...
        let mut history = Vec::new();
        for _ in 0..6 {
            history.push(message("user"));
            history.push(message("assistant"));
            history.push(message("tool"));
            history.push(message("assistant"));
        }
        let (start, end) = transcript_window(&history, history.len(), 4);
        assert_eq!(end, history.len());
        assert_eq!(history[start].role, "user");
        assert!(start > 0);
        assert!(history.len() - start <= 4);
    }

    #[test]
    fn transcript_window_handles_short_histories() {
        let history = vec![message("user"), message("assistant")];
        let (start, end) = transcript_window(&history, history.len(), 40);
        assert_eq!((start, end), (0, 2));
    }

    #[test]
    fn third_party_provider_validation_is_openai_only() {
        assert!(validate_provider_protocol("https://api.deepseek.com/v1", "openai").is_ok());
        assert!(validate_provider_protocol("https://api.anthropic.com", "openai").is_err());
        assert!(validate_provider_protocol("https://api.deepseek.com", "anthropic").is_err());
        assert!(validate_provider_protocol("https://api.deepseek.com", "other").is_err());
    }
}
