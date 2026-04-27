#include "xml_parser.h"
#include <tinyxml2.h>
#include <stdexcept>
#include <string>

PayloadData parse_xml_payload(const std::string& xml) {
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS)
        throw std::runtime_error(
            std::string("XML parse error: ") + doc.ErrorStr());

    auto* root = doc.FirstChildElement("anpr_payload");
    if (!root)
        throw std::runtime_error("XML missing <anpr_payload> root element");

    auto get_text = [&](const char* tag) -> std::string {
        auto* el = root->FirstChildElement(tag);
        if (!el || !el->GetText())
            throw std::runtime_error(
                std::string("XML missing element: <") + tag + ">");
        return el->GetText();
    };

    PayloadData pd;
    pd.device_id = get_text("device_id");
    pd.timestamp = get_text("timestamp");
    pd.image_b64 = get_text("image");
    return pd;
}
