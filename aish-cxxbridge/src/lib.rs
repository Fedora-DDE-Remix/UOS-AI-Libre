//! Thin cxx bridge around `aish-chat`.
//!
//! The bridge deliberately returns JSON instead of throwing across FFI. A Qt
//! wrapper can connect `wait_event` to a worker thread or poll `poll_event`
//! from a timer, then emit a Qt signal. This keeps the Rust/C++ boundary small
//! and panic-safe.

use std::time::Duration;

use serde_json::json;

#[cxx::bridge]
mod ffi {
    extern "Rust" {
        type BridgeService;

        fn new_service(config_json: &str) -> Box<BridgeService>;
        fn new_service_from_default_config() -> Box<BridgeService>;
        fn create_session(service: &mut BridgeService, model: &str) -> String;
        fn send_message(
            service: &mut BridgeService,
            session_id: &str,
            text: &str,
            images_json: &str,
        ) -> String;
        fn wait_event(service: &mut BridgeService, timeout_ms: u64) -> String;
        fn poll_event(service: &mut BridgeService) -> String;
        fn cancel_session(service: &mut BridgeService, session_id: &str);
        fn approve(service: &mut BridgeService, request_id: &str, decision: i32) -> bool;
        fn respond_interaction(
            service: &mut BridgeService,
            request_id: &str,
            response_json: &str,
        ) -> bool;
        fn tool_names(service: &mut BridgeService, session_id: &str) -> String;
        fn tool_catalog(service: &mut BridgeService) -> String;
        fn policy_info(service: &mut BridgeService) -> String;
        fn save_policy(service: &mut BridgeService, fields_json: &str) -> String;
        fn save_policy_rules(
            service: &mut BridgeService,
            rules_json: &str,
            expected_fingerprint: &str,
        ) -> String;
        fn preview_policy_command(
            service: &mut BridgeService,
            command: &str,
            draft_rules_json: &str,
        ) -> String;
        fn session_approvals(service: &mut BridgeService, session_id: &str) -> String;
        fn revoke_session_approval(
            service: &mut BridgeService,
            session_id: &str,
            kind: &str,
            first: &str,
            second: &str,
        ) -> String;
        fn clear_session_approvals(service: &mut BridgeService, session_id: &str) -> String;
        fn skill_catalog(service: &mut BridgeService) -> String;
        fn refresh_skills_live(service: &mut BridgeService) -> String;
        fn trust_skill(service: &mut BridgeService, name: &str) -> String;
        fn skill_read(service: &mut BridgeService, name: &str, max_bytes: u32) -> String;
        fn skill_verify(service: &mut BridgeService, name: &str) -> String;
        fn skill_remove(service: &mut BridgeService, name: &str) -> String;
        fn skill_search_start(service: &mut BridgeService, query: &str, limit: u32) -> String;
        fn skill_install_start(service: &mut BridgeService, skill_id: &str) -> String;
        fn mcp_status(service: &mut BridgeService) -> String;
        fn refresh_mcp_tools(service: &mut BridgeService) -> String;
        fn mcp_reload(service: &mut BridgeService) -> String;
        fn mcp_add(service: &mut BridgeService, args_json: &str) -> String;
        fn mcp_update(service: &mut BridgeService, name: &str, args_json: &str) -> String;
        fn mcp_remove(service: &mut BridgeService, name: &str) -> String;
        fn mcp_set_master_enabled(service: &mut BridgeService, enabled: bool) -> String;
        fn mcp_set_enabled(service: &mut BridgeService, name: &str, enabled: bool) -> String;
        fn mcp_set_trust(
            service: &mut BridgeService,
            name: &str,
            tool: &str,
            level: &str,
        ) -> String;
        fn mcp_test(service: &mut BridgeService, name: &str) -> String;
        fn mcp_test_draft(service: &mut BridgeService, args_json: &str) -> String;
        fn memory_list(service: &mut BridgeService, limit: u32) -> String;
        fn audit_tail(service: &mut BridgeService, limit: u32) -> String;
        fn clear_audit_events(service: &mut BridgeService) -> String;
        fn list_sessions(service: &mut BridgeService, limit: u32) -> String;
        fn resume_session(service: &mut BridgeService, session_id: &str) -> String;
        fn resume_session_window(
            service: &mut BridgeService,
            session_id: &str,
            limit: u32,
        ) -> String;
        fn history_page(
            service: &mut BridgeService,
            session_id: &str,
            before: u32,
            limit: u32,
        ) -> String;
        fn undo_to_message(
            service: &mut BridgeService,
            session_id: &str,
            keep_count: u32,
        ) -> String;
        fn delete_session(service: &mut BridgeService, session_id: &str) -> String;
        fn rename_session(service: &mut BridgeService, session_id: &str, title: &str) -> String;
        fn config_info(service: &mut BridgeService) -> String;
        fn save_model_config(
            service: &mut BridgeService,
            model: &str,
            api_base: &str,
            api_key: &str,
        ) -> String;
        fn provider_info(service: &mut BridgeService) -> String;
        fn save_provider_config(
            service: &mut BridgeService,
            name: &str,
            api_base: &str,
            api_key: &str,
            model: &str,
            protocol: &str,
            activate: bool,
            current_name: &str,
        ) -> String;
        fn delete_provider_account(service: &mut BridgeService, name: &str) -> String;
        fn model_fetch_start(
            service: &mut BridgeService,
            api_base: &str,
            api_key: &str,
            protocol: &str,
        ) -> String;
        fn set_workspace(service: &mut BridgeService, session_id: &str, path: &str) -> String;
        fn shutdown(service: &mut BridgeService);
    }
}

