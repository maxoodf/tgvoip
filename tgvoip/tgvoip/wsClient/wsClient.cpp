/**
* @file wsClient/wsClient.cpp
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#include <cstring>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <webrtc/rtc_base/logging.h>
#pragma GCC diagnostic pop

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include "wsClient.h"

static uint32_t g_msgSizeLimit = 64 * 1024;
static uint32_t g_packetSize = 1024;

wsClient_t::wsClient_t(std::string _host, uint16_t _port, std::string _path,
                       bool _ssl, uint16_t _ioTimeout,
                       std::string _token): m_host(std::move(_host)),
                                            m_port(_port),
                                            m_path(std::move(_path)),
                                            m_token(std::move(_token)) {
    RTC_LOG(INFO) << "wsClient: launching...";
    lws_set_log_level(0, nullptr);

    std::memset(&m_wsProtocol, 0, sizeof(m_wsProtocol));
    m_wsProtocol.name = m_protoName.c_str();
    m_wsProtocol.callback = wsClient_t::cbService;
    m_wsProtocol.tx_packet_size = g_packetSize;
    m_wsProtocol.rx_buffer_size = g_packetSize;

    memset(&m_contextInfo, 0, sizeof(m_contextInfo));
    m_contextInfo.port = CONTEXT_PORT_NO_LISTEN;
    m_contextInfo.ka_time = static_cast<uint16_t>(_ioTimeout * 2) ;
    m_contextInfo.ka_probes = 1;
    m_contextInfo.ka_interval = 1;
    m_contextInfo.timeout_secs = static_cast<uint16_t>(_ioTimeout);
//    m_contextInfo.ws_ping_pong_interval = static_cast<uint16_t>(_ioTimeout);
    m_contextInfo.ws_ping_pong_interval = static_cast<uint16_t>(_ioTimeout / 2);
    m_contextInfo.protocols = &m_wsProtocol;
    m_contextInfo.extensions = nullptr;
    m_contextInfo.gid = -1;
    m_contextInfo.uid = -1;
    m_contextInfo.user = this;
    if (_ssl) {
        RTC_LOG(INFO) << "wsClient: initializing SSL/TLS...";
        m_contextInfo.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    }
    RTC_LOG(INFO) << "wsClient: launched";
}

wsClient_t::~wsClient_t() noexcept {
    RTC_LOG(INFO) << "wsClient: stopping...";
    stop();
    RTC_LOG(INFO) << "wsClient: stopped";
}

bool wsClient_t::start(cbOnRegistered_t _cbOnRegistered,
                       cbOnCall_t _cbOnCall,
                       cbOnSdp_t _cbOnSdp,
                       cbOnIce_t _cbOnIce,
                       cbOnDisconnected_t _cbOnDisconnected,
                       void *_ctx) {
    if (m_wsState != wsState_t::DISCONNECTED) {
        RTC_LOG(INFO) << "wsClient: already started, start call ignored";
        return false;
    }

    m_context = lws_create_context(&m_contextInfo);
    if (m_context == nullptr) {
        RTC_LOG(INFO) << "wsClient: failed to create LWS context";
        return false;
    }

    m_wsState = wsState_t::CONNECTING;

    memset(&m_connectInfo, 0, sizeof(m_connectInfo));
    m_connectInfo.address = m_host.c_str();
    m_connectInfo.port = m_port;
    m_connectInfo.path = m_path.c_str();

    m_connectInfo.context = m_context;
    m_connectInfo.host = m_connectInfo.address;
    m_connectInfo.origin = m_connectInfo.address;
    m_connectInfo.protocol = m_protoName.c_str();
    m_connectInfo.ietf_version_or_minus_one = -1;
    if (m_contextInfo.options & LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT) {
        m_connectInfo.ssl_connection = 1;
        RTC_LOG(INFO) << "wsClient: SSL/TLS connection will be used";
    }

    m_cbOnRegistered = std::move(_cbOnRegistered);
    m_cbOnSdp = std::move(_cbOnSdp);
    m_cbOnIce = std::move(_cbOnIce);
    m_cbOnCall = std::move(_cbOnCall);
    m_cbOnDisconnected = std::move(_cbOnDisconnected);
    m_ctx = _ctx;

    m_eventProcessingThread = std::make_unique<std::thread>(wsClient_t::eventProcessingWorker, this);

    return connect();
}

void wsClient_t::stop() {
    m_stopFlag = true;
    if (m_eventProcessingThread) {
        m_eventProcessingThread->join();
    }
    m_eventProcessingThread = nullptr;

    if (m_context != nullptr) {
        lws_context_destroy(m_context);
    }
}
/*
void wsClient_t::processEvents() {
    lws_service(m_context, 0);
}
*/

