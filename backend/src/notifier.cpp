#include "notifier.h"
#include "config.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <thread>
#include <sstream>
#include <iostream>
#include <cstring>
#include <fstream>
#include <mutex>
#include <ctime>
#include <algorithm>
#include <optional>

using json = nlohmann::json;

// CURL read callback — must be a plain function, not a lambda with captures.
struct SmtpPayload {
    std::string data;
    size_t      pos = 0;
};

struct FcmServiceAccount {
    std::string project_id;
    std::string client_email;
    std::string private_key;
    std::string token_uri;
};

static std::mutex g_fcm_mutex;
static bool g_fcm_credentials_loaded = false;
static std::optional<FcmServiceAccount> g_fcm_credentials;
static std::string g_fcm_access_token;
static std::time_t g_fcm_access_token_expiry = 0;

static size_t smtp_read_cb(char* buf, size_t size, size_t nmemb, void* userp) {
    auto* p = static_cast<SmtpPayload*>(userp);
    size_t remaining = p->data.size() - p->pos;
    size_t to_copy   = std::min(remaining, size * nmemb);
    if (to_copy == 0) return 0;
    std::memcpy(buf, p->data.c_str() + p->pos, to_copy);
    p->pos += to_copy;
    return to_copy;
}

static size_t curl_write_to_string_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

static std::string base64url_encode_bytes(const unsigned char* data, size_t len) {
    if (!data || len == 0) return "";
    std::string b64;
    b64.resize(4 * ((len + 2) / 3));
    int out_len = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&b64[0]), data, static_cast<int>(len));
    if (out_len <= 0) return "";
    b64.resize(static_cast<size_t>(out_len));

    for (char& c : b64) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    while (!b64.empty() && b64.back() == '=') b64.pop_back();
    return b64;
}

static std::string base64url_encode_str(const std::string& input) {
    return base64url_encode_bytes(reinterpret_cast<const unsigned char*>(input.data()), input.size());
}

static std::string sign_rs256_base64url(const std::string& data, const std::string& private_key_pem) {
    BIO* key_bio = BIO_new_mem_buf(private_key_pem.data(), static_cast<int>(private_key_pem.size()));
    if (!key_bio) return "";
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(key_bio, nullptr, nullptr, nullptr);
    BIO_free(key_bio);
    if (!pkey) return "";

    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        EVP_PKEY_free(pkey);
        return "";
    }

    std::string signature_b64url;
    do {
        if (EVP_DigestSignInit(md_ctx, nullptr, EVP_sha256(), nullptr, pkey) != 1) break;
        if (EVP_DigestSignUpdate(md_ctx, data.data(), data.size()) != 1) break;

        size_t sig_len = 0;
        if (EVP_DigestSignFinal(md_ctx, nullptr, &sig_len) != 1 || sig_len == 0) break;

        std::string sig_raw(sig_len, '\0');
        if (EVP_DigestSignFinal(md_ctx, reinterpret_cast<unsigned char*>(&sig_raw[0]), &sig_len) != 1) break;
        sig_raw.resize(sig_len);
        signature_b64url = base64url_encode_str(sig_raw);
    } while (false);

    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return signature_b64url;
}

static std::optional<FcmServiceAccount> load_fcm_service_account_from_file() {
    const std::string path = cfg_fcm_service_account_file();
    if (path.empty()) return std::nullopt;

    std::ifstream in(path);
    if (!in) {
        std::cerr << "[FCM] Cannot open service account file: " << path << "\n";
        return std::nullopt;
    }

    json doc;
    try {
        in >> doc;
    } catch (const std::exception& e) {
        std::cerr << "[FCM] Invalid service account JSON: " << e.what() << "\n";
        return std::nullopt;
    }

    FcmServiceAccount sa;
    sa.project_id   = doc.value("project_id", "");
    sa.client_email = doc.value("client_email", "");
    sa.private_key  = doc.value("private_key", "");
    sa.token_uri    = doc.value("token_uri", "https://oauth2.googleapis.com/token");

    if (sa.project_id.empty() || sa.client_email.empty() || sa.private_key.empty() || sa.token_uri.empty()) {
        std::cerr << "[FCM] Service account JSON missing required fields\n";
        return std::nullopt;
    }

    return sa;
}

static std::optional<FcmServiceAccount> get_fcm_service_account_locked() {
    if (!g_fcm_credentials_loaded) {
        g_fcm_credentials = load_fcm_service_account_from_file();
        g_fcm_credentials_loaded = true;
    }
    return g_fcm_credentials;
}

