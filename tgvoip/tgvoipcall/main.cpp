#include <fstream>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <sys/time.h>

#ifdef __linux__
#include <sys/eventfd.h>
#endif

#include "tgvoip/TgVoip.h"

#ifdef __linux__
int fd_stop = 0;
#else
int fd_stop[2];
#endif

bool quiting = false;

void quit() {
    quiting = true;
    uint64_t val = 2;
#ifdef __linux__
    write(fd_stop, &val, sizeof(val));
#else
    write(fd_stop[1], &val, sizeof(val));
#endif
}

void wait_quit() {
    uint64_t val;
#ifdef __linux__
    read(fd_stop, &val, sizeof(val));
#else
    read(fd_stop[0], &val, sizeof(val));
#endif
}

namespace call {
TgVoip *_tgVoip = nullptr;
auto outgoing = std::fstream();
auto preprocessed = std::fstream();
auto inbound = std::fstream();
bool playing = true;
bool recorded = false;
bool failed = false;

uint64_t init_ts = 0;
uint64_t first_read_ts = 0;
uint64_t last_read_ts = 0; 
uint64_t first_write_ts = 0; 
uint64_t last_write_ts = 0;

uint64_t get_microseconds() {
  struct timeval tv;

  gettimeofday(&tv, NULL);
  return (uint64_t)(tv.tv_sec) * 1000000 + (uint64_t) tv.tv_usec;
} 

void callback_state_change(TgVoipState state) {
    switch (state) {
        case TgVoipState::WaitInit:
        case TgVoipState::WaitInitAck:
        case TgVoipState::Estabilished:
        case TgVoipState::Reconnecting:
            break;

        case TgVoipState::Failed:
            if (!recorded) {
              std::cerr << "Timeout while establishing the connection" << std::endl;
            }
            quit();
            break;

        default:
            std::cerr << "Unexpected new call state" << std::endl;
            quit();
            break;
    }
}

void play(int16_t* data, size_t len) {
    if (!playing || quiting)
        return;
    outgoing.read((char *) data, len * sizeof(int16_t));
    playing = !outgoing.fail();
    if (first_read_ts == 0) {
        first_read_ts = get_microseconds();
    }
    last_read_ts = get_microseconds();
    if (!playing) {
        wait_quit();
    }
}

void record(int16_t* data, size_t len) {
    if (quiting)
        return;
    recorded = true;
    inbound.write((char *)data, len * sizeof(int16_t));
    if (first_write_ts == 0) {
        first_write_ts = get_microseconds();
    }
    last_write_ts = get_microseconds();
}

void intermediate(int16_t* data, size_t len) {
    if (quiting)
        return;
    preprocessed.write((char *)data, len * sizeof(int16_t));
}

inline uint8_t letter_to_byte(char c) {
    if (c >= '0' && c <= '9') {
        return static_cast<uint8_t>(c - '0');
    } else {
        return static_cast<uint8_t>(10 + (c - 'a'));
    }
}

template <typename T>
void hex_to_char(const std::string& in, T* out) {
    for (size_t i = 0; i < in.size() / 2; ++i)
        out[i] = (letter_to_byte(in[i*2]) << 4) | letter_to_byte(in[i*2+1]);
}

void close_files() {
    if (outgoing.is_open())
        outgoing.close();
    if (preprocessed.is_open())
        preprocessed.close();
    if (inbound.is_open())
        inbound.close();
}

const char* usage() {
    return
    "Usage: tgvoipcall host:port tag [options]\n"
    "  host:port            Reflector IP address and port\n"
    "  tag hex              Participant's tag (16 bytes)\n"
    "\n"
    "Options:\n"
    " -k hex                Encryption key (256 bytes)\n"
    " -i file               Audio file (PCM) to be send the other side\n"
    " -o file               File (PCM) for recording the audio from the other side\n"
    " -p file               Preprocessed audio file (PCM) before sending to the other side\n"
    " -c config             Server configuration file (JSON)\n"
    " -r {caller|callee}    The role of the call participant\n"
    " -t type               Network type:\n"
    "                          0 - NET_TYPE_UNKNOWN\n"
    "                          1 - NET_TYPE_GPRS\n"
    "                          2 - NET_TYPE_EDGE\n"
    "                          3 - NET_TYPE_3G\n"
    "                          4 - NET_TYPE_HSPA\n"
    "                          5 - NET_TYPE_LTE\n"
    "                          6 - NET_TYPE_WIFI (default)\n"
    "                          7 - NET_TYPE_ETHERNET\n"
    "                          8 - NET_TYPE_OTHER_HIGH_SPEED\n"
    "                          9 - NET_TYPE_OTHER_LOW_SPEED\n"
    "                         10 - NET_TYPE_DIALUP\n"
    "                         11 - NET_TYPE_OTHER_MOBILE\n"
    " -s {never|always}     Data saving\n"
    " -n {no|yes}           Noise suspension\n"
    " -g {no|yes}           Automatic gain control\n";
}

void init(int argc, char **argv) {
    TgVoipEndpoint ep;
    ep.endpointId = 1;
    ep.type = TgVoipEndpointType::UdpRelay;

    if (argc < 3)
        throw std::invalid_argument(usage());

    char buf[16];
    if (sscanf(argv[1], "%15[0-9.]:%hu", buf, &ep.port) != 2)
        throw std::invalid_argument(std::string("Incorrect reflector address: ") + argv[1]);
    TgVoipEdpointHost host = {
            std::string(buf),
            std::string(),
    };
    ep.host = host;

    int len;
    sscanf(argv[2], "%*32[0-9a-f]%n", &len);
    if (len != 32)
        throw std::invalid_argument(std::string("Incorrect reflector tag: ") + argv[2]);
    hex_to_char(argv[2], ep.peerTag);

    int opt;
    char key[256];
    bool is_caller = true;
    std::string in, out, preproc;
    TgVoipDataSaving data_saving = TgVoipDataSaving::Never;
    bool enable_ns = false;
    bool enable_agc = false;
    TgVoipNetworkType netType = TgVoipNetworkType::WiFi;

    while ((opt = getopt(argc - 2, argv + 2, "k:i:o:p:c:r:t:s:n:g:")) != -1) {
        switch (opt) {
            case 'k':
                sscanf(optarg, "%*512[0-9a-f]%n", &len);
                if (len != 512)
                    throw std::invalid_argument(std::string("Incorrect encryption key: ") + optarg);
                hex_to_char(optarg, key);
                break;
            case 'i':
                in.assign(optarg);
                break;
            case 'o':
                out.assign(optarg);
                break;
            case 'p':
                preproc.assign(optarg);
                break;
            case 'c': {
                std::ifstream stream(optarg);
		std::string config_str((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
                TgVoip::setGlobalServerConfig(config_str);
                break;
            }
            case 'r':
                is_caller = (strcmp(optarg, "caller") == 0);
                break;
            case 't': {
                int network_type_i = std::stoi(optarg);
                if (network_type_i < 0 || network_type_i > 11)
                    throw std::invalid_argument(std::string("Incorrect network type: ") + optarg);

                switch (network_type_i) {
                  case 0:
                    netType = TgVoipNetworkType::Unknown;
                    break;
                  case 1:
                    netType = TgVoipNetworkType::Gprs;
                    break;
                  case 2:
                    netType = TgVoipNetworkType::Edge;
                    break;
                  case 3:
                    netType = TgVoipNetworkType::ThirdGeneration;
                    break;
                  case 4:
                    netType = TgVoipNetworkType::Hspa;
                    break;
                  case 5:
                    netType = TgVoipNetworkType::Lte;
                    break;
                  case 6:
                    netType = TgVoipNetworkType::WiFi;
                    break;
                  case 7:
                    netType = TgVoipNetworkType::Ethernet;
                    break;
                  case 8:
                    netType = TgVoipNetworkType::OtherHighSpeed;
                    break;
                  case 9:
                    netType = TgVoipNetworkType::OtherLowSpeed;
                    break;
                  case 10:
                    netType = TgVoipNetworkType::OtherMobile;
                    break;
                  case 11:
                    netType = TgVoipNetworkType::Dialup;
                    break;
                }
                break;
            }
            case 's':
                data_saving = (strcmp(optarg, "always") == 0) ? TgVoipDataSaving::Always : TgVoipDataSaving::Never;
                break;
            case 'n':
                enable_ns = (strcmp(optarg, "yes") == 0);
                break;
            case 'g':
                enable_agc = (strcmp(optarg, "yes") == 0);
                break;
            case '?':
                throw std::invalid_argument(usage());
            default:
                throw std::invalid_argument(std::string("Unknown option: ") + char(optopt));
        }
    }

    if (in.empty() || out.empty() || preproc.empty())
        throw std::invalid_argument("Unspecified input, output or preprocessed audio files");

    outgoing.open(in, std::ios::in | std::ios::binary);
    preprocessed.open(preproc, std::ios::out | std::ios::binary);
    inbound.open(out, std::ios::out | std::ios::binary);
    if (!outgoing || !preprocessed || !inbound)
        throw std::runtime_error("At least one of input, output or preprocessed audio files could not be opened");

/*
    TgVoipConfig config = {
      .initializationTimeout = 8,
      .receiveTimeout = 3,
      .dataSaving = data_saving,
      .enableP2P = false,
      .enableAEC = false,
      .enableNS = enable_ns,
      .enableAGC = enable_agc,
      .enableCallUpgrade = false,
      .logPath = "",
      .maxApiLayer = 92
    };
*/
    TgVoipConfig config = {
            8,
            3,
            data_saving,
            false,
            false,
            enable_ns,
            enable_agc,
            false,
            "",
            92
    };

    std::vector<uint8_t> derivedStateValue;

    std::vector<uint8_t> encryptionKeyValue = std::vector<unsigned char>(key, key + 256);;

/*
    TgVoipEncryptionKey encryptionKey = {
      .value = encryptionKeyValue,
      .isOutgoing = is_caller,
    };
*/
    TgVoipEncryptionKey encryptionKey = {
            encryptionKeyValue,
            is_caller,
    };

/*
    TgVoipAudioDataCallbacks audioCallbacks = {
      .input = play,
      .output = record,
      .preprocessed = intermediate,
    };
*/
    TgVoipAudioDataCallbacks audioCallbacks = {
            play,
            record,
            intermediate,
    };

    init_ts = get_microseconds();

    _tgVoip = TgVoip::makeInstance(
        config,
        { derivedStateValue },
        {ep},
        nullptr,
        netType,
        encryptionKey,
        audioCallbacks
    );

    _tgVoip->setOnStateUpdated(callback_state_change);
}

void stop() {
    if (_tgVoip) {
        TgVoipFinalState finalState = _tgVoip->stop();
        delete _tgVoip;
        _tgVoip = nullptr;

        if (recorded) {
          std::cout << finalState.debugLog << std::endl;
          std::cout << "TIMESTAMPS: " << init_ts << ","
                    << first_read_ts << "," << last_read_ts << ","
                    << first_write_ts << "," << last_write_ts << std::endl;
        }
    }
    close_files();
}

}

int main(int argc, char *argv[]) {
#ifdef __linux__
    fd_stop = eventfd(0, EFD_SEMAPHORE);
#else
    pipe(fd_stop);
#endif

    try {
        call::init(argc, argv);
    }
    catch (std::exception &err) {
        std::cerr << err.what() << std::endl;
#ifdef __linux__
        close(fd_stop);
#else
        close(fd_stop[0]);
        close(fd_stop[1]);
#endif
        return 1;
    }

    wait_quit();
    call::stop();

#ifdef __linux__
    close(fd_stop);
#else
    close(fd_stop[0]);
    close(fd_stop[1]);
#endif
    return call::recorded ? 0 : 1;
}

