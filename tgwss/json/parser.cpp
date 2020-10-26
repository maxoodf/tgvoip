/**
* @file json/parser.cpp
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#include <vector>
#include <fstream>

#include "rapidjson/error/en.h"

#include "parser.h"

namespace tgwss {
    parser_t::parser_t(const char *_jsonStr) {
        if (_jsonStr == nullptr) {
            throw std::runtime_error("parser: null pointer passed");
        }
        m_json.Parse(_jsonStr);
        if (m_json.HasParseError()) {
            throw std::runtime_error(std::string("parser: failed to parse JSON. ")
                                     + rapidjson::GetParseError_En(m_json.GetParseError())
                                     + " Offset " + std::to_string(m_json.GetErrorOffset()));
        }
    }
} // namespace tgwss
