/**
* @file logger/logger.cpp
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#include <syslog.h>

#include <iomanip>
#include <iostream>

#include "logger.h"

namespace tgwss {
    logger_t::~logger_t() {
        if (m_logDst == logDst_t::LD_SYSLOG) {
            closelog();
        } else if (m_logDst == logDst_t::LD_FILE) {
            m_ofs.close();
        }

        m_workFlag = false;
        {
            std::unique_lock<std::mutex> lck(m_mtxLog);
            m_checkQueueFlag = true;
            m_cvLog.notify_one();
        }
        m_workerThread.join();
    }

    void logger_t::init(const std::string &_logPrefix, const std::string &_logTo, const std::string &_logLevel) {
        m_logPrefix = _logPrefix;
        if (_logLevel == "critical") {
            m_logLevel = logLevel_t::LL_CRITICAL;
        } else if (_logLevel == "error") {
            m_logLevel = logLevel_t::LL_ERROR;
        } else if (_logLevel == "warning") {
            m_logLevel = logLevel_t::LL_WARNING;
        } else if (_logLevel == "notice") {
            m_logLevel = logLevel_t::LL_NOTICE;
        } else if (_logLevel == "info") {
            m_logLevel = logLevel_t::LL_INFO;
        } else if (_logLevel == "debug") {
            m_logLevel = logLevel_t::LL_DEBUG;
        } else {
            std::cout << _logLevel << std::endl;
            throw std::runtime_error("wrong logging level");
        }

        if (_logTo == "syslog") {
            m_logDst = logDst_t::LD_SYSLOG;
            openlog(m_logPrefix.c_str(), 0, 0);
        } else if (_logTo == "console") {
            m_logDst = logDst_t::LD_CONSOLE;
        } else if (_logTo.length() > 0) {
//            m_ofs.open(_logTo, std::ios_base::out | std::ios_base::app);
            m_ofs.open(_logTo, std::ofstream::out | std::ofstream::ate | std::ofstream::app);
            if (!m_ofs.is_open()) {
                throw std::runtime_error("failed to open " + _logTo);
            }
            m_fileName = _logTo;
            m_logDst = logDst_t::LD_FILE;
        } else {
            throw std::runtime_error("wrong logging destination");
        }
    }

    int logger_t::levelMapper(logLevel_t _logLevel) const noexcept {
        switch (_logLevel) {
            case logLevel_t::LL_CRITICAL:
                return LOG_CRIT;
            case logLevel_t::LL_ERROR:
                return LOG_ERR;
            case logLevel_t::LL_WARNING:
                return LOG_WARNING;
            case logLevel_t::LL_NOTICE:
                return LOG_NOTICE;
            case logLevel_t::LL_INFO:
                return LOG_INFO;
            case logLevel_t::LL_DEBUG:
                return LOG_DEBUG;
        }

        return LOG_DEBUG;
    }


    void logger_t::worker() noexcept {
        while (m_workFlag) {
            logRecord_t logRecord;
            {
                std::unique_lock<std::mutex> lck(m_mtxLog);
                while (!m_checkQueueFlag) {
                    m_cvLog.wait(lck);
                }

                if (!m_logQueue.empty()) {
                    try {
                        logRecord = m_logQueue.front();
                        m_logQueue.pop();
                    } catch (...) {
                        continue;
                    }
                } else {
                    m_checkQueueFlag = false;
                    continue;
                }
                if (m_logQueue.empty()) {
                    m_checkQueueFlag = false;
                }
            }
            try {
                auto recTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::get<0>(logRecord).time_since_epoch()).count();

                auto recTimeInSec = static_cast<long>(recTime / 1000);
                auto restMS = static_cast<uint16_t>(recTime % 1000);

                struct tm timeInfo{};
                localtime_r(&recTimeInSec, &timeInfo);

                if (m_logDst == logDst_t::LD_SYSLOG) {
                    syslog(levelMapper(std::get<1>(logRecord)), "%s", std::get<2>(logRecord).c_str());
                } else if (m_logDst == logDst_t::LD_CONSOLE) {
                    std::unique_lock<std::mutex> lck(m_mtxIO);
                    std::cout <<
                              1900 + timeInfo.tm_year << "." <<
                              std::setfill('0') << std::setw(2) << timeInfo.tm_mon + 1 << "." <<
                              std::setfill('0') << std::setw(2) << timeInfo.tm_mday << " " <<
                              std::setfill('0') << std::setw(2) << timeInfo.tm_hour << ":" <<
                              std::setfill('0') << std::setw(2) << timeInfo.tm_min << ":" <<
                              std::setfill('0') << std::setw(2) << timeInfo.tm_sec << "." <<
                              std::setfill('0') << std::setw(3) << restMS << ": " <<
                              std::get<2>(logRecord) <<
                              std::endl << std::flush;
                } else if (m_logDst == logDst_t::LD_FILE) {
                    std::unique_lock<std::mutex> lck(m_mtxIO);
                    m_ofs <<
                          1900 + timeInfo.tm_year << "." <<
                          std::setfill('0') << std::setw(2) << timeInfo.tm_mon + 1 << "." <<
                          std::setfill('0') << std::setw(2) << timeInfo.tm_mday << " " <<
                          std::setfill('0') << std::setw(2) << timeInfo.tm_hour << ":" <<
                          std::setfill('0') << std::setw(2) << timeInfo.tm_min << ":" <<
                          std::setfill('0') << std::setw(2) << timeInfo.tm_sec << "." <<
                          std::setfill('0') << std::setw(3) << restMS << ": " <<
                          std::get<2>(logRecord) <<
                          std::endl << std::flush;
                }
            } catch (...) {}
        }
    }

    void logger_t::reopen() noexcept {
        if (m_logDst == logDst_t::LD_FILE) {
            try {
                std::unique_lock<std::mutex> lck(m_mtxIO);
                m_ofs.close();
                m_ofs.open(m_fileName, std::ofstream::out | std::ofstream::ate | std::ofstream::app);
            } catch (...) {}
        }
    }
} // namespace tgwss
