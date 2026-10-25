#pragma once
#include <memory>
#include <string>

namespace loblox::game {
// Host-only policy: imported Lua never chooses an identity or filesystem path.
struct ProgressOptions {
    std::string identity;
    std::string directory;
    bool persistent=true;
};
class ProgressStore {
public:
    ProgressStore();~ProgressStore();
    ProgressStore(const ProgressStore&)=delete;
    ProgressStore& operator=(const ProgressStore&)=delete;
    void open(const ProgressOptions& options);
    std::string get(const std::string& key); // empty string means absent; otherwise JSON
    void set(const std::string& key,const std::string& json);
    bool remove(const std::string& key);
    bool persistent() const;
    bool pending() const;
    const std::string& error() const;
    void tick(float dt);
    void flush();
    void close();
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
