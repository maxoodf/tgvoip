/**
* @file tgvoip/webRTCPeer.cpp
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <webrtc/rtc_base/logging.h>
#include <webrtc/rtc_base/thread.h>
#include <webrtc/api/task_queue/default_task_queue_factory.h>
#include <webrtc/api/audio_codecs/audio_encoder_factory_template.h>
#include <webrtc/api/audio_codecs/audio_decoder_factory_template.h>
#include <webrtc/api/audio_codecs/opus/audio_decoder_opus.h>
#include <webrtc/api/audio_codecs/opus/audio_encoder_opus.h>
#include <webrtc/api/call/call_factory_interface.h>
#include <webrtc/api/peer_connection_interface.h>
#include <webrtc/api/rtc_event_log/rtc_event_log_factory.h>
#include <webrtc/api/scoped_refptr.h>
#include <webrtc/media/base/media_engine.h>
#include <webrtc/media/engine/webrtc_media_engine.h>
#include <webrtc/modules/audio_device/include/audio_device.h>
#include <webrtc/modules/audio_processing/include/audio_processing.h>
#pragma GCC diagnostic pop

#include "server.h"
#include "sessionDescriptionObserver.h"
#include "fileAudioDeviceModule.h"
#include "webRTCPeer.h"

// bit rate (Mb), sampling rate (Hz), frame size (ms)
static const uint8_t opusSettings[6][3] = {
        {56, 48, 20}, //Ethernet
        {56, 48, 40},  // WiFi
        {32, 28, 60},  // 4G mobile
        {18, 16, 120}, // 3G mobile
        {14, 12, 120}, // 2G+ mobile
        {10, 10, 100}  // 2G mobile
};

webRTCPeer_t::webRTCPeer_t(cbAudioData_t _in,
                           cbAudioData_t _out,
                           TgVoipNetworkType _netType): webrtc::PeerConnectionObserver(),
                                                        webrtc::CreateSessionDescriptionObserver(),
                                                        m_cbInAudioData(std::move(_in)),
                                                        m_cbOutAudioData(std::move(_out)),
                                                        m_netType(_netType) {
//#ifndef NDEBUG
    initLog();
//#endif
    m_server = std::make_unique<server_t>(this);
    m_thread = std::make_unique<rtc::AutoSocketServerThread>(m_server.get());
    m_thread->Start();
}

webRTCPeer_t::~webRTCPeer_t() {
    RTC_LOG(INFO) << "webRTCPeer: destroying...";
    if (m_peerConnection) {
        m_peerConnection = nullptr;
    }
    m_peerConnectionFactory = nullptr;
    // make sure stop() call is finished
//    std::unique_lock<std::mutex> lck(m_stopMtx);
    if (m_thread) {
//        m_thread->Stop();
    }
    RTC_LOG(INFO) << "webRTCPeer: destroyed";
}

void webRTCPeer_t::initLog() {
    rtc::LogMessage::LogThreads(true);
    rtc::LogMessage::LogTimestamps(true);
    rtc::LogMessage::LogToDebug(rtc::LS_INFO);
}

void webRTCPeer_t::setHangupCallback(cbHangup_t _cbHangup, void *_ctx) {
    m_cbHangup = std::move(_cbHangup);
    m_hangupCtx = _ctx;
}

bool webRTCPeer_t::init() {
    if (!setState(peerState_t::INITIALIZING)) {
        return false;
    }

    RTC_LOG(INFO) << "webRTCPeer: creating peer connection factory...";

    webrtc::PeerConnectionFactoryDependencies factoryDependencies;
    factoryDependencies.network_thread = nullptr;
    factoryDependencies.worker_thread = nullptr;
    factoryDependencies.signaling_thread = nullptr;
//    factoryDependencies.signaling_thread = m_thread.get();

    factoryDependencies.task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
    factoryDependencies.call_factory = webrtc::CreateCallFactory();
    factoryDependencies.event_log_factory = std::make_unique<webrtc::RtcEventLogFactory>(
            factoryDependencies.task_queue_factory.get());

    cricket::MediaEngineDependencies media_dependencies;
    media_dependencies.task_queue_factory = factoryDependencies.task_queue_factory.get();

    rtc::scoped_refptr<webrtc::AudioDeviceModule> audioDeviceModule;
    audioDeviceModule = fileAudioDeviceModule_t::Create(m_cbInAudioData,
                                                        m_cbOutAudioData,
                                                        webRTCPeer_t::onHangup,
                                                        this,
                                                        factoryDependencies.task_queue_factory.get());
    audioDeviceModule->SetStereoPlayout(false);
    audioDeviceModule->SetStereoRecording(false);
    media_dependencies.adm = std::move(audioDeviceModule);

//    webrtc::AudioEncoderOpusConfig oc;

    media_dependencies.audio_encoder_factory = webrtc::CreateAudioEncoderFactory<webrtc::AudioEncoderOpus>();
    media_dependencies.audio_decoder_factory = webrtc::CreateAudioDecoderFactory<webrtc::AudioDecoderOpus>();
/*
    media_dependencies.audio_encoder_factory = webrtc::CreateAudioEncoderFactory<
            webrtc::AudioEncoderOpus, webrtc::AudioEncoderG722, webrtc::AudioEncoderG711
            >();
    media_dependencies.audio_decoder_factory = webrtc::CreateAudioDecoderFactory<
            webrtc::AudioDecoderOpus, webrtc::AudioDecoderG722, webrtc::AudioDecoderG711
            >();
*/
    media_dependencies.audio_processing = webrtc::AudioProcessingBuilder().Create();

    media_dependencies.audio_mixer = nullptr;
    media_dependencies.video_encoder_factory = nullptr;
    media_dependencies.video_decoder_factory = nullptr;
    factoryDependencies.media_engine = cricket::CreateMediaEngine(std::move(media_dependencies));

    m_peerConnectionFactory = CreateModularPeerConnectionFactory(std::move(factoryDependencies));

    if (!m_peerConnectionFactory) {
        RTC_LOG(INFO) << "webRTCPeer: failed to creat peer connection factory";
        return false;
    }

    RTC_LOG(INFO) << "webRTCPeer: creating RTC config...";

    webrtc::PeerConnectionInterface::RTCConfiguration RTCConfig;
    RTCConfig.disable_ipv6 = true;
    RTCConfig.set_suspend_below_min_bitrate(false);
    RTCConfig.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    RTCConfig.enable_dtls_srtp = true;

    switch (m_netType) {
        case TgVoipNetworkType::Ethernet: {
            RTCConfig.network_preference = rtc::AdapterType::ADAPTER_TYPE_ETHERNET;
            m_bitRateKb = opusSettings[0][0];
            m_sampleRateHz = opusSettings[0][1] * 1000;
            m_pTimeMs = opusSettings[0][2];
            break;
        }
        case TgVoipNetworkType::WiFi: {
            RTCConfig.network_preference = rtc::AdapterType::ADAPTER_TYPE_WIFI;
            m_bitRateKb = opusSettings[1][0];
            m_sampleRateHz = opusSettings[1][1] * 1000;
            m_pTimeMs = opusSettings[1][2];
            break;
        }
        case TgVoipNetworkType::Hspa:
        case TgVoipNetworkType::Lte:
        case TgVoipNetworkType::OtherHighSpeed: {
            RTCConfig.network_preference = rtc::AdapterType::ADAPTER_TYPE_CELLULAR;
            m_bitRateKb = opusSettings[2][0];
            m_sampleRateHz = opusSettings[2][1] * 1000;
            m_pTimeMs = opusSettings[2][2];
            break;
        }
        case TgVoipNetworkType::ThirdGeneration: {
            RTCConfig.network_preference = rtc::AdapterType::ADAPTER_TYPE_CELLULAR;
            m_bitRateKb = opusSettings[3][0];
            m_sampleRateHz = opusSettings[3][1] * 1000;
            m_pTimeMs = opusSettings[3][2];
            break;
        }
        case TgVoipNetworkType::Edge: {
            RTCConfig.network_preference = rtc::AdapterType::ADAPTER_TYPE_CELLULAR;
            m_bitRateKb = opusSettings[4][0];
            m_sampleRateHz = opusSettings[4][1] * 1000;
            m_pTimeMs = opusSettings[4][2];
            break;
        }
        case TgVoipNetworkType::Gprs:
        case TgVoipNetworkType::OtherMobile:
        case TgVoipNetworkType::Dialup:
        case TgVoipNetworkType::OtherLowSpeed: {
            RTCConfig.network_preference = rtc::AdapterType::ADAPTER_TYPE_CELLULAR;
            m_bitRateKb = opusSettings[5][0];
            m_sampleRateHz = opusSettings[5][1] * 1000;
            m_pTimeMs = opusSettings[5][2];
            break;
        }
        case TgVoipNetworkType::Unknown:
        default: {
            RTCConfig.network_preference = rtc::AdapterType::ADAPTER_TYPE_UNKNOWN;
            m_bitRateKb = opusSettings[3][0];
            m_sampleRateHz = opusSettings[3][1] * 1000;
            m_pTimeMs = opusSettings[3][2];
            break;
        }
    }

    webrtc::PeerConnectionInterface::IceServer server;
    server.uri = "turn:x.x.x.x:3478";
    server.username = "username";
    server.password = "password";
    RTCConfig.disable_link_local_networks = true;
    RTCConfig.servers.push_back(server);

    RTC_LOG(INFO) << "webRTCPeer: creating peer connection...";
    m_peerConnection = m_peerConnectionFactory->CreatePeerConnection(RTCConfig,
                                                                     webrtc::PeerConnectionDependencies(this));
