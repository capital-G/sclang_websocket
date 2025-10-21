cmake_minimum_required(VERSION 3.12)
project(sclang_websocket)
set(CMAKE_CXX_STANDARD 17)

add_library(sclang_websocket SHARED
    src/websocket.cpp
    src/gluon_websocket.cpp
)

# supercollider
target_include_directories(sclang_websocket PUBLIC
    ${SC_SRC_PATH}/include/lang
)

# boost
target_include_directories(sclang_websocket PRIVATE
    ${CMAKE_SOURCE_DIR}/external/boost
)
