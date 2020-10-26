/**
* @file wsClient/wsClient.h
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#ifndef TESTWEBRTC_WSCLIENT_H
#define TESTWEBRTC_WSCLIENT_H

#include <string>
#include <vector>
#include <queue>
#include <chrono>
#include <atomic>
#include <thread>
#include <functional>

#include <libwebsockets.h>

class wsClient_t {
public:
    using cbSdpSessionDescription_t = std::function<bool(const std::string &_type,
                                                         const std::string &_sdpMsg,
                                                         void *_ctx)>;
    using cbIceCandidate_t = std::function<bool(const std::string &_sdpMID,
                                                int _sdpMLineIndex,
                                                const std::string &_sdpCandidate,
                                                void *_ctx)>;

    using cbOnRegistered_t = std::function<void(cbSdpSessionDescription_t _cbSdpSessionDescription,
                                                cbIceCandidate_t _cbIceCandidate,
                                                void *_localCtx,
                                                void *_remoteCtx)>;
    using cbOnCall_t = std::function<void(bool _caller,
                                          void *_ctx)>;
    using cbOnSdp_t = std::function<bool(bool _isOffer,
                                         const std::string &_sdpMsg,
                                         void *_ctx)>;
    using cbOnIce_t = std::function<bool(const std::string &_sdpMid,
                                         int _sdpMLineIndex,
                                         const std::string &_candidate,
                                         void *_ctx)>;
    using cbOnDisconnected_t = std::function<void(void *_ctx)>;

    enum class wsState_t {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        REGISTERING,
        REGISTERED,
        CALL_REQUESTED,
        CALL_PENDING,
        CALL_CONFIRMED,
        SDP_NEGOTIATING,
        SDP_NEGOTIATED,
        ICE_NEGOTIATING,
        ICE_NEGOTIATED
    };

private:
    const std::string m_host;
    const uint16_t m_port;
    const std::string m_path;
    const std::string m_token;

    std::string m_remoteToken;

    std::atomic<wsState_t> m_wsState {wsState_t::DISCONNECTED};

    struct lws_protocols m_wsProtocol {};
    lws_context_creation_info m_contextInfo {};
    lws_client_connect_info m_connectInfo {};
    lws *m_lws = nullptr;

    lws_context *m_context = nullptr;
    std::string m_protoName = "tgwss";

    std::vector<char> m_readBuf;
    std::queue<std::vector<unsigned char>> m_writeBufQueue;

    std::unique_ptr<std::thread> m_eventProcessingThread;
    std::atomic<bool> m_stopFlag {false};

    cbOnRegistered_t m_cbOnRegistered = nullptr;
    cbOnSdp_t m_cbOnSdp = nullptr;
    cbOnIce_t m_cbOnIce = nullptr;
    cbOnCall_t m_cbOnCall = nullptr;
    cbOnDisconnected_t m_cbOnDisconnected = nullptr;
    void *m_ctx = nullptr;

    std::string m_calleeToken;

    uint8_t m_connectAttempts = 0;
    uint8_t m_callAttempts = 0;

    std::chrono::time_point<std::chrono::high_resolution_clock> m_started;

public:
    wsClient_t(std::string _host, uint16_t _port, std::string _path,
               bool _ssl, uint16_t _ioTimeout,
               std::string _token);
    virtual ~wsClient_t() noexcept;

    wsClient_t(const wsClient_t &) = delete;
    wsClient_t(const wsClient_t &&) = delete;
    void operator=(const wsClient_t &) = delete;
    void operator=(const wsClient_t &&) = delete;

    bool start(cbOnRegistered_t _cbOnRegistered,
               cbOnCall_t _cbOnCall,
               cbOnSdp_t _cbOnSdp,
               cbOnIce_t _cbOnIce,
               cbOnDisconnected_t _cbOnDisconnected,
               void *_ctx);
    void stop();
//    void processEvents();

    wsState_t state() const noexcept {return m_wsState;}
    void callTo(std::string _to) {m_calleeToken = std::move(_to);}

    static bool sdpSessionDescription(const std::string &_type,
                                      const std::string &_sdpMsg,
                                      void *_ctx);
    static bool iceCandidate(const std::string &_sdpMID,
                             int _sdpMLineIndex,
                             const std::string &_sdpCandidate,
                             void *_ctx);

private:
    static int cbService(struct lws *_wsi, enum lws_callback_reasons _reason, void *_user, void *_in, size_t _len);
    static void eventProcessingWorker(wsClient_t *_wsClient);
    static void closeWithErrMsg(struct lws *_lws, lws_close_status _status, const std::string &_errMsg) noexcept;

    bool connect() noexcept;
    bool write(const void *_message, std::size_t _size) noexcept;
    bool login();
    bool parse(const std::vector<char> &_msg);
    bool callRequest();
};

#endif //TESTWEBRTC_WSCLIENT_H