//    m_peerConnection = m_peerConnectionFactory->CreatePeerConnection(RTCConfig, nullptr, nullptr, this);

    if (!m_peerConnection) {
        RTC_LOG(INFO) << "webRTCPeer: failed to initialize peer connection";
        return false;
    }

    webrtc::PeerConnectionInterface::BitrateParameters bitrateParam;
    bitrateParam.min_bitrate_bps = absl::optional<int>((m_bitRateKb > 12)?(m_bitRateKb * 1024 / 2):(6 * 1024));
    bitrateParam.current_bitrate_bps = absl::optional<int>(m_bitRateKb * 1024);
    bitrateParam.max_bitrate_bps = absl::optional<int>(m_bitRateKb * 1024);
    auto brpRet = m_peerConnection->SetBitrate(bitrateParam);
    if (!brpRet.ok()) {
        RTC_LOG(LS_ERROR) << "webRTCPeer: failed to set bitrate: "<< brpRet.message();
        return false;
    }

    cricket::AudioOptions ao;
/* default options
        ao.echo_cancellation = true;
        ao.auto_gain_control = true;
        ao.noise_suppression = true;
        ao.highpass_filter = true;
        ao.stereo_swapping = false;
        ao.audio_jitter_buffer_max_packets = 200;
        ao.audio_jitter_buffer_fast_accelerate = false;
        ao.audio_jitter_buffer_min_delay_ms = 0;
        ao.audio_jitter_buffer_enable_rtx_handling = false;
        ao.typing_detection = true;
        ao.experimental_agc = false;
        ao.experimental_ns = false;
        ao.residual_echo_detector = true;
*/

