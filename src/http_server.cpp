#include "session/http_server.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <iostream>
#include <thread>

namespace storeforge::session {

HttpServer::HttpServer(std::string host, std::uint16_t port, std::shared_ptr<RequestRouter> router)
    : host_(std::move(host)), port_(port), router_(std::move(router)) {}

void HttpServer::run() {
    boost::asio::io_context io_context{1};
    auto address = boost::asio::ip::make_address(host_);
    boost::asio::ip::tcp::acceptor acceptor(io_context, {address, port_});

    std::cout << "session-service listening on " << host_ << ':' << port_ << std::endl;

    while (true) {
        boost::asio::ip::tcp::socket socket(io_context);
        acceptor.accept(socket);
        std::thread(&HttpServer::serve_connection, this, std::move(socket)).detach();
    }
}

void HttpServer::serve_connection(boost::asio::ip::tcp::socket socket) {
    try {
        boost::beast::flat_buffer buffer;
        HttpRequest request;
        boost::beast::http::read(socket, buffer, request);
        auto response = router_->handle(request);
        boost::beast::http::write(socket, response);
        boost::system::error_code error;
        socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, error);
    } catch (const std::exception& error) {
        std::cerr << "request failed: " << error.what() << std::endl;
    }
}

}
