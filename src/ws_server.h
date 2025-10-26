#pragma once

#include <queue>
#include <thread>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "ws_common.h"

namespace beast = boost::beast;
using tcp = boost::asio::ip::tcp;

// websocket server implementation using boost beast
// communication with primitives is handeled in `SC_WebSocketPrim`.
// See boost beast documentation for `do_read` -> `on_read` worklflow.
namespace SC_Websocket {

/** A wrapper class for the websocket communication thread.
This gets initiated into a static variable upon request.
Due to this static lifetime the singleton only gets deleted when sclang closes.
*/
class WebSocketThread {
public:
    static std::shared_ptr<WebSocketThread> getInstance();

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

// A WebSocketSession is essentially a websocket connection from our WebSocket server.
// This gets build by the WebSocketListener.
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
    beast::websocket::stream<tcp::socket> mWs;
    beast::flat_buffer mBuffer;
    std::queue<WebSocketData> mOutQueue;
    // a primitive mutex - see `do_write` and `on_write` - @todo use atomic in this case?
    bool mIsWriting = false;
    int mListeningPort;

public:
    // we store a reference pointer to ourselves upon creation
    // this pointer acts as an identifier on the sclang side,
    // so it is possible to distinguish connections and we can
    // call the callback on the matching sclang WebSocketConnection
    // WebSocketSession* m_ownAddress;

    // take ownership of socket
    explicit WebSocketSession(boost::asio::ip::tcp::socket&& socket, int listeningPort);

    void run();

    void enqueueMessage(WebSocketData message);

    void close();

    // converts WebSocketData to beast flat_buffer
    static boost::asio::const_buffer toAsioBuffer(const WebSocketData& message);

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
    std::shared_ptr<WebSocketThread> mThread;

public:
    // take ownership shared ptr of our web socket thread so we maintain the lifetime of the thread
    WebSocketListener(const std::shared_ptr<WebSocketThread>& webSocketThread, boost::asio::ip::tcp::endpoint endpoint,
                      boost::beast::error_code& ec);

    void run();

    void stop();

private:
    void doAccept();

    void onAccept(beast::error_code ec, boost::asio::ip::tcp::socket socket);
};
}