bool wsClient_t::connect() noexcept {
    m_lws = lws_client_connect_via_info(&m_connectInfo);
    if (m_lws == nullptr) {
        RTC_LOG(INFO) << "wsClient: LWS connect call failed";
        lws_context_destroy(m_context);
        m_context = nullptr;
        return false;
    }

    return true;
}

void wsClient_t::eventProcessingWorker(wsClient_t *_wsClient) {
    while (!_wsClient->m_stopFlag) {
        // process lws events
        lws_service(_wsClient->m_context, 0);

        switch (_wsClient->state()) {
            case wsClient_t::wsState_t::REGISTERED: {
                if (!_wsClient->m_calleeToken.empty()) {
                    _wsClient->callRequest();
                }
                break;
            }
            default: {
                break;
            }
        }

    }
}

int wsClient_t::cbService(struct lws *_wsi, enum lws_callback_reasons _reason,
                          void *_user, void *_data, size_t _size) {
    auto wsClient = static_cast<wsClient_t *>(lws_context_user(lws_get_context(_wsi)));
    if (wsClient == nullptr) {
        return 1;
    }

    switch (_reason) {
        case LWS_CALLBACK_OPENSSL_PERFORM_SERVER_CERT_VERIFICATION: {
            // !!! remove this callback processing from production code !!!
            X509_STORE_CTX_set_error(reinterpret_cast<X509_STORE_CTX *>(_user), X509_V_OK);
            return 0;
        }
        case LWS_CALLBACK_CLIENT_ESTABLISHED: {
            wsClient->m_wsState = wsState_t::CONNECTED;
            wsClient->m_connectAttempts = 0;
            RTC_LOG(INFO) << "cbService: connected to server";

            wsClient->m_started = std::chrono::high_resolution_clock::now();

            if (!wsClient->login()) {
                return -1;
            }
            break;
        }
        case LWS_CALLBACK_CLIENT_RECEIVE_PONG: {
            switch (wsClient->m_wsState) {
                case wsState_t::CONNECTING:
                case wsState_t::CONNECTED:
                case wsState_t::REGISTERING:
                case wsState_t::REGISTERED: {
                    auto processingTime = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::high_resolution_clock::now() - wsClient->m_started
                    ).count();
                    if (processingTime > 30) {
                        RTC_LOG(INFO) << "cbService: no call within 30 seconds";
                        return -1;
                    }
                }
                default: {
                    break;
                }
            }
            break;
        }
        case LWS_CALLBACK_CLIENT_RECEIVE: {
            RTC_LOG(INFO) << "cbService: message received, size: "
                          << _size << ", remaining size: " << lws_remaining_packet_payload(_wsi) << " - "
                          << static_cast<char *>(_data);

            if (wsClient->m_readBuf.size() + _size > g_msgSizeLimit) {
                std::string errStr = R"({"error": "too many unparsed data received"})";
                wsClient_t::closeWithErrMsg(_wsi, LWS_CLOSE_STATUS_INVALID_PAYLOAD, errStr);
                RTC_LOG(INFO) << "cbService: too many unparsed data received";
                return -1;
            }

            wsClient->m_readBuf.insert(wsClient->m_readBuf.end(),
                                     static_cast<char *>(_data),
                                     static_cast<char *>(_data) + _size);

            // is it final part of message?
            if (!lws_is_final_fragment(_wsi) || (lws_remaining_packet_payload(_wsi) > 0)) {
                return 0; // no - wait for more data
            }

            std::vector<char> jsonMsg = std::move(wsClient->m_readBuf);
            if (!wsClient->parse(jsonMsg)) {
                // callee is offline, trying to repeat 5 call attempts with 2 sec delay
                if ((wsClient->state() == wsState_t::CALL_PENDING) && (wsClient->m_callAttempts < 5)) {
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    wsClient->m_callAttempts++;
                    wsClient->callRequest();
                    break;
                }
                return -1;
            }
            break;
        }
        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            try {
                if (wsClient->m_writeBufQueue.empty()) {
                    break;
                }
                std::vector<unsigned char> buf = std::move(wsClient->m_writeBufQueue.front());
                wsClient->m_writeBufQueue.pop();

                RTC_LOG(INFO) << "write: message sending, size " << buf.size() << " - "
                              << std::string(reinterpret_cast<char *>(buf.data()) + LWS_PRE,
                                             buf.size() - LWS_PRE);

                if (lws_write(_wsi, buf.data() + LWS_PRE, buf.size() - LWS_PRE, LWS_WRITE_TEXT) < 0) {
                    RTC_LOG(INFO) << "write: client " << _wsi << ", failed";
                    return -1;
                }

                if (!wsClient->m_writeBufQueue.empty()) {
                    lws_callback_on_writable(_wsi);
                }
            } catch (...) {
                RTC_LOG(INFO) << "write: client "  << _wsi << ", failed (out of memory?)";
                return -1;
            }

            break;
        }
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR: {
            if (wsClient->m_connectAttempts < 3) {
                RTC_LOG(INFO) << "cbService: callback client connection error, trying to reconnect...";
                std::this_thread::sleep_for(std::chrono::seconds(1));
                wsClient->m_connectAttempts++;
                return wsClient->connect();
            }
            wsClient->m_wsState = wsState_t::DISCONNECTED;
            wsClient->m_cbOnDisconnected(wsClient->m_ctx);
            RTC_LOG(INFO) << "cbService: callback client connection error";
            break;
        }
        case LWS_CALLBACK_CLOSED: {
            wsClient->m_wsState = wsState_t::DISCONNECTED;
            wsClient->m_cbOnDisconnected(wsClient->m_ctx);
            RTC_LOG(INFO) << "cbService: connection closed";
            break;
        }
        case LWS_CALLBACK_CLIENT_CLOSED: {
            wsClient->m_wsState = wsState_t::DISCONNECTED;
            wsClient->m_cbOnDisconnected(wsClient->m_ctx);
            RTC_LOG(INFO) << "cbService: client's connection closed";
            break;
        }
        default: {
        }
    }

    return 0;
}