/* custom options
*/
    ao.audio_jitter_buffer_min_delay_ms = 0;
    ao.audio_jitter_buffer_max_packets = 100;
    ao.audio_jitter_buffer_enable_rtx_handling = true;
    ao.audio_jitter_buffer_fast_accelerate = false;
    ao.audio_network_adaptor = true;
//        ao.audio_network_adaptor_config = ""

    ao.echo_cancellation = false;
    ao.noise_suppression = false;
    ao.auto_gain_control = false;
    ao.highpass_filter = false;
    ao.experimental_agc = false;
    ao.experimental_ns  = false;
    ao.residual_echo_detector = false;
    ao.typing_detection = false;

    rtc::scoped_refptr<webrtc::AudioTrackInterface> audioTrack(
            m_peerConnectionFactory->CreateAudioTrack(
                    "audioStream", m_peerConnectionFactory->CreateAudioSource(ao)));
    auto atRet = m_peerConnection->AddTrack(audioTrack, {"audioStream"});
    if (!atRet.ok()) {
        RTC_LOG(LS_ERROR) << "webRTCPeer: failed to add audio track to PeerConnection: "
                          << atRet.error().message();
        return false;
    }
/*
    auto p = ret.value()->GetParameters();
    for (auto &e:p.encodings) {
        e.max_bitrate_bps = 16 * 1024;
        e.min_bitrate_bps = 8 * 1024;
    }
    auto r = ret.value()->SetParameters(p);
    if (!r.ok()) {
        RTC_LOG(LS_ERROR) << "webRTCPeer: failed to set bitrate: " << r.message();
        return false;
    }
*/
    RTC_LOG(INFO) << "webRTCPeer: peer connection created";

    if (m_caller) {
        m_peerConnection->CreateOffer(this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
        RTC_LOG(INFO) << "webRTCPeer: offer created";
    }

    setState(peerState_t::INITIALIZED);

    return true;
}

