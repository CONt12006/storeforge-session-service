#include "session/session_service.hpp"

#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

using namespace storeforge::session;

class MemorySessionStore final : public ISessionStore {
public:
    void create(const CreateSessionCommand& command, std::size_t) override {
        SessionRecord record;
        record.id = command.session_id;
        record.user_id = command.user_id;
        record.token_hash = command.token_hash;
        records_[record.id] = record;
    }

    std::optional<SessionRecord> find(const std::string& session_id) override {
        auto it = records_.find(session_id);
        if (it == records_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    bool rotate(const RotateSessionCommand& command) override {
        auto it = records_.find(command.session_id);
        if (it == records_.end() || it->second.token_hash != command.current_token_hash) {
            return false;
        }
        it->second.token_hash = command.new_token_hash;
        return true;
    }

    bool revoke(const std::string& session_id, std::optional<std::int64_t> user_id) override {
        auto it = records_.find(session_id);
        if (it == records_.end()) {
            return false;
        }
        if (user_id.has_value() && it->second.user_id != *user_id) {
            return false;
        }
        records_.erase(it);
        return true;
    }

    std::size_t revoke_all(std::int64_t user_id) override {
        std::size_t removed{};
        for (auto it = records_.begin(); it != records_.end();) {
            if (it->second.user_id == user_id) {
                it = records_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }

    std::vector<SessionRecord> list(std::int64_t user_id) override {
        std::vector<SessionRecord> result;
        for (const auto& [id, record] : records_) {
            if (record.user_id == user_id) {
                result.push_back(record);
            }
        }
        return result;
    }

    bool healthy() override { return true; }

private:
    std::unordered_map<std::string, SessionRecord> records_;
};

int main() {
    auto store = std::make_shared<MemorySessionStore>();
    SessionService service(store, 5);
    const std::string first_hash(64, 'a');
    const std::string second_hash(64, 'b');

    CreateSessionCommand create;
    create.session_id = "session-1";
    create.user_id = 42;
    create.token_hash = first_hash;
    create.ttl_seconds = 3600;

    auto created = service.create(create);
    assert(created.user_id == 42);
    assert(service.verify("session-1", first_hash).valid);
    assert(!service.verify("session-1", second_hash).valid);

    RotateSessionCommand rotate;
    rotate.session_id = "session-1";
    rotate.current_token_hash = first_hash;
    rotate.new_token_hash = second_hash;
    rotate.ttl_seconds = 3600;

    assert(service.rotate(rotate));
    assert(!service.verify("session-1", first_hash).valid);
    assert(service.verify("session-1", second_hash).valid);
    assert(service.list(42).size() == 1);
    assert(service.revoke("session-1", 42));
    assert(service.list(42).empty());
    return 0;
}
