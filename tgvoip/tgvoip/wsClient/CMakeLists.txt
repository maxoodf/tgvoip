project(wsclient)
cmake_minimum_required(VERSION 3.5)

set(PROJECT_INCLUDE_DIR ${PROJECT_ROOT_DIR})
set(PROJECT_SOURCE_DIR .)

set(PRJ_SRCS
        ${PROJECT_SOURCE_DIR}/wsClient.h
        ${PROJECT_SOURCE_DIR}/wsClient.cpp
        )

add_library(${WSCLIENT_LIB} STATIC ${PRJ_SRCS})
target_link_libraries(${WSCLIENT_LIB}
        ${LIBWEBSOCKETS_LIBRARIES}
        ${LIBS}
        )
