#pragma once

#include <queue>
#include <thread>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "ws_common.h"

namespace beast = boost::beast;
using tcp = boost::asio::ip::tcp;

class WebSocketSession;
using NewSessionCallback = std::function<void(std::shared_ptr<WebSocketSession>)>;
using SessionConnectionStateCallback = std::function<void(bool)>;
using SessionMessageReceivedCallback = std::function<void(WebSocketData& message)>;

// websocket server implementation using boost beast
// communication with primitives is handeled in `SC_WebSocketPrim`.
// See boost beast documentation for `do_read` -> `on_read` worklflow.

// A WebSocketSession is essentially a websocket connection from our WebSocket server.
// This gets build by the WebSocketListener.
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
    beast::websocket::stream<tcp::socket> mWs;
    beast::flat_buffer mBuffer;
    std::queue<WebSocketData> mOutQueue;
    // a primitive mutex - see `do_write` and `on_write` - @todo use atomic in this case?
    bool mIsWriting = false;
    int mListeningPort;
    // identifier for sclang side
    int mSessionId;

public:
    // we store a reference pointer to ourselves upon creation
    // this pointer acts as an identifier on the sclang side,
    // so it is possible to distinguish connections and we can
    // call the callback on the matching sclang WebSocketConnection
    // WebSocketSession* m_ownAddress;

    // take ownership of socket
    explicit WebSocketSession(boost::asio::ip::tcp::socket&& socket, int listeningPort, int sessionId);

    void run();

    void enqueueMessage(WebSocketData message);

    void close();

    // converts WebSocketData to beast flat_buffer
    static boost::asio::const_buffer toAsioBuffer(const WebSocketData& message);

    int getSessionId() const { return mSessionId; }

    SessionConnectionStateCallback mConnectionStateCallback;
    SessionMessageReceivedCallback mMessageReceivedCallback;

private:
    void onRun();

    void onAccept(beast::error_code ec);

    void doRead();

    void onRead(beast::error_code ec, std::size_t bytesTransferred);

    void doWrite(void);

    void onWrite(beast::error_code ec, std::size_t bytesTransferred);
};

// acts as a server which listens for incoming connections
class WebSocketListener : public std::enable_shared_from_this<WebSocketListener> {
    boost::asio::io_context& mIoContext;
    boost::asio::ip::tcp::acceptor mAcceptor;
    boost::asio::ip::tcp::endpoint mEndpoint;

public:
    // take ownership shared ptr of our web socket thread so we maintain the lifetime of the thread
    WebSocketListener(
        boost::asio::io_context& context,
        std::string& host,
        int port,
        beast::error_code& ec
    );

    void run();

    void stop();

    NewSessionCallback mNewSessionCallback;

private:
    void doAccept();

    void onAccept(beast::error_code ec, boost::asio::ip::tcp::socket socket);
};
