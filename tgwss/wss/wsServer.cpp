/**
* @file wss/wsServer.cpp
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#include <cstring>
#include <vector>
#include <algorithm>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/writer.h>

#include "json/confParser.h"
#include "logger/logger.h"
#include "wsServer.h"

namespace tgwss {
    static uint32_t g_msgSizeLimit = 64 * 1024;
    static uint32_t g_packetSize = 1024;

    wsServer_t::wsServer_t(const confParser_t *_confParser, logger_t *_logger) : m_logger(_logger) {
        m_logger->log(logger_t::logLevel_t::LL_DEBUG, "wsServer: launching...");

        lws_set_log_level(0, nullptr);

        std::memset(&m_wsProtocol, 0, sizeof(m_wsProtocol));
        m_wsProtocol.name = "tgwss";
        m_wsProtocol.callback = wsServer_t::wscbService;
        m_wsProtocol.tx_packet_size = g_packetSize;
        m_wsProtocol.rx_buffer_size = g_packetSize;

        std::memset(&m_wsInfo, 0, sizeof(m_wsInfo));
        m_wsInfo.port = _confParser->bindPort();
        m_wsInfo.ka_time = _confParser->ioTimeout() * 2;
        m_wsInfo.ka_probes = 1;
        m_wsInfo.ka_interval = 1;
        m_wsInfo.timeout_secs = _confParser->ioTimeout();
        m_wsInfo.ws_ping_pong_interval = _confParser->ioTimeout() / 2;
        m_wsInfo.gid = -1;
        m_wsInfo.uid = -1;
        m_wsInfo.user = reinterpret_cast<void *>(this);
        if (_confParser->ssl()) {
            m_wsInfo.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
            m_wsInfo.ssl_cert_filepath = _confParser->certFile().c_str();
            m_wsInfo.ssl_private_key_filepath = _confParser->pkeyFile().c_str();
        }
        m_wsInfo.protocols = &m_wsProtocol;

        m_wsContext = lws_create_context(&m_wsInfo);
        if (m_wsContext == nullptr) {
            throw std::runtime_error("WS context create failed");
        }

        m_logger->log(logger_t::logLevel_t::LL_NOTICE, "wsServer: launched");
    }

    wsServer_t::~wsServer_t() {
        m_logger->log(logger_t::logLevel_t::LL_DEBUG, "wsServer: stopping...");
        stop();
        lws_context_destroy(m_wsContext);

        m_logger->log(logger_t::logLevel_t::LL_NOTICE, "wsServer: stopped");
    }

    void wsServer_t::eventProcessingWorker(wsServer_t *_wsServer) {
        while (!_wsServer->m_stopFlag) {
            // process lws events
            lws_service(_wsServer->m_wsContext, 0);
        }
    }

    void wsServer_t::start() {
        m_eventProcessingThread = std::make_unique<std::thread>(wsServer_t::eventProcessingWorker, this);
    }

    void wsServer_t::stop() {
        m_stopFlag = true;
        if (m_eventProcessingThread) {
            m_eventProcessingThread->join();
        }
        m_eventProcessingThread = nullptr;
    }

    int wsServer_t::wscbService(struct lws *_lws, enum lws_callback_reasons _reason,
                                void *, void *_data, size_t _size) noexcept {
        auto wsServer = static_cast<wsServer_t *>(lws_context_user(lws_get_context(_lws)));
        if (wsServer == nullptr) {
            return -1;
        }

        switch (_reason) {
            case LWS_CALLBACK_ESTABLISHED: {
                wsServer->m_logger->log(logger_t::logLevel_t::LL_NOTICE,
                                        "wscbService: connection established, client {:p}",
                                        fmt::ptr(_lws));
                break;
            }

            case LWS_CALLBACK_CLIENT_CONNECTION_ERROR: {
//            atomicGuard_t atomicGuard(&wsServer->m_atomicLock);
                wsServer->m_logger->log(logger_t::logLevel_t::LL_NOTICE,
                                        "wscbService: client connection error, client {:p}",
                                        fmt::ptr(_lws));
                wsServer->remove(_lws);
                break;
            }
            case LWS_CALLBACK_CLOSED: {
//            atomicGuard_t atomicGuard(&wsServer->m_atomicLock);
                wsServer->m_logger->log(logger_t::logLevel_t::LL_NOTICE,
                                        "wscbService: connection closed, client {:p}",
                                        fmt::ptr(_lws));
                wsServer->remove(_lws);
                break;
            }
            case LWS_CALLBACK_CLIENT_CLOSED: {
//            atomicGuard_t atomicGuard(&wsServer->m_atomicLock);
                wsServer->m_logger->log(logger_t::logLevel_t::LL_NOTICE,
                                        "wscbService: client's connection closed, client {:p}",
                                        fmt::ptr(_lws));
                wsServer->remove(_lws);
                break;
            }

            case LWS_CALLBACK_RECEIVE: {
//            atomicGuard_t atomicGuard(&wsServer->m_atomicLock);
                wsServer->m_logger->log(logger_t::logLevel_t::LL_DEBUG,
                                        "wscbService: data received, client {:p}, message: {:s}, "
                                        "size {:d}, remaining size: {:d}",
                                        fmt::ptr(_lws),
                                        std::string(static_cast<char *>(_data), _size),
                                        _size,
                                        lws_remaining_packet_payload(_lws));

                // is it a new peer?
                auto p = wsServer->m_peers.find(_lws);
                if (p == wsServer->m_peers.end()) {
                    // new peer, try to authorize
                    if (!wsServer->logon(_lws, static_cast<char *>(_data), _size)) {
                        wsServer->m_logger->log(logger_t::logLevel_t::LL_WARNING,
                                                "wscbService: auth failed, client {:p}",
                                                fmt::ptr(_lws));
                        return -1;
                    } else {
                        wsServer->m_logger->log(logger_t::logLevel_t::LL_DEBUG,
                                                "wscbService: auth processed, client {:p}",
                                                fmt::ptr(_lws));
                    }
                } else {
                    // known peer, retransmit message
                    if (!wsServer->retransmit(_lws, static_cast<char *>(_data), _size)) {
                        wsServer->m_logger->log(logger_t::logLevel_t::LL_WARNING,
                                                "wscbService: data retransmission failed, client {:p}",
                                                fmt::ptr(_lws));
                        return -1;
                    }
                }

                break;
            }

            case LWS_CALLBACK_SERVER_WRITEABLE: {
                try {
                    auto cl = wsServer->m_peers.find(_lws);
                    if (cl == wsServer->m_peers.end() || cl->second->writeQueue.empty()) {
                        break;
                    }
                    std::vector<unsigned char> buf = std::move(cl->second->writeQueue.front());
                    cl->second->writeQueue.pop();

                    wsServer->m_logger->log(logger_t::logLevel_t::LL_DEBUG,
                                            "write: client {:p}, message: {:s}, size {:d}",
                                            fmt::ptr(_lws),
                                            std::string(reinterpret_cast<char *>(buf.data()) + LWS_PRE,
                                                        buf.size() - LWS_PRE),
                                            buf.size());

                    if (lws_write(_lws, buf.data() + LWS_PRE, buf.size() - LWS_PRE, LWS_WRITE_TEXT) < 0) {
                        wsServer->m_logger->log(logger_t::logLevel_t::LL_WARNING,
                                                "write: client {:p}, failed",
                                                fmt::ptr(_lws));
                        return -1;
                    }

                    if (!cl->second->writeQueue.empty()) {
                        lws_callback_on_writable(_lws);
                    } else {
                        if (cl->second->closeStatus != LWS_CLOSE_STATUS_NO_STATUS) {
                            lws_close_reason(_lws, cl->second->closeStatus, buf.data() + LWS_PRE, buf.size() - LWS_PRE);
                            return -1;
                        }
                    }
                } catch (...) {
                    wsServer->m_logger->log(logger_t::logLevel_t::LL_ERROR,
                                            "write: client {:p}, failed (out of memory?)",
                                            fmt::ptr(_lws));
                    return -1;
                }

                break;
            }

            default: {
                break;
            }
        }

        return 0;
    }

    bool wsServer_t::write(struct lws *_lws,
                           const void *_message,
                           std::size_t _size,
                           lws_close_status _closeStatus) noexcept {
        try {
            std::vector<unsigned char> buf(LWS_PRE + _size, 0);
            std::memmove(buf.data() + LWS_PRE, _message, _size);

            auto cl = m_peers.find(_lws);
            if (cl != m_peers.end()) {
                cl->second->writeQueue.emplace(buf);
                cl->second->closeStatus = _closeStatus;
                lws_callback_on_writable(_lws);

                return true;
            }
            m_logger->log(logger_t::logLevel_t::LL_ERROR,
                          "write: unknown client {:p}",
                          fmt::ptr(_lws));
        } catch (...) {
            m_logger->log(logger_t::logLevel_t::LL_ERROR,
                          "write: client {:p}, failed",
                          fmt::ptr(_lws));
        }

        return false;
    }

    void wsServer_t::closeWithErrMsg(struct lws *_lws, lws_close_status _status, const std::string &_errMsg) noexcept {
        try {
            m_logger->log(logger_t::logLevel_t::LL_DEBUG,
                          "closeWithErrMsg: {:s}",
                          _errMsg);

            write(_lws, _errMsg.c_str(), _errMsg.length(), _status);
//            std::vector<unsigned char> errBuf(_errMsg.length());
//            std::memcpy(errBuf.data(), _errMsg.data(), _errMsg.length());
//            lws_close_reason(_lws, _status, errBuf.data(), errBuf.size());
        } catch (...) {
            m_logger->log(logger_t::logLevel_t::LL_ERROR, "close: unknown error");
        }
    }

    bool wsServer_t::logon(struct lws *_lws, const void *_data, std::size_t _size) noexcept {
        try {
            // parse _data
            // client: {"type": "logon", token: "token_value"}
            // server: {"type": "logon", "status":true}
            rapidjson::Document json;
            json.Parse(static_cast<const char *>(_data), _size);
            if (json.HasParseError()) {
                auto errStr = std::string(R"({"error": "failed to parse JSON. )")
                              + rapidjson::GetParseError_En(json.GetParseError())
                              + " Offset " + std::to_string(json.GetErrorOffset()) + "\"}";
                closeWithErrMsg(_lws, LWS_CLOSE_STATUS_INVALID_PAYLOAD, errStr);
                m_logger->log(logger_t::logLevel_t::LL_WARNING,
                              "logon: failed to parse message - {:s}",
                              std::string(reinterpret_cast<const char *>(_data), _size));
                return false;
            }
            // check message type
            if (json.HasMember("type") && json["type"].IsString() && std::string(json["type"].GetString()) == "logon") {
                // client peer registration request
                if (!json.HasMember("token") || !json["token"].IsString()) {
                    std::string errStr = R"({"error": "'token' missed"})";
                    closeWithErrMsg(_lws, LWS_CLOSE_STATUS_INVALID_PAYLOAD, errStr);
                    m_logger->log(logger_t::logLevel_t::LL_WARNING,
                                  "logon: 'token' missed - {:s}",
                                  std::string(reinterpret_cast<const char *>(_data), _size));
                    return false;
                }
                std::string token = json["token"].GetString();
                if (token.length() < 10) {
                    std::string errStr = R"({"error": "wrong 'token' format"})";
                    closeWithErrMsg(_lws, LWS_CLOSE_STATUS_INVALID_PAYLOAD, errStr);
                    m_logger->log(logger_t::logLevel_t::LL_WARNING,
                                  "logon: wrong 'token' format - {:s}",
                                  std::string(reinterpret_cast<const char *>(_data), _size));
                    return false;
                }
                for (const auto &i:m_peers) {
                    if (i.second->token == token) {
                        std::string errStr = R"({"error": "'token' is already online"})";
                        closeWithErrMsg(_lws, LWS_CLOSE_STATUS_INVALID_PAYLOAD, errStr);
                        m_logger->log(logger_t::logLevel_t::LL_WARNING,
                                      "logon: 'token' is already online - {:s}",
                                      token);
                        return false;
                    }
                }

                m_peers.emplace(_lws, std::make_unique<peerData_t>(token));
                const std::string reply = R"({"type": "logon", "status":true})";
                return write(_lws, reply.data(), reply.size());
            }
            std::string errStr = R"({"error": "unexpected message"})";
            closeWithErrMsg(_lws, LWS_CLOSE_STATUS_UNEXPECTED_CONDITION, errStr);
            m_logger->log(logger_t::logLevel_t::LL_WARNING,
                          "logon: unexpected message - {:s}",
                          std::string(reinterpret_cast<const char *>(_data), _size));
        } catch (...) {
            std::string errStr = R"({"error": "internal error"})";
            closeWithErrMsg(_lws, LWS_CLOSE_STATUS_UNEXPECTED_CONDITION, errStr);
            m_logger->log(logger_t::logLevel_t::LL_ERROR, "logon: internal error");
        }

        return false;
    }

    bool wsServer_t::retransmit(struct lws *_lws, const void *_data, std::size_t _size) noexcept {
        try {
            auto peer = m_peers.find(_lws);
            if (peer != m_peers.end()) {
                peer->second->readBuf.insert(peer->second->readBuf.end(),
                                             static_cast<const char *>(_data),
                                             static_cast<const char *>(_data) + _size);
                if (peer->second->readBuf.size() > g_msgSizeLimit) {
                    m_logger->log(logger_t::logLevel_t::LL_WARNING,
                                  "retransmit: message size is out of limits, client peer {:p}, message size {:d}",
                                  fmt::ptr(_lws), peer->second->readBuf.size());
                    return false;
                }

                // is it final part of message?
                if (!lws_is_final_fragment(_lws) || (lws_remaining_packet_payload(_lws) > 0)) {
                    m_logger->log(logger_t::logLevel_t::LL_DEBUG,
                                  "retransmit: waiting for more data, client peer {:p}",
                                  fmt::ptr(_lws));
                    return true; // no, waiting for more data
                }

                // complete message received
                std::vector<char> message = std::move(peer->second->readBuf);

                if (peer->second->subscriber == nullptr) { // new call
                    // parse message
                    // client: {"type": "call", token: "token_value"}
                    // server: {"type": "call", "status":true}
                    rapidjson::Document json;
                    json.Parse(static_cast<const char *>(_data), _size);
                    if (json.HasParseError()) {
                        auto errStr = std::string(R"({"error": "failed to parse JSON. )")
                                      + rapidjson::GetParseError_En(json.GetParseError())
                                      + " Offset " + std::to_string(json.GetErrorOffset()) + "\"}";
                        closeWithErrMsg(_lws, LWS_CLOSE_STATUS_INVALID_PAYLOAD, errStr);
                        m_logger->log(logger_t::logLevel_t::LL_WARNING,
                                      "retransmit: failed to parse message - {:s}",
                                      std::string(reinterpret_cast<const char *>(_data), _size));
                        return false;
                    }
                    // check message type
                    if (json.HasMember("type") && json["type"].IsString() &&
                        std::string(json["type"].GetString()) == "call") {
                        // client peer call request
                        if (!json.HasMember("to") || !json["to"].IsString()) {
                            std::string errStr = R"({"error": "'to' missed"})";
                            closeWithErrMsg(_lws, LWS_CLOSE_STATUS_INVALID_PAYLOAD, errStr);
                            m_logger->log(logger_t::logLevel_t::LL_WARNING,
                                          "retransmit: 'to' missed - {:s}",
                                          std::string(reinterpret_cast<const char *>(_data), _size));
                            return false;
                        }
                        std::string token = json["to"].GetString();
                        if (token.length() < 10) {
                            std::string errStr = R"({"error": "wrong 'to' format"})";
                            closeWithErrMsg(_lws, LWS_CLOSE_STATUS_INVALID_PAYLOAD, errStr);
                            m_logger->log(logger_t::logLevel_t::LL_WARNING,
                                          "retransmit: wrong 'to' format - {:s}",
                                          std::string(reinterpret_cast<const char *>(_data), _size));
                            return false;
                        }
                        for (const auto &i:m_peers) {
                            if (i.second->token == token) {
                                peer->second->subscriber = i.first;
                                i.second->subscriber = _lws;
                                std::string reply = R"({"type": "call", "from": ")" + peer->second->token + R"("})";
                                return write(i.first, reply.c_str(), reply.length());
                            }
                        }
                        std::string errStr = R"({"type": "call", "status": false})";
                        m_logger->log(logger_t::logLevel_t::LL_WARNING,
                                      "retransmit: 'token' is offline - {:s}",
                                      token);
//                        closeWithErrMsg(_lws, LWS_CLOSE_STATUS_INVALID_PAYLOAD, errStr);
                        return write(_lws, errStr.c_str(), errStr.length());
                    } else {
                        std::string errStr = R"({"error": "unexpected message"})";
                        closeWithErrMsg(_lws, LWS_CLOSE_STATUS_UNEXPECTED_CONDITION, errStr);
                        m_logger->log(logger_t::logLevel_t::LL_WARNING,
                                      "retransmit: unexpected message - {:s}",
                                      std::string(reinterpret_cast<const char *>(_data), _size));
                    }

                    return false;
                }

                return write(peer->second->subscriber, message.data(), message.size());
            }
            m_logger->log(logger_t::logLevel_t::LL_WARNING,
                          "retransmit: unknown client peer {:p}",
                          fmt::ptr(_lws));
        } catch (...) {
            m_logger->log(logger_t::logLevel_t::LL_ERROR, "retransmit: internal error");
        }

        return false;
    }

    void wsServer_t::remove(struct lws *_lws) noexcept {
        try {
            auto peer = m_peers.find(_lws);
            if (peer == m_peers.end()) {
                m_logger->log(logger_t::logLevel_t::LL_WARNING,
                              "remove: unknown client peer {:p}",
                              fmt::ptr(_lws));
                return;
            }
            auto subscriber = m_peers.find(peer->second->subscriber);
            if (subscriber != m_peers.end()) {
                std::string msgToPeer = R"({"type": "info", "subscriber": "disconnected"})";
                write(subscriber->first, msgToPeer.data(), msgToPeer.length());
                subscriber->second->subscriber = nullptr;
            }
            m_peers.erase(peer);
            m_logger->log(logger_t::logLevel_t::LL_DEBUG, "remove: peer {:p}", fmt::ptr(_lws));
        } catch (...) {
            m_logger->log(logger_t::logLevel_t::LL_ERROR, "remove: internal error");
        }
    }
} // namespace tgwss
