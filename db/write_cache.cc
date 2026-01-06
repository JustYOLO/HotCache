#include "db/write_cache.h"

#include <algorithm> // Required for std::min_element, etc.
#include <tuple>
#include <utility>
#include <vector>

#include "util/mutexlock.h"

namespace ROCKSDB_NAMESPACE {

WriteCache::WriteCache(size_t capacity_bytes,
                       EvictionCallback eviction_callback,
                       WriteCachePolicy policy,
                       size_t per_entry_overhead_bytes,
                       RecordCallback record_callback,
                       OldestSeqCallback oldest_seq_callback,
                       EvictionStatsCallback eviction_stats_callback)
    : capacity_bytes_(capacity_bytes),
      per_entry_overhead_bytes_(per_entry_overhead_bytes),
      eviction_callback_(std::move(eviction_callback)),
      record_callback_(std::move(record_callback)),
      oldest_seq_callback_(std::move(oldest_seq_callback)),
      eviction_stats_callback_(std::move(eviction_stats_callback)),
      policy_(policy) {
  if (capacity_bytes_ <= kGlobalOverheadBytes) {
    capacity_bytes_ = 0;
    return;
  }
  if (per_entry_overhead_bytes_ == 0) {
    per_entry_overhead_bytes_ = DefaultPerEntryOverheadBytes();
  }
  RecomputeMetadataBytesLocked();
}

WriteCache::~WriteCache() {
  // Destructor
}

void WriteCache::Touch(typename std::unordered_map<std::string, CacheEntry>::iterator it) {
  // Assumes mutex_ is held.
  if (policy_ == WriteCachePolicy::kLRU) {
    lru_list_.erase(it->second.lru_iterator);
    lru_list_.push_front(it->first);
    it->second.lru_iterator = lru_list_.begin();
    return;
  }
  uint64_t old_freq = it->second.frequency;
  
  // Erase key from its old frequency list
  freq_lists_[old_freq].erase(it->second.lfu_iterator);
  if (freq_lists_[old_freq].empty()) {
    freq_lists_.erase(old_freq);
  }

  // Increment frequency and insert into new frequency list
  uint64_t new_freq = ++it->second.frequency;
  freq_lists_[new_freq].push_front(it->first);
  it->second.lfu_iterator = freq_lists_[new_freq].begin();
  MaybeDecayLocked();
}

bool WriteCache::EvictLocked(EvictedEntry* evicted) {
  // Assumes mutex_ is held and cache is full.
  if (policy_ == WriteCachePolicy::kLRU) {
    if (lru_list_.empty()) {
      return false;
    }
    const std::string key_to_evict = lru_list_.back();
    auto cache_it = cache_.find(key_to_evict);
    if (cache_it != cache_.end()) {
      if (cache_it->second.seq > 0) {
        RemoveLiveSeqLocked(cache_it->second.seq);
      }
      if (evicted && !cache_it->second.value.empty()) {
        evicted->key = cache_it->first;
        evicted->value = cache_it->second.value;
        evicted->has_value = true;
      }
      current_bytes_ -= EntryCharge(cache_it->first, cache_it->second);
      lru_list_.erase(cache_it->second.lru_iterator);
      cache_.erase(cache_it);
    } else {
      lru_list_.pop_back();
    }
  } else {
    if (freq_lists_.empty()) {
      return false;
    }

    // Get the list of keys with the lowest frequency (first element in freq_lists_)
    auto& lfu_list = freq_lists_.begin()->second;
    // Evict the least recently added key from that list (back of the list)
    const std::string& key_to_evict = lfu_list.back();

    auto cache_it = cache_.find(key_to_evict);
    if (cache_it != cache_.end()) {
      if (cache_it->second.seq > 0) {
        RemoveLiveSeqLocked(cache_it->second.seq);
      }
      if (evicted && !cache_it->second.value.empty()) {
        evicted->key = cache_it->first;
        evicted->value = cache_it->second.value;
        evicted->has_value = true;
      }
      current_bytes_ -= EntryCharge(cache_it->first, cache_it->second);
      cache_.erase(cache_it);
    }
    lfu_list.pop_back();

    // If the list for this frequency is now empty, remove the frequency entry
    if (lfu_list.empty()) {
      freq_lists_.erase(freq_lists_.begin());
    }
  }
  RecomputeMetadataBytesLocked();
  return true;
}

size_t WriteCache::EntryCharge(const std::string& key,
                               const CacheEntry& entry) const {
  return key.size() + entry.value.size();
}

size_t WriteCache::DefaultPerEntryOverheadBytes() {
  return sizeof(CacheEntry) + sizeof(std::string) +
         sizeof(std::list<std::string>::iterator);
}

size_t WriteCache::ComputeMetadataBytes() const {
  size_t bytes = kGlobalOverheadBytes;
  bytes += cache_.bucket_count() * sizeof(void*);
  bytes += cache_.size() *
           (sizeof(std::pair<const std::string, CacheEntry>) +
            2 * sizeof(void*));
  bytes += freq_lists_.size() *
           (sizeof(std::pair<const uint64_t, std::list<std::string>>) +
            2 * sizeof(void*));
  bytes += cache_.size() *
           (sizeof(std::list<std::string>::value_type) + 2 * sizeof(void*));
  bytes += lru_list_.size() *
           (sizeof(std::list<std::string>::value_type) + 2 * sizeof(void*));
  bytes += cache_.size() * per_entry_overhead_bytes_;
  return bytes;
}

void WriteCache::RecomputeMetadataBytesLocked() {
  metadata_bytes_ = ComputeMetadataBytes();
}

void WriteCache::RemoveLiveSeqLocked(uint64_t seq) {
  if (seq == 0) {
    return;
  }
  auto it = live_seqs_.find(seq);
  if (it != live_seqs_.end()) {
    live_seqs_.erase(it);
  }
}

void WriteCache::AddLiveSeqLocked(uint64_t seq) {
  if (seq == 0) {
    return;
  }
  live_seqs_.insert(seq);
}

uint64_t WriteCache::CurrentOldestLiveSeqLocked() const {
  if (live_seqs_.empty()) {
    return 0;
  }
  return *live_seqs_.begin();
}

bool WriteCache::Put(const Slice& key, const Slice& value,
                     const WriteOptions& write_options) {
  std::vector<EvictedEntry> evicted_entries;
  std::vector<std::tuple<RecordType, uint64_t, std::string, std::string>>
      records;
  bool updated = false;
  uint64_t evicted_count = 0;
  uint64_t new_oldest_seq = 0;
  {
    MutexLock lock(&mutex_);
    if (capacity_bytes_ == 0) {
      return false;
    }
    auto it = cache_.find(key.ToString());
    if (it != cache_.end()) {
      // Key found, update its value and frequency
      const size_t old_charge = EntryCharge(it->first, it->second);
      it->second.value = value.ToString();
      const size_t new_charge = EntryCharge(it->first, it->second);
      if (new_charge > old_charge) {
        current_bytes_ += (new_charge - old_charge);
      } else {
        current_bytes_ -= (old_charge - new_charge);
      }
      Touch(it);
      RemoveLiveSeqLocked(it->second.seq);
      it->second.seq = next_seq_++;
      AddLiveSeqLocked(it->second.seq);
      records.emplace_back(RecordType::kPut, it->second.seq, it->first,
                           it->second.value);
      RecomputeMetadataBytesLocked();
      while (TotalBytes() > capacity_bytes_ && !cache_.empty()) {
        EvictedEntry evicted;
        if (!EvictLocked(&evicted)) {
          break;
        }
        ++evicted_count;
        if (evicted.has_value) {
          records.emplace_back(RecordType::kDelete, next_seq_++, evicted.key,
                               std::string());
          evicted_entries.push_back(std::move(evicted));
        }
      }
      new_oldest_seq = CurrentOldestLiveSeqLocked();
      updated = true;
    }
  }
  if (updated && record_callback_) {
    for (const auto& record : records) {
      const RecordType type = std::get<0>(record);
      const uint64_t seq = std::get<1>(record);
      const std::string& record_key = std::get<2>(record);
      const std::string& record_value = std::get<3>(record);
      if (type == RecordType::kPut) {
        Slice value_slice(record_value);
        record_callback_(type, seq, Slice(record_key), &value_slice,
                         &write_options);
      } else {
        record_callback_(type, seq, Slice(record_key), nullptr, nullptr);
      }
    }
  }
  if (updated && eviction_callback_) {
    for (const auto& evicted : evicted_entries) {
      eviction_callback_(Slice(evicted.key), Slice(evicted.value));
    }
  }
  if (updated && eviction_stats_callback_ && evicted_count > 0) {
    eviction_stats_callback_(evicted_count);
  }
  if (updated && oldest_seq_callback_ && new_oldest_seq != oldest_live_seq_) {
    oldest_live_seq_ = new_oldest_seq;
    oldest_seq_callback_(oldest_live_seq_);
  }
  return updated;
}

bool WriteCache::Get(const Slice& key, std::string* value) {
  MutexLock lock(&mutex_);
  auto it = cache_.find(key.ToString());
  if (it != cache_.end()) {
    *value = it->second.value;
    Touch(it);
    return true;
  }
  return false;
}

void WriteCache::Update(const Slice& key, uint64_t count) {
  std::vector<EvictedEntry> evicted_entries;
  std::vector<std::tuple<RecordType, uint64_t, std::string, std::string>>
      records;
  uint64_t evicted_count = 0;
  uint64_t new_oldest_seq = 0;
  {
    MutexLock lock(&mutex_);
    if (capacity_bytes_ == 0) {
      return;
    }

    auto it = cache_.find(key.ToString());
    if (it != cache_.end()) {
      if (policy_ == WriteCachePolicy::kLRU) {
        Touch(it);
      } else {
        // Key already exists, just update its frequency.
        // The logic is similar to Touch but with a different frequency increment.
        uint64_t old_freq = it->second.frequency;
        freq_lists_[old_freq].erase(it->second.lfu_iterator);
        if (freq_lists_[old_freq].empty()) {
          freq_lists_.erase(old_freq);
        }
        uint64_t new_freq = it->second.frequency += count;
        std::string key_str_in_list = it->first; // Store to avoid iterator invalidation
        freq_lists_[new_freq].push_front(key_str_in_list);
        it->second.lfu_iterator = freq_lists_[new_freq].begin();
        MaybeDecayLocked();
      }
      return;
    }

    // Insert the new key. The value is empty and will be filled by a `Put`.
    uint64_t new_freq = count;
    std::string key_str = key.ToString(); // Convert Slice to std::string once
    const size_t entry_charge = key_str.size();
    if (entry_charge > capacity_bytes_) {
      return;
    }
    std::list<std::string>::iterator lfu_it;
    std::list<std::string>::iterator lru_it;
    if (policy_ == WriteCachePolicy::kLRU) {
      lru_list_.push_front(key_str);
      lru_it = lru_list_.begin();
      new_freq = 1;
    } else {
      freq_lists_[new_freq].push_front(key_str);
      lfu_it = freq_lists_[new_freq].begin();
      lru_it = lru_list_.end();
    }
    current_bytes_ += entry_charge;
    cache_.emplace(std::move(key_str),
                   CacheEntry{"", new_freq, 0, lfu_it, lru_it});
    RecomputeMetadataBytesLocked();
    while (TotalBytes() > capacity_bytes_ && !cache_.empty()) {
      EvictedEntry evicted;
      if (!EvictLocked(&evicted)) {
        break;
      }
      ++evicted_count;
      if (evicted.has_value) {
        records.emplace_back(RecordType::kDelete, next_seq_++, evicted.key,
                             std::string());
        evicted_entries.push_back(std::move(evicted));
      }
    }
    new_oldest_seq = CurrentOldestLiveSeqLocked();
    MaybeDecayLocked();
  }
  if (record_callback_) {
    for (const auto& record : records) {
      const RecordType type = std::get<0>(record);
      const uint64_t seq = std::get<1>(record);
      const std::string& record_key = std::get<2>(record);
      record_callback_(type, seq, Slice(record_key), nullptr, nullptr);
    }
  }
  if (eviction_callback_) {
    for (const auto& evicted : evicted_entries) {
      eviction_callback_(Slice(evicted.key), Slice(evicted.value));
    }
  }
  if (eviction_stats_callback_ && evicted_count > 0) {
    eviction_stats_callback_(evicted_count);
  }
  if (oldest_seq_callback_ && new_oldest_seq != oldest_live_seq_) {
    oldest_live_seq_ = new_oldest_seq;
    oldest_seq_callback_(oldest_live_seq_);
  }
}

void WriteCache::MaybeDecayLocked() {
  if (policy_ != WriteCachePolicy::kLFU) {
    return;
  }
  if (kFrequencyDecayIntervalOps == 0) {
    return;
  }
  ++ops_since_decay_;
  if (ops_since_decay_ < kFrequencyDecayIntervalOps) {
    return;
  }
  ops_since_decay_ = 0;
  if (cache_.empty()) {
    return;
  }
  freq_lists_.clear();
  for (auto& entry_pair : cache_) {
    auto& entry = entry_pair.second;
    entry.frequency = std::max<uint64_t>(1, entry.frequency / 2);
    freq_lists_[entry.frequency].push_front(entry_pair.first);
    entry.lfu_iterator = freq_lists_[entry.frequency].begin();
  }
}

}  // namespace ROCKSDB_NAMESPACE
