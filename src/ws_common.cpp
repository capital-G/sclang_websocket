#include "ws_common.h"

#include <iostream>

WebSocketData convertData(boost::beast::flat_buffer& buffer, size_t bytesTransferred, bool isText) {
    WebSocketData data;
    if (isText) {
        data = boost::beast::buffers_to_string(buffer.data());
    } else {
        auto array_data = std::vector<uint8_t>(bytesTransferred);
        std::memcpy(array_data.data(), buffer.data().data(), bytesTransferred);
        data = array_data;
    }
    buffer.consume(bytesTransferred);
    return data;
}

boost::asio::io_context& WebSocketThread::getContext() { return mIoContext; }

void WebSocketThread::start() {
    if (!mThread) {
#ifdef SC_WEBSOCKET_DEBUG
        std::cout << "Start websocket thread" << std::endl;
#endif
        mThread = std::make_shared<std::thread>([this]() {
            auto work = boost::asio::make_work_guard(mIoContext);
            mIoContext.run();
        });
    }
}

void WebSocketThread::stop() {
    mIoContext.stop();
    if (mThread && mThread->joinable()) {
        mThread->join();
    }
    mThread.reset();
}

WebSocketThread::~WebSocketThread() {
    std::cout << "Clean websocket thread" << std::endl;
    stop();
}

WebSocketThread::WebSocketThread() {}
