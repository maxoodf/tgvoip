project(tgvoipcall)

set(PROJECT_INCLUDE_DIR ${PROJECT_ROOT_DIR})
set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(LIBS "-Wl,-rpath,./")

set(TGVOIPTEST ${PROJECT_NAME})
set(TGVOIPTEST_FILES
        ${PROJECT_SOURCE_DIR}/main.cpp
        )
add_executable(${TGVOIPTEST} ${TGVOIPTEST_FILES})
target_link_libraries(${TGVOIPTEST}
        ${TGVOIP_LIB}
        ${LIBS}
        )
