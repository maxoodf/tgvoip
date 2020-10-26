/**
* @file tgvoip/fileAudioDevice.cpp
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <webrtc/rtc_base/checks.h>
#include <webrtc/rtc_base/logging.h>
#include <webrtc/rtc_base/platform_thread.h>
#include <webrtc/rtc_base/time_utils.h>
#include <webrtc/system_wrappers/include/sleep.h>
#pragma GCC diagnostic pop

#include "fileAudioDevice.h"

const int kRecordingFixedSampleRate = 48000;
const size_t kRecordingNumChannels = 1;
const int kPlayoutFixedSampleRate = 48000;
const size_t kPlayoutNumChannels = 1;
const size_t kPlayoutBufferSize =
        kPlayoutFixedSampleRate / 100 * kPlayoutNumChannels * 2;
const size_t kRecordingBufferSize =
        kRecordingFixedSampleRate / 100 * kRecordingNumChannels * 2;

fileAudioDevice_t::fileAudioDevice_t(cbAudioData_t in,
                                     cbAudioData_t out,
                                     std::function<void(void *)> cbHangup,
                                     void *ctx):
        _playout_index(0),
        _record_index(0),
        _ptrAudioBuffer(nullptr),
        _recordingBuffer(nullptr),
        _playoutBuffer(nullptr),
        _recordingFramesLeft(0),
        _playoutFramesLeft(0),
        _recordingBufferSizeIn10MS(0),
        _recordingFramesIn10MS(0),
        _playoutFramesIn10MS(0),
        _playing(false),
        _recording(false),
        _lastCallPlayoutMillis(0),
        _lastCallRecordMillis(0),
        _cbHangup(std::move(cbHangup)),
        _ctx(ctx),
        _cbInAudioData(std::move(in)),
        _cbOutAudioData(std::move(out)) {
}

int32_t fileAudioDevice_t::ActiveAudioLayer(
        webrtc::AudioDeviceModule::AudioLayer& ) const {
    return -1;
}

fileAudioDevice_t::~fileAudioDevice_t() {
}

webrtc::AudioDeviceGeneric::InitStatus fileAudioDevice_t::Init() {
    return InitStatus::OK;
}

int32_t fileAudioDevice_t::Terminate() {
    return 0;
}

bool fileAudioDevice_t::Initialized() const {
    return true;
}

int16_t fileAudioDevice_t::PlayoutDevices() {
    return 1;
}

int16_t fileAudioDevice_t::RecordingDevices() {
    return 1;
}

int32_t fileAudioDevice_t::PlayoutDeviceName(uint16_t index,
                                             char name[webrtc::kAdmMaxDeviceNameSize],
                                             char guid[webrtc::kAdmMaxGuidSize]) {
    const char* kName = "file_audio_device";
    const char* kGuid = "file_audio_device_unique_id";
    if (index < 1) {
        memset(name, 0, webrtc::kAdmMaxDeviceNameSize);
        memset(guid, 0, webrtc::kAdmMaxGuidSize);
        memcpy(name, kName, strlen(kName));
        memcpy(guid, kGuid, strlen(guid));
        return 0;
    }
    return -1;
}

int32_t fileAudioDevice_t::RecordingDeviceName(uint16_t index,
                                               char name[webrtc::kAdmMaxDeviceNameSize],
                                               char guid[webrtc::kAdmMaxGuidSize]) {
    const char* kName = "file_audio_device";
    const char* kGuid = "file_audio_device_unique_id";
    if (index < 1) {
        memset(name, 0, webrtc::kAdmMaxDeviceNameSize);
        memset(guid, 0, webrtc::kAdmMaxGuidSize);
        memcpy(name, kName, strlen(kName));
        memcpy(guid, kGuid, strlen(guid));
        return 0;
    }
    return -1;
}

int32_t fileAudioDevice_t::SetPlayoutDevice(uint16_t index) {
    if (index == 0) {
        _playout_index = index;
        return 0;
    }
    return -1;
}

int32_t fileAudioDevice_t::SetPlayoutDevice(
        webrtc::AudioDeviceModule::WindowsDeviceType ) {
    return -1;
}

int32_t fileAudioDevice_t::SetRecordingDevice(uint16_t index) {
    if (index == 0) {
        _record_index = index;
        return _record_index;
    }
    return -1;
}

int32_t fileAudioDevice_t::SetRecordingDevice(
        webrtc::AudioDeviceModule::WindowsDeviceType ) {
    return -1;
}

int32_t fileAudioDevice_t::PlayoutIsAvailable(bool& available) {
    if (_playout_index == 0) {
        available = true;
        return _playout_index;
    }
    available = false;
    return -1;
}

int32_t fileAudioDevice_t::InitPlayout() {
    rtc::CritScope lock(&_critSect);

    if (_playing) {
        return -1;
    }

    _playoutFramesIn10MS = static_cast<size_t>(kPlayoutFixedSampleRate / 100);

    if (_ptrAudioBuffer) {
        // Update webrtc audio buffer with the selected parameters
        _ptrAudioBuffer->SetPlayoutSampleRate(kPlayoutFixedSampleRate);
        _ptrAudioBuffer->SetPlayoutChannels(kPlayoutNumChannels);
    }
    return 0;
}

bool fileAudioDevice_t::PlayoutIsInitialized() const {
    return _playoutFramesIn10MS != 0;
}

int32_t fileAudioDevice_t::RecordingIsAvailable(bool& available) {
    if (_record_index == 0) {
        available = true;
        return _record_index;
    }
    available = false;
    return -1;
}

int32_t fileAudioDevice_t::InitRecording() {
    rtc::CritScope lock(&_critSect);

    if (_recording) {
        return -1;
    }

    _recordingFramesIn10MS = static_cast<size_t>(kRecordingFixedSampleRate / 100);

    if (_ptrAudioBuffer) {
        _ptrAudioBuffer->SetRecordingSampleRate(kRecordingFixedSampleRate);
        _ptrAudioBuffer->SetRecordingChannels(kRecordingNumChannels);
    }
    return 0;
}

bool fileAudioDevice_t::RecordingIsInitialized() const {
    return _recordingFramesIn10MS != 0;
}

int32_t fileAudioDevice_t::StartPlayout() {
    if (_playing) {
        return 0;
    }

    _playing = true;
    _playoutFramesLeft = 0;

    if (!_playoutBuffer) {
        _playoutBuffer = new int8_t[kPlayoutBufferSize];
    }
    if (!_playoutBuffer) {
        _playing = false;
        return -1;
    }

    // PLAYOUT
    if (!_outputFilename.empty() || _cbOutAudioData) {
        if (!_outputFilename.empty()) {
            _outputFile = webrtc::FileWrapper::OpenWriteOnly(_outputFilename.c_str());
            if (!_outputFile.is_open()) {
                RTC_LOG(LS_ERROR) << "Failed to open playout file: " << _outputFilename;
                _playing = false;
                delete[] _playoutBuffer;
                _playoutBuffer = nullptr;
                return -1;
            }
            RTC_LOG(LS_INFO) << "Started playout capture to output file: "
                             << _outputFilename;
        }
        _ptrThreadPlay = std::make_unique<rtc::PlatformThread>(
                PlayThreadFunc, this, "webrtc_audio_module_play_thread",
                rtc::kRealtimePriority);
        _ptrThreadPlay->Start();
    }

    return 0;
}

int32_t fileAudioDevice_t::StopPlayout() {
    {
        rtc::CritScope lock(&_critSect);
        _playing = false;
    }

    // stop playout thread first
    if (_ptrThreadPlay) {
        _ptrThreadPlay->Stop();
        _ptrThreadPlay.reset();
    }

    rtc::CritScope lock(&_critSect);

    _playoutFramesLeft = 0;
    delete[] _playoutBuffer;
    _playoutBuffer = nullptr;
    if (_outputFile.is_open()) {
        _outputFile.Close();
        RTC_LOG(LS_INFO) << "Stopped playout capture to output file: " << _outputFilename;
    }

    return 0;
}

bool fileAudioDevice_t::Playing() const {
    return _playing;
}

int32_t fileAudioDevice_t::StartRecording() {
    _recording = true;

    // Make sure we only create the buffer once.
    _recordingBufferSizeIn10MS =
            _recordingFramesIn10MS * kRecordingNumChannels * 2;
    if (!_recordingBuffer) {
        _recordingBuffer = new int8_t[_recordingBufferSizeIn10MS];
    }

    if (!_inputFilename.empty() || _cbInAudioData) {
        if (!_inputFilename.empty()) {
            _inputFile = webrtc::FileWrapper::OpenReadOnly(_inputFilename.c_str());
            if (!_inputFile.is_open()) {
                RTC_LOG(LS_ERROR) << "Failed to open audio input file: "
                                  << _inputFilename;
                _recording = false;
                delete[] _recordingBuffer;
                _recordingBuffer = nullptr;
                return -1;
            }

            RTC_LOG(LS_INFO) << "Started recording from input file: " << _inputFilename;
        }
        _ptrThreadRec = std::make_unique<rtc::PlatformThread>(
                RecThreadFunc, this, "webrtc_audio_module_capture_thread",
                rtc::kRealtimePriority);

        _ptrThreadRec->Start();
    }

    return 0;
}

int32_t fileAudioDevice_t::StopRecording() {
    {
        rtc::CritScope lock(&_critSect);
        _recording = false;
    }

    if (_ptrThreadRec) {
        _ptrThreadRec->Stop();
        _ptrThreadRec.reset();
    }

    rtc::CritScope lock(&_critSect);
    _recordingFramesLeft = 0;
    if (_recordingBuffer) {
        delete[] _recordingBuffer;
        _recordingBuffer = nullptr;
    }
    if (_inputFile.is_open()) {
        _inputFile.Close();
        RTC_LOG(LS_INFO) << "Stopped recording from input file: " << _inputFilename;
    }

    return 0;
}

bool fileAudioDevice_t::Recording() const {
    return _recording;
}

int32_t fileAudioDevice_t::InitSpeaker() {
    return -1;
}

bool fileAudioDevice_t::SpeakerIsInitialized() const {
    return false;
}

int32_t fileAudioDevice_t::InitMicrophone() {
    return 0;
}

bool fileAudioDevice_t::MicrophoneIsInitialized() const {
    return true;
}

int32_t fileAudioDevice_t::SpeakerVolumeIsAvailable(bool& ) {
    return -1;
}

int32_t fileAudioDevice_t::SetSpeakerVolume(uint32_t ) {
    return -1;
}

int32_t fileAudioDevice_t::SpeakerVolume(uint32_t& ) const {
    return -1;
}

int32_t fileAudioDevice_t::MaxSpeakerVolume(uint32_t& ) const {
    return -1;
}

int32_t fileAudioDevice_t::MinSpeakerVolume(uint32_t& ) const {
    return -1;
}

int32_t fileAudioDevice_t::MicrophoneVolumeIsAvailable(bool& ) {
    return -1;
}

int32_t fileAudioDevice_t::SetMicrophoneVolume(uint32_t ) {
    return -1;
}

int32_t fileAudioDevice_t::MicrophoneVolume(uint32_t& ) const {
    return -1;
}

int32_t fileAudioDevice_t::MaxMicrophoneVolume(uint32_t& ) const {
    return -1;
}

int32_t fileAudioDevice_t::MinMicrophoneVolume(uint32_t& ) const {
    return -1;
}

int32_t fileAudioDevice_t::SpeakerMuteIsAvailable(bool& ) {
    return -1;
}

int32_t fileAudioDevice_t::SetSpeakerMute(bool ) {
    return -1;
}

int32_t fileAudioDevice_t::SpeakerMute(bool& ) const {
    return -1;
}

int32_t fileAudioDevice_t::MicrophoneMuteIsAvailable(bool& ) {
    return -1;
}

int32_t fileAudioDevice_t::SetMicrophoneMute(bool ) {
    return -1;
}

int32_t fileAudioDevice_t::MicrophoneMute(bool& ) const {
    return -1;
}

int32_t fileAudioDevice_t::StereoPlayoutIsAvailable(bool& available) {
    available = false;
    return 0;
}
int32_t fileAudioDevice_t::SetStereoPlayout(bool ) {
    return -1;
}

int32_t fileAudioDevice_t::StereoPlayout(bool& enabled) const {
    enabled = false;
    return 0;
}

int32_t fileAudioDevice_t::StereoRecordingIsAvailable(bool& available) {
    available = false;
    return 0;
}

int32_t fileAudioDevice_t::SetStereoRecording(bool ) {
    return -1;
}

int32_t fileAudioDevice_t::StereoRecording(bool& enabled) const {
    enabled = false;
    return 0;
}

int32_t fileAudioDevice_t::PlayoutDelay(uint16_t& ) const {
    return 0;
}

void fileAudioDevice_t::AttachAudioBuffer(webrtc::AudioDeviceBuffer* audioBuffer) {
    rtc::CritScope lock(&_critSect);

    _ptrAudioBuffer = audioBuffer;

    // Inform the AudioBuffer about default settings for this implementation.
    // Set all values to zero here since the actual settings will be done by
    // InitPlayout and InitRecording later.
    _ptrAudioBuffer->SetRecordingSampleRate(0);
    _ptrAudioBuffer->SetPlayoutSampleRate(0);
    _ptrAudioBuffer->SetRecordingChannels(0);
    _ptrAudioBuffer->SetPlayoutChannels(0);
}

void fileAudioDevice_t::PlayThreadFunc(void* pThis) {
    fileAudioDevice_t* device = static_cast<fileAudioDevice_t*>(pThis);
    while (device->PlayThreadProcess()) {
    }
}

void fileAudioDevice_t::RecThreadFunc(void* pThis) {
    fileAudioDevice_t* device = static_cast<fileAudioDevice_t*>(pThis);
    while (device->RecThreadProcess()) {
    }
}

bool fileAudioDevice_t::PlayThreadProcess() {
    if (!_playing) {
        return false;
    }
    int64_t currentTime = rtc::TimeMillis();
//    _critSect.Enter();

    if (_lastCallPlayoutMillis == 0 || currentTime - _lastCallPlayoutMillis >= 10) {
//        _critSect.Leave();
        _ptrAudioBuffer->RequestPlayoutData(_playoutFramesIn10MS);

//        _critSect.Enter();
        _playoutFramesLeft = _ptrAudioBuffer->GetPlayoutData(_playoutBuffer);

        RTC_DCHECK_EQ(_playoutFramesIn10MS, _playoutFramesLeft);
        if (_outputFile.is_open()) {
            _outputFile.Write(_playoutBuffer, kPlayoutBufferSize);
        }
        if (_cbOutAudioData) {
            _cbOutAudioData(reinterpret_cast<int16_t *>(_playoutBuffer),
                            kPlayoutBufferSize / sizeof(int16_t));
        }
        _lastCallPlayoutMillis = currentTime;
    }
    _playoutFramesLeft = 0;
//    _critSect.Leave();

    int64_t deltaTimeMillis = rtc::TimeMillis() - currentTime;
    if (deltaTimeMillis < 10) {
        webrtc::SleepMs(10 - deltaTimeMillis);
    }

    return true;
}

bool fileAudioDevice_t::RecThreadProcess() {
    if (!_recording) {
        return false;
    }

    int64_t currentTime = rtc::TimeMillis();
//    _critSect.Enter();

    if (_lastCallRecordMillis == 0 || currentTime - _lastCallRecordMillis >= 10) {
        if (_inputFile.is_open()) {
            if (_inputFile.Read(_recordingBuffer, kRecordingBufferSize) <= 0) {
//                _critSect.Leave();
                _cbHangup(_ctx);
                return false;
            }
        }
        if (_cbInAudioData) {
            _cbInAudioData(reinterpret_cast<int16_t *>(_recordingBuffer),
                           kRecordingBufferSize / sizeof(int16_t));
        }
        _ptrAudioBuffer->SetRecordedBuffer(_recordingBuffer,
                                           _recordingFramesIn10MS);

        _lastCallRecordMillis = currentTime;
//        _critSect.Leave();
        _ptrAudioBuffer->DeliverRecordedData();
//        _critSect.Enter();
    }

//    _critSect.Leave();

    int64_t deltaTimeMillis = rtc::TimeMillis() - currentTime;
    if (deltaTimeMillis < 10) {
        webrtc::SleepMs(10 - deltaTimeMillis);
    }

    return true;
}
