#pragma once
#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <sqlite3.h>

struct WatchlistEntry {
    int         plate_id    = 0;
    std::string license_plate;
    std::string owner_name;
    std::string category;   // VIP | Staff | Blacklisted | Resident
    bool        auto_grant  = false;
};

struct AccessLog {
    int         log_id          = 0;
    std::string timestamp;
    std::string license_plate;
    float       confidence_score = 0.0f;
    bool        access_status   = false;
    std::string image_path;
};

struct AdminUser {
    int         user_id    = 0;
    std::string username;
    std::string contact_email;
    std::string contact_phone;
    std::string alert_preference;  // All | Only_Blacklisted | Only_VIP | None
};

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    void init_schema();

    // Access_Logs
    void insert_log(const std::string& timestamp, const std::string& plate,
                    float confidence, bool access_granted,
                    const std::string& image_path);

    // access_status: -1=all, 0=denied, 1=granted
    std::vector<AccessLog> get_logs(const std::string& plate_filter = "",
                                    const std::string& from         = "",
                                    const std::string& to           = "",
                                    int                access_status = -1);

    // Watchlist
    std::optional<WatchlistEntry> find_plate(const std::string& plate);
    std::vector<WatchlistEntry>   get_watchlist();
    void add_plate(const std::string& plate, const std::string& owner,
                   const std::string& category, bool auto_grant);
    void remove_plate(const std::string& plate);

    // Admin_Users — only read, add via direct DB seed
    std::vector<AdminUser> get_admins_for_alert(const std::string& category);

private:
    sqlite3*          db_    = nullptr;
    mutable std::mutex mutex_;

    void exec(const std::string& sql);
};
