/**
* @file ws/main.cpp
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#include <unistd.h>
#include <getopt.h>
#include <signal.h>

#include <iostream>

#include "json/confParser.h"
#include "logger/logger.h"
#include "wsServer.h"

static void usage(const char *_name) {
    std::cout  << _name << " [options]" << std::endl
               << "  Options:" << std::endl
               << "    -c, --conf-file <name>" << std::endl
               << "      Define configuration file location" << std::endl
               << "    -d, --daemon" << std::endl
               << "      Run as a background process (daemon)" << std::endl
               << "    -h, --help" << std::endl
               << "      Show usage information and exit" << std::endl;
}

static struct option longopts[] = {
        {"conf-file",   required_argument, nullptr, 'c'},
        {"daemon",      no_argument,       nullptr, 'd'},
        {"help",        no_argument,       nullptr, 'h'},
        {nullptr,       0,                 nullptr, 0}
};

static const char *pidFileName = "/var/run/tgwss.pid";

int main(int argc, char *argv[]) {
    bool daemonize = false;
    try {
        std::string confFile;

        int ch;
        while ((ch = getopt_long(argc, argv, "c:dh", longopts, nullptr)) != -1) {
            switch (ch) {
                case 'c':
                    confFile = optarg;
                    break;
                case 'd':
                    daemonize = true;
                    break;
                case ':':
                case '?':
                case 'h':
                    usage(argv[0]);
                    return EXIT_SUCCESS;
                default:
                    usage(argv[0]);
                    return EXIT_FAILURE;
            }
        }

        if (confFile.empty()) {
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        auto confParser = &tgwss::confParser_t::confParser();
        confParser->init(confFile);

        if (daemonize) {
            if (confParser->logDst() == "console") {
                std::cerr << "Logging to console unavailable in daemon mode." << std::endl;
                return EXIT_FAILURE;
            }
            switch (fork()) {
                case 0:
                    break;
                case -1:
                    return EXIT_FAILURE;
                default:
                    return EXIT_SUCCESS;
            }

            if (setsid() < 0) {
                return EXIT_FAILURE;
            }

            switch (fork()) {
                case 0:
                    break;
                case -1:
                    return EXIT_FAILURE;
                default:
                    return EXIT_SUCCESS;
            }

            if (chdir("/") != 0) {
                return EXIT_FAILURE;
            }

            auto fd_l = sysconf(_SC_OPEN_MAX);
            if (fd_l < 0) {
                return EXIT_FAILURE;
            }

            for (long fd = 0; fd < fd_l; ++fd) {
                close(fd);
            }

            auto pid = getpid();
            std::ofstream ofs(pidFileName);
            ofs << pid;
            ofs.close();
        }

        // create logger insrance
        auto &logger = tgwss::logger_t::logger();
        logger.init("tgwss", confParser->logDst(), confParser->logLevel());

        { // run server
            sigset_t sigSet;
            if ((sigemptyset(&sigSet) != 0) ||
                (sigaddset(&sigSet, SIGINT) != 0) || //exit
                (sigaddset(&sigSet, SIGQUIT) != 0) || //exit
                (sigaddset(&sigSet, SIGTERM) != 0) || //exit
                (sigaddset(&sigSet, SIGHUP) != 0) || //reopen log file
                (sigprocmask(SIG_BLOCK, &sigSet, nullptr) != 0) ||
                (signal(SIGPIPE, SIG_IGN) == SIG_ERR)) { // ignore

                throw std::runtime_error("failed to set signal handlers");
            }

            tgwss::wsServer_t wsServer(confParser, &logger);
            wsServer.start();
            int sign = 0;
            while (true) {
                if (sigwait(&sigSet, &sign) != 0) {
                    throw std::runtime_error("sigwait failed");
                }

                switch (sign) {
                    case SIGINT:
                    case SIGQUIT:
                    case SIGTERM:
                        break;
                    case SIGHUP:
                        logger.reopen();
                        continue;
                    default:
                        continue;
                }
                wsServer.stop();
                break;
            }
        }

        if (daemonize) {
            unlink(pidFileName);
        }

        return EXIT_SUCCESS;
    } catch (const std::exception &_e) {
        std::cerr << _e.what() << std::endl;
    } catch (...) {
        std::cerr << "unknown error" << std::endl;
    }

    if (daemonize) {
        unlink(pidFileName);
    }

    return EXIT_FAILURE;
}
