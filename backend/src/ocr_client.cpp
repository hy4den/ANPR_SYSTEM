#include "ocr_client.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <cctype>
#include <iostream>

using json = nlohmann::json;

static size_t write_cb(void* data, size_t size, size_t nmemb, std::string* out) {
    out->append(static_cast<char*>(data), size * nmemb);
    return size * nmemb;
}

OCRResult call_ocr_api(const std::vector<uint8_t>& jpeg_bytes,
                       const std::string& api_token) {
    OCRResult result;
    if (api_token.empty()) {
        std::cerr << "[OCR] No API token set – skipping plate recognition\n";
        return result;
    }

    CURL* curl = curl_easy_init();
    if (!curl) return result;

    std::string response_body;
    std::string auth_header = "Authorization: Token " + api_token;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, auth_header.c_str());

    curl_mime* mime   = curl_mime_init(curl);
    curl_mimepart* pt = curl_mime_addpart(mime);
    curl_mime_name    (pt, "upload");
    curl_mime_data    (pt, reinterpret_cast<const char*>(jpeg_bytes.data()),
                       jpeg_bytes.size());
    curl_mime_type    (pt, "image/jpeg");
    curl_mime_filename(pt, "plate.jpg");

    curl_easy_setopt(curl, CURLOPT_URL,           "https://api.platerecognizer.com/v1/plate-reader/");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    headers);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST,      mime);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       10L);

    CURLcode res = curl_easy_perform(curl);
    curl_mime_free(mime);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[OCR] curl error: " << curl_easy_strerror(res) << "\n";
        return result;
    }

    try {
        auto j = json::parse(response_body);
        if (j.contains("results") && !j["results"].empty()) {
            auto& r       = j["results"][0];
            result.plate  = r.value("plate", "");
            // Plate Recognizer returns score 0–1, convert to percentage
            result.confidence = r.value("score", 0.0f) * 100.0f;
            result.success    = !result.plate.empty();
            for (auto& c : result.plate) c = static_cast<char>(std::toupper(c));
        }
    } catch (const std::exception& e) {
        std::cerr << "[OCR] JSON parse error: " << e.what()
                  << "\nRaw: " << response_body << "\n";
    }

    return result;
}
