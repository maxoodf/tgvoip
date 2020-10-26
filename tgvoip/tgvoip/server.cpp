/**
* @file tgvoip/server.cpp
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#include "webRTCPeer.h"
#include "server.h"

server_t::server_t(webRTCPeer_t *_peer): m_peer(_peer) {
}

void server_t::SetMessageQueue(rtc::Thread *_thread) {
    m_thread = _thread;
}

bool server_t::Wait(int , bool _io) {
    switch (m_peer->state()) {
        case webRTCPeer_t::peerState_t::CALL_REQUESTED: {
            m_peer->init();
            break;
        }
        case webRTCPeer_t::peerState_t::CALL_HANGUP: {
            m_peer->stop();
            break;
        }
//        case webRTCPeer_t::peerState_t::STOPPING: {
//            m_thread->Quit();
//            return false;
//        }
        default: {
            break;
        }
    }

    return rtc::PhysicalSocketServer::Wait(100, _io);
}
