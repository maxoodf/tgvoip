project(tgvoip)
cmake_minimum_required(VERSION 3.5)

set(PROJECT_ROOT_DIR ${CMAKE_SOURCE_DIR})
set(CMAKE_BINARY_DIR ${PROJECT_ROOT_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -Werror -fPIC -fno-rtti")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -funroll-loops -ftree-vectorize -fPIC -DNDEBUG")

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    set(LIBS "-pthread -ldl -lrt")
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
    add_definitions(-DWEBRTC_POSIX)
    add_definitions(-DWEBRTC_LINUX)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_definitions(-DWEBRTC_POSIX)
    add_definitions(-DWEBRTC_MAC)
endif()

set(LOCAL_INCLUDE_DIR ${PROJECT_ROOT_DIR})
include_directories(${LOCAL_INCLUDE_DIR})

find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig")

pkg_check_modules(CRYPTO REQUIRED libcrypto)
include_directories(${CRYPTO_INCLUDE_DIRS})

pkg_check_modules(SSL REQUIRED libssl)
include_directories(${SSL_INCLUDE_DIRS})

find_package(Libwebsockets REQUIRED)
include_directories(${LIBWEBSOCKETS_INCLUDE_DIRS})

find_package(RapidJSON REQUIRED)
include_directories(${RapidJSON_INCLUDE_DIRS})

find_path(WEBRTC_INCLUDE_DIR webrtc/api/proxy.h
        /usr/local/include
        /usr/include
        )
include_directories(${WEBRTC_INCLUDE_DIR})
include_directories(${WEBRTC_INCLUDE_DIR}/webrtc)

find_library(WEBRTC_LIB webrtc PATHS
        /usr/local/lib
        /usr/lib
        )

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
    find_library(COREAUDIO_LIBRARY CoreAudio)
    find_library(AVFOUNDATION_LIBRARY AVFoundation)
    find_library(COREGRAPHICS_LIBRARY CoreGraphics)
    find_library(COREVIDEO_LIBRARY CoreVideo)
    find_library(COREMEDIA_LIBRARY CoreMedia)
    find_library(FOUNDATION_LIBRARY Foundation)

    list(APPEND WEBRTC_LIB
            ${AUDIOTOOLBOX_LIBRARY}
            ${COREAUDIO_LIBRARY}
            ${AVFOUNDATION_LIBRARY}
            ${COREGRAPHICS_LIBRARY}
            ${COREVIDEO_LIBRARY}
            ${COREMEDIA_LIBRARY}
            ${FOUNDATION_LIBRARY}
            )
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    find_library(LIB_X11 X11)
    list(APPEND WEBRTC_LIB
            ${LIB_X11}
            )
endif()

set(WSCLIENT_LIB webrtcws)
set(TGVOIP_LIB tgvoip)

add_definitions(-DTGVOIP_USE_CALLBACK_AUDIO_IO)

add_subdirectory(tgvoip)
add_subdirectory(tgvoipcall)
