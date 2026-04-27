#include "api_handler.h"
#include "crypto.h"
#include "xml_parser.h"
#include "ocr_client.h"
#include "notifier.h"
#include "config.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <ctime>
#include <string_view>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string server_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    char buf[25];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return buf;
}

static std::string save_jpeg(const std::vector<uint8_t>& bytes,
                              const std::string& ts) {
    fs::create_directories(IMAGE_DIR);
    std::string fname = ts;
    for (auto& c : fname) if (c == ':') c = '-';
    std::string path = std::string(IMAGE_DIR) + "/" + fname + ".jpg";
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    return path;
}

static void cors(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Admin-Token");
}

static std::string extract_bearer_token(const httplib::Request& req) {
    if (!req.has_header("Authorization")) return "";
    std::string auth = req.get_header_value("Authorization");
    constexpr std::string_view prefix = "Bearer ";
    if (auth.rfind(prefix.data(), 0) == 0) return auth.substr(prefix.size());
    return "";
}

static bool is_admin_authorized(const httplib::Request& req) {
    const std::string expected = cfg_admin_token();
    if (expected.empty()) return false;

    if (req.has_header("X-Admin-Token") && req.get_header_value("X-Admin-Token") == expected)
        return true;

    if (extract_bearer_token(req) == expected) return true;

    // Used for direct image links opened in new tabs.
    if (req.has_param("admin_token") && req.get_param_value("admin_token") == expected)
        return true;

    return false;
}

static bool require_admin_auth(const httplib::Request& req, httplib::Response& res) {
    if (is_admin_authorized(req)) return true;
    res.status = 401;
    json err = {{"error", "Unauthorized admin request"}};
    res.set_content(err.dump(), "application/json");
    return false;
}

// ── Route registration ────────────────────────────────────────────────────────

