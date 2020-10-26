project(gvoip)

set(PROJECT_INCLUDE_DIR ${PROJECT_ROOT_DIR})
set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(wsClient)

set(TGVOIP_SRCS
        ${PROJECT_SOURCE_DIR}/TgVoip.h
        ${PROJECT_SOURCE_DIR}/TgVoip.cpp
        ${PROJECT_SOURCE_DIR}/server.h
        ${PROJECT_SOURCE_DIR}/server.cpp
        ${PROJECT_SOURCE_DIR}/sessionDescriptionObserver.h
        ${PROJECT_SOURCE_DIR}/webRTCPeer.h
        ${PROJECT_SOURCE_DIR}/webRTCPeer.cpp
        ${PROJECT_SOURCE_DIR}/fileAudioDevice.h
        ${PROJECT_SOURCE_DIR}/fileAudioDevice.cpp
        ${PROJECT_SOURCE_DIR}/fileAudioDeviceModule.h
        ${PROJECT_SOURCE_DIR}/fileAudioDeviceModule.cpp
        )

add_library(${TGVOIP_LIB} SHARED ${TGVOIP_SRCS})
set_target_properties(${TGVOIP_LIB} PROPERTIES
        SOVERSION 0
        VERSION 0.0.0
        )

target_link_libraries(${TGVOIP_LIB}
        ${WSCLIENT_LIB}
        ${LIBWEBSOCKETS_LIBRARIES}
        ${WEBRTC_LIB}
        ${LIBS}
        )
