#include "ws_client.h"

#include "ws_server.h"
#include "boost/asio/strand.hpp"

#include <iostream>



namespace beast = boost::beast;
using tcp = boost::asio::ip::tcp;


WebSocketClient::WebSocketClient(boost::asio::io_context& ioContext):
    mIoContext(ioContext),
    mResolver(boost::asio::make_strand(mIoContext)),
    mWs(boost::asio::make_strand(mIoContext)) {
    std::cout << "ioContext address = " << &ioContext << std::endl;
}

void WebSocketClient::run(const std::string& host_, std::string& port) {
    mHost = host_;

    mResolver.async_resolve(mHost, port, beast::bind_front_handler(&WebSocketClient::onResolve, shared_from_this()));
}

beast::error_code WebSocketClient::closeConnection() {
    beast::error_code ec;
    mWs.close(beast::websocket::close_code::normal, ec);
    return ec;
}

void WebSocketClient::enqueueMessage(WebSocketData& message) {
    // dispatch via asio to ensure thread safety
    boost::asio::dispatch(mWs.get_executor(), [message, self = shared_from_this()]() mutable {
        self->mOutQueue.push(message);
        self->doWrite();
    });
}

void WebSocketClient::onResolve(beast::error_code ec, boost::asio::ip::tcp::resolver::results_type results) {
    if (ec) {
        std::cout << "Could not resolve host: " << ec.message().c_str() << std::endl;
        return;
    }
    beast::get_lowest_layer(mWs).async_connect(
        results, beast::bind_front_handler(&WebSocketClient::onConnect, shared_from_this()));
}

void WebSocketClient::onConnect(beast::error_code ec,
                                boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint) {
    if (ec) {
        std::cout << "Could not connect to host: " << ec.message().c_str() << std::endl;
        return;
    }
    // Set a decorator to change the User-Agent of the handshake
    mWs.set_option(beast::websocket::stream_base::decorator([](beast::websocket::request_type& req) {
        req.set(beast::http::field::user_agent, std::string("sclang websocket-client"));
    }));

    // Update the host_ string. This will provide the value of the
    // Host HTTP header during the WebSocket handshake.
    // See https://tools.ietf.org/html/rfc7230#section-5.4
    mHost += ':' + std::to_string(endpoint.port());

    // Perform the websocket handshake
    mWs.async_handshake(mHost, "/", beast::bind_front_handler(&WebSocketClient::onHandshake, shared_from_this()));
}

void WebSocketClient::onHandshake(beast::error_code ec) {
    if (ec) {
        std::cout << "Could not handshake: " << ec.message().c_str() << std::endl;
    }
    mConnected = true;
    // SC_Websocket_Lang::WebSocketClient::setConnectionStatus(m_self, true);
    doRead();
}

void WebSocketClient::doRead() {
    mWs.async_read(mBuffer, beast::bind_front_handler(&WebSocketClient::onRead, shared_from_this()));
}

void WebSocketClient::onRead(beast::error_code ec, std::size_t bytesTransferred) {
    if (ec) {
        mConnected = false;
        // SC_Websocket_Lang::WebSocketClient::setConnectionStatus(m_self, false);
        if (ec == boost::system::errc::operation_canceled || ec == boost::asio::error::eof) {
            return;
        }
        std::cout << "Failed to read websocket message: " << ec.message().c_str() << std::endl;
        return;
    };

    auto message = convertData(mBuffer, bytesTransferred, mWs.got_text());

    // SC_Websocket_Lang::WebSocketClient::receivedMessage(m_self, message);

    doRead();
}

void WebSocketClient::doWrite() {
    if (!mConnected) {
        std::cout << "WebSocket client is not connected - can not send out message" << std::endl;
        return;
    }
    if (!mIsWriting && !mOutQueue.empty()) {
        mIsWriting = true;
        auto message = mOutQueue.front();

        mWs.text(std::holds_alternative<std::string>(message));
        mWs.async_write(boost::asio::buffer(SC_Websocket::WebSocketSession::toAsioBuffer(message)),
                        beast::bind_front_handler(&WebSocketClient::onWrite, shared_from_this()));
    }
}

void WebSocketClient::onWrite(beast::error_code ec, std::size_t bytesTransferred) {
    mIsWriting = false;
    if (ec) {
        std::cout << "Failed to write websocket message: " << ec.message().c_str() << std::endl;
    }
    mOutQueue.pop();
    doWrite();
}