void register_routes(httplib::Server& svr, Database& db) {

    // CORS preflight for all routes
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        cors(res);
        res.status = 204;
    });

    // ── GET /api/health ───────────────────────────────────────────────────────
    svr.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        cors(res);
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    // ── POST /api/anpr/process ────────────────────────────────────────────────
    // Main endpoint: ESP32 posts encrypted payload → returns plate + access decision.
    svr.Post("/api/anpr/process", [&db](const httplib::Request& req,
                                        httplib::Response& res) {
        cors(res);
        try {
            // 1. Decrypt AES-256 CBC
            std::string xml = aes256_decrypt_b64(req.body);

            // 2. Parse XML
            PayloadData pd = parse_xml_payload(xml);

            // 3. Decode base64 image → JPEG bytes
            auto jpeg = base64_decode(pd.image_b64);

            // 4. Save JPEG to disk
            std::string ts   = pd.timestamp.empty() ? server_timestamp() : pd.timestamp;
            std::string path = save_jpeg(jpeg, ts);

            // 5. OCR
            OCRResult ocr  = call_ocr_api(jpeg, cfg_ocr_token());
            std::string plate = ocr.success ? ocr.plate : "UNKNOWN";
            float confidence  = ocr.confidence;

            // 6. Watchlist lookup → access decision
            auto entry = db.find_plate(plate);
            bool access_granted = false;
            std::string category = "Unknown";

            if (entry) {
                category      = entry->category;
                // Blacklisted → always denied; others depend on AutoGrant flag
                access_granted = (category != "Blacklisted") && entry->auto_grant;
            }

            // 7. Log to database
            db.insert_log(ts, plate, confidence, access_granted, path);

            // 8. Async notification for special categories
            if (entry && (category == "VIP" || category == "Blacklisted")) {
                auto admins = db.get_admins_for_alert(category);
                notify_admins_async(admins, plate, category, ts);
            }

            // 9. Return JSON to ESP32
            json resp = {{"plate", plate}, {"access_granted", access_granted}};
            res.set_content(resp.dump(), "application/json");

            std::cout << "[ANPR] device=" << pd.device_id
                      << " plate=" << plate
                      << " cat=" << category
                      << " conf=" << confidence << "%"
                      << " → " << (access_granted ? "GRANTED" : "DENIED") << "\n";

        } catch (const std::exception& e) {
            std::cerr << "[ERROR] /api/anpr/process: " << e.what() << "\n";
            res.status = 400;
            json err = {{"error", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    // ── GET /api/logs ─────────────────────────────────────────────────────────
    // Query params: plate, from, to, status (0|1)
    svr.Get("/api/logs", [&db](const httplib::Request& req,
                                httplib::Response& res) {
        cors(res);
        if (!require_admin_auth(req, res)) return;
        std::string plate  = req.get_param_value("plate");
        std::string from   = req.get_param_value("from");
        std::string to     = req.get_param_value("to");
        int status = -1;
        if (req.has_param("status")) {
            try { status = std::stoi(req.get_param_value("status")); }
            catch (...) {}
        }

        auto logs = db.get_logs(plate, from, to, status);
        json arr  = json::array();
        for (const auto& l : logs) {
            arr.push_back({
                {"log_id",         l.log_id},
                {"timestamp",      l.timestamp},
                {"license_plate",  l.license_plate},
                {"confidence",     l.confidence_score},
                {"access_granted", l.access_status},
                {"image_path",     l.image_path}
            });
        }
        res.set_content(arr.dump(), "application/json");
    });

    // ── GET /api/watchlist ────────────────────────────────────────────────────
    svr.Get("/api/watchlist", [&db](const httplib::Request& req,
                                     httplib::Response& res) {
        cors(res);
        if (!require_admin_auth(req, res)) return;
        auto list = db.get_watchlist();
        json arr  = json::array();
        for (const auto& e : list) {
            arr.push_back({
                {"plate_id",      e.plate_id},
                {"license_plate", e.license_plate},
                {"owner_name",    e.owner_name},
                {"category",      e.category},
                {"auto_grant",    e.auto_grant}
            });
        }
        res.set_content(arr.dump(), "application/json");
    });

    // ── POST /api/watchlist ───────────────────────────────────────────────────
    // Body: {"license_plate":"34ABC123","owner_name":"Ali","category":"Staff","auto_grant":true}
    svr.Post("/api/watchlist", [&db](const httplib::Request& req,
                                      httplib::Response& res) {
        cors(res);
        if (!require_admin_auth(req, res)) return;
        try {
            auto body        = json::parse(req.body);
            std::string plate    = body.at("license_plate");
            std::string owner    = body.value("owner_name",  "");
            std::string category = body.value("category",    "Resident");
            bool auto_grant      = body.value("auto_grant",  false);
            db.add_plate(plate, owner, category, auto_grant);
            res.status = 201;
            res.set_content(R"({"ok":true})", "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json err = {{"error", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    // ── DELETE /api/watchlist/:plate ──────────────────────────────────────────
    svr.Delete(R"(/api/watchlist/([^/]+))",
               [&db](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        if (!require_admin_auth(req, res)) return;
        std::string plate = req.matches[1].str();
        db.remove_plate(plate);
        res.set_content(R"({"ok":true})", "application/json");
    });

    // ── GET /api/images/:filename ─────────────────────────────────────────────
    // Serves captured JPEG images (used by the React frontend).
    svr.Get(R"(/api/images/([^/]+\.jpg))",
            [](const httplib::Request& req, httplib::Response& res) {
        cors(res);
        if (!require_admin_auth(req, res)) return;
        std::string fname = req.matches[1].str();
        // Prevent path traversal
        if (fname.find("..") != std::string::npos) {
            res.status = 400;
            return;
        }
        std::string path = std::string(IMAGE_DIR) + "/" + fname;
        std::ifstream f(path, std::ios::binary);
        if (!f) { res.status = 404; return; }
        std::ostringstream ss;
        ss << f.rdbuf();
        res.set_content(ss.str(), "image/jpeg");
    });
}