static std::string build_fcm_jwt(const FcmServiceAccount& sa, std::time_t now_ts) {
    json header = {
        {"alg", "RS256"},
        {"typ", "JWT"}
    };
    json payload = {
        {"iss", sa.client_email},
        {"scope", "https://www.googleapis.com/auth/firebase.messaging"},
        {"aud", sa.token_uri},
        {"iat", now_ts},
        {"exp", now_ts + 3600}
    };

    const std::string encoded_header  = base64url_encode_str(header.dump());
    const std::string encoded_payload = base64url_encode_str(payload.dump());
    if (encoded_header.empty() || encoded_payload.empty()) return "";

    const std::string signing_input = encoded_header + "." + encoded_payload;
    const std::string signature = sign_rs256_base64url(signing_input, sa.private_key);
    if (signature.empty()) {
        std::cerr << "[FCM] Failed to sign JWT with service account key\n";
        return "";
    }
    return signing_input + "." + signature;
}

static std::string fetch_fcm_access_token_locked() {
    const std::time_t now_ts = std::time(nullptr);
    if (!g_fcm_access_token.empty() && now_ts < g_fcm_access_token_expiry) {
        return g_fcm_access_token;
    }

    auto sa_opt = get_fcm_service_account_locked();
    if (!sa_opt) return "";
    const FcmServiceAccount& sa = *sa_opt;

    const std::string jwt = build_fcm_jwt(sa, now_ts);
    if (jwt.empty()) return "";

    CURL* curl = curl_easy_init();
    if (!curl) return "";

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

    char* escaped_assertion = curl_easy_escape(curl, jwt.c_str(), static_cast<int>(jwt.size()));
    if (!escaped_assertion) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return "";
    }

    const std::string post_data =
        "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=" +
        std::string(escaped_assertion);
    curl_free(escaped_assertion);

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, sa.token_uri.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(post_data.size()));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_to_string_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode rc = curl_easy_perform(curl);
    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK || status_code >= 300) {
        std::cerr << "[FCM] OAuth token fetch failed: "
                  << (rc == CURLE_OK ? "HTTP " + std::to_string(status_code) : curl_easy_strerror(rc))
                  << "\n";
        return "";
    }

    json token_json;
    try {
        token_json = json::parse(response);
    } catch (const std::exception& e) {
        std::cerr << "[FCM] OAuth token response parse error: " << e.what() << "\n";
        return "";
    }

    const std::string access_token = token_json.value("access_token", "");
    if (access_token.empty()) {
        std::cerr << "[FCM] OAuth response has no access_token\n";
        return "";
    }

    int expires_in = 3600;
    if (token_json.contains("expires_in")) {
        const auto& exp = token_json["expires_in"];
        if (exp.is_number_integer()) {
            expires_in = exp.get<int>();
        } else if (exp.is_string()) {
            try {
                expires_in = std::stoi(exp.get<std::string>());
            } catch (const std::exception& e) {
                std::cerr << "[FCM] Invalid expires_in value: " << e.what() << "\n";
            }
        }
    }

    g_fcm_access_token = access_token;
    g_fcm_access_token_expiry = now_ts + std::max(60, expires_in - 60);
    return g_fcm_access_token;
}

static void send_email(const std::string& to_email,
                       const std::string& plate,
                       const std::string& category,
                       const std::string& timestamp) {
    const std::string smtp_user = cfg_smtp_user();
    const std::string smtp_pass = cfg_smtp_pass();
    const std::string smtp_host = cfg_smtp_host();
    const int         smtp_port = cfg_smtp_port();

    if (smtp_user.empty() || smtp_pass.empty()) return;

    std::ostringstream body;
    body << "Date: " << timestamp          << "\r\n"
         << "To: "   << to_email           << "\r\n"
         << "From: ANPR System <" << smtp_user << ">\r\n"
         << "Subject: [ANPR] " << category << " tespit edildi: " << plate << "\r\n"
         << "\r\n"
         << "ANPR Sistem Bildirimi\r\n\r\n"
         << "Kategori : " << category  << "\r\n"
         << "Plaka    : " << plate     << "\r\n"
         << "Zaman    : " << timestamp << "\r\n";

    SmtpPayload payload{body.str(), 0};

    CURL* curl = curl_easy_init();
    if (!curl) return;

    std::string url = "smtps://" + smtp_host + ":" + std::to_string(smtp_port);
    std::string from_addr = "<" + smtp_user + ">";

    struct curl_slist* rcpts = nullptr;
    rcpts = curl_slist_append(rcpts, to_email.c_str());

    curl_easy_setopt(curl, CURLOPT_URL,          url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERNAME,     smtp_user.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD,     smtp_pass.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM,    from_addr.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT,    rcpts);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, smtp_read_cb);
    curl_easy_setopt(curl, CURLOPT_READDATA,     &payload);
    curl_easy_setopt(curl, CURLOPT_UPLOAD,       1L);
    curl_easy_setopt(curl, CURLOPT_USE_SSL,      CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,      15L);

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK)
        std::cerr << "[SMTP] send failed: " << curl_easy_strerror(rc) << "\n";

    curl_slist_free_all(rcpts);
    curl_easy_cleanup(curl);
}