pub struct BridgeService {
    inner: Option<aish_chat::ChatService>,
    error: Option<String>,
}

fn err(message: impl Into<String>) -> String {
    json!({"ok": false, "error": message.into()}).to_string()
}

fn wrap_service(service: Result<aish_chat::ChatService, String>) -> Box<BridgeService> {
    match service {
        Ok(inner) => Box::new(BridgeService {
            inner: Some(inner),
            error: None,
        }),
        Err(error) => Box::new(BridgeService {
            inner: None,
            error: Some(error),
        }),
    }
}

pub fn new_service(config_json: &str) -> Box<BridgeService> {
    wrap_service(
        std::panic::catch_unwind(|| aish_chat::ChatService::from_json(config_json))
            .unwrap_or_else(|_| Err("panic while creating chat service".to_string())),
    )
}

pub fn new_service_from_default_config() -> Box<BridgeService> {
    wrap_service(
        std::panic::catch_unwind(|| aish_chat::ChatService::from_aish_config())
            .unwrap_or_else(|_| Err("panic while creating chat service".to_string())),
    )
}

pub fn create_session(service: &mut BridgeService, model: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.create_session(Some(model))
    })) {
        Ok(Ok(session_id)) => json!({"ok": true, "session_id": session_id}).to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while creating session"),
    }
}

pub fn send_message(
    service: &mut BridgeService,
    session_id: &str,
    text: &str,
    images_json: &str,
) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    let images: Vec<String> = match serde_json::from_str(images_json) {
        Ok(images) => images,
        Err(error) => return err(format!("invalid images JSON: {error}")),
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.send(session_id, text, images)
    })) {
        Ok(Ok(())) => json!({"ok": true}).to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while sending message"),
    }
}

pub fn wait_event(service: &mut BridgeService, timeout_ms: u64) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match inner.wait_event(Duration::from_millis(timeout_ms)) {
        Some(event) => {
            serde_json::to_string(&event).unwrap_or_else(|_| err("failed to serialize event"))
        }
        None => String::new(),
    }
}

pub fn poll_event(service: &mut BridgeService) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match inner.poll_event() {
        Some(event) => {
            serde_json::to_string(&event).unwrap_or_else(|_| err("failed to serialize event"))
        }
        None => String::new(),
    }
}

pub fn cancel_session(service: &mut BridgeService, session_id: &str) {
    if let Some(inner) = service.inner.as_ref() {
        inner.cancel(session_id);
    }
}

pub fn approve(service: &mut BridgeService, request_id: &str, decision: i32) -> bool {
    service
        .inner
        .as_ref()
        .map(|inner| inner.approve(request_id, decision))
        .unwrap_or(false)
}

