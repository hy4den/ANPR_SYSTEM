#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct OCRResult {
    std::string plate;
    float       confidence = 0.0f;
    bool        success    = false;
};

// Sends JPEG bytes to Plate Recognizer API and returns plate + confidence.
// Returns success=false if token is empty or API call fails.
OCRResult call_ocr_api(const std::vector<uint8_t>& jpeg_bytes,
                       const std::string& api_token);
