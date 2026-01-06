#pragma once

#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "port/port.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"

namespace ROCKSDB_NAMESPACE {

// A write cache that sits above the memtable. Capacity is tracked in bytes and
// includes estimated metadata overhead.
class WriteCache {
 public:
  using EvictionCallback =
      std::function<void(const Slice& key, const Slice& value)>;
  enum class RecordType { kPut, kDelete };
  using RecordCallback = std::function<void(RecordType type, uint64_t seq,
                                            const Slice& key,
                                            const Slice* value,
                                            const WriteOptions* write_options)>;
  using OldestSeqCallback = std::function<void(uint64_t oldest_live_seq)>;
  using EvictionStatsCallback = std::function<void(uint64_t evicted_count)>;

  explicit WriteCache(size_t capacity_bytes = kDefaultCacheCapacityBytes,
                      EvictionCallback eviction_callback = nullptr,
                      size_t per_entry_overhead_bytes = 0,
                      RecordCallback record_callback = nullptr,
                      OldestSeqCallback oldest_seq_callback = nullptr,
                      EvictionStatsCallback eviction_stats_callback = nullptr);
  ~WriteCache();

  bool Put(const Slice& key, const Slice& value,
           const WriteOptions& write_options);

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
    uint64_t seq;
    // Iterator to the list of keys in freq_lists_
    std::list<std::string>::iterator lfu_iterator;
  };
  struct EvictedEntry {
    std::string key;
    std::string value;
    bool has_value = false;
  };
  constexpr static size_t kDefaultCacheCapacityBytes = 64 << 20;
  constexpr static size_t kGlobalOverheadBytes = 4 << 10;
  constexpr static uint64_t kFrequencyDecayIntervalOps = 1 << 20;

  std::unordered_map<std::string, CacheEntry> cache_;
  // Map from frequency to a list of keys with that frequency
  std::map<uint64_t, std::list<std::string>> freq_lists_;
  size_t capacity_bytes_;
  size_t current_bytes_ = 0;
  size_t metadata_bytes_ = 0;
  size_t per_entry_overhead_bytes_ = 0;
  EvictionCallback eviction_callback_;
  RecordCallback record_callback_;
  OldestSeqCallback oldest_seq_callback_;
  EvictionStatsCallback eviction_stats_callback_;
  std::multiset<uint64_t> live_seqs_;
  uint64_t oldest_live_seq_ = 0;
  uint64_t next_seq_ = 1;
  uint64_t ops_since_decay_ = 0;

  // Mutex for thread-safe access to the cache.
  port::Mutex mutex_;

  // Private helper to update a key's frequency and move it
  void Touch(typename std::unordered_map<std::string, CacheEntry>::iterator it);
  // Private helper for eviction
  bool EvictLocked(EvictedEntry* evicted);
  size_t EntryCharge(const std::string& key, const CacheEntry& entry) const;
  static size_t DefaultPerEntryOverheadBytes();
  size_t ComputeMetadataBytes() const;
  void RecomputeMetadataBytesLocked();
  size_t TotalBytes() const { return current_bytes_ + metadata_bytes_; }
  void RemoveLiveSeqLocked(uint64_t seq);
  void AddLiveSeqLocked(uint64_t seq);
  uint64_t CurrentOldestLiveSeqLocked() const;
  void MaybeDecayLocked();
};

}  // namespace ROCKSDB_NAMESPACE