bool wsClient_t::write(const void *_message, std::size_t _size) noexcept {
    if ((m_wsState == wsState_t::DISCONNECTED) || (m_wsState == wsState_t::CONNECTING)) {
        return false;
    }
    try {
        std::vector<unsigned char> buf(LWS_PRE + _size, 0);
        std::memmove(buf.data() + LWS_PRE, _message, _size);
        m_writeBufQueue.emplace(buf);
        lws_callback_on_writable(m_lws);
        return true;
    } catch (...) {
        RTC_LOG(INFO) << "wsClient: message sending failed (out of memory?)";
        return false;
    }
}

void wsClient_t::closeWithErrMsg(struct lws *_lws, lws_close_status _status, const std::string &_errMsg) noexcept {
    try {
        std::vector<unsigned char> errBuf(_errMsg.length());
        std::memcpy(errBuf.data(), _errMsg.data(), _errMsg.length());
        lws_close_reason(_lws, _status, errBuf.data(), errBuf.size());
    } catch (...) {
    }
}

bool wsClient_t::login() {
    if (m_wsState != wsState_t::CONNECTED) {
        return false;
    }
    rapidjson::Document jsonMessage;
    jsonMessage.SetObject();

    // {"type": "logon", token: "token_value", "status":true}
    jsonMessage.AddMember("type", "logon", jsonMessage.GetAllocator());
    rapidjson::Value token;
    token.SetString(m_token.c_str(),
                    static_cast<rapidjson::SizeType>(m_token.length()),
                    jsonMessage.GetAllocator());
    jsonMessage.AddMember("token", token, jsonMessage.GetAllocator());
    rapidjson::StringBuffer jsonStr;
    rapidjson::Writer<rapidjson::StringBuffer> writer(jsonStr);
    jsonMessage.Accept(writer);

    if (write(jsonStr.GetString(), jsonStr.GetLength())) {
        m_wsState = wsState_t::REGISTERING;
        return true;
    } else {
        return false;
    }
}

