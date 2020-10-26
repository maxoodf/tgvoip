/**
* @file tgvoip/fileAudioDeviceModule.h
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#ifndef TESTWEBRTC_FILEAUDIODEVICEMODULE_H
#define TESTWEBRTC_FILEAUDIODEVICEMODULE_H

//#include <stdint.h>

//#include <memory>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <api/task_queue/task_queue_factory.h>
#include <modules/audio_device/audio_device_buffer.h>
#include <modules/audio_device/include/audio_device.h>
#pragma GCC diagnostic pop

namespace webrtc {
    class AudioManager;
} // namespace webrtc

class fileAudioDevice_t;

class fileAudioDeviceModule_t: public webrtc::AudioDeviceModule {
public:
    using cbAudioData_t = std::function<void(int16_t* data, size_t len)>;

public:
    explicit fileAudioDeviceModule_t(webrtc::TaskQueueFactory* task_queue_factory);
    ~fileAudioDeviceModule_t() override;

    static rtc::scoped_refptr<fileAudioDeviceModule_t> Create(cbAudioData_t _in,
                                                              cbAudioData_t _out,
                                                              std::function<void(void *)> _cb,
                                                              void *_ctx,
                                                              webrtc::TaskQueueFactory* task_queue_factory);

    int32_t CreateFileAudioDevice(cbAudioData_t _in,
                                  cbAudioData_t _out,
                                  std::function<void(void *)> _cb,
                                  void *_ctx);
    int32_t AttachAudioBuffer();

    // Retrieve the currently utilized audio layer
    int32_t ActiveAudioLayer(AudioLayer* audioLayer) const override;

    // Full-duplex transportation of PCM audio
    int32_t RegisterAudioCallback(webrtc::AudioTransport* audioCallback) override;

    // Main initializaton and termination
    int32_t Init() override;
    int32_t Terminate() override;
    bool Initialized() const override;

    // Device enumeration
    int16_t PlayoutDevices() override;
    int16_t RecordingDevices() override;
    int32_t PlayoutDeviceName(uint16_t index,
                              char name[webrtc::kAdmMaxDeviceNameSize],
                              char guid[webrtc::kAdmMaxGuidSize]) override;
    int32_t RecordingDeviceName(uint16_t index,
                                char name[webrtc::kAdmMaxDeviceNameSize],
                                char guid[webrtc::kAdmMaxGuidSize]) override;

    // Device selection
    int32_t SetPlayoutDevice(uint16_t index) override;
    int32_t SetPlayoutDevice(WindowsDeviceType device) override;
    int32_t SetRecordingDevice(uint16_t index) override;
    int32_t SetRecordingDevice(WindowsDeviceType device) override;

    // Audio transport initialization
    int32_t PlayoutIsAvailable(bool* available) override;
    int32_t InitPlayout() override;
    bool PlayoutIsInitialized() const override;
    int32_t RecordingIsAvailable(bool* available) override;
    int32_t InitRecording() override;
    bool RecordingIsInitialized() const override;

    // Audio transport control
    int32_t StartPlayout() override;
    int32_t StopPlayout() override;
    bool Playing() const override;
    int32_t StartRecording() override;
    int32_t StopRecording() override;
    bool Recording() const override;

    // Audio mixer initialization
    int32_t InitSpeaker() override;
    bool SpeakerIsInitialized() const override;
    int32_t InitMicrophone() override;
    bool MicrophoneIsInitialized() const override;

    // Speaker volume controls
    int32_t SpeakerVolumeIsAvailable(bool* available) override;
    int32_t SetSpeakerVolume(uint32_t volume) override;
    int32_t SpeakerVolume(uint32_t* volume) const override;
    int32_t MaxSpeakerVolume(uint32_t* maxVolume) const override;
    int32_t MinSpeakerVolume(uint32_t* minVolume) const override;

    // Microphone volume controls
    int32_t MicrophoneVolumeIsAvailable(bool* available) override;
    int32_t SetMicrophoneVolume(uint32_t volume) override;
    int32_t MicrophoneVolume(uint32_t* volume) const override;
    int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const override;
    int32_t MinMicrophoneVolume(uint32_t* minVolume) const override;

    // Speaker mute control
    int32_t SpeakerMuteIsAvailable(bool* available) override;
    int32_t SetSpeakerMute(bool enable) override;
    int32_t SpeakerMute(bool* enabled) const override;

    // Microphone mute control
    int32_t MicrophoneMuteIsAvailable(bool* available) override;
    int32_t SetMicrophoneMute(bool enable) override;
    int32_t MicrophoneMute(bool* enabled) const override;

    // Stereo support
    int32_t StereoPlayoutIsAvailable(bool* available) const override;
    int32_t SetStereoPlayout(bool enable) override;
    int32_t StereoPlayout(bool* enabled) const override;
    int32_t StereoRecordingIsAvailable(bool* available) const override;
    int32_t SetStereoRecording(bool enable) override;
    int32_t StereoRecording(bool* enabled) const override;

    // Delay information and control
    int32_t PlayoutDelay(uint16_t* delayMS) const override;

    bool BuiltInAECIsAvailable() const override;
    int32_t EnableBuiltInAEC(bool enable) override;
    bool BuiltInAGCIsAvailable() const override;
    int32_t EnableBuiltInAGC(bool enable) override;
    bool BuiltInNSIsAvailable() const override;
    int32_t EnableBuiltInNS(bool enable) override;

private:
    bool initialized_ = false;
    webrtc::AudioDeviceBuffer audio_device_buffer_;
    std::unique_ptr<fileAudioDevice_t> audio_device_;
};

#endif  // TESTWEBRTC_FILEAUDIODEVICEMODULE_H
