#pragma once
#include <cstdlib>
#include <string>

// ── AES-256 CBC ───────────────────────────────────────────────────────────────
// Must match esp32-firmware/src/config.h exactly (32 and 16 bytes).
static constexpr char AES_KEY[33] = "0123456789ABCDEF0123456789ABCDEF";
static constexpr char AES_IV[17]  = "0000000000000000";

// ── Server ────────────────────────────────────────────────────────────────────
static constexpr int   SERVER_PORT = 8000;
static constexpr char* DB_PATH     = nullptr;       // overridden at runtime
static constexpr char  DB_DEFAULT[] = "anpr.db";
static constexpr char  IMAGE_DIR[]  = "captured_images";
static constexpr int   TLS_PORT     = 8443;

// OCR confidence below this threshold flags the log for manual review.
static constexpr float OCR_MIN_CONFIDENCE = 70.0f;

// ── Runtime config from environment variables ─────────────────────────────────
inline std::string cfg_ocr_token() { auto e = std::getenv("OCR_API_TOKEN"); return e ? e : ""; }
inline std::string cfg_smtp_host() { auto e = std::getenv("SMTP_HOST");     return e ? e : "smtp.gmail.com"; }
inline std::string cfg_smtp_user() { auto e = std::getenv("SMTP_USER");     return e ? e : ""; }
inline std::string cfg_smtp_pass() {
    auto e = std::getenv("SMTP_PASS");
    if (e && *e) return e;
    // Some providers expose this as an API token instead of "password".
    e = std::getenv("SMTP_API_TOKEN");
    return e ? e : "";
}
inline int         cfg_smtp_port() { auto e = std::getenv("SMTP_PORT");     return e ? std::atoi(e) : 465; }
inline std::string cfg_admin_token() {
    auto e = std::getenv("ADMIN_API_TOKEN");
    return (e && *e) ? e : "anpr-dev-admin-token";
}
inline std::string cfg_tls_cert_file() { auto e = std::getenv("TLS_CERT_FILE"); return e ? e : ""; }
inline std::string cfg_tls_key_file()  { auto e = std::getenv("TLS_KEY_FILE");  return e ? e : ""; }
inline int         cfg_tls_port() {
    auto e = std::getenv("TLS_PORT");
    return e ? std::atoi(e) : TLS_PORT;
}
inline std::string cfg_fcm_project_id() {
    auto e = std::getenv("FCM_PROJECT_ID");
    return e ? e : "";
}
inline std::string cfg_fcm_service_account_file() {
    auto e = std::getenv("FCM_SERVICE_ACCOUNT_FILE");
    return e ? e : "";
}
