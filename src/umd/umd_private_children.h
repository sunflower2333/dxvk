#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace dxvk::umd {

// Runtime private storage is a key, never a callable object. Each creation
// has a distinct ticket so a failed, suspended creator cannot erase a newer
// object at the same address. Epoch ownership also survives device address
// reuse. Lookup and removal pin the child without touching runtime bytes.
template<typename Object, typename Epoch>
class PrivateChildren final {
  struct Record {
    std::shared_ptr<Epoch> epoch;
    std::shared_ptr<const char> ticket;
    std::shared_ptr<Object> object;
  };
public:
  class Creation final {
    friend class PrivateChildren;
    PrivateChildren* registry = nullptr;
    void* storage = nullptr;
    std::shared_ptr<const char> ticket;
    Creation(PrivateChildren* value, void* key, std::shared_ptr<const char> token)
    : registry(value), storage(key), ticket(std::move(token)) { }
  public:
    Creation() = default;
    Creation(const Creation&) = delete;
    Creation& operator=(const Creation&) = delete;
    Creation(Creation&& other) noexcept
    : registry(std::exchange(other.registry, nullptr)), storage(other.storage), ticket(std::move(other.ticket)) { }
    ~Creation() { if (registry) registry->cancel(storage, ticket); }
    explicit operator bool() const { return registry != nullptr; }
    bool publish(const std::shared_ptr<Object>& object) {
      if (!registry || !object) return false;
      std::lock_guard<std::mutex> guard(registry->mutex);
      const auto entry = registry->records.find(storage);
      if (entry == registry->records.end() || entry->second.ticket != ticket || entry->second.object)
        return false;
      entry->second.object = object;
      registry = nullptr;
      return true;
    }
  };
  Creation begin(void* storage, const std::shared_ptr<Epoch>& epoch) {
    if (!storage || !epoch) return {};
    auto ticket = std::make_shared<const char>(0);
    std::lock_guard<std::mutex> guard(mutex);
    if (!records.emplace(storage, Record{epoch, ticket, {}}).second) return {};
    return Creation(this, storage, std::move(ticket));
  }
  std::shared_ptr<Object> lookup(void* storage, const std::shared_ptr<Epoch>& epoch) {
    std::lock_guard<std::mutex> guard(mutex);
    const auto entry = records.find(storage);
    return entry != records.end() && entry->second.epoch == epoch ? entry->second.object : nullptr;
  }
  std::shared_ptr<Object> remove(void* storage, const std::shared_ptr<Epoch>& epoch,
      const std::shared_ptr<Object>& expected) {
    std::lock_guard<std::mutex> guard(mutex);
    const auto entry = records.find(storage);
    if (entry == records.end() || entry->second.epoch != epoch || !expected || entry->second.object != expected)
      return {};
    auto object = std::move(entry->second.object);
    records.erase(entry);
    return object;
  }
  template<typename Retire>
  void clear(const std::shared_ptr<Epoch>& epoch, Retire&& retire) {
    for (;;) {
      std::shared_ptr<Object> object;
      {
        std::lock_guard<std::mutex> guard(mutex);
        auto entry = records.begin();
        while (entry != records.end()) {
          if (entry->second.epoch != epoch) { ++entry; continue; }
          object = std::move(entry->second.object);
          entry = records.erase(entry);
          if (object) break;
        }
      }
      if (!object) return;
      // Retirement can reenter the registry; never hold its lock here.
      retire(std::move(object));
    }
  }
private:
  void cancel(void* storage, const std::shared_ptr<const char>& ticket) {
    std::lock_guard<std::mutex> guard(mutex);
    const auto entry = records.find(storage);
    if (entry != records.end() && entry->second.ticket == ticket && !entry->second.object)
      records.erase(entry);
  }
  std::mutex mutex;
  std::unordered_map<void*, Record> records;
};

}