pub fn respond_interaction(
    service: &mut BridgeService,
    request_id: &str,
    response_json: &str,
) -> bool {
    service
        .inner
        .as_ref()
        .map(|inner| inner.respond_interaction(request_id, response_json))
        .unwrap_or(false)
}

pub fn tool_names(service: &mut BridgeService, session_id: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.tool_names(session_id)
    })) {
        Ok(Ok(names)) => json!({"ok": true, "tools": names}).to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while listing tools"),
    }
}

pub fn tool_catalog(service: &mut BridgeService) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.tool_catalog().to_string()
    }))
    .unwrap_or_else(|_| err("panic while reading tool catalog"))
}

pub fn policy_info(service: &mut BridgeService) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.policy_info().to_string()
    }))
    .unwrap_or_else(|_| err("panic while reading security policy"))
}

pub fn save_policy(service: &mut BridgeService, fields_json: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    let fields: serde_json::Value = match serde_json::from_str(fields_json) {
        Ok(value) => value,
        Err(error) => return err(format!("invalid policy JSON: {error}")),
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| inner.save_policy(&fields))) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while saving security policy"),
    }
}

pub fn save_policy_rules(
    service: &mut BridgeService,
    rules_json: &str,
    expected_fingerprint: &str,
) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    let rules: serde_json::Value = match serde_json::from_str(rules_json) {
        Ok(value) => value,
        Err(error) => return err(format!("invalid rules JSON: {error}")),
    };
    let expected = (!expected_fingerprint.trim().is_empty()).then_some(expected_fingerprint);
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.save_policy_rules(&rules, expected)
    })) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while saving policy rules"),
    }
}

pub fn preview_policy_command(
    service: &mut BridgeService,
    command: &str,
    draft_rules_json: &str,
) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    let draft = if draft_rules_json.trim().is_empty() {
        None
    } else {
        match serde_json::from_str::<serde_json::Value>(draft_rules_json) {
            Ok(value) => Some(value),
            Err(error) => return err(format!("invalid rules JSON: {error}")),
        }
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.preview_policy_command(command, draft.as_ref())
    })) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while previewing policy"),
    }
}

pub fn session_approvals(service: &mut BridgeService, session_id: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.session_approvals(session_id)
    })) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while reading session approvals"),
    }
}

pub fn revoke_session_approval(
    service: &mut BridgeService,
    session_id: &str,
    kind: &str,
    first: &str,
    second: &str,
) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.revoke_session_approval(session_id, kind, first, second)
    })) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while revoking session approval"),
    }
}

pub fn clear_session_approvals(service: &mut BridgeService, session_id: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.clear_session_approvals(session_id)
    })) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while clearing session approvals"),
    }
}

pub fn skill_catalog(service: &mut BridgeService) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.skill_catalog().to_string()
    }))
    .unwrap_or_else(|_| err("panic while reading skills"))
}

pub fn trust_skill(service: &mut BridgeService, name: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| inner.trust_skill(name))) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while trusting skill"),
    }
}

pub fn refresh_skills_live(service: &mut BridgeService) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.refresh_skills_live().to_string()
    }))
    .unwrap_or_else(|_| err("panic while refreshing live skills"))
}

pub fn skill_read(service: &mut BridgeService, name: &str, max_bytes: u32) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.skill_read(name, max_bytes as usize)
    })) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while reading skill"),
    }
}

pub fn skill_verify(service: &mut BridgeService, name: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| inner.skill_verify(name))) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while verifying skill"),
    }
}

pub fn skill_remove(service: &mut BridgeService, name: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| inner.skill_remove(name))) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while removing skill"),
    }
}

pub fn skill_search_start(service: &mut BridgeService, query: &str, limit: u32) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.start_skill_search(query, limit as usize).to_string()
    }))
    .unwrap_or_else(|_| err("panic while starting skill search"))
}

pub fn skill_install_start(service: &mut BridgeService, skill_id: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.start_skill_install(skill_id).to_string()
    }))
    .unwrap_or_else(|_| err("panic while starting skill install"))
}

pub fn mcp_status(service: &mut BridgeService) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.mcp_status().to_string()
    }))
    .unwrap_or_else(|_| err("panic while reading MCP status"))
}

pub fn mcp_reload(service: &mut BridgeService) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.mcp_reload().to_string()
    }))
    .unwrap_or_else(|_| err("panic while reloading MCP"))
}