static void send_access_granted_email(const std::string& to_email,
                                      const std::string& owner_name,
                                      const std::string& plate,
                                      const std::string& timestamp) {
    const std::string smtp_user = cfg_smtp_user();
    const std::string smtp_pass = cfg_smtp_pass();
    const std::string smtp_host = cfg_smtp_host();
    const int         smtp_port = cfg_smtp_port();

    if (smtp_user.empty() || smtp_pass.empty()) return;

    const std::string recipient_name = owner_name.empty() ? "Kullanici" : owner_name;

    std::ostringstream body;
    body << "Date: " << timestamp << "\r\n"
         << "To: " << to_email << "\r\n"
         << "From: ANPR System <" << smtp_user << ">\r\n"
         << "Subject: [ANPR] Bariyer acildi: " << plate << "\r\n"
         << "\r\n"
         << "Merhaba " << recipient_name << ",\r\n\r\n"
         << "Araciniz icin gecis onayi verildi ve bariyer acildi.\r\n"
         << "Plaka : " << plate << "\r\n"
         << "Zaman : " << timestamp << "\r\n";

    SmtpPayload payload{body.str(), 0};

    CURL* curl = curl_easy_init();
    if (!curl) return;

    std::string url = "smtps://" + smtp_host + ":" + std::to_string(smtp_port);
    std::string from_addr = "<" + smtp_user + ">";

    struct curl_slist* rcpts = nullptr;
    rcpts = curl_slist_append(rcpts, to_email.c_str());

    curl_easy_setopt(curl, CURLOPT_URL,          url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERNAME,     smtp_user.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD,     smtp_pass.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM,    from_addr.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT,    rcpts);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, smtp_read_cb);
    curl_easy_setopt(curl, CURLOPT_READDATA,     &payload);
    curl_easy_setopt(curl, CURLOPT_UPLOAD,       1L);
    curl_easy_setopt(curl, CURLOPT_USE_SSL,      CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,      15L);

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK)
        std::cerr << "[SMTP] access-granted send failed: " << curl_easy_strerror(rc) << "\n";

    curl_slist_free_all(rcpts);
    curl_easy_cleanup(curl);
}

static void send_mobile_push(const DeviceToken& device_token,
                             const std::string& plate,
                             const std::string& timestamp) {
    if (device_token.token.empty()) return;

    std::string access_token;
    std::string project_id = cfg_fcm_project_id();
    {
        std::lock_guard<std::mutex> lk(g_fcm_mutex);
        access_token = fetch_fcm_access_token_locked();
        if (project_id.empty()) {
            auto sa_opt = get_fcm_service_account_locked();
            if (sa_opt) project_id = sa_opt->project_id;
        }
    }
    if (access_token.empty() || project_id.empty()) return;

    const json payload = {
        {"message", {
            {"token", device_token.token},
            {"notification", {
                {"title", "ANPR Bildirimi"},
                {"body", std::string("Bariyer acildi: ") + plate}
            }},
            {"data", {
                {"event", "access_granted"},
                {"plate", plate},
                {"timestamp", timestamp},
                {"platform", device_token.platform}
            }},
            {"webpush", {
                {"headers", {{"Urgency", "high"}}}
            }}
        }}
    };

    CURL* curl = curl_easy_init();
    if (!curl) return;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    const std::string auth_header = "Authorization: Bearer " + access_token;
    headers = curl_slist_append(headers, auth_header.c_str());

    std::string response;
    const std::string url = "https://fcm.googleapis.com/v1/projects/" + project_id + "/messages:send";
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    const std::string body = payload.dump();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_to_string_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode rc = curl_easy_perform(curl);
    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

    if (rc != CURLE_OK || status_code >= 300) {
        std::cerr << "[FCM] push send failed: "
                  << (rc == CURLE_OK ? "HTTP " + std::to_string(status_code) : curl_easy_strerror(rc))
                  << " response=" << response << "\n";
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

void notify_admins_async(const std::vector<AdminUser>& admins,
                         const std::string& plate,
                         const std::string& category,
                         const std::string& timestamp) {
    for (const auto& admin : admins) {
        if (admin.contact_email.empty()) continue;
        // Detached thread — fire and forget
        std::thread([=]() {
            send_email(admin.contact_email, plate, category, timestamp);
        }).detach();
    }
}

void notify_owner_access_granted_async(const std::string& owner_email,
                                       const std::string& owner_name,
                                       const std::string& plate,
                                       const std::string& timestamp) {
    if (owner_email.empty()) return;
    std::thread([=]() {
        send_access_granted_email(owner_email, owner_name, plate, timestamp);
    }).detach();
}

void notify_mobile_access_granted_async(const std::vector<DeviceToken>& device_tokens,
                                        const std::string& plate,
                                        const std::string& timestamp) {
    for (const auto& t : device_tokens) {
        if (t.token.empty()) continue;
        std::thread([=]() {
            send_mobile_push(t, plate, timestamp);
        }).detach();
    }
}
