#pragma once

#include <hiredis/hiredis.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace storeforge::session {

class RedisReply {
public:
    explicit RedisReply(redisReply* reply = nullptr);
    RedisReply(RedisReply&& other) noexcept;
    RedisReply& operator=(RedisReply&& other) noexcept;
    RedisReply(const RedisReply&) = delete;
    RedisReply& operator=(const RedisReply&) = delete;
    ~RedisReply();

    redisReply* get() const;
    redisReply& operator*() const;
    redisReply* operator->() const;

private:
    redisReply* reply_;
};

class RedisClient {
public:
    RedisClient(std::string host, int port, int database, std::string password);
    ~RedisClient();

    RedisReply command(const std::vector<std::string>& args);
    bool ping();

private:
    void connect();
    void disconnect();
    RedisReply command_locked(const std::vector<std::string>& args);

    std::string host_;
    int port_;
    int database_;
    std::string password_;
    redisContext* context_{};
    std::mutex mutex_;
};

}
