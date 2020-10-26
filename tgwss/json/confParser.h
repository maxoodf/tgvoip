/**
* @file json/confParser.h
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#ifndef TGWSS_CONFPARSER_H
#define TGWSS_CONFPARSER_H

#include <string>
#include <unordered_set>
#include <memory>

#include "parser.h"

namespace tgwss {
    class confParser_t {
    private:
        std::unique_ptr<parser_t> m_parser;

        // network
        uint16_t m_bindPort = 8080;
        uint16_t m_connLimit = 8;
        uint16_t m_ioTimeout = 30;
        bool m_ssl = false;
        std::string m_certFile;
        std::string m_pkeyFile;

        // log
        std::string m_logDst;
        std::string m_logLevel;

    public:
        static confParser_t &confParser() {
            static confParser_t confParser;
            return confParser;
        }
        ~confParser_t() = default;

        confParser_t(const confParser_t &) = delete;
        void operator=(const confParser_t &) = delete;
        confParser_t(const confParser_t &&) = delete;
        void operator=(const confParser_t &&) = delete;

        void init(const std::string &_confFile);

        uint16_t bindPort() const {return m_bindPort;}
        uint16_t connLimit() const {return m_connLimit;}
        uint16_t ioTimeout() const {return  m_ioTimeout;}
        bool ssl() const {return  m_ssl;}
        const std::string &certFile() const {return m_certFile;}
        const std::string &pkeyFile() const {return m_pkeyFile;}

        const std::string &logDst() const {return  m_logDst;}
        const std::string &logLevel () const {return m_logLevel;}

    private:
        confParser_t() = default;

        void loadFile(const std::string &_fileName);
    };
} // namespace tgwss

#endif //TGWSS_CONFPARSER_H
