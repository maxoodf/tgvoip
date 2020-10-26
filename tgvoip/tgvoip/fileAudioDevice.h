/**
* @file tgvoip/fileAudioDevice.h
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#ifndef TESTWEBRTC_FILEAUDIODEVICE_H
#define TESTWEBRTC_FILEAUDIODEVICE_H

//#include <stdio.h>

#include <memory>
#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <webrtc/modules/audio_device/audio_device_generic.h>
#include <webrtc/rtc_base/critical_section.h>
#include <webrtc/rtc_base/system/file_wrapper.h>
#include <webrtc/rtc_base/time_utils.h>
#pragma GCC diagnostic pop

namespace rtc {
    class PlatformThread;
}  // namespace rtc

// This is a fake audio device which plays audio from a file as its microphone
// and plays out into a file.
class fileAudioDevice_t : public webrtc::AudioDeviceGeneric {
public:
    using cbAudioData_t = std::function<void(int16_t* data, size_t len)>;

public:
    // Constructs a file audio device with |id|. It will read audio from
    // |inputFilename| and record output audio to |outputFilename|.
    //
    // The input file should be a readable 48k stereo raw file, and the output
    // file should point to a writable location. The output format will also be
    // 48k stereo raw audio.
    fileAudioDevice_t(cbAudioData_t in,
                      cbAudioData_t out,
                      std::function<void(void *)> cbHangup,
                      void *ctx);
    ~fileAudioDevice_t() override;

    // Retrieve the currently utilized audio layer
    int32_t ActiveAudioLayer(
            webrtc::AudioDeviceModule::AudioLayer& audioLayer) const override;

    // Main initializaton and termination
    InitStatus Init() override;
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
    int32_t SetPlayoutDevice(
            webrtc::AudioDeviceModule::WindowsDeviceType device) override;
    int32_t SetRecordingDevice(uint16_t index) override;
    int32_t SetRecordingDevice(
            webrtc::AudioDeviceModule::WindowsDeviceType device) override;

    // Audio transport initialization
    int32_t PlayoutIsAvailable(bool& available) override;
    int32_t InitPlayout() override;
    bool PlayoutIsInitialized() const override;
    int32_t RecordingIsAvailable(bool& available) override;
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
    int32_t SpeakerVolumeIsAvailable(bool& available) override;
    int32_t SetSpeakerVolume(uint32_t volume) override;
    int32_t SpeakerVolume(uint32_t& volume) const override;
    int32_t MaxSpeakerVolume(uint32_t& maxVolume) const override;
    int32_t MinSpeakerVolume(uint32_t& minVolume) const override;

    // Microphone volume controls
    int32_t MicrophoneVolumeIsAvailable(bool& available) override;
    int32_t SetMicrophoneVolume(uint32_t volume) override;
    int32_t MicrophoneVolume(uint32_t& volume) const override;
    int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const override;
    int32_t MinMicrophoneVolume(uint32_t& minVolume) const override;

    // Speaker mute control
    int32_t SpeakerMuteIsAvailable(bool& available) override;
    int32_t SetSpeakerMute(bool enable) override;
    int32_t SpeakerMute(bool& enabled) const override;

    // Microphone mute control
    int32_t MicrophoneMuteIsAvailable(bool& available) override;
    int32_t SetMicrophoneMute(bool enable) override;
    int32_t MicrophoneMute(bool& enabled) const override;

    // Stereo support
    int32_t StereoPlayoutIsAvailable(bool& available) override;
    int32_t SetStereoPlayout(bool enable) override;
    int32_t StereoPlayout(bool& enabled) const override;
    int32_t StereoRecordingIsAvailable(bool& available) override;
    int32_t SetStereoRecording(bool enable) override;
    int32_t StereoRecording(bool& enabled) const override;

    // Delay information and control
    int32_t PlayoutDelay(uint16_t& delayMS) const override;

    void AttachAudioBuffer(webrtc::AudioDeviceBuffer* audioBuffer) override;

private:
    static void RecThreadFunc(void*);
    static void PlayThreadFunc(void*);
    bool RecThreadProcess();
    bool PlayThreadProcess();

    int32_t _playout_index;
    int32_t _record_index;
    webrtc::AudioDeviceBuffer* _ptrAudioBuffer;
    int8_t* _recordingBuffer;  // In bytes.
    int8_t* _playoutBuffer;    // In bytes.
    uint32_t _recordingFramesLeft;
    uint32_t _playoutFramesLeft;
    rtc::CriticalSection _critSect;

    size_t _recordingBufferSizeIn10MS;
    size_t _recordingFramesIn10MS;
    size_t _playoutFramesIn10MS;

    // TODO(pbos): Make plain members instead of pointers and stop resetting them.
    std::unique_ptr<rtc::PlatformThread> _ptrThreadRec;
    std::unique_ptr<rtc::PlatformThread> _ptrThreadPlay;

    bool _playing;
    bool _recording;
    int64_t _lastCallPlayoutMillis;
    int64_t _lastCallRecordMillis;

    std::function<void(void *)> _cbHangup = nullptr;
    void *_ctx = nullptr;

    webrtc::FileWrapper _outputFile;
    webrtc::FileWrapper _inputFile;
    std::string _outputFilename;
    std::string _inputFilename;

    cbAudioData_t _cbInAudioData = nullptr;
    cbAudioData_t _cbOutAudioData = nullptr;
};

#endif  // TESTWEBRTC_FILEAUDIODEVICE_H
