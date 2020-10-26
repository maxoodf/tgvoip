/**
* @file json/parser.h
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#ifndef TGWSS_PARSER_H
#define TGWSS_PARSER_H

#include <rapidjson/document.h>

namespace tgwss {
    class parser_t final {
    private:
        rapidjson::Document m_json;

    public:
        parser_t() = default;
        explicit parser_t(const char *_jsonStr);

        parser_t(const parser_t &) = delete;
        void operator=(const parser_t &) = delete;
        parser_t(const parser_t &&) = delete;
        void operator=(const parser_t &&) = delete;

        ~parser_t() = default;

        const rapidjson::Document &json() const {return m_json;}
        rapidjson::Document &json() {return m_json;}
    };
} // namespace tgwss

#endif //TGWSS_PARSER_H