pub fn refresh_mcp_tools(service: &mut BridgeService) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.refresh_mcp_tools().to_string()
    }))
    .unwrap_or_else(|_| err("panic while refreshing MCP tools"))
}

fn mcp_args(value: &str) -> Result<serde_json::Value, String> {
    serde_json::from_str(value).map_err(|error| format!("invalid MCP JSON: {error}"))
}

pub fn mcp_add(service: &mut BridgeService, args_json: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    let args = match mcp_args(args_json) {
        Ok(args) => args,
        Err(error) => return err(error),
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| inner.mcp_add(&args))) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while adding MCP server"),
    }
}

pub fn mcp_update(service: &mut BridgeService, name: &str, args_json: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    let args = match mcp_args(args_json) {
        Ok(args) => args,
        Err(error) => return err(error),
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.mcp_update(name, &args)
    })) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while updating MCP server"),
    }
}

pub fn mcp_remove(service: &mut BridgeService, name: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| inner.mcp_remove(name))) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while removing MCP server"),
    }
}

pub fn mcp_set_master_enabled(service: &mut BridgeService, enabled: bool) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.mcp_set_master_enabled(enabled)
    })) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while changing MCP master switch"),
    }
}

pub fn mcp_set_enabled(service: &mut BridgeService, name: &str, enabled: bool) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.mcp_set_enabled(name, enabled)
    })) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while enabling MCP server"),
    }
}

pub fn mcp_set_trust(service: &mut BridgeService, name: &str, tool: &str, level: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.mcp_set_trust(name, tool, level)
    })) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while changing MCP trust"),
    }
}

pub fn mcp_test(service: &mut BridgeService, name: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| inner.start_mcp_test(name))) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while testing MCP server"),
    }
}

pub fn mcp_test_draft(service: &mut BridgeService, args_json: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    let args = match mcp_args(args_json) {
        Ok(args) => args,
        Err(error) => return err(error),
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.start_mcp_test_draft(&args)
    })) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while testing MCP draft"),
    }
}

pub fn memory_list(service: &mut BridgeService, limit: u32) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.memory_list(limit as usize).to_string()
    }))
    .unwrap_or_else(|_| err("panic while reading memory"))
}

pub fn audit_tail(service: &mut BridgeService, limit: u32) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.audit_tail(limit as usize)
    })) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while reading audit events"),
    }
}

pub fn clear_audit_events(service: &mut BridgeService) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.clear_audit_events()
    })) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while clearing audit events"),
    }
}

pub fn list_sessions(service: &mut BridgeService, limit: u32) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.list_sessions(limit as usize)
    })) {
        Ok(Ok(sessions)) => json!({"ok": true, "sessions": sessions}).to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while listing sessions"),
    }
}

pub fn resume_session(service: &mut BridgeService, session_id: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.resume_session(session_id)
    })) {
        Ok(Ok((model, messages, cwd, warning))) => json!({
            "ok": true,
            "session_id": session_id,
            "model": model,
            "messages": messages,
            "cwd": cwd.to_string_lossy(),
            "warning": warning,
        })
        .to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while resuming session"),
    }
}

pub fn resume_session_window(
    service: &mut BridgeService,
    session_id: &str,
    limit: u32,
) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.resume_session_window(session_id, limit as usize)
    })) {
        Ok(Ok((model, messages, total, start, cwd, warning))) => json!({
            "ok": true,
            "session_id": session_id,
            "model": model,
            "messages": messages,
            "total": total,
            "start": start,
            "cwd": cwd.to_string_lossy(),
            "warning": warning,
        })
        .to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while resuming session window"),
    }
}

pub fn history_page(
    service: &mut BridgeService,
    session_id: &str,
    before: u32,
    limit: u32,
) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.history_page(session_id, before as usize, limit as usize)
    })) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while loading history page"),
    }
}

pub fn undo_to_message(
    service: &mut BridgeService,
    session_id: &str,
    keep_count: u32,
) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.undo_to_message(session_id, keep_count as usize)
    })) {
        Ok(Ok(value)) => value.to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while undoing session history"),
    }
}

