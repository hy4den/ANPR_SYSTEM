#include "database.h"
#include <stdexcept>

static bool table_has_column(sqlite3* db, const char* table, const char* column) {
    std::string sql = "PRAGMA table_info(" + std::string(table) + ")";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto col = sqlite3_column_text(stmt, 1);
        if (!col) continue;
        if (std::string(reinterpret_cast<const char*>(col)) == column) {
            found = true;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK)
        throw std::runtime_error(
            std::string("Cannot open DB: ") + sqlite3_errmsg(db_));
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

void Database::exec(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown";
        sqlite3_free(err);
        throw std::runtime_error("SQL error: " + msg);
    }
}

void Database::init_schema() {
    std::lock_guard<std::mutex> lk(mutex_);
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA foreign_keys=ON;");

    exec(R"(
        CREATE TABLE IF NOT EXISTS Access_Logs (
            LogID           INTEGER PRIMARY KEY AUTOINCREMENT,
            Timestamp       TEXT    NOT NULL,
            LicensePlate    TEXT,
            ConfidenceScore REAL    CHECK(ConfidenceScore BETWEEN 0.0 AND 100.0),
            AccessStatus    INTEGER NOT NULL DEFAULT 0,
            ImagePath       TEXT
        );
    )");

    exec(R"(
        CREATE TABLE IF NOT EXISTS Watchlist (
            PlateID      INTEGER PRIMARY KEY AUTOINCREMENT,
            LicensePlate TEXT    UNIQUE NOT NULL,
            OwnerName    TEXT,
            OwnerEmail   TEXT,
            Category     TEXT    CHECK(Category IN ('VIP','Staff','Blacklisted','Resident')),
            AutoGrant    INTEGER NOT NULL DEFAULT 0
        );
    )");

    if (!table_has_column(db_, "Watchlist", "OwnerEmail")) {
        exec("ALTER TABLE Watchlist ADD COLUMN OwnerEmail TEXT;");
    }

    exec(R"(
        CREATE TABLE IF NOT EXISTS Admin_Users (
            UserID          INTEGER PRIMARY KEY AUTOINCREMENT,
            Username        TEXT UNIQUE NOT NULL,
            PasswordHash    TEXT NOT NULL,
            ContactEmail    TEXT,
            ContactPhone    TEXT,
            AlertPreference TEXT CHECK(AlertPreference IN
                ('All','Only_Blacklisted','Only_VIP','None')) DEFAULT 'All'
        );
    )");

    exec(R"(
        CREATE TABLE IF NOT EXISTS Device_Tokens (
            Token       TEXT PRIMARY KEY,
            Platform    TEXT NOT NULL DEFAULT 'web',
            UpdatedAt   TEXT NOT NULL DEFAULT (datetime('now'))
        );
    )");
}

// ── Access_Logs ───────────────────────────────────────────────────────────────

void Database::insert_log(const std::string& timestamp,
                          const std::string& plate,
                          float confidence, bool access_granted,
                          const std::string& image_path) {
    std::lock_guard<std::mutex> lk(mutex_);
    const char* sql =
        "INSERT INTO Access_Logs"
        "(Timestamp,LicensePlate,ConfidenceScore,AccessStatus,ImagePath)"
        " VALUES(?,?,?,?,?)";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text  (stmt, 1, timestamp.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 2, plate.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, confidence);
    sqlite3_bind_int   (stmt, 4, access_granted ? 1 : 0);
    sqlite3_bind_text  (stmt, 5, image_path.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::vector<AccessLog> Database::get_logs(const std::string& plate_filter,
                                          const std::string& from,
                                          const std::string& to,
                                          int access_status) {
    std::lock_guard<std::mutex> lk(mutex_);

    std::string sql =
        "SELECT LogID,Timestamp,LicensePlate,ConfidenceScore,AccessStatus,ImagePath"
        " FROM Access_Logs WHERE 1=1";

    // Collect string params in binding order
    std::vector<std::string> str_params;
    if (!plate_filter.empty()) { sql += " AND LicensePlate LIKE ?"; str_params.push_back("%" + plate_filter + "%"); }
    if (!from.empty())         { sql += " AND Timestamp >= ?";      str_params.push_back(from); }
    if (!to.empty())           { sql += " AND Timestamp <= ?";      str_params.push_back(to); }

    bool has_status = (access_status >= 0);
    if (has_status)            sql += " AND AccessStatus = ?";

    sql += " ORDER BY LogID DESC LIMIT 500";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);

    int idx = 1;
    for (const auto& p : str_params)
        sqlite3_bind_text(stmt, idx++, p.c_str(), -1, SQLITE_TRANSIENT);
    if (has_status)
        sqlite3_bind_int(stmt, idx, access_status);

    std::vector<AccessLog> logs;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AccessLog l;
        l.log_id           = sqlite3_column_int(stmt, 0);
        auto ts            = sqlite3_column_text(stmt, 1);
        l.timestamp        = ts ? reinterpret_cast<const char*>(ts) : "";
        auto pl            = sqlite3_column_text(stmt, 2);
        l.license_plate    = pl ? reinterpret_cast<const char*>(pl) : "";
        l.confidence_score = static_cast<float>(sqlite3_column_double(stmt, 3));
        l.access_status    = sqlite3_column_int(stmt, 4) != 0;
        auto ip            = sqlite3_column_text(stmt, 5);
        l.image_path       = ip ? reinterpret_cast<const char*>(ip) : "";
        logs.push_back(l);
    }
    sqlite3_finalize(stmt);
    return logs;
}

// ── Watchlist ─────────────────────────────────────────────────────────────────

