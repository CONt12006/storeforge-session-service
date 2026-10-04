#include "session/config.hpp"
#include "session/http_server.hpp"
#include "session/redis_client.hpp"
#include "session/request_router.hpp"
#include "session/session_service.hpp"
#include "session/session_store.hpp"

#include <iostream>
#include <memory>

int main() {
    try {
        auto config = storeforge::session::Config::from_environment();
        auto redis = std::make_shared<storeforge::session::RedisClient>(
            config.redis_host(),
            config.redis_port(),
            config.redis_database(),
            config.redis_password()
        );
        auto store = std::make_shared<storeforge::session::RedisSessionStore>(redis);
        auto service = std::make_shared<storeforge::session::SessionService>(
            store,
            config.max_sessions_per_user()
        );
        auto router = std::make_shared<storeforge::session::RequestRouter>(service, config.api_key());
        storeforge::session::HttpServer server(config.host(), config.port(), router);
        server.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "fatal error: " << error.what() << std::endl;
        return 1;
    }
}