pub fn delete_session(service: &mut BridgeService, session_id: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.delete_session(session_id)
    })) {
        Ok(Ok(())) => json!({"ok": true}).to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while deleting session"),
    }
}

pub fn rename_session(service: &mut BridgeService, session_id: &str, title: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.rename_session(session_id, title)
    })) {
        Ok(Ok(())) => json!({"ok": true}).to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while renaming session"),
    }
}

pub fn config_info(service: &mut BridgeService) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    json!({"ok": true, "config": inner.config_info()}).to_string()
}

pub fn save_model_config(
    service: &mut BridgeService,
    model: &str,
    api_base: &str,
    api_key: &str,
) -> String {
    let path = match std::panic::catch_unwind(|| {
        aish_chat::ChatService::save_model_settings(model, api_base, api_key)
    }) {
        Ok(Ok(path)) => path,
        Ok(Err(error)) => return err(error),
        Err(_) => return err("panic while saving model configuration"),
    };

    // The running service holds the old API key in memory. Recreate it so the
    // next session uses the saved configuration; active sessions are dropped.
    match std::panic::catch_unwind(|| aish_chat::ChatService::from_aish_config()) {
        Ok(Ok(inner)) => {
            service.inner = Some(inner);
            service.error = None;
            let config = service
                .inner
                .as_ref()
                .map(|inner| inner.config_info())
                .unwrap_or_else(|| json!({}));
            json!({"ok": true, "config_path": path, "config": config}).to_string()
        }
        Ok(Err(error)) => err(format!(
            "configuration saved to {path}, but reloading it failed: {error}"
        )),
        Err(_) => err(format!(
            "configuration saved to {path}, but reloading it panicked"
        )),
    }
}

pub fn provider_info(service: &mut BridgeService) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.provider_info().to_string()
    }))
    .unwrap_or_else(|_| err("panic while reading providers"))
}

pub fn save_provider_config(
    service: &mut BridgeService,
    name: &str,
    api_base: &str,
    api_key: &str,
    model: &str,
    protocol: &str,
    activate: bool,
    current_name: &str,
) -> String {
    let path = match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        aish_chat::ChatService::save_provider_settings(
            name, api_base, api_key, model, protocol, activate, current_name,
        )
    })) {
        Ok(Ok(path)) => path,
        Ok(Err(error)) => return err(error),
        Err(_) => return err("panic while saving provider configuration"),
    };
    match std::panic::catch_unwind(|| aish_chat::ChatService::from_aish_config()) {
        Ok(Ok(inner)) => {
            let providers = inner.provider_info();
            let config = inner.config_info();
            service.inner = Some(inner);
            service.error = None;
            json!({
                "ok": true,
                "config_path": path,
                "providers": providers["providers"].clone(),
                "config": config,
            })
            .to_string()
        }
        Ok(Err(error)) => err(format!(
            "provider saved to {path}, but reloading the service failed: {error}"
        )),
        Err(_) => err(format!(
            "provider saved to {path}, but reloading the service panicked"
        )),
    }
}

pub fn delete_provider_account(service: &mut BridgeService, name: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        aish_chat::ChatService::delete_provider_account(name)
    })) {
        Ok(Ok(_)) => json!({
            "ok": true,
            "providers": inner.provider_info()["providers"].clone(),
        })
        .to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while deleting provider"),
    }
}

pub fn model_fetch_start(
    service: &mut BridgeService,
    api_base: &str,
    api_key: &str,
    protocol: &str,
) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner
            .start_model_fetch(api_base, api_key, protocol)
            .to_string()
    }))
    .unwrap_or_else(|_| err("panic while starting model fetch"))
}

pub fn set_workspace(service: &mut BridgeService, session_id: &str, path: &str) -> String {
    let Some(inner) = service.inner.as_ref() else {
        return err(service
            .error
            .clone()
            .unwrap_or_else(|| "service unavailable".into()));
    };
    let session_id = (!session_id.trim().is_empty()).then_some(session_id);
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        inner.set_workspace(session_id, path)
    })) {
        Ok(Ok(canonical)) => json!({"ok": true, "path": canonical}).to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while switching workspace"),
    }
}

pub fn shutdown(service: &mut BridgeService) {
    if let Some(inner) = service.inner.as_ref() {
        inner.shutdown();
    }
}
