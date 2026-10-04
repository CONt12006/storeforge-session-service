#pragma once

#include "session/request_router.hpp"

#include <boost/asio.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace storeforge::session {

class HttpServer {
public:
    HttpServer(std::string host, std::uint16_t port, std::shared_ptr<RequestRouter> router);
    void run();

private:
    void serve_connection(boost::asio::ip::tcp::socket socket);

    std::string host_;
    std::uint16_t port_;
    std::shared_ptr<RequestRouter> router_;
};

}
