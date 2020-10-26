/**
* @file wss/wsServer.h
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#ifndef TGWSS_WSSERVER_H
#define TGWSS_WSSERVER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <atomic>

#include <libwebsockets.h>
#include <libwebsockets/lws-network-helper.h>

namespace tgwss {
    class confParser_t;
    class logger_t;

    class wsServer_t {
    private:
        struct lws_protocols m_wsProtocol {};
        struct lws_context_creation_info m_wsInfo {};
        struct lws_context *m_wsContext = nullptr;

        logger_t *m_logger = nullptr;

        // basic client data
        struct peerData_t {
            lws_close_status closeStatus = LWS_CLOSE_STATUS_NO_STATUS;
            std::string token;
            std::vector<char> readBuf;
            std::queue<std::vector<unsigned char>> writeQueue;
            struct lws *subscriber = nullptr;

            explicit peerData_t(std::string _token): token(std::move(_token)) {}
        };

        // pairs of peer's lws & their data
        std::unordered_map<struct lws *, std::unique_ptr<peerData_t>> m_peers;

        std::atomic<bool> m_stopFlag {false};
        std::unique_ptr<std::thread> m_eventProcessingThread;

    public:
        wsServer_t(const confParser_t *_confParser, logger_t *_logger);
        ~wsServer_t();

        void start();
        void stop();

    private:
        static int wscbService(struct lws *_lws, enum lws_callback_reasons _reason,
                               void *_user, void *_data, size_t _size) noexcept;
        static void eventProcessingWorker(wsServer_t *_wsServer);

        bool write(struct lws *_lws,
                   const void *_message,
                   std::size_t _size,
                   lws_close_status _closeStatus = LWS_CLOSE_STATUS_NO_STATUS) noexcept;
        void closeWithErrMsg(struct lws *_lws, enum lws_close_status _status, const std::string &_errMsg) noexcept;
        bool logon(struct lws *_lws, const void *_data, std::size_t _size) noexcept;
        bool retransmit(struct lws *_lws, const void *_data, std::size_t _size) noexcept;
        void remove(struct lws *_lws) noexcept;
    };
} // namespace tgwss

#endif //TGWSS_WSSERVER_H
