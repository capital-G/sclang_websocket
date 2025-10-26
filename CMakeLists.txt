cmake_minimum_required(VERSION 3.12)
project(sclang_websocket)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_SHARED_LIBRARY_SUFFIX ".gluon")
set(CMAKE_SHARED_LIBRARY_PREFIX "")

add_library(sclang_websocket SHARED
    src/ws_server.cpp
    src/ws_gluon.cpp
    src/ws_client.cpp
    src/ws_common.cpp
)

# supercollider
target_include_directories(sclang_websocket PUBLIC
    ${SC_SRC_PATH}/include/lang
)

# boost
target_include_directories(sclang_websocket PRIVATE
    ${CMAKE_SOURCE_DIR}/external/boost
)

# install
install(TARGETS sclang_websocket LIBRARY DESTINATION ${PROJECT_NAME})

file(GLOB_RECURSE SC_FILES "${CMAKE_SOURCE_DIR}/src/Classes/*.sc")
install(FILES ${SC_FILES} DESTINATION ${PROJECT_NAME}/Classes)

file(GLOB_RECURSE SCHELP_FILES "${CMAKE_SOURCE_DIR}/src/HelpSource/Classes/*.schelp")
install(FILES ${SCHELP_FILES} DESTINATION ${PROJECT_NAME}/HelpSource/Classes)