#include <thread>
#include <chrono>
void webRTCPeer_t::stop() {
    if (!setState(peerState_t::STOPPING)) {
        return;
    }
    RTC_LOG(INFO) << "webRTCPeer: stopping...";

    if (m_caller) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }

    if (m_cbHangup) {
        m_cbHangup(m_hangupCtx);
    }

/*
    std::unique_lock<std::mutex> lck(m_stopMtx);
    if (m_peerConnection) {
        m_peerConnection = nullptr;
    }
    m_peerConnectionFactory = nullptr;
*/
    RTC_LOG(INFO) << "webRTCPeer: stopped";
}

bool webRTCPeer_t::setState(peerState_t _state) {
    switch (_state) {
        case peerState_t::INITIALIZING: {
            if (m_peerState != peerState_t::CALL_REQUESTED) {
                return false;
            }
            break;
        }
        case peerState_t::CALL_HANGUP:
        case peerState_t::STOPPING: {
            if (m_peerState == peerState_t::STOPPING) {
                return false;
            }
            break;
        }
        default: {
            break;
        }
    }

    m_peerState = _state;
    return true;
}

void webRTCPeer_t::onRegistered(cbSdpSessionDescription_t _cbSdpSessionDescription,
                                cbIceCandidate_t _cbIceCandidate,
                                void *_cbCtx,
                                void *_localCtx) {
    auto peer = reinterpret_cast<webRTCPeer_t *>(_localCtx);
    peer->m_cbSdpSessionDescription = std::move(_cbSdpSessionDescription);
    peer->m_cbIceCandidate = std::move(_cbIceCandidate);
    peer->m_ctx = _cbCtx;
    peer->setState(peerState_t::REGISTERED);
}

void webRTCPeer_t::onSignalingDisconnected(void *_ctx) {
    auto peer = reinterpret_cast<webRTCPeer_t *>(_ctx);

    peer->setState(peerState_t::CALL_HANGUP);
    RTC_LOG(INFO) << "onSignalingDisconnected: triggered";

/*
    if (peer->state() != peerState_t::INITIALIZED) {
        peer->setState(peerState_t::CALL_HANGUP);
        RTC_LOG(INFO) << "onSignalingDisconnected: triggered";
    } else {
        RTC_LOG(INFO) << "onSignalingDisconnected: triggered, but ignored (call in proccess)";
    }
*/
}

void webRTCPeer_t::onCall(bool _caller, void *_ctx) {
    auto peer = reinterpret_cast<webRTCPeer_t *>(_ctx);
    peer->m_caller = _caller;
    peer->setState(peerState_t::CALL_REQUESTED);
}

void webRTCPeer_t::onHangup(void *_ctx) {
    auto peer = reinterpret_cast<webRTCPeer_t *>(_ctx);
    peer->setState(peerState_t::CALL_HANGUP);
    RTC_LOG(INFO) << "onHangup: triggered";
}

bool webRTCPeer_t::onSDP(bool _isOffer, const std::string &_sdpMsg, void *_ctx) {
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> sessionDescription =
            webrtc::CreateSessionDescription(_isOffer?webrtc::SdpType::kOffer:webrtc::SdpType::kAnswer,
                                             _sdpMsg,
                                             &error);
    if (!sessionDescription) {
        RTC_LOG(INFO) << "onSDP: SdpParseError - " << error.description;
        return false;
    }

    auto peer = reinterpret_cast<webRTCPeer_t *>(_ctx);
    if (!peer->m_peerConnection) {
        return false;
    }
    peer->m_peerConnection->SetRemoteDescription(
            setSessionDescriptionObserver_t::Create(),
            sessionDescription.release());

    if (_isOffer) {
        peer->m_peerConnection->CreateAnswer(peer, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    }

    return true;
}

bool webRTCPeer_t::onICE(const std::string &_sdpMid, int _sdpMLineIndex, const std::string &_candidate, void *_ctx) {
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::IceCandidateInterface> iceCandidate(
            webrtc::CreateIceCandidate(_sdpMid, _sdpMLineIndex, _candidate, &error));
    if (!iceCandidate) {
        RTC_LOG(INFO) << "onICE: SdpParseError - " << error.description;
        return false;
    }

    if (iceCandidate->candidate().type() != "relay") {
        RTC_LOG(INFO) << "onICE: candidate ignored, not a relay";
        return true;
    }

    auto peer = reinterpret_cast<webRTCPeer_t *>(_ctx);
    if (!peer->m_peerConnection || !peer->m_peerConnection->AddIceCandidate(iceCandidate.get())) {
        RTC_LOG(INFO) << "onICE: failed to apply ICE candidate";
        return false;
    }
    RTC_LOG(INFO) << "onICE: ICE candidate added";

    return true;
}

// PeerConnectionObserver implementation.
// Triggered when the SignalingState changed.
void webRTCPeer_t::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState _signalingState) {
    RTC_LOG(INFO) << "webRTCPeer: signaling connection state changed";
    if (_signalingState == webrtc::PeerConnectionInterface::SignalingState::kClosed) {
        RTC_LOG(INFO) << "webRTCPeer: signaling server connection closed";
        setState(peerState_t::CALL_HANGUP);
    }
// must be implemented
}