bool wsClient_t::parse(const std::vector<char> &_msg) {
    RTC_LOG(INFO) << "parse: new message received from signaling server \""
                  << std::string(_msg.data(), _msg.size())
                  << "\"";

    rapidjson::Document json;
    json.Parse(_msg.data(), _msg.size());
    if (json.HasParseError()) {
        RTC_LOG(INFO) << "parse: failed to parse server's message";
        return false;
    }

    if (json.HasMember("error") && json["error"].IsString()) {
        std::string error = json["error"].GetString();
        RTC_LOG(INFO) << "parse: " << error;
        return false;
    }
    if (json.HasMember("type") && json["type"].IsString()) {
        std::string type = json["type"].GetString();
        if ((type == "info") &&  json.HasMember("subscriber") && json["subscriber"].IsString()) {
            std::string subscriber = json["subscriber"].GetString();
            if (subscriber == "disconnected")
            RTC_LOG(INFO) << "parse: subscriber disconnected";
            return false;
        }
        if ((type == "logon") &&  json.HasMember("status") &&
            json["status"].IsBool() && json["status"].GetBool()) {
            RTC_LOG(INFO) << "parse: registered on signaling server";
            m_wsState = wsState_t::REGISTERED;
            m_cbOnRegistered(wsClient_t::sdpSessionDescription,
                             wsClient_t::iceCandidate,
                             this,
                             m_ctx);
            return true;
        }
        if ((type == "call") &&  json.HasMember("from") && json["from"].IsString()) {
            m_remoteToken =  json["from"].GetString();
            RTC_LOG(INFO) << "parse: call requested from " << m_remoteToken;
            m_wsState = wsState_t::CALL_REQUESTED;

            std::string reply = R"({"type": "call", "status": true})";
            if (write(reply.c_str(), reply.length())) {
                m_wsState = wsState_t::CALL_REQUESTED;
                m_cbOnCall(false, m_ctx);
                return true;
            }

            return true;
        }
        if ((type == "call") &&  json.HasMember("status") && json["status"].IsBool()) {
            if (json["status"].GetBool()) {
                RTC_LOG(INFO) << "parse: call requested";
                m_wsState = wsState_t::CALL_CONFIRMED;
                m_cbOnCall(true, m_ctx);
                return true;
            } else {
                RTC_LOG(INFO) << "parse: subscriber is offline";
                m_wsState = wsState_t::CALL_PENDING;
                return false;
            }
        }
        if ((type == "offer") &&  (json.HasMember("sdp") && json["sdp"].IsString())) {
            RTC_LOG(INFO) << "parse: SDP offer received";
            m_wsState = wsState_t::SDP_NEGOTIATED;
            return m_cbOnSdp(true, json["sdp"].GetString(), m_ctx);
        }
        if ((type == "answer") &&  (json.HasMember("sdp") && json["sdp"].IsString())) {
            RTC_LOG(INFO) << "parse: SDP answer received";
            m_wsState = wsState_t::SDP_NEGOTIATED;
            return m_cbOnSdp(false, json["sdp"].GetString(), m_ctx);
        }
    } else if (json.HasMember("candidate")) {
        RTC_LOG(INFO) << "parse: ICE candidate description received";
        std::string sdpMid;
        int sdpMLineIndex;
        std::string candidate;

        if (json["candidate"].IsString()) {
            if ((json.HasMember("sdpMid") && json["sdpMid"].IsString()) &&
                (json.HasMember("sdpMLineIndex") && json["sdpMLineIndex"].IsInt())) {
                sdpMid = json["sdpMid"].GetString();
                sdpMLineIndex = json["sdpMLineIndex"].GetInt();
                candidate = json["candidate"].GetString();
            } else {
                RTC_LOG(INFO) << "parse: failed to parse sdpMid, sdpMLineIndex & candidate values";
                return false;
            }
        } else if (json["candidate"].IsObject()) {
            if ((json["candidate"].HasMember("sdpMid") && json["candidate"]["sdpMid"].IsString()) &&
                (json["candidate"].HasMember("sdpMLineIndex") && json["candidate"]["sdpMLineIndex"].IsInt()) &&
                (json["candidate"].HasMember("candidate") && json["candidate"]["candidate"].IsString())) {
                sdpMid = json["candidate"]["sdpMid"].GetString();
                sdpMLineIndex = json["candidate"]["sdpMLineIndex"].GetInt();
                candidate = json["candidate"]["candidate"].GetString();
            } else {
                RTC_LOG(INFO) << "parse: failed to parse sdpMid, sdpMLineIndex & candidate values";
                return false;
            }
        } else {
            RTC_LOG(INFO) << "parse: failed to parse ICE candidate";
            return false;
        }


        m_wsState = wsState_t::ICE_NEGOTIATED;
        return m_cbOnIce(sdpMid, sdpMLineIndex, candidate, m_ctx);
    }

    RTC_LOG(INFO) << "procManager: failed to process reply";
    return false;
}

