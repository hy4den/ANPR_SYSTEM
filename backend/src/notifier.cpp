#include "notifier.h"
#include "config.h"
#include <curl/curl.h>
#include <thread>
#include <sstream>
#include <iostream>
#include <cstring>

// CURL read callback — must be a plain function, not a lambda with captures.
struct SmtpPayload {
    std::string data;
    size_t      pos = 0;
};

static size_t smtp_read_cb(char* buf, size_t size, size_t nmemb, void* userp) {
    auto* p = static_cast<SmtpPayload*>(userp);
    size_t remaining = p->data.size() - p->pos;
    size_t to_copy   = std::min(remaining, size * nmemb);
    if (to_copy == 0) return 0;
    std::memcpy(buf, p->data.c_str() + p->pos, to_copy);
    p->pos += to_copy;
    return to_copy;
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