std::optional<WatchlistEntry> Database::find_plate(const std::string& plate) {
    std::lock_guard<std::mutex> lk(mutex_);
    const char* sql =
        "SELECT PlateID,LicensePlate,OwnerName,OwnerEmail,Category,AutoGrant"
        " FROM Watchlist WHERE UPPER(LicensePlate)=UPPER(?)";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, plate.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<WatchlistEntry> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        WatchlistEntry e;
        e.plate_id      = sqlite3_column_int(stmt, 0);
        auto lp         = sqlite3_column_text(stmt, 1);
        e.license_plate = lp ? reinterpret_cast<const char*>(lp) : "";
        auto on         = sqlite3_column_text(stmt, 2);
        e.owner_name    = on ? reinterpret_cast<const char*>(on) : "";
        auto oe         = sqlite3_column_text(stmt, 3);
        e.owner_email   = oe ? reinterpret_cast<const char*>(oe) : "";
        auto cat        = sqlite3_column_text(stmt, 4);
        e.category      = cat ? reinterpret_cast<const char*>(cat) : "";
        e.auto_grant    = sqlite3_column_int(stmt, 5) != 0;
        result = e;
    }
    sqlite3_finalize(stmt);
    return result;
}

std::vector<WatchlistEntry> Database::get_watchlist() {
    std::lock_guard<std::mutex> lk(mutex_);
    const char* sql =
        "SELECT PlateID,LicensePlate,OwnerName,OwnerEmail,Category,AutoGrant FROM Watchlist";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    std::vector<WatchlistEntry> list;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        WatchlistEntry e;
        e.plate_id      = sqlite3_column_int(stmt, 0);
        auto lp         = sqlite3_column_text(stmt, 1);
        e.license_plate = lp ? reinterpret_cast<const char*>(lp) : "";
        auto on         = sqlite3_column_text(stmt, 2);
        e.owner_name    = on ? reinterpret_cast<const char*>(on) : "";
        auto oe         = sqlite3_column_text(stmt, 3);
        e.owner_email   = oe ? reinterpret_cast<const char*>(oe) : "";
        auto cat        = sqlite3_column_text(stmt, 4);
        e.category      = cat ? reinterpret_cast<const char*>(cat) : "";
        e.auto_grant    = sqlite3_column_int(stmt, 5) != 0;
        list.push_back(e);
    }
    sqlite3_finalize(stmt);
    return list;
}

void Database::add_plate(const std::string& plate, const std::string& owner,
                         const std::string& owner_email,
                         const std::string& category, bool auto_grant) {
    std::lock_guard<std::mutex> lk(mutex_);
    const char* sql =
        "INSERT OR REPLACE INTO Watchlist"
        "(LicensePlate,OwnerName,OwnerEmail,Category,AutoGrant) VALUES(?,?,?,?,?)";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, plate.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, owner.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, owner_email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 5, auto_grant ? 1 : 0);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::remove_plate(const std::string& plate) {
    std::lock_guard<std::mutex> lk(mutex_);
    const char* sql =
        "DELETE FROM Watchlist WHERE UPPER(LicensePlate)=UPPER(?)";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, plate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// ── Admin_Users ───────────────────────────────────────────────────────────────

std::vector<AdminUser> Database::get_admins_for_alert(const std::string& category) {
    std::lock_guard<std::mutex> lk(mutex_);
    const char* sql =
        "SELECT UserID,Username,ContactEmail,ContactPhone,AlertPreference"
        " FROM Admin_Users"
        " WHERE AlertPreference='All'"
        "    OR (AlertPreference='Only_VIP'         AND ?1='VIP')"
        "    OR (AlertPreference='Only_Blacklisted' AND ?1='Blacklisted')";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, category.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<AdminUser> admins;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AdminUser a;
        a.user_id          = sqlite3_column_int(stmt, 0);
        auto un            = sqlite3_column_text(stmt, 1);
        a.username         = un  ? reinterpret_cast<const char*>(un)  : "";
        auto em            = sqlite3_column_text(stmt, 2);
        a.contact_email    = em  ? reinterpret_cast<const char*>(em)  : "";
        auto cp            = sqlite3_column_text(stmt, 3);
        a.contact_phone    = cp  ? reinterpret_cast<const char*>(cp)  : "";
        auto ap            = sqlite3_column_text(stmt, 4);
        a.alert_preference = ap  ? reinterpret_cast<const char*>(ap)  : "";
        admins.push_back(a);
    }
    sqlite3_finalize(stmt);
    return admins;
}

void Database::register_device_token(const std::string& token, const std::string& platform) {
    std::lock_guard<std::mutex> lk(mutex_);
    const char* sql =
        "INSERT INTO Device_Tokens(Token,Platform,UpdatedAt)"
        " VALUES(?1,?2,datetime('now'))"
        " ON CONFLICT(Token) DO UPDATE SET"
        " Platform=excluded.Platform,"
        " UpdatedAt=datetime('now')";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, platform.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::unregister_device_token(const std::string& token) {
    std::lock_guard<std::mutex> lk(mutex_);
    const char* sql = "DELETE FROM Device_Tokens WHERE Token=?1";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::vector<DeviceToken> Database::get_device_tokens() {
    std::lock_guard<std::mutex> lk(mutex_);
    const char* sql = "SELECT Token,Platform FROM Device_Tokens";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);

    std::vector<DeviceToken> tokens;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DeviceToken d;
        auto tk = sqlite3_column_text(stmt, 0);
        auto pf = sqlite3_column_text(stmt, 1);
        d.token = tk ? reinterpret_cast<const char*>(tk) : "";
        d.platform = pf ? reinterpret_cast<const char*>(pf) : "web";
        if (!d.token.empty()) tokens.push_back(d);
    }
    sqlite3_finalize(stmt);
    return tokens;
}