// Triggered when media is received on a new stream from remote peer.
void webRTCPeer_t::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> ) {
    RTC_LOG(INFO) << "webRTCPeer: media received on a new stream from remote peer";
}

// Triggered when a remote peer closes a stream.
void webRTCPeer_t::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface>) {
    RTC_LOG(INFO) << "webRTCPeer: stream is closed by remote peer";
}

// Triggered when a remote peer opens a data channel.
void webRTCPeer_t::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface>) {
    RTC_LOG(INFO) << "webRTCPeer: data channel opened by remote peer";
// must be implemented
}

// Triggered when renegotiation is needed. For example, an ICE restart
// has begun.
void webRTCPeer_t::OnRenegotiationNeeded() {
    RTC_LOG(INFO) << "webRTCPeer: renegotiation is needed";
// must be implemented
}

// Called any time the IceConnectionState changes.
//
// Note that our ICE states lag behind the standard slightly. The most
// notable differences include the fact that "failed" occurs after 15
// seconds, not 30, and this actually represents a combination ICE + DTLS
// state, so it may be "failed" if DTLS fails while ICE succeeds.
void webRTCPeer_t::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState _iceConnectionState) {
    RTC_LOG(INFO) << "webRTCPeer: IceConnectionState changed: " << _iceConnectionState;

    switch (_iceConnectionState) {
        case webrtc::PeerConnectionInterface::kIceConnectionConnected: {
            break;
        }
        case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
        case webrtc::PeerConnectionInterface::kIceConnectionFailed:
        case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
        case webrtc::PeerConnectionInterface::kIceConnectionClosed: {
            RTC_LOG(INFO) << "webRTCPeer: ICE connection closed";
            setState(peerState_t::CALL_HANGUP);
            break;
        }
        default: {
            break;
        }
    }
// must be implemented
}

// Called any time the IceGatheringState changes.
void webRTCPeer_t::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState ) {
    RTC_LOG(INFO) << "webRTCPeer: IceGatheringState is changed";
// must be implemented
}

// A new ICE candidate has been gathered.
void webRTCPeer_t::OnIceCandidate(const webrtc::IceCandidateInterface *_candidate) {
// must be implemented
    RTC_LOG(INFO) << "webRTCPeer: a new ICE candidate has been gathered";

    std::string sdp;
    if (!_candidate->ToString(&sdp)) {
        RTC_LOG(INFO) << "OnIceCandidate: Failed to serialize ICE candidate";
        setState(peerState_t::CALL_HANGUP);
        return;
    }
    m_cbIceCandidate(_candidate->sdp_mid(), _candidate->sdp_mline_index(), sdp, m_ctx);
}

// Ice candidates have been removed.
// TODO(honghaiz): Make this a pure virtual method when all its subclasses
// implement it.
void webRTCPeer_t::OnIceCandidatesRemoved(const std::vector<cricket::Candidate>& ) {
    RTC_LOG(INFO) << "webRTCPeer: Ice candidates have been removed";
}

// Called when the ICE connection receiving status changes.
void webRTCPeer_t::OnIceConnectionReceivingChange(bool ) {
    RTC_LOG(INFO) << "webRTCPeer: ICE connection receiving status changes";
}

