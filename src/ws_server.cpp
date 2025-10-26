#include <queue>
#include <iostream>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>

#include "ws_server.h"


#define SC_WEBSOCKET_DEBUG

namespace beast = boost::beast;
using tcp = boost::asio::ip::tcp;


namespace SC_Websocket {

std::shared_ptr<WebSocketThread> WebSocketThread::getInstance() {
    static std::shared_ptr<WebSocketThread> instance_ = std::make_shared<WebSocketThread>();
    return instance_;
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

WebSocketSession::WebSocketSession(boost::asio::ip::tcp::socket&& socket, int listeningPort):
    mWs(std::move(socket)),
    mListeningPort(listeningPort) {}

void WebSocketSession::run() {
#ifdef SC_WEBSOCKET_DEBUG
    std::cout << "Start run of session" << std::endl;
#endif
    boost::asio::dispatch(mWs.get_executor(), beast::bind_front_handler(&WebSocketSession::onRun, shared_from_this()));
}

void WebSocketSession::enqueueMessage(WebSocketData message) {
    // dispatch via asio to ensure thread safety
    boost::asio::dispatch(mWs.get_executor(), [message, self = shared_from_this()]() mutable {
        self->mOutQueue.push(message);
        self->doWrite();
    });
}

void WebSocketSession::close() { mWs.close("Goodbye"); }

boost::asio::const_buffer WebSocketSession::toAsioBuffer(const WebSocketData& message) {
    return std::visit([](const auto& arg) { return boost::asio::buffer(arg); }, message);
}

void WebSocketSession::onRun() {
#ifdef SC_WEBSOCKET_DEBUG
    std::cout << "Session started" << std::endl;
#endif
    mWs.set_option(beast::websocket::stream_base::timeout::suggested((beast::role_type::server)));

    mWs.async_accept(beast::bind_front_handler(&WebSocketSession::onAccept, shared_from_this()));
}

void WebSocketSession::onAccept(beast::error_code ec) {
    if (ec) {
#ifdef SC_WEBSOCKET_DEBUG
        std::cout << "Session accept error: " << ec.message().c_str() << std::endl;
#endif
        return;
    }
#ifdef SC_WEBSOCKET_DEBUG
    std::cout << "Session accepted" << std::endl;
#endif

    // SC_Websocket_Lang::WebSocketConnection::newLangConnection(m_ownAddress, m_listeningPort);
    doRead();
}

void WebSocketSession::doRead() {
    mWs.async_read(mBuffer, beast::bind_front_handler(&WebSocketSession::onRead, shared_from_this()));
}

void WebSocketSession::onRead(beast::error_code ec, std::size_t bytesTransferred) {
    if (ec) {
        if (ec == boost::asio::error::eof || ec == beast::websocket::error::closed
            || ec == boost::asio::error::operation_aborted) {
#ifdef SC_WEBSOCKET_DEBUG
            std::cout << "Session closed" << std::endl;
#endif
        } else {
            std::cout << "Websocket connection error: " << ec.message().c_str() << std::endl;
        };
        // SC_Websocket_Lang::WebSocketConnection::closeLangConnection(m_ownAddress);
        return;
    }
    auto message = convertData(mBuffer, bytesTransferred, mWs.got_text());
    // SC_Websocket_Lang::WebSocketConnection::receiveLangMessage(m_ownAddress, message);
    // continue async loop to await websocket message
    doRead();
}

void WebSocketSession::doWrite() {
    if (!mIsWriting && !mOutQueue.empty()) {
        mIsWriting = true;
        auto message = mOutQueue.front();

        // if a string, indicate it as a text message
        mWs.text(std::holds_alternative<std::string>(message));

        mWs.async_write(boost::asio::buffer(toAsioBuffer(message)),
                        boost::beast::bind_front_handler(&WebSocketSession::onWrite, shared_from_this()));
    }
}

void WebSocketSession::onWrite(beast::error_code ec, std::size_t bytesTransferred) {
    mIsWriting = false;
    if (ec) {
        std::cout << "Sending websocket message failed: " << ec.message().c_str() << std::endl;
    }
    mOutQueue.pop();
    // do this loop until our queue is empty
    doWrite();
}

WebSocketListener::WebSocketListener(const std::shared_ptr<WebSocketThread>& webSocketThread,
                                     boost::asio::ip::tcp::endpoint endpoint, boost::beast::error_code& ec):
    mIoContext(webSocketThread->getContext()),
    mAcceptor(webSocketThread->getContext()),
    mThread(webSocketThread) {
    mAcceptor.open(endpoint.protocol(), ec);
    if (ec) {
#ifdef SC_WEBSOCKET_DEBUG
        std::cout << "Could not open endpoint - " << ec.message().c_str() << std::endl;
#endif
        return;
    }

    mAcceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true), ec);
    if (ec) {
#ifdef SC_WEBSOCKET_DEBUG
        std::cout << "Could not set reuse address: " << ec.message().c_str() << std::endl;
#endif
        return;
    }

    mAcceptor.bind(endpoint, ec);
    if (ec) {
#ifdef SC_WEBSOCKET_DEBUG
        std::cout << "Could not bind to endpoint: " << ec.message().c_str() << std::endl;
#endif
        return;
    }

    mAcceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
#ifdef SC_WEBSOCKET_DEBUG
        std::cout << "Could not listen on endpoint: " << ec.message().c_str() << std::endl;
#endif
        return;
    }
}

void WebSocketListener::run() {
#ifdef SC_WEBSOCKET_DEBUG
    std::cout << "Starting websocket listener..." << std::endl;
#endif
    doAccept();
}

void WebSocketListener::stop() {
    boost::system::error_code ec;
    mAcceptor.close(ec);
    if (ec) {
        std::cout << "Could not close websocket: " << ec.message().c_str() << std::endl;
    }
}

void WebSocketListener::doAccept() {
#ifdef SC_WEBSOCKET_DEBUG
    std::cout << "Starting websocket accept..." << std::endl;
#endif
    mAcceptor.async_accept(boost::asio::make_strand(mIoContext),
                           beast::bind_front_handler(&WebSocketListener::onAccept, shared_from_this()));
}

void WebSocketListener::onAccept(beast::error_code ec, boost::asio::ip::tcp::socket socket) {
    if (ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }
        std::cout << "Could not accept connection: " << ec.message().c_str() << std::endl;
        return;
    }
#ifdef SC_WEBSOCKET_DEBUG
    std::cout << "accepted connection" << std::endl;
#endif
    auto session = std::make_shared<WebSocketSession>(std::move(socket), mAcceptor.local_endpoint().port());
    // session->m_ownAddress = session.get();
    session->run();
    doAccept();
}
}
