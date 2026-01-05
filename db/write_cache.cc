#include "db/write_cache.h"

#include <algorithm> // Required for std::min_element, etc.
#include <utility>
#include <vector>

#include "util/mutexlock.h"

namespace ROCKSDB_NAMESPACE {

WriteCache::WriteCache(size_t capacity_bytes,
                       EvictionCallback eviction_callback)
    : capacity_bytes_(capacity_bytes),
      eviction_callback_(std::move(eviction_callback)) {
}

WriteCache::~WriteCache() {
  // Destructor
}

void WriteCache::Touch(typename std::unordered_map<std::string, CacheEntry>::iterator it) {
  // Assumes mutex_ is held.
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
}

bool WriteCache::EvictLocked(EvictedEntry* evicted) {
  // Assumes mutex_ is held and cache is full.
  if (freq_lists_.empty()) {
    return false;
  }
  
  // Get the list of keys with the lowest frequency (first element in freq_lists_)
  auto& lfu_list = freq_lists_.begin()->second;
  // Evict the least recently added key from that list (back of the list)
  const std::string& key_to_evict = lfu_list.back();
  
  auto cache_it = cache_.find(key_to_evict);
  if (cache_it != cache_.end()) {
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
  return true;
}

size_t WriteCache::EntryCharge(const std::string& key,
                               const CacheEntry& entry) const {
  return key.size() + entry.value.size();
}

bool WriteCache::Put(const Slice& key, const Slice& value) {
  std::vector<EvictedEntry> evicted_entries;
  bool updated = false;
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
      while (current_bytes_ > capacity_bytes_ && !cache_.empty()) {
        EvictedEntry evicted;
        if (!EvictLocked(&evicted)) {
          break;
        }
        if (evicted.has_value) {
          evicted_entries.push_back(std::move(evicted));
        }
      }
      updated = true;
    }
  }
  if (updated && eviction_callback_) {
    for (const auto& evicted : evicted_entries) {
      eviction_callback_(Slice(evicted.key), Slice(evicted.value));
    }
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
  {
    MutexLock lock(&mutex_);
    if (capacity_bytes_ == 0) {
      return;
    }

    auto it = cache_.find(key.ToString());
    if (it != cache_.end()) {
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
      return;
    }

    // Insert the new key. The value is empty and will be filled by a `Put`.
    uint64_t new_freq = count;
    std::string key_str = key.ToString(); // Convert Slice to std::string once
    const size_t entry_charge = key_str.size();
    if (entry_charge > capacity_bytes_) {
      return;
    }
    while (current_bytes_ + entry_charge > capacity_bytes_ &&
           !cache_.empty()) {
      EvictedEntry evicted;
      if (!EvictLocked(&evicted)) {
        break;
      }
      if (evicted.has_value) {
        evicted_entries.push_back(std::move(evicted));
      }
    }
    freq_lists_[new_freq].push_front(key_str);
    current_bytes_ += entry_charge;
    cache_.emplace(std::move(key_str),
                   CacheEntry{"", new_freq, freq_lists_[new_freq].begin()});
  }
  if (eviction_callback_) {
    for (const auto& evicted : evicted_entries) {
      eviction_callback_(Slice(evicted.key), Slice(evicted.value));
    }
  }
}

}  // namespace ROCKSDB_NAMESPACE
