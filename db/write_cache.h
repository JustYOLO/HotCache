#pragma once

#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "port/port.h"
#include "rocksdb/slice.h"

namespace ROCKSDB_NAMESPACE {

// A write cache that sits above the memtable. Capacity is tracked in bytes.
class WriteCache {
 public:
  using EvictionCallback =
      std::function<void(const Slice& key, const Slice& value)>;

  explicit WriteCache(size_t capacity_bytes = kDefaultCacheCapacityBytes,
                      EvictionCallback eviction_callback = nullptr);
  ~WriteCache();

  bool Put(const Slice& key, const Slice& value);

  // Gets a value for a key. Returns true if found.
  // `value` is an out-parameter.
  bool Get(const Slice& key, std::string* value);

  // Called during flush to update the cache with a potential candidate key
  // and its frequency.
  void Update(const Slice& key, uint64_t count);

 private:
  struct CacheEntry {
    std::string value;
    uint64_t frequency;
    // Iterator to the list of keys in freq_lists_
    std::list<std::string>::iterator lfu_iterator;
  };
  struct EvictedEntry {
    std::string key;
    std::string value;
    bool has_value = false;
  };
  constexpr static size_t kDefaultCacheCapacityBytes = 64 << 20;

  std::unordered_map<std::string, CacheEntry> cache_;
  // Map from frequency to a list of keys with that frequency
  std::map<uint64_t, std::list<std::string>> freq_lists_;
  size_t capacity_bytes_;
  size_t current_bytes_ = 0;
  EvictionCallback eviction_callback_;

  // Mutex for thread-safe access to the cache.
  port::Mutex mutex_;

  // Private helper to update a key's frequency and move it
  void Touch(typename std::unordered_map<std::string, CacheEntry>::iterator it);
  // Private helper for eviction
  bool EvictLocked(EvictedEntry* evicted);
  size_t EntryCharge(const std::string& key, const CacheEntry& entry) const;
};

}  // namespace ROCKSDB_NAMESPACE
