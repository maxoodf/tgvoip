/**
* @file tgvoip/TgVoip.cpp
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif
#include <webrtc/rtc_base/ssl_adapter.h>
#include <webrtc/rtc_base/ref_counted_object.h>
#include <webrtc/base/memory/scoped_refptr.h>
#include <webrtc/rtc_base/logging.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#pragma GCC diagnostic pop

#include "tgvoip/wsClient/wsClient.h"
#include "tgvoip/webRTCPeer.h"
#include "TgVoip.h"

static webRTCPeer_t::cbAudioData_t preCB = nullptr;

void preprocessedCallback(const int16_t *_data, size_t _size) {
    if (preCB) {
        preCB(const_cast<int16_t *>(_data), _size);
    }
}

void TgVoip::setLoggingFunction(std::function<void(std::string const &)> ) {
}

void TgVoip::setGlobalServerConfig(const std::string &) {
}

class TgVoipImpl: public TgVoip {
private:
    std::function<void(TgVoipState)> onStateUpdated_;

    std::unique_ptr<wsClient_t> wsClient_;
    scoped_refptr<webRTCPeer_t> peer_;

public:
    TgVoipImpl(
            std::vector<TgVoipEndpoint> const &/*ep*/,
            TgVoipPersistentState const &,
            std::unique_ptr<TgVoipProxy> const &,
            TgVoipConfig const &/*cfg*/,
            TgVoipEncryptionKey const &ek,
            TgVoipNetworkType _netType
#ifdef TGVOIP_USE_CUSTOM_CRYPTO
    ,
        TgVoipCrypto const &crypto
#endif
#ifdef TGVOIP_USE_CALLBACK_AUDIO_IO
            ,
            TgVoipAudioDataCallbacks const &adc
#endif
    ) {
//#ifndef NDEBUG
        rtc::LogMessage::LogThreads(true);
        rtc::LogMessage::LogTimestamps(true);
        rtc::LogMessage::LogToDebug(rtc::LS_INFO);
//    rtc::LogMessage::LogToDebug(rtc::LS_NONE);
//#endif

        wsClient_ = std::make_unique<wsClient_t>(
                "127.0.0.1",
                8080,
                std::string(),
                true,
                10,
                std::string((ek.isOutgoing)?"caller":"callee") + "_123456789"
        );

        peer_ = new rtc::RefCountedObject<webRTCPeer_t>(adc.input, adc.output, _netType);
        preCB = adc.preprocessed;

        if (!wsClient_->start(webRTCPeer_t::onRegistered,
                              webRTCPeer_t::onCall,
                              webRTCPeer_t::onSDP,
                              webRTCPeer_t::onICE,
                              webRTCPeer_t::onSignalingDisconnected,
                              peer_.get())) {
            throw std::runtime_error("TgVoip::makeInstance: failed initialize WS connection");
        }

        rtc::InitializeSSL();

        if (ek.isOutgoing) {
//            wsClient_->callTo(std::string("callee") + "-" + std::to_string(*peerID1) + "-" + std::to_string(*peerID2));
            wsClient_->callTo("callee_123456789");
        }
    }

    ~TgVoipImpl() override {
        rtc::CleanupSSL();
    }

    static void onHangup(void *_ctx) {
        auto p = reinterpret_cast<TgVoipImpl *>(_ctx);
        p->onStateUpdated_(TgVoipState::Failed);
    }

    TgVoipFinalState stop() override {
        peer_ = nullptr;
        wsClient_ = nullptr;

        return TgVoipFinalState();
    }

    void setNetworkType(TgVoipNetworkType /*networkType*/) override {
    }

    void setMuteMicrophone(bool /*muteMicrophone*/) override {
    }

    void setAudioOutputGainControlEnabled(bool /*enabled*/) override {
    }

    void setEchoCancellationStrength(int /*strength*/) override {
    }

    std::string getLastError() override {
        return std::string{};
    }

    std::string getDebugInfo() override {
        return std::string{};
    }

    int64_t getPreferredRelayId() override {
        return 0;
    }

    TgVoipTrafficStats getTrafficStats() override {
        return TgVoipTrafficStats{};
    }

    TgVoipPersistentState getPersistentState() override {
        return TgVoipPersistentState{};
    }

    void setOnStateUpdated(std::function<void(TgVoipState)> onStateUpdated) override {
        onStateUpdated_ = std::move(onStateUpdated);
        peer_.get()->setHangupCallback(onHangup, this);
    }

    void setOnSignalBarsUpdated(std::function<void(int)> /*onSignalBarsUpdated*/) override {
    }
};

TgVoip::~TgVoip() {
}

TgVoip *TgVoip::makeInstance(
        TgVoipConfig const &config,
        TgVoipPersistentState const &persistentState,
        std::vector<TgVoipEndpoint> const &endpoints,
        std::unique_ptr<TgVoipProxy> const &proxy,
        TgVoipNetworkType initialNetworkType,
        TgVoipEncryptionKey const &encryptionKey
#ifdef TGVOIP_USE_CUSTOM_CRYPTO
,
    TgVoipCrypto const &crypto
#endif
#ifdef TGVOIP_USE_CALLBACK_AUDIO_IO
        ,
        TgVoipAudioDataCallbacks const &audioDataCallbacks
#endif
) {
    return new TgVoipImpl(
            endpoints,
            persistentState,
            proxy,
            config,
            encryptionKey,
            initialNetworkType
#ifdef TGVOIP_USE_CUSTOM_CRYPTO
    ,
        crypto
#endif
#ifdef TGVOIP_USE_CALLBACK_AUDIO_IO
            ,
            audioDataCallbacks
#endif
    );
}
