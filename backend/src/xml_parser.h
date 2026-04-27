#pragma once
#include <string>

struct PayloadData {
    std::string device_id;
    std::string timestamp;
    std::string image_b64;  // base64-encoded JPEG from ESP32
};

// Parses the <anpr_payload> XML produced by the ESP32 firmware.
PayloadData parse_xml_payload(const std::string& xml);
