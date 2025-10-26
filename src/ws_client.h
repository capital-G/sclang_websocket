#pragma once

#include <queue>
#include <thread>

#include <boost/beast/core.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "ws_common.h"
#include "boost/beast/websocket/stream.hpp"

namespace beast = boost::beast;
using tcp = boost::asio::ip::tcp;

using ConnectionChangeCallback = std::function<void(bool)>;
using MessageCallback = std::function<void(WebSocketData&)>;

// the client consumes from an external websocket server
// see https://www.boost.org/doc/libs/latest/libs/beast/example/websocket/client/async/websocket_client_async.cpp
class WebSocketClient : public std::enable_shared_from_this<WebSocketClient> {
    std::string mHost;
    int mPort;

    boost::asio::io_context& mIoContext;  // @todo maybe do not store this here?
    boost::asio::ip::tcp::resolver mResolver;
    beast::websocket::stream<beast::tcp_stream> mWs;
    beast::flat_buffer mBuffer;
    bool mConnected = false;
    bool mIsWriting = false;
    std::queue<WebSocketData> mOutQueue;

public:
    explicit WebSocketClient(boost::asio::io_context&, std::string host, int port);

    void connect();

    beast::error_code closeConnection();

    // send a message to the server via a queue
    void enqueueMessage(WebSocketData& message);

    // sclang callbacks - pay attention...
    ConnectionChangeCallback mSclangConnectionChangeCallback;
    MessageCallback mSclangOnMessageCallback;

private:
    void onResolve(beast::error_code ec, boost::asio::ip::tcp::resolver::results_type results);

    void onConnect(beast::error_code ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint);

    void onHandshake(beast::error_code ec);

    void doRead();

    void onRead(beast::error_code ec, std::size_t bytesTransferred);

    void onClose() { mConnected = false; }

    void doWrite();

    void onWrite(beast::error_code ec, std::size_t bytesTransferred);
};
