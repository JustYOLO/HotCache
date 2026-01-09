#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include "db/log_writer.h"
#include "db/write_cache.h"
#include "rocksdb/env.h"
#include "rocksdb/file_system.h"
#include "rocksdb/io_status.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "util/mutexlock.h"

namespace ROCKSDB_NAMESPACE {

class FSDirectory;
struct ImmutableDBOptions;
struct MutableDBOptions;
struct FileOptions;
class IOTracer;
class Logger;

// Write-ahead log for the write cache with custom sequence numbers.
class WriteCacheWAL {
 public:
  WriteCacheWAL(const ImmutableDBOptions& immutable_db_options,
                const MutableDBOptions& mutable_db_options,
                const FileOptions& file_options,
                const std::shared_ptr<IOTracer>& io_tracer,
                FSDirectory* wal_dir, Logger* info_log);
  ~WriteCacheWAL();

  Status LogPut(uint64_t seq, const Slice& key, const Slice& value,
                const WriteOptions* write_options);
  Status LogDelete(uint64_t seq, const Slice& key,
                   const WriteOptions* write_options);
  void IncrementInPlaceUpdateCount();
  void AddEvictedCount(uint64_t evicted_count);
  void AddLogicalBytes(uint64_t bytes);
  void AddGetHit();
  void AddGetMiss();
  void GetStatsString(std::string* value);

  void UpdateOldestLiveSeq(uint64_t oldest_live_seq);

 private:
  struct WalFile {
    uint64_t number = 0;
    uint64_t min_seq = 0;
    uint64_t max_seq = 0;
    std::string path;
  };

  Status EnsureWriter(const WriteOptions* write_options);
  Status OpenNewLog(const WriteOptions* write_options, uint64_t number);
  Status WriteRecord(const WriteOptions* write_options, const Slice& record,
                     uint64_t seq, WriteCache::RecordType type);
  void MaybeRotate(const WriteOptions* write_options);
  void DeleteObsolete();
  std::string MakeWalFileName(uint64_t number) const;
  uint64_t FindNextLogNumber() const;

  const ImmutableDBOptions& immutable_db_options_;
  const MutableDBOptions& mutable_db_options_;
  const FileOptions& file_options_;
  std::shared_ptr<FileSystem> fs_;
  std::shared_ptr<IOTracer> io_tracer_;
  FSDirectory* wal_dir_;
  Logger* info_log_;
  uint64_t max_log_file_size_;
  bool log_dir_synced_;
  uint64_t oldest_live_seq_;
  uint64_t next_log_number_;
  WalFile current_file_;
  std::unique_ptr<log::Writer> current_writer_;
  std::deque<WalFile> closed_files_;
  uint64_t total_bytes_written_;
  uint64_t put_count_;
  uint64_t delete_count_;
  uint64_t in_place_update_count_;
  uint64_t evicted_count_;
  uint64_t logical_bytes_;
  uint64_t get_hit_count_;
  uint64_t get_miss_count_;
  uint64_t last_log_bytes_reported_;
  uint64_t deleted_file_count_;
  port::Mutex mutex_;
};

}  // namespace ROCKSDB_NAMESPACE
