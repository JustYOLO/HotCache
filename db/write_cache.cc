#include "db/write_cache.h"
#include "util/mutexlock.h"
#include <algorithm> // Required for std::min_element, etc.

namespace ROCKSDB_NAMESPACE {

WriteCache::WriteCache(size_t capacity) : capacity_(capacity) {
  cache_.reserve(capacity_);
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

void WriteCache::Evict() {
  // TODO: eviction handling needed (key_to_evict -> memtable)
  // Assumes mutex_ is held and cache is full.
  if (freq_lists_.empty()) {
    return;
  }
  
  // Get the list of keys with the lowest frequency (first element in freq_lists_)
  auto& lfu_list = freq_lists_.begin()->second;
  // Evict the least recently added key from that list (back of the list)
  const std::string& key_to_evict = lfu_list.back();
  
  cache_.erase(key_to_evict);
  lfu_list.pop_back();
  
  // If the list for this frequency is now empty, remove the frequency entry
  if (lfu_list.empty()) {
    freq_lists_.erase(freq_lists_.begin());
  }
}

bool WriteCache::Put(const Slice& key, const Slice& value) {
  MutexLock lock(&mutex_);
  auto it = cache_.find(key.ToString());
  if (it != cache_.end()) {
    // Key found, update its value and frequency
    it->second.value = value.ToString();
    Touch(it);
    return true;
  }
  return false;
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
  MutexLock lock(&mutex_);

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

  // Key doesn't exist, we need to insert it.
  if (cache_.size() >= capacity_) {
    Evict();
  }

  // Insert the new key. The value is empty and will be filled by a `Put`.
  uint64_t new_freq = count;
  std::string key_str = key.ToString(); // Convert Slice to std::string once
  freq_lists_[new_freq].push_front(key_str);
  cache_.emplace(std::move(key_str),
                 CacheEntry{"", new_freq, freq_lists_[new_freq].begin()});
}

}  // namespace ROCKSDB_NAMESPACE