/**
* @file json/confParser.cpp
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#include <vector>
#include <fstream>

#include "rapidjson/error/en.h"

#include "confParser.h"

namespace tgwss {
    void confParser_t::loadFile(const std::string &_fileName) {
        std::vector<char> data;
        try {
            // load file
            std::ifstream ifs;
            ifs.exceptions(std::ifstream::failbit);
            ifs.open(_fileName, std::ifstream::in | std::ifstream::binary);
            auto startPos = ifs.tellg();
            ifs.ignore(std::numeric_limits<std::streamsize>::max());
            auto size = static_cast<std::size_t>(ifs.gcount());
            ifs.seekg(startPos);
            data.resize(size + 1, 0);
            ifs.read(data.data(), size);
            ifs.close();
        } catch (...) {
            throw std::runtime_error("confParser: failed to read config file");
        }

        m_parser = std::make_unique<parser_t>(data.data());
    }

    void confParser_t::init(const std::string &_confFile) {
        loadFile(_confFile);

        if (!m_parser) {
            throw std::runtime_error("confParser: failed to init parser");
        }

        if (!m_parser->json().HasMember("network") || !m_parser->json()["network"].IsObject()) {
            throw std::runtime_error("confParser: failed to parse network config section");
        }

        if (!m_parser->json()["network"].HasMember("bind_port") ||
            !m_parser->json()["network"]["bind_port"].IsUint()) {
            throw std::runtime_error("confParser: failed to parse \"bind_port\" parameter");
        }
        uint32_t tmpBindPort = m_parser->json()["network"]["bind_port"].GetUint();
        if ((tmpBindPort < 1) || (tmpBindPort > 65535)) {
            throw std::runtime_error("confParser: wrong \"bind_port\" value");
        }
        m_bindPort = static_cast<uint16_t>(tmpBindPort);

        if (!m_parser->json()["network"].HasMember("conn_limit") ||
            !m_parser->json()["network"]["conn_limit"].IsUint()) {
            throw std::runtime_error("confParser: failed to parse \"conn_limit\" parameter");
        }
        uint32_t tmpConnLimit = m_parser->json()["network"]["conn_limit"].GetUint();
        if ((tmpConnLimit < 1) || (tmpConnLimit > 65535)) {
            throw std::runtime_error("confParser: wrong \"conn_limit\" value");
        }
        m_connLimit = static_cast<uint16_t>(tmpConnLimit);

        if (!m_parser->json()["network"].HasMember("io_timeout") ||
            !m_parser->json()["network"]["io_timeout"].IsUint()) {
            throw std::runtime_error("confParser: failed to parse \"io_timeout\" parameter");
        }
        uint32_t tmpIOTimeout = m_parser->json()["network"]["io_timeout"].GetUint();
        if ((tmpIOTimeout < 1) || (tmpIOTimeout > 65535)) {
            throw std::runtime_error("confParser: wrong \"io_timeout\" value");
        }
        m_ioTimeout = static_cast<uint16_t>(tmpIOTimeout);

        if (!m_parser->json()["network"].HasMember("ssl") ||
            !m_parser->json()["network"]["ssl"].IsBool()) {
            throw std::runtime_error("confParser: failed to parse \"ssl\" parameter");
        }
        m_ssl = m_parser->json()["network"]["ssl"].GetBool();

        if (m_ssl) {
            if (!m_parser->json()["network"].HasMember("cert_file") ||
                !m_parser->json()["network"]["cert_file"].IsString()) {
                throw std::runtime_error("confParser: failed to parse \"cert_file\" parameter");
            }
            m_certFile = m_parser->json()["network"]["cert_file"].GetString();
            if (m_certFile.empty()) {
                throw std::runtime_error("confParser: wrong \"cert_file\" value");
            }

            if (!m_parser->json()["network"].HasMember("pkey_file") ||
                !m_parser->json()["network"]["pkey_file"].IsString()) {
                throw std::runtime_error("confParser: failed to parse \"pkey_file\" parameter");
            }
            m_pkeyFile = m_parser->json()["network"]["pkey_file"].GetString();
            if (m_pkeyFile.empty()) {
                throw std::runtime_error("confParser: wrong \"pkey_file\" value");
            }
        }

        if (!m_parser->json().HasMember("log") || !m_parser->json()["log"].IsObject()) {
            throw std::runtime_error("failed to parse log config section");
        }

        if (!m_parser->json()["log"].HasMember("destination") || !m_parser->json()["log"]["destination"].IsString()) {
            throw std::runtime_error("failed to parse \"destination\" parameter");
        }
        m_logDst = m_parser->json()["log"]["destination"].GetString();
        if (m_logDst.empty()) {
            throw std::runtime_error("wrong \"destination\" value");
        }

        if (!m_parser->json()["log"].HasMember("level") || !m_parser->json()["log"]["level"].IsString()) {
            throw std::runtime_error("failed to parse \"level\" parameter");
        }
        m_logLevel = m_parser->json()["log"]["level"].GetString();
        if (m_logLevel.empty()) {
            throw std::runtime_error("wrong \"level\" value");
        }
    }
} // namespace tgwss
