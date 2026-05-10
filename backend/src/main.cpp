#include <iostream>
#include <thread>
#include <memory>
#include <httplib.h>
#include "database.h"
#include "api_handler.h"
#include "config.h"

int main() {
    std::cout << "=== ANPR Backend v1.0 ===\n";
    std::cout << "Port    : " << SERVER_PORT << "\n";
    std::cout << "DB      : " << DB_DEFAULT  << "\n";
    std::cout << "Images  : " << IMAGE_DIR   << "\n";
    std::cout << "Admin   : token protected APIs enabled\n";

    if (cfg_ocr_token().empty())
        std::cout << "[WARN] OCR_API_TOKEN not set – plates will be 'UNKNOWN'\n";
    if (cfg_smtp_user().empty() || cfg_smtp_pass().empty())
        std::cout << "[WARN] SMTP_USER and SMTP_PASS/SMTP_API_TOKEN must be set – email alerts disabled\n";
    if (cfg_fcm_service_account_file().empty())
        std::cout << "[WARN] FCM_SERVICE_ACCOUNT_FILE not set – mobile push disabled\n";

    Database db(DB_DEFAULT);
    db.init_schema();
    std::cout << "Database schema initialized.\n";

    httplib::Server http_svr;
    register_routes(http_svr, db);

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    std::thread tls_thread;
    const std::string cert_file = cfg_tls_cert_file();
    const std::string key_file  = cfg_tls_key_file();
    if (!cert_file.empty() && !key_file.empty()) {
        auto tls_svr = std::make_shared<httplib::SSLServer>(cert_file.c_str(), key_file.c_str());
        if (!tls_svr->is_valid()) {
            std::cerr << "[WARN] TLS cert/key invalid — HTTPS disabled\n";
        } else {
            register_routes(*tls_svr, db);
            const int tls_port = cfg_tls_port();
            std::cout << "HTTPS   : enabled on 0.0.0.0:" << tls_port << "\n";
            tls_thread = std::thread([tls_svr, tls_port]() {
                if (!tls_svr->listen("0.0.0.0", tls_port))
                    std::cerr << "[WARN] HTTPS listener failed on port " << tls_port << "\n";
            });
            tls_thread.detach();
        }
    } else {
        std::cout << "[WARN] TLS_CERT_FILE / TLS_KEY_FILE not set — HTTPS disabled\n";
    }
#else
    std::cout << "[WARN] Backend built without OpenSSL — HTTPS disabled\n";
#endif

    std::cout << "HTTP    : listening on 0.0.0.0:" << SERVER_PORT << " ...\n\n";
    if (!http_svr.listen("0.0.0.0", SERVER_PORT)) {
        std::cerr << "[FATAL] Cannot bind to port " << SERVER_PORT << "\n";
        return 1;
    }
    return 0;
}
