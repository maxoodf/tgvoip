project(tgwss)
cmake_minimum_required(VERSION 3.5)

set(PROJECT_ROOT_DIR ${CMAKE_SOURCE_DIR})
set(CMAKE_BINARY_DIR ${PROJECT_ROOT_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -Werror -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -funroll-loops -ftree-vectorize -fPIC -DNDEBUG")

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    set(LIBS "-pthread")
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
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

find_package(fmt)
include_directories(${fmt_INCLUDE_DIRS})
set(FMT_LIB "fmt::fmt")

set(WS_SERVER ${PROJECT_NAME})
set(SERVER_FILES
        ${PROJECT_SOURCE_DIR}/logger/logger.h
        ${PROJECT_SOURCE_DIR}/logger/logger.cpp
        ${PROJECT_SOURCE_DIR}/json/parser.h
        ${PROJECT_SOURCE_DIR}/json/parser.cpp
        ${PROJECT_SOURCE_DIR}/json/confParser.h
        ${PROJECT_SOURCE_DIR}/json/confParser.cpp
        ${PROJECT_SOURCE_DIR}/wss/wsServer.h
        ${PROJECT_SOURCE_DIR}/wss/wsServer.cpp
        ${PROJECT_SOURCE_DIR}/wss/main.cpp
        )
add_executable(${WS_SERVER} ${SERVER_FILES})
target_link_libraries(${WS_SERVER}
        ${LIBWEBSOCKETS_LIBRARIES}
        ${FMT_LIB}
        ${LIBS}
        )
