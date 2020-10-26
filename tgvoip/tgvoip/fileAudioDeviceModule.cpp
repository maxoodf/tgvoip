/**
* @file tgvoip/fileAudioDeviceModule.cpp
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

//#include <stddef.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <webrtc/api/scoped_refptr.h>
#include <webrtc/modules/audio_device/audio_device_generic.h>
#include <webrtc/rtc_base/checks.h>
#include <webrtc/rtc_base/logging.h>
#include <webrtc/rtc_base/ref_counted_object.h>
#include <webrtc/system_wrappers/include/metrics.h>
#pragma GCC diagnostic pop

#include "fileAudioDevice.h"
#include "fileAudioDeviceModule.h"

rtc::scoped_refptr<fileAudioDeviceModule_t> fileAudioDeviceModule_t::Create(cbAudioData_t _in,
                                                                            cbAudioData_t _out,
                                                                            std::function<void(void *)> _cb,
                                                                            void *_ctx,
                                                                            webrtc::TaskQueueFactory* tqf) {
    RTC_LOG(INFO) << __FUNCTION__;

    // Create the generic reference counted (platform independent) implementation.
    rtc::scoped_refptr<fileAudioDeviceModule_t> audioDevice(
            new rtc::RefCountedObject<fileAudioDeviceModule_t>(tqf));

    if (audioDevice->CreateFileAudioDevice(std::move(_in),
                                           std::move(_out),
                                           std::move(_cb),
                                           _ctx) == -1) {
        return nullptr;
    }
    // Ensure that the generic audio buffer can communicate with the platform
    // specific parts.
    if (audioDevice->AttachAudioBuffer() == -1) {
        return nullptr;
    }

    return audioDevice;
}

fileAudioDeviceModule_t::fileAudioDeviceModule_t(
        webrtc::TaskQueueFactory* tqf): audio_device_buffer_(tqf) {
    RTC_LOG(INFO) << __FUNCTION__;
}

int32_t fileAudioDeviceModule_t::CreateFileAudioDevice(cbAudioData_t _in,
                                                       cbAudioData_t _out,
                                                       std::function<void(void *)> _cb,
                                                       void *_ctx) {
    audio_device_ = std::make_unique<fileAudioDevice_t>(std::move(_in),
                                                        std::move(_out),
                                                        std::move(_cb),
                                                        _ctx);
    RTC_LOG(INFO) << "File Audio APIs will be utilized.";
    if (!audio_device_) {
        RTC_LOG(LS_ERROR) << "Failed to create the platform specific ADM implementation.";
        return -1;
    }

    return 0;
}

int32_t fileAudioDeviceModule_t::AttachAudioBuffer() {
    RTC_LOG(INFO) << __FUNCTION__;
    audio_device_->AttachAudioBuffer(&audio_device_buffer_);
    return 0;
}

fileAudioDeviceModule_t::~fileAudioDeviceModule_t() {
    RTC_LOG(INFO) << __FUNCTION__;
}

int32_t fileAudioDeviceModule_t::ActiveAudioLayer(AudioLayer* audioLayer) const {
    RTC_LOG(INFO) << __FUNCTION__;
    AudioLayer activeAudio;
    if (audio_device_->ActiveAudioLayer(activeAudio) == -1) {
        return -1;
    }
    *audioLayer = activeAudio;
    return 0;
}

int32_t fileAudioDeviceModule_t::Init() {
    RTC_LOG(INFO) << __FUNCTION__;
    if (initialized_)
        return 0;
    RTC_CHECK(audio_device_);
    webrtc::AudioDeviceGeneric::InitStatus status = audio_device_->Init();
    RTC_HISTOGRAM_ENUMERATION(
            "WebRTC.Audio.InitializationResult", static_cast<int>(status),
            static_cast<int>(webrtc::AudioDeviceGeneric::InitStatus::NUM_STATUSES));
    if (status != webrtc::AudioDeviceGeneric::InitStatus::OK) {
        RTC_LOG(LS_ERROR) << "Audio device initialization failed.";
        return -1;
    }
    initialized_ = true;
    return 0;
}

int32_t fileAudioDeviceModule_t::Terminate() {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_)
        return 0;
    if (audio_device_->Terminate() == -1) {
        return -1;
    }
    initialized_ = false;
    return 0;
}

bool fileAudioDeviceModule_t::Initialized() const {
    return initialized_;
}

int32_t fileAudioDeviceModule_t::InitSpeaker() {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    return audio_device_->InitSpeaker();
}

int32_t fileAudioDeviceModule_t::InitMicrophone() {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    return audio_device_->InitMicrophone();
}

int32_t fileAudioDeviceModule_t::SpeakerVolumeIsAvailable(bool* available) {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    bool isAvailable = false;
    if (audio_device_->SpeakerVolumeIsAvailable(isAvailable) == -1) {
        return -1;
    }
    *available = isAvailable;
    RTC_LOG(INFO) << "output: " << isAvailable;
    return 0;
}

int32_t fileAudioDeviceModule_t::SetSpeakerVolume(uint32_t volume) {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << volume << ")";
    if (!initialized_) {
        return -1;
    }
    return audio_device_->SetSpeakerVolume(volume);
}

int32_t fileAudioDeviceModule_t::SpeakerVolume(uint32_t* volume) const {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    uint32_t level = 0;
    if (audio_device_->SpeakerVolume(level) == -1) {
        return -1;
    }
    *volume = level;
    RTC_LOG(INFO) << "output: " << *volume;
    return 0;
}

bool fileAudioDeviceModule_t::SpeakerIsInitialized() const {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return false;
    }
    bool isInitialized = audio_device_->SpeakerIsInitialized();
    RTC_LOG(INFO) << "output: " << isInitialized;
    return isInitialized;
}

bool fileAudioDeviceModule_t::MicrophoneIsInitialized() const {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return false;
    }
    bool isInitialized = audio_device_->MicrophoneIsInitialized();
    RTC_LOG(INFO) << "output: " << isInitialized;
    return isInitialized;
}

int32_t fileAudioDeviceModule_t::MaxSpeakerVolume(uint32_t* maxVolume) const {
    if (!initialized_) {
        return -1;
    }
    uint32_t maxVol = 0;
    if (audio_device_->MaxSpeakerVolume(maxVol) == -1) {
        return -1;
    }
    *maxVolume = maxVol;
    return 0;
}

int32_t fileAudioDeviceModule_t::MinSpeakerVolume(uint32_t* minVolume) const {
    if (!initialized_) {
        return -1;
    }
    uint32_t minVol = 0;
    if (audio_device_->MinSpeakerVolume(minVol) == -1) {
        return -1;
    }
    *minVolume = minVol;
    return 0;
}

int32_t fileAudioDeviceModule_t::SpeakerMuteIsAvailable(bool* available) {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    bool isAvailable = false;
    if (audio_device_->SpeakerMuteIsAvailable(isAvailable) == -1) {
        return -1;
    }
    *available = isAvailable;
    RTC_LOG(INFO) << "output: " << isAvailable;
    return 0;
}

int32_t fileAudioDeviceModule_t::SetSpeakerMute(bool enable) {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
    if (!initialized_) {
        return -1;
    }
    return audio_device_->SetSpeakerMute(enable);
}

int32_t fileAudioDeviceModule_t::SpeakerMute(bool* enabled) const {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    bool muted = false;
    if (audio_device_->SpeakerMute(muted) == -1) {
        return -1;
    }
    *enabled = muted;
    RTC_LOG(INFO) << "output: " << muted;
    return 0;
}

int32_t fileAudioDeviceModule_t::MicrophoneMuteIsAvailable(bool* available) {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    bool isAvailable = false;
    if (audio_device_->MicrophoneMuteIsAvailable(isAvailable) == -1) {
        return -1;
    }
    *available = isAvailable;
    RTC_LOG(INFO) << "output: " << isAvailable;
    return 0;
}

int32_t fileAudioDeviceModule_t::SetMicrophoneMute(bool enable) {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
    if (!initialized_) {
        return -1;
    }
    return (audio_device_->SetMicrophoneMute(enable));
}

int32_t fileAudioDeviceModule_t::MicrophoneMute(bool* enabled) const {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    bool muted = false;
    if (audio_device_->MicrophoneMute(muted) == -1) {
        return -1;
    }
    *enabled = muted;
    RTC_LOG(INFO) << "output: " << muted;
    return 0;
}

int32_t fileAudioDeviceModule_t::MicrophoneVolumeIsAvailable(bool* available) {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    bool isAvailable = false;
    if (audio_device_->MicrophoneVolumeIsAvailable(isAvailable) == -1) {
        return -1;
    }
    *available = isAvailable;
    RTC_LOG(INFO) << "output: " << isAvailable;
    return 0;
}

int32_t fileAudioDeviceModule_t::SetMicrophoneVolume(uint32_t volume) {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << volume << ")";
    if (!initialized_) {
        return -1;
    }
    return (audio_device_->SetMicrophoneVolume(volume));
}

int32_t fileAudioDeviceModule_t::MicrophoneVolume(uint32_t* volume) const {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    uint32_t level = 0;
    if (audio_device_->MicrophoneVolume(level) == -1) {
        return -1;
    }
    *volume = level;
    RTC_LOG(INFO) << "output: " << *volume;
    return 0;
}

int32_t fileAudioDeviceModule_t::StereoRecordingIsAvailable(
        bool* available) const {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    bool isAvailable = false;
    if (audio_device_->StereoRecordingIsAvailable(isAvailable) == -1) {
        return -1;
    }
    *available = isAvailable;
    RTC_LOG(INFO) << "output: " << isAvailable;
    return 0;
}

int32_t fileAudioDeviceModule_t::SetStereoRecording(bool enable) {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
    if (!initialized_) {
        return -1;
    }
    if (audio_device_->RecordingIsInitialized()) {
        RTC_LOG(LERROR)
        << "unable to set stereo mode after recording is initialized";
        return -1;
    }
    if (audio_device_->SetStereoRecording(enable) == -1) {
        if (enable) {
            RTC_LOG(WARNING) << "failed to enable stereo recording";
        }
        return -1;
    }
    int8_t nChannels(1);
    if (enable) {
        nChannels = 2;
    }
    audio_device_buffer_.SetRecordingChannels(nChannels);
    return 0;
}

int32_t fileAudioDeviceModule_t::StereoRecording(bool* enabled) const {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    bool stereo = false;
    if (audio_device_->StereoRecording(stereo) == -1) {
        return -1;
    }
    *enabled = stereo;
    RTC_LOG(INFO) << "output: " << stereo;
    return 0;
}

int32_t fileAudioDeviceModule_t::StereoPlayoutIsAvailable(bool* available) const {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    bool isAvailable = false;
    if (audio_device_->StereoPlayoutIsAvailable(isAvailable) == -1) {
        return -1;
    }
    *available = isAvailable;
    RTC_LOG(INFO) << "output: " << isAvailable;
    return 0;
}

int32_t fileAudioDeviceModule_t::SetStereoPlayout(bool enable) {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
    if (!initialized_) {
        return -1;
    }
    if (audio_device_->PlayoutIsInitialized()) {
        RTC_LOG(LERROR)
        << "unable to set stereo mode while playing side is initialized";
        return -1;
    }
    if (audio_device_->SetStereoPlayout(enable)) {
        RTC_LOG(WARNING) << "stereo playout is not supported";
        return -1;
    }
    int8_t nChannels(1);
    if (enable) {
        nChannels = 2;
    }
    audio_device_buffer_.SetPlayoutChannels(nChannels);
    return 0;
}

int32_t fileAudioDeviceModule_t::StereoPlayout(bool* enabled) const {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    bool stereo = false;
    if (audio_device_->StereoPlayout(stereo) == -1) {
        return -1;
    }
    *enabled = stereo;
    RTC_LOG(INFO) << "output: " << stereo;
    return 0;
}

int32_t fileAudioDeviceModule_t::PlayoutIsAvailable(bool* available) {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    bool isAvailable = false;
    if (audio_device_->PlayoutIsAvailable(isAvailable) == -1) {
        return -1;
    }
    *available = isAvailable;
    RTC_LOG(INFO) << "output: " << isAvailable;
    return 0;
}

int32_t fileAudioDeviceModule_t::RecordingIsAvailable(bool* available) {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    bool isAvailable = false;
    if (audio_device_->RecordingIsAvailable(isAvailable) == -1) {
        return -1;
    }
    *available = isAvailable;
    RTC_LOG(INFO) << "output: " << isAvailable;
    return 0;
}

int32_t fileAudioDeviceModule_t::MaxMicrophoneVolume(uint32_t* maxVolume) const {
    if (!initialized_) {
        return -1;
    }
    uint32_t maxVol(0);
    if (audio_device_->MaxMicrophoneVolume(maxVol) == -1) {
        return -1;
    }
    *maxVolume = maxVol;
    return 0;
}

int32_t fileAudioDeviceModule_t::MinMicrophoneVolume(uint32_t* minVolume) const {
    if (!initialized_) {
        return -1;
    }
    uint32_t minVol(0);
    if (audio_device_->MinMicrophoneVolume(minVol) == -1) {
        return -1;
    }
    *minVolume = minVol;
    return 0;
}

int16_t fileAudioDeviceModule_t::PlayoutDevices() {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    uint16_t nPlayoutDevices = audio_device_->PlayoutDevices();
    RTC_LOG(INFO) << "output: " << nPlayoutDevices;
    return (int16_t)(nPlayoutDevices);
}

int32_t fileAudioDeviceModule_t::SetPlayoutDevice(uint16_t index) {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << index << ")";
    if (!initialized_) {
        return -1;
    }
    return audio_device_->SetPlayoutDevice(index);
}

int32_t fileAudioDeviceModule_t::SetPlayoutDevice(WindowsDeviceType device) {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    return audio_device_->SetPlayoutDevice(device);
}

int32_t fileAudioDeviceModule_t::PlayoutDeviceName(
        uint16_t index,
        char name[webrtc::kAdmMaxDeviceNameSize],
        char guid[webrtc::kAdmMaxGuidSize]) {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << index << ", ...)";
    if (!initialized_) {
        return -1;
    }
    if (name == NULL) {
        return -1;
    }
    if (audio_device_->PlayoutDeviceName(index, name, guid) == -1) {
        return -1;
    }
    if (name != NULL) {
        RTC_LOG(INFO) << "output: name = " << name;
    }
    if (guid != NULL) {
        RTC_LOG(INFO) << "output: guid = " << guid;
    }
    return 0;
}

int32_t fileAudioDeviceModule_t::RecordingDeviceName(
        uint16_t index,
        char name[webrtc::kAdmMaxDeviceNameSize],
        char guid[webrtc::kAdmMaxGuidSize]) {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << index << ", ...)";
    if (!initialized_) {
        return -1;
    }
    if (name == NULL) {
        return -1;
    }
    if (audio_device_->RecordingDeviceName(index, name, guid) == -1) {
        return -1;
    }
    if (name != NULL) {
        RTC_LOG(INFO) << "output: name = " << name;
    }
    if (guid != NULL) {
        RTC_LOG(INFO) << "output: guid = " << guid;
    }
    return 0;
}

int16_t fileAudioDeviceModule_t::RecordingDevices() {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    uint16_t nRecordingDevices = audio_device_->RecordingDevices();
    RTC_LOG(INFO) << "output: " << nRecordingDevices;
    return (int16_t)nRecordingDevices;
}

int32_t fileAudioDeviceModule_t::SetRecordingDevice(uint16_t index) {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << index << ")";
    if (!initialized_) {
        return -1;
    }
    return audio_device_->SetRecordingDevice(index);
}

int32_t fileAudioDeviceModule_t::SetRecordingDevice(WindowsDeviceType device) {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    return audio_device_->SetRecordingDevice(device);
}

int32_t fileAudioDeviceModule_t::InitPlayout() {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    if (PlayoutIsInitialized()) {
        return 0;
    }
    int32_t result = audio_device_->InitPlayout();
    RTC_LOG(INFO) << "output: " << result;
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.InitPlayoutSuccess",
                          static_cast<int>(result == 0));
    return result;
}

int32_t fileAudioDeviceModule_t::InitRecording() {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    if (RecordingIsInitialized()) {
        return 0;
    }
    int32_t result = audio_device_->InitRecording();
    RTC_LOG(INFO) << "output: " << result;
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.InitRecordingSuccess",
                          static_cast<int>(result == 0));
    return result;
}

bool fileAudioDeviceModule_t::PlayoutIsInitialized() const {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return false;
    }
    return audio_device_->PlayoutIsInitialized();
}

bool fileAudioDeviceModule_t::RecordingIsInitialized() const {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return false;
    }
    return audio_device_->RecordingIsInitialized();
}

int32_t fileAudioDeviceModule_t::StartPlayout() {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    if (Playing()) {
        return 0;
    }
    audio_device_buffer_.StartPlayout();
    int32_t result = audio_device_->StartPlayout();
    RTC_LOG(INFO) << "output: " << result;
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.StartPlayoutSuccess",
                          static_cast<int>(result == 0));
    return result;
}

int32_t fileAudioDeviceModule_t::StopPlayout() {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    int32_t result = audio_device_->StopPlayout();
    audio_device_buffer_.StopPlayout();
    RTC_LOG(INFO) << "output: " << result;
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.StopPlayoutSuccess",
                          static_cast<int>(result == 0));
    return result;
}

bool fileAudioDeviceModule_t::Playing() const {
    if (!initialized_) {
        return false;
    }
    return audio_device_->Playing();
}

int32_t fileAudioDeviceModule_t::StartRecording() {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    if (Recording()) {
        return 0;
    }
    audio_device_buffer_.StartRecording();
    int32_t result = audio_device_->StartRecording();
    RTC_LOG(INFO) << "output: " << result;
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.StartRecordingSuccess",
                          static_cast<int>(result == 0));
    return result;
}

int32_t fileAudioDeviceModule_t::StopRecording() {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return -1;
    }
    int32_t result = audio_device_->StopRecording();
    audio_device_buffer_.StopRecording();
    RTC_LOG(INFO) << "output: " << result;
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.StopRecordingSuccess",
                          static_cast<int>(result == 0));
    return result;
}

bool fileAudioDeviceModule_t::Recording() const {
    if (!initialized_) {
        return false;
    }
    return audio_device_->Recording();
}

int32_t fileAudioDeviceModule_t::RegisterAudioCallback(
        webrtc::AudioTransport* audioCallback) {
    RTC_LOG(INFO) << __FUNCTION__;
    return audio_device_buffer_.RegisterAudioCallback(audioCallback);
}

int32_t fileAudioDeviceModule_t::PlayoutDelay(uint16_t* delayMS) const {
    if (!initialized_) {
        return -1;
    }
    uint16_t delay = 0;
    if (audio_device_->PlayoutDelay(delay) == -1) {
        RTC_LOG(LERROR) << "failed to retrieve the playout delay";
        return -1;
    }
    *delayMS = delay;
    return 0;
}

bool fileAudioDeviceModule_t::BuiltInAECIsAvailable() const {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return false;
    }
    bool isAvailable = audio_device_->BuiltInAECIsAvailable();
    RTC_LOG(INFO) << "output: " << isAvailable;
    return isAvailable;
}

int32_t fileAudioDeviceModule_t::EnableBuiltInAEC(bool enable) {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
    if (!initialized_) {
        return -1;
    }
    int32_t ok = audio_device_->EnableBuiltInAEC(enable);
    RTC_LOG(INFO) << "output: " << ok;
    return ok;
}

bool fileAudioDeviceModule_t::BuiltInAGCIsAvailable() const {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return false;
    }
    bool isAvailable = audio_device_->BuiltInAGCIsAvailable();
    RTC_LOG(INFO) << "output: " << isAvailable;
    return isAvailable;
}

int32_t fileAudioDeviceModule_t::EnableBuiltInAGC(bool enable) {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
    if (!initialized_) {
        return -1;
    }
    int32_t ok = audio_device_->EnableBuiltInAGC(enable);
    RTC_LOG(INFO) << "output: " << ok;
    return ok;
}

bool fileAudioDeviceModule_t::BuiltInNSIsAvailable() const {
    RTC_LOG(INFO) << __FUNCTION__;
    if (!initialized_) {
        return false;
    }
    bool isAvailable = audio_device_->BuiltInNSIsAvailable();
    RTC_LOG(INFO) << "output: " << isAvailable;
    return isAvailable;
}

int32_t fileAudioDeviceModule_t::EnableBuiltInNS(bool enable) {
    RTC_LOG(INFO) << __FUNCTION__ << "(" << enable << ")";
    if (!initialized_) {
        return -1;
    }
    int32_t ok = audio_device_->EnableBuiltInNS(enable);
    RTC_LOG(INFO) << "output: " << ok;
    return ok;
}
