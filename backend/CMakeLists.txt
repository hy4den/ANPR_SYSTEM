cmake_minimum_required(VERSION 3.16)
project(anpr_backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── System dependencies (install via: brew install mbedtls curl sqlite3) ──────
find_package(PkgConfig REQUIRED)
find_package(OpenSSL   REQUIRED)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)

# ── Header-only / small libs via FetchContent ─────────────────────────────────
include(FetchContent)

FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.15.3
    GIT_SHALLOW    TRUE
)
set(HTTPLIB_REQUIRE_OPENSSL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(tinyxml2
    GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
    GIT_TAG        10.0.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(tinyxml2)

FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/database.cpp
    src/crypto.cpp
    src/xml_parser.cpp
    src/ocr_client.cpp
    src/notifier.cpp
    src/api_handler.cpp
)

add_executable(anpr_backend ${SOURCES})

target_include_directories(anpr_backend PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SQLITE3_INCLUDE_DIRS}
)

target_link_libraries(anpr_backend PRIVATE
    httplib::httplib
    tinyxml2::tinyxml2
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    SQLite3::SQLite3
    CURL::libcurl
    pthread
)
