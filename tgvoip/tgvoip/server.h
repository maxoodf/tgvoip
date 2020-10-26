/**
* @file tgvoip/server.h
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#ifndef WEBRTC_SOCKETSERVER_H
#define WEBRTC_SOCKETSERVER_H

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif
#include <webrtc/base/memory/scoped_refptr.h>
#include <webrtc/rtc_base/physical_socket_server.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#pragma GCC diagnostic pop

class webRTCPeer_t;

class server_t: public rtc::PhysicalSocketServer {
private:
    rtc::Thread *m_thread = nullptr;
    webRTCPeer_t *m_peer = nullptr;

public:
    explicit server_t(webRTCPeer_t *_peer);
    ~server_t() override = default;

    void SetMessageQueue(rtc::Thread *_thread) override;
    bool Wait(int _cms, bool _io) override;
};

#endif //WEBRTC_SOCKETSERVER_H
