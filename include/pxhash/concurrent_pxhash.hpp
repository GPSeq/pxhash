#ifndef PXHASH_CONCURRENT_PXHASH_HPP
#define PXHASH_CONCURRENT_PXHASH_HPP

#include "pxhash/pxhash.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pxhash {

template <typename KeyType, typename ValueType, typename Hash = std::hash<KeyType>,
          typename Eq = std::equal_to<KeyType>>
class ConcurrentPXHash {
public:
  using key_type = KeyType;
  using mapped_type = ValueType;
  using size_type = std::size_t;

  explicit ConcurrentPXHash(size_type shard_count = 64, Hash hash = Hash{}, Eq eq = Eq{})
      : hasher_(std::move(hash)), eq_(std::move(eq)) {
    if (shard_count == 0) throw std::invalid_argument("ConcurrentPXHash requires at least one shard");
    shards_.reserve(shard_count);
    for (size_type i = 0; i < shard_count; ++i) {
      shards_.push_back(std::make_unique<Shard>(hasher_, eq_));
    }
  }

  ConcurrentPXHash(const ConcurrentPXHash&) = delete;
  ConcurrentPXHash& operator=(const ConcurrentPXHash&) = delete;
  ConcurrentPXHash(ConcurrentPXHash&&) noexcept = default;
  ConcurrentPXHash& operator=(ConcurrentPXHash&&) noexcept = default;

  [[nodiscard]] size_type shard_count() const noexcept { return shards_.size(); }

  [[nodiscard]] size_type size() const {
    size_type total = 0;
    for (const auto& shard : shards_) {
      std::shared_lock lock(shard->mutex);
      total += shard->map.size();
    }
    return total;
  }

  [[nodiscard]] bool empty() const { return size() == 0; }

  void clear() {
    for (auto& shard : shards_) {
      std::unique_lock lock(shard->mutex);
      shard->map.clear();
    }
  }

  void reserve(size_type n) {
    const size_type per_shard = (n + shards_.size() - 1) / shards_.size();
    for (auto& shard : shards_) {
      std::unique_lock lock(shard->mutex);
      shard->map.reserve(per_shard);
    }
  }

  bool insert(const KeyType& key, const ValueType& value) { return insert_or_assign(key, value); }

  bool insert(KeyType&& key, ValueType&& value) {
    return insert_or_assign(std::move(key), std::move(value));
  }

  template <class KArg, class VArg>
  bool insert_or_assign(KArg&& key, VArg&& value) {
    Shard& shard = shard_for(key);
    std::unique_lock lock(shard.mutex);
    return shard.map.insert_or_assign(std::forward<KArg>(key), std::forward<VArg>(value));
  }

  template <class... Args>
  bool try_emplace(const KeyType& key, Args&&... args) {
    Shard& shard = shard_for(key);
    std::unique_lock lock(shard.mutex);
    return shard.map.try_emplace(key, std::forward<Args>(args)...);
  }

  template <class... Args>
  bool try_emplace(KeyType&& key, Args&&... args) {
    Shard& shard = shard_for(key);
    std::unique_lock lock(shard.mutex);
    return shard.map.try_emplace(std::move(key), std::forward<Args>(args)...);
  }

  bool find(const KeyType& key, ValueType& out_value) const {
    const Shard& shard = shard_for(key);
    std::shared_lock lock(shard.mutex);
    return shard.map.find(key, out_value);
  }

  [[nodiscard]] bool contains(const KeyType& key) const {
    const Shard& shard = shard_for(key);
    std::shared_lock lock(shard.mutex);
    return shard.map.contains(key);
  }

  bool erase(const KeyType& key) {
    Shard& shard = shard_for(key);
    std::unique_lock lock(shard.mutex);
    return shard.map.erase(key);
  }

private:
  struct Shard {
    Shard(const Hash& hash, const Eq& eq) : map(0, hash, eq) {}

    mutable std::shared_mutex mutex;
    PXHash<KeyType, ValueType, Hash, Eq> map;
  };

  template <class LookupKey>
  size_type shard_index(const LookupKey& key) const {
    return mix_hash(hasher_(key)) % shards_.size();
  }

  template <class LookupKey>
  Shard& shard_for(const LookupKey& key) {
    return *shards_[shard_index(key)];
  }

  template <class LookupKey>
  const Shard& shard_for(const LookupKey& key) const {
    return *shards_[shard_index(key)];
  }

  Hash hasher_{};
  Eq eq_{};
  std::vector<std::unique_ptr<Shard>> shards_;
};

}  // namespace pxhash

#endif
