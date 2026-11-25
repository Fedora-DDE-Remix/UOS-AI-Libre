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
        fn list_sessions(service: &mut BridgeService, limit: u32) -> String;
        fn resume_session(service: &mut BridgeService, session_id: &str) -> String;
        fn delete_session(service: &mut BridgeService, session_id: &str) -> String;
        fn rename_session(service: &mut BridgeService, session_id: &str, title: &str) -> String;
        fn config_info(service: &mut BridgeService) -> String;
        fn save_model_config(
            service: &mut BridgeService,
            model: &str,
            api_base: &str,
            api_key: &str,
        ) -> String;
        fn set_workspace(path: &str) -> String;
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
        Ok(Ok((model, messages))) => json!({
            "ok": true,
            "session_id": session_id,
            "model": model,
            "messages": messages,
        })
        .to_string(),
        Ok(Err(error)) => err(error),
        Err(_) => err("panic while resuming session"),
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

pub fn set_workspace(path: &str) -> String {
    let canonical = match std::fs::canonicalize(path) {
        Ok(path) => path,
        Err(error) => return err(format!("cannot open workspace {path}: {error}")),
    };
    if !canonical.is_dir() {
        return err(format!(
            "workspace is not a directory: {}",
            canonical.display()
        ));
    }
    match std::env::set_current_dir(&canonical) {
        Ok(()) => json!({
            "ok": true,
            "path": canonical.to_string_lossy(),
        })
        .to_string(),
        Err(error) => err(format!("cannot switch workspace: {error}")),
    }
}

pub fn shutdown(service: &mut BridgeService) {
    if let Some(inner) = service.inner.as_ref() {
        inner.shutdown();
    }
}
