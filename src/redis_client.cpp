#include "session/redis_client.hpp"

#include <stdexcept>

namespace storeforge::session {

RedisReply::RedisReply(redisReply* reply) : reply_(reply) {}

RedisReply::RedisReply(RedisReply&& other) noexcept : reply_(other.reply_) {
    other.reply_ = nullptr;
}

RedisReply& RedisReply::operator=(RedisReply&& other) noexcept {
    if (this != &other) {
        if (reply_ != nullptr) {
            freeReplyObject(reply_);
        }
        reply_ = other.reply_;
        other.reply_ = nullptr;
    }
    return *this;
}

RedisReply::~RedisReply() {
    if (reply_ != nullptr) {
        freeReplyObject(reply_);
    }
}

redisReply* RedisReply::get() const { return reply_; }
redisReply& RedisReply::operator*() const { return *reply_; }
redisReply* RedisReply::operator->() const { return reply_; }

RedisClient::RedisClient(std::string host, int port, int database, std::string password)
    : host_(std::move(host)), port_(port), database_(database), password_(std::move(password)) {
    connect();
}

RedisClient::~RedisClient() {
    disconnect();
}

void RedisClient::connect() {
    disconnect();
    context_ = redisConnect(host_.c_str(), port_);

    if (context_ == nullptr || context_->err != 0) {
        std::string message = context_ == nullptr ? "unable to allocate Redis context" : context_->errstr;
        disconnect();
        throw std::runtime_error("Redis connection failed: " + message);
    }

    if (!password_.empty()) {
        auto reply = command_locked({"AUTH", password_});
        if (reply.get() == nullptr || reply->type == REDIS_REPLY_ERROR) {
            throw std::runtime_error("Redis authentication failed");
        }
    }

    auto reply = command_locked({"SELECT", std::to_string(database_)});
    if (reply.get() == nullptr || reply->type == REDIS_REPLY_ERROR) {
        throw std::runtime_error("Redis database selection failed");
    }
}

void RedisClient::disconnect() {
    if (context_ != nullptr) {
        redisFree(context_);
        context_ = nullptr;
    }
}

RedisReply RedisClient::command_locked(const std::vector<std::string>& args) {
    std::vector<const char*> argv;
    std::vector<std::size_t> lengths;
    argv.reserve(args.size());
    lengths.reserve(args.size());

    for (const auto& arg : args) {
        argv.push_back(arg.data());
        lengths.push_back(arg.size());
    }

    auto* raw = static_cast<redisReply*>(redisCommandArgv(
        context_,
        static_cast<int>(argv.size()),
        argv.data(),
        lengths.data()
    ));

    if (raw == nullptr) {
        throw std::runtime_error("Redis command failed");
    }

    RedisReply reply(raw);
    if (reply->type == REDIS_REPLY_ERROR) {
        throw std::runtime_error(std::string("Redis error: ") + reply->str);
    }

    return reply;
}

RedisReply RedisClient::command(const std::vector<std::string>& args) {
    std::lock_guard lock(mutex_);

    try {
        return command_locked(args);
    } catch (...) {
        connect();
        return command_locked(args);
    }
}

bool RedisClient::ping() {
    try {
        auto reply = command({"PING"});
        return reply.get() != nullptr && reply->type == REDIS_REPLY_STATUS && std::string(reply->str, reply->len) == "PONG";
    } catch (...) {
        return false;
    }
}

}
