/**
* @file logger/logger.cp
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/


#ifndef TGWSS_LOGGER_H
#define TGWSS_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <queue>
#include <tuple>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <map>
#include <atomic>

#include "fmt/format.h"

namespace tgwss {
    class logger_t final {
    public:
        enum class logLevel_t {
            LL_CRITICAL,
            LL_ERROR,
            LL_WARNING,
            LL_NOTICE,
            LL_INFO,
            LL_DEBUG
        };

    private:
        enum class logDst_t {
            LD_SYSLOG,
            LD_CONSOLE,
            LD_FILE
        };
        const std::map<logLevel_t, std::string> m_logLevelStrs = {
                {logLevel_t::LL_CRITICAL, "CRITICAL"},
                {logLevel_t::LL_ERROR,    "ERROR"},
                {logLevel_t::LL_WARNING,  "WARNING"},
                {logLevel_t::LL_NOTICE,   "NOTICE"},
                {logLevel_t::LL_INFO,     "INFO"},
                {logLevel_t::LL_DEBUG,    "DEBUG"}
        };
        std::string m_logPrefix;
        logDst_t m_logDst = logDst_t::LD_SYSLOG;
        logLevel_t m_logLevel = logLevel_t::LL_ERROR;
        std::ofstream m_ofs;
        std::string m_fileName;

        using logRecord_t = std::tuple<std::chrono::time_point<std::chrono::system_clock>, logLevel_t, std::string>;
        std::queue<logRecord_t> m_logQueue;
        std::mutex m_mtxLog;
        std::condition_variable m_cvLog;
        bool m_checkQueueFlag = false;

        std::thread m_workerThread;
        std::atomic<bool> m_workFlag;
        std::mutex m_mtxIO;

        std::map<std::thread::id, uint64_t> m_dispatchThreads;

    public:
        static logger_t &logger() {
            static logger_t logger;
            return logger;
        }

        logger_t(const logger_t &) = delete;
        void operator=(const logger_t &) = delete;
        logger_t(const logger_t &&) = delete;
        void operator=(const logger_t &&) = delete;

        ~logger_t();

        void init(const std::string &_logPrefix, const std::string &_logTo, const std::string &_logLevel);

        template<typename... args_t>
        void log(logLevel_t _logLevel, const std::string &_fmt, const args_t &..._args) noexcept {
            if (_logLevel > m_logLevel) {
                return;
            }

            auto timeStamp = std::chrono::system_clock::now();

            try {
                std::unique_lock<std::mutex> lck(m_mtxLog);

                if (m_dispatchThreads.find(std::this_thread::get_id()) == m_dispatchThreads.end()) {
                    m_dispatchThreads[std::this_thread::get_id()] = m_dispatchThreads.size();
                }

                std::string buffer = "[" + m_logPrefix + "] "
                                     + ((m_logLevel == logLevel_t::LL_DEBUG) ?
                                        "[THR#"
                                        + std::to_string(m_dispatchThreads[std::this_thread::get_id()])
                                        + "] " : "") + "[" + m_logLevelStrs.at(_logLevel) + "] " + _fmt;

                m_logQueue.push(logRecord_t(timeStamp, _logLevel, fmt::format(buffer, _args...)));
                m_checkQueueFlag = true;
                m_cvLog.notify_one();
            } catch (...) {}
        }

        void reopen() noexcept;

    private:
        logger_t() : m_workFlag(true) {
            m_workerThread = std::thread(&logger_t::worker, this);
        }

        inline int levelMapper(logLevel_t) const noexcept;

        void worker() noexcept;
    };
} // namespace tgwss

#endif // TGWSS_LOGGER_H
