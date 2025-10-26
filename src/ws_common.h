#pragma once

#include <thread>

#include <boost/beast.hpp>

// a websocket message can either be a byte array or a string
// see https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/message_event
using WebSocketData = std::variant<std::vector<uint8_t>, std::string>;

// converts a raw beast buffer into a consumable data object
WebSocketData convertData(boost::beast::flat_buffer& buffer, size_t bytesTransferred, bool isText);

/** A wrapper class for the websocket communication thread.
This gets initiated into a static variable upon request.
Due to this static lifetime the singleton only gets deleted when sclang closes.
*/
class WebSocketThread {
public:
    static std::shared_ptr<WebSocketThread> instance();

    // Deleted copy/move constructors
    WebSocketThread(const WebSocketThread&) = delete;
    WebSocketThread& operator=(const WebSocketThread&) = delete;
    WebSocketThread(WebSocketThread&&) = delete;
    WebSocketThread& operator=(WebSocketThread&&) = delete;

    boost::asio::io_context& getContext();

    void start();

    void stop();

    ~WebSocketThread();

    // how to make this private?
    WebSocketThread();

private:
    boost::asio::io_context mIoContext;
    std::shared_ptr<std::thread> mThread;
};