// This is called when a receiver and its track are created.
// TODO(zhihuang): Make this pure virtual when all subclasses implement it.
// Note: This is called with both Plan B and Unified Plan semantics. Unified
// Plan users should prefer OnTrack, OnAddTrack is only called as backwards
// compatibility (and is called in the exact same situations as OnTrack).
void webRTCPeer_t::OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> ,
                              const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>> &) {
    RTC_LOG(INFO) << "webRTCPeer: receiver and its track are created";
/*
    if (_receiver->track()->kind() == webrtc::MediaStreamTrackInterface::kAudioKind) {
        auto *track = static_cast<webrtc::AudioTrackInterface *>(_receiver->track().get());
    }
*/
}

// This is called when signaling indicates a transceiver will be receiving
// media from the remote endpoint. This is fired during a call to
// SetRemoteDescription. The receiving track can be accessed by:
// |transceiver->receiver()->track()| and its associated streams by
// |transceiver->receiver()->streams()|.
// Note: This will only be called if Unified Plan semantics are specified.
// This behavior is specified in section 2.2.8.2.5 of the "Set the
// RTCSessionDescription" algorithm:
// https://w3c.github.io/webrtc-pc/#set-description
void webRTCPeer_t::OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> ) {
    RTC_LOG(INFO) << "remote track added";
}

// Called when signaling indicates that media will no longer be received on a
// track.
// With Plan B semantics, the given receiver will have been removed from the
// PeerConnection and the track muted.
// With Unified Plan semantics, the receiver will remain but the transceiver
// will have changed direction to either sendonly or inactive.
// https://w3c.github.io/webrtc-pc/#process-remote-track-removal
// TODO(hbos,deadbeef): Make pure virtual when all subclasses implement it.
void webRTCPeer_t::OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> ) {
    RTC_LOG(INFO) << "webRTCPeer: remote track removed";
}

// Called when an interesting usage is detected by WebRTC.
// An appropriate action is to add information about the context of the
// PeerConnection and write the event to some kind of "interesting events"
// log function.
// The heuristics for defining what constitutes "interesting" are
// implementation-defined.
void webRTCPeer_t::OnInterestingUsage(int ) {
    RTC_LOG(INFO) << "webRTCPeer: an interesting usage is detected by WebRTC";
}

// CreateSessionDescriptionObserver implementation.
void webRTCPeer_t::OnSuccess(webrtc::SessionDescriptionInterface *_desc) {
    RTC_LOG(INFO) << "webRTCPeer: SDP message received";
    m_peerConnection->SetLocalDescription(setSessionDescriptionObserver_t::Create(), _desc);

    std::string type;
    if (_desc->GetType() == webrtc::SdpType::kAnswer) {
        type = "answer";
    } else {
        type = "offer";
    }

    std::string sdp;
    if (!_desc->ToString(&sdp)) {
        RTC_LOG(INFO) << "webRTCPeer: failed to serialize SDP message";
        setState(peerState_t::CALL_HANGUP);
        return;
    }

    // set SRTP bitrate and packet size
    auto pTimeStr = std::to_string(m_pTimeMs);
    std::string mediaStr = "m=audio 9 UDP/TLS/RTP/SAVPF 111 110";
    auto p = sdp.find(mediaStr);
    if (p != std::string::npos) {
        sdp.replace(p, mediaStr.length(),
                    mediaStr +
                    "\r\nb=AS:" + std::to_string(m_bitRateKb) +
                    "\r\na=ptime:" + pTimeStr +
                    "\r\na=maxptime:" + pTimeStr);
    }

    // disable stereo, set encoder and decoder bitrates
    std::string fmtpStr = "a=fmtp:111 minptime=10;useinbandfec=1";
    p = sdp.find(fmtpStr);
    if (p != std::string::npos) {
        auto bitRateStr = std::to_string(static_cast<int>(m_bitRateKb * 1024 / 1.05));
        auto sampleRateStr = std::to_string(m_sampleRateHz);

        sdp.replace(p, fmtpStr.length(),
                    std::string("a=fmtp:111 ;useinbandfec=1;minptime=" + pTimeStr) +
                    ";stereo=0;sprop-stereo=0" +
                    ";maxplaybackrate=" + sampleRateStr +
                    ";sprop-maxcapturerate=" + sampleRateStr +
                    ";maxaveragebitrate=" + bitRateStr);
    }

    m_cbSdpSessionDescription(type, sdp, m_ctx);
}

void webRTCPeer_t::OnFailure(webrtc::RTCError _error) {
    RTC_LOG(INFO) << "webRTCPeer: RTC error - " << _error.message();
    setState(peerState_t::CALL_HANGUP);
}
