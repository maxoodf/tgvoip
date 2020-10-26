/**
* @file tgvoip/webRTCPeer.h
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#ifndef TESTWEBRTC_WEBRTCPEER_H
#define TESTWEBRTC_WEBRTCPEER_H

#include <mutex>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <webrtc/api/peer_connection_interface.h>
#pragma GCC diagnostic pop

#include "TgVoip.h"

namespace webrtc {
    class TaskQueueFactory;
    class AudioDeviceModule;
}

class server_t;

class webRTCPeer_t: public webrtc::PeerConnectionObserver,
                    public webrtc::CreateSessionDescriptionObserver {
public:
    using cbAudioData_t = std::function<void(int16_t* data, size_t len)>;

    using cbHangup_t = std::function<void(void *_ctx)>;

    using cbSdpSessionDescription_t = std::function<bool(const std::string &_type,
                                                         const std::string &_sdpMsg,
                                                         void *_ctx)>;

    using cbIceCandidate_t = std::function<bool(const std::string &_sdpMID,
                                                int _sdpMLineIndex,
                                                const std::string &_sdpCandidate,
                                                void *_ctx)>;

    enum class peerState_t {
        UNINITIALIZED,
        REGISTERED,
        CALL_REQUESTED,
        INITIALIZING,
        INITIALIZED,
        CALL_HANGUP,
        STOPPING
    };

private:
    cbAudioData_t m_cbInAudioData = nullptr;
    cbAudioData_t m_cbOutAudioData = nullptr;
    TgVoipNetworkType m_netType = TgVoipNetworkType::Unknown;

    std::unique_ptr<server_t> m_server;
    std::unique_ptr<rtc::AutoSocketServerThread> m_thread;

    rtc::scoped_refptr<webrtc::PeerConnectionInterface> m_peerConnection;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> m_peerConnectionFactory;

    std::atomic<peerState_t> m_peerState {peerState_t::UNINITIALIZED};

    cbSdpSessionDescription_t m_cbSdpSessionDescription = nullptr;
    cbIceCandidate_t m_cbIceCandidate = nullptr;
    void *m_ctx = nullptr;

    bool m_caller = false;

    cbHangup_t m_cbHangup = nullptr;
    void *m_hangupCtx = nullptr;

    uint8_t m_bitRateKb = 0;
    uint16_t m_sampleRateHz = 0;
    uint8_t m_pTimeMs = 0;

public:
    webRTCPeer_t(cbAudioData_t _in, cbAudioData_t _out, TgVoipNetworkType _netType);
    ~webRTCPeer_t() override;

    bool init();
    void setHangupCallback(cbHangup_t _cbHangup, void *_ctx);
    void stop();

    peerState_t state() const {return m_peerState;}
    bool setState(peerState_t _state);

    static void onRegistered(cbSdpSessionDescription_t _cbSdpSessionDescription,
                             cbIceCandidate_t _cbIceCandidate,
                             void *_cbCtx,
                             void *_localCtx);
    static void onSignalingDisconnected(void *_ctx);
    static void onCall(bool _caller, void *_ctx);
    static void onHangup(void *_ctx);
    static bool onSDP(bool _isOffer, const std::string &_sdpMsg, void *_ctx);
    static bool onICE(const std::string &_sdpMid, int _sdpMLineIndex, const std::string &_candidate, void *_ctx);

// PeerConnectionObserver implementation.
    // Triggered when the SignalingState changed.
    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState ) override;

    // Triggered when media is received on a new stream from remote peer.
    void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface>) override;

    // Triggered when a remote peer closes a stream.
    void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface>) override;

    // Triggered when a remote peer opens a data channel.
    void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> ) override;

    // Triggered when renegotiation is needed. For example, an ICE restart
    // has begun.
    void OnRenegotiationNeeded() override;

    // Called any time the IceConnectionState changes.
    //
    // Note that our ICE states lag behind the standard slightly. The most
    // notable differences include the fact that "failed" occurs after 15
    // seconds, not 30, and this actually represents a combination ICE + DTLS
    // state, so it may be "failed" if DTLS fails while ICE succeeds.
    void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState ) override;

    // Called any time the IceGatheringState changes.
    void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState ) override;

    // A new ICE candidate has been gathered.
    void OnIceCandidate(const webrtc::IceCandidateInterface* ) override;

    // Ice candidates have been removed.
    // TODO(honghaiz): Make this a pure virtual method when all its subclasses
    // implement it.
    void OnIceCandidatesRemoved(const std::vector<cricket::Candidate>& ) override;

    // Called when the ICE connection receiving status changes.
    void OnIceConnectionReceivingChange(bool ) override;

    // This is called when a receiver and its track are created.
    // TODO(zhihuang): Make this pure virtual when all subclasses implement it.
    // Note: This is called with both Plan B and Unified Plan semantics. Unified
    // Plan users should prefer OnTrack, OnAddTrack is only called as backwards
    // compatibility (and is called in the exact same situations as OnTrack).
    void OnAddTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> ,
                    const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& ) override;

    // This is called when signaling indicates a transceiver will be receiving
    // media from the remote endpoint. This is fired during a call to
    // SetRemoteDescription. The receiving track can be accessed by:
    // |transceiver->receiver()->track()| and its associated streams by
    // |transceiver->receiver()->streams()|.
    // Note: This will only be called if Unified Plan semantics are specified.
    // This behavior is specified in section 2.2.8.2.5 of the "Set the
    // RTCSessionDescription" algorithm:
    // https://w3c.github.io/webrtc-pc/#set-description
    void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> ) override;

    // Called when signaling indicates that media will no longer be received on a
    // track.
    // With Plan B semantics, the given receiver will have been removed from the
    // PeerConnection and the track muted.
    // With Unified Plan semantics, the receiver will remain but the transceiver
    // will have changed direction to either sendonly or inactive.
    // https://w3c.github.io/webrtc-pc/#process-remote-track-removal
    // TODO(hbos,deadbeef): Make pure virtual when all subclasses implement it.
    void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> ) override;

    // Called when an interesting usage is detected by WebRTC.
    // An appropriate action is to add information about the context of the
    // PeerConnection and write the event to some kind of "interesting events"
    // log function.
    // The heuristics for defining what constitutes "interesting" are
    // implementation-defined.
    void OnInterestingUsage(int ) override;

// CreateSessionDescriptionObserver implementation.
    void OnSuccess(webrtc::SessionDescriptionInterface *_desc) override;

    void OnFailure(webrtc::RTCError ) override;

private:
    static void initLog();
};

#endif //TESTWEBRTC_WEBRTCPEER_H
