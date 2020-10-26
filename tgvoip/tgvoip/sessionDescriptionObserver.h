/**
* @file tgvoip/sessionDescriptionObserver.cpp
* @brief
* @author Max Fomichev
* @date 29.01.2020
* @copyright Apache License v.2 (http://www.apache.org/licenses/LICENSE-2.0)
*/

#ifndef WEBRTC_SESSIONDESCRIPTIONOBSERVER_H
#define WEBRTC_SESSIONDESCRIPTIONOBSERVER_H

//#include <webrtc/rtc_base/refcountedobject.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <webrtc/api/video/video_frame.h>
#include <webrtc/rtc_base/ref_counted_object.h>
#include <webrtc/api/jsep.h>
#pragma GCC diagnostic pop

class setSessionDescriptionObserver_t: public webrtc::SetSessionDescriptionObserver {
public:
    static setSessionDescriptionObserver_t* Create() {
        return new rtc::RefCountedObject<setSessionDescriptionObserver_t>();
    }

    virtual void OnSuccess() {
        RTC_LOG(INFO) << __FUNCTION__;
    }

    virtual void OnFailure(webrtc::RTCError error) {
        RTC_LOG(INFO) << __FUNCTION__ << " " << ToString(error.type()) << ": "
                      << error.message();
    }
};

#endif //WEBRTC_SESSIONDESCRIPTIONOBSERVER_H