bool wsClient_t::callRequest() {
    rapidjson::Document jsonMessage;
    jsonMessage.SetObject();

    // {"type": "call", to: "token_value"}
    jsonMessage.AddMember("type", "call", jsonMessage.GetAllocator());
    rapidjson::Value token;
    token.SetString(m_calleeToken.c_str(),
                    static_cast<rapidjson::SizeType>(m_calleeToken.length()),
                    jsonMessage.GetAllocator());
    jsonMessage.AddMember("to", token, jsonMessage.GetAllocator());
    rapidjson::StringBuffer jsonStr;
    rapidjson::Writer<rapidjson::StringBuffer> writer(jsonStr);
    jsonMessage.Accept(writer);

    if (write(jsonStr.GetString(), jsonStr.GetLength())) {
        m_wsState = wsState_t::CALL_REQUESTED;
        return true;
    }

    return false;
}

bool wsClient_t::sdpSessionDescription(const std::string &_type,
                                       const std::string &_sdpMsg,
                                       void *_ctx) {
    rapidjson::Document jsonMessage;
    jsonMessage.SetObject();

    rapidjson::Value typeKey;
    typeKey.SetString(_type.c_str(),
                      static_cast<rapidjson::SizeType>(_type.length()),
                      jsonMessage.GetAllocator());
    jsonMessage.AddMember("type", typeKey, jsonMessage.GetAllocator());

    rapidjson::Value sdpKey;
    sdpKey.SetString(_sdpMsg.c_str(),
                     static_cast<rapidjson::SizeType>(_sdpMsg.length()),
                     jsonMessage.GetAllocator());
    jsonMessage.AddMember("sdp", sdpKey, jsonMessage.GetAllocator());

    rapidjson::StringBuffer jsonStr;
    rapidjson::Writer<rapidjson::StringBuffer> writer(jsonStr);
    jsonMessage.Accept(writer);

    auto wsClient = reinterpret_cast<wsClient_t *>(_ctx);
    if (wsClient->write(jsonStr.GetString(), jsonStr.GetLength())) {
        wsClient->m_wsState = wsState_t::SDP_NEGOTIATING;
        return true;
    }

    return false;
}

bool wsClient_t::iceCandidate(const std::string &_sdpMID,
                              int _sdpMLineIndex,
                              const std::string &_sdpCandidate,
                              void *_ctx) {
    rapidjson::Document jsonMessage;
    jsonMessage.SetObject();

    rapidjson::Value sdpMidKey;
    sdpMidKey.SetString(_sdpMID.c_str(),
                        static_cast<rapidjson::SizeType>(_sdpMID.length()),
                        jsonMessage.GetAllocator());
    jsonMessage.AddMember("sdpMid", sdpMidKey, jsonMessage.GetAllocator());

    rapidjson::Value sdpMLineIndexKey;
    sdpMLineIndexKey.SetInt(_sdpMLineIndex);
    jsonMessage.AddMember("sdpMLineIndex", sdpMLineIndexKey, jsonMessage.GetAllocator());

    rapidjson::Value sdpCandidateKey;
    sdpCandidateKey.SetString(_sdpCandidate.c_str(),
                              static_cast<rapidjson::SizeType>(_sdpCandidate.length()),
                              jsonMessage.GetAllocator());
    jsonMessage.AddMember("candidate", sdpCandidateKey, jsonMessage.GetAllocator());

    rapidjson::StringBuffer jsonStr;
    rapidjson::Writer<rapidjson::StringBuffer> writer(jsonStr);
    jsonMessage.Accept(writer);

    auto wsClient = reinterpret_cast<wsClient_t *>(_ctx);
    if (wsClient->write(jsonStr.GetString(), jsonStr.GetLength())) {
        wsClient->m_wsState = wsState_t::ICE_NEGOTIATING;
        return true;
    }

    return false;
}
