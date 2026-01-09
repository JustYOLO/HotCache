#include "db/write_cache_wal.h"

#include <algorithm>
#include <cinttypes>
#include <cstdlib>
#include <vector>

#include "file/file_util.h"
#include "file/filename.h"
#include "file/read_write_util.h"
#include "file/writable_file_writer.h"
#include "options/options_helper.h"
#include "logging/logging.h"
#include "rocksdb/file_system.h"
#include "util/coding.h"

namespace ROCKSDB_NAMESPACE {
namespace {

enum class CacheWalRecordType : char { kPut = 1, kDelete = 2 };

void AppendRecord(std::string* dst, CacheWalRecordType type, uint64_t seq,
                  const Slice& key, const Slice* value) {
  PutVarint64(dst, seq);
  dst->push_back(static_cast<char>(type));
  PutVarint32(dst, static_cast<uint32_t>(key.size()));
  dst->append(key.data(), key.size());
  if (type == CacheWalRecordType::kPut && value != nullptr) {
    PutVarint32(dst, static_cast<uint32_t>(value->size()));
    dst->append(value->data(), value->size());
  } else {
    PutVarint32(dst, 0);
  }
}

}  // namespace

WriteCacheWAL::WriteCacheWAL(const ImmutableDBOptions& immutable_db_options,
                             const MutableDBOptions& mutable_db_options,
                             const FileOptions& file_options,
                             const std::shared_ptr<IOTracer>& io_tracer,
                             FSDirectory* wal_dir, Logger* info_log)
    : immutable_db_options_(immutable_db_options),
      mutable_db_options_(mutable_db_options),
      file_options_(file_options),
      fs_(immutable_db_options_.fs),
      io_tracer_(io_tracer),
      wal_dir_(wal_dir),
      info_log_(info_log),
      max_log_file_size_(immutable_db_options_.write_cache_wal_max_file_size),
      log_dir_synced_(false),
      oldest_live_seq_(0),
      next_log_number_(FindNextLogNumber()),
      total_bytes_written_(0),
      put_count_(0),
      delete_count_(0),
      in_place_update_count_(0),
      evicted_count_(0),
      logical_bytes_(0),
      last_log_bytes_reported_(0),
      deleted_file_count_(0) {}

WriteCacheWAL::~WriteCacheWAL() {
  if (current_writer_) {
    current_writer_->Close(WriteOptions());
    current_writer_.reset();
  }
}

Status WriteCacheWAL::LogPut(uint64_t seq, const Slice& key,
                             const Slice& value,
                             const WriteOptions* write_options) {
  std::string record;
  AppendRecord(&record, CacheWalRecordType::kPut, seq, key, &value);
  return WriteRecord(write_options, Slice(record), seq,
                     WriteCache::RecordType::kPut);
}

Status WriteCacheWAL::LogDelete(uint64_t seq, const Slice& key,
                                const WriteOptions* write_options) {
  std::string record;
  AppendRecord(&record, CacheWalRecordType::kDelete, seq, key, nullptr);
  return WriteRecord(write_options, Slice(record), seq,
                     WriteCache::RecordType::kDelete);
}

void WriteCacheWAL::IncrementInPlaceUpdateCount() {
  MutexLock lock(&mutex_);
  ++in_place_update_count_;
}

void WriteCacheWAL::AddEvictedCount(uint64_t evicted_count) {
  if (evicted_count == 0) {
    return;
  }
  MutexLock lock(&mutex_);
  evicted_count_ += evicted_count;
}

void WriteCacheWAL::AddLogicalBytes(uint64_t bytes) {
  if (bytes == 0) {
    return;
  }
  MutexLock lock(&mutex_);
  logical_bytes_ += bytes;
}

void WriteCacheWAL::GetStatsString(std::string* value) {
  MutexLock lock(&mutex_);
  value->clear();
  value->append("** WriteCache stats ** \n");
  value->append("write_cache_wal_bytes: ");
  value->append(std::to_string(total_bytes_written_));
  value->append("\nwrite_cache_logical_bytes: ");
  value->append(std::to_string(logical_bytes_));
  value->append("\nputs: ");
  value->append(std::to_string(put_count_));
  value->append("\ndeletes: ");
  value->append(std::to_string(delete_count_));
  value->append("\nin_place_updates: ");
  value->append(std::to_string(in_place_update_count_));
  value->append("\nevicted_entries: ");
  value->append(std::to_string(evicted_count_));
  value->append("\noldest_live_seq: ");
  value->append(std::to_string(oldest_live_seq_));
  value->append("\nopen_log_number: ");
  value->append(std::to_string(current_file_.number));
  value->append("\nopen_log_min_seq: ");
  value->append(std::to_string(current_file_.min_seq));
  value->append("\nopen_log_max_seq: ");
  value->append(std::to_string(current_file_.max_seq));
  value->append("\nclosed_files: ");
  value->append(std::to_string(closed_files_.size()));
  value->append("\ndeleted_files: ");
  value->append(std::to_string(deleted_file_count_));
  value->append("\n\n");
}

void WriteCacheWAL::UpdateOldestLiveSeq(uint64_t oldest_live_seq) {
  MutexLock lock(&mutex_);
  oldest_live_seq_ = oldest_live_seq;
  DeleteObsolete();
}

Status WriteCacheWAL::WriteRecord(const WriteOptions* write_options,
                                  const Slice& record, uint64_t seq,
                                  WriteCache::RecordType type) {
  MutexLock lock(&mutex_);
  Status s = EnsureWriter(write_options);
  if (!s.ok()) {
    return s;
  }
  IOStatus io_s = current_writer_->AddRecord(
      write_options ? *write_options : WriteOptions(), record);
  if (!io_s.ok()) {
    return io_s;
  }
  total_bytes_written_ += record.size();
  if (current_file_.min_seq == 0 || seq < current_file_.min_seq) {
    current_file_.min_seq = seq;
  }
  if (seq > current_file_.max_seq) {
    current_file_.max_seq = seq;
  }
  if (type == WriteCache::RecordType::kPut) {
    ++put_count_;
  } else {
    ++delete_count_;
  }

  if (write_options && write_options->sync) {
    IOOptions opts;
    io_s = WritableFileWriter::PrepareIOOptions(*write_options, opts);
    if (!io_s.ok()) {
      return io_s;
    }
    io_s = current_writer_->file()->Sync(opts,
                                         immutable_db_options_.use_fsync);
    if (!io_s.ok()) {
      return io_s;
    }
    if (!log_dir_synced_ && wal_dir_ != nullptr) {
      io_s = wal_dir_->FsyncWithDirOptions(
          IOOptions(), nullptr,
          DirFsyncOptions(DirFsyncOptions::FsyncReason::kNewFileSynced));
      if (!io_s.ok()) {
        return io_s;
      }
      log_dir_synced_ = true;
    }
  }

  MaybeRotate(write_options);

  if (total_bytes_written_ - last_log_bytes_reported_ >= (64 << 20)) {
    last_log_bytes_reported_ = total_bytes_written_;
    ROCKS_LOG_INFO(info_log_,
                   "WriteCache WAL bytes=%" PRIu64
                   " puts=%" PRIu64 " deletes=%" PRIu64
                   " in_place_updates=%" PRIu64,
                   total_bytes_written_, put_count_, delete_count_,
                   in_place_update_count_);
  }
  return Status::OK();
}

void WriteCacheWAL::MaybeRotate(const WriteOptions* write_options) {
  if (max_log_file_size_ == 0 || !current_writer_) {
    return;
  }
  const uint64_t size = current_writer_->file()->GetFileSize();
  if (size < max_log_file_size_) {
    return;
  }
  current_writer_->Close(WriteOptions());
  if (current_file_.number != 0) {
    closed_files_.push_back(current_file_);
  }
  current_writer_.reset();
  current_file_ = WalFile{};
  DeleteObsolete();
  Status s = OpenNewLog(write_options, next_log_number_++);
  if (!s.ok()) {
    ROCKS_LOG_WARN(info_log_, "WriteCacheWAL rotate failed: %s",
                   s.ToString().c_str());
  }
}

Status WriteCacheWAL::EnsureWriter(const WriteOptions* write_options) {
  if (current_writer_) {
    return Status::OK();
  }
  return OpenNewLog(write_options, next_log_number_++);
}

Status WriteCacheWAL::OpenNewLog(const WriteOptions* write_options,
                                 uint64_t number) {
  DBOptions db_options =
      BuildDBOptions(immutable_db_options_, mutable_db_options_);
  FileOptions opt_file_options =
      fs_->OptimizeForLogWrite(file_options_, db_options);
  if (immutable_db_options_.wal_write_temperature != Temperature::kUnknown) {
    opt_file_options.temperature = immutable_db_options_.wal_write_temperature;
  }

  std::string log_fname = MakeWalFileName(number);
  std::unique_ptr<FSWritableFile> lfile;
  IOStatus io_s = NewWritableFile(fs_.get(), log_fname, &lfile, opt_file_options);
  if (!io_s.ok()) {
    return io_s;
  }

  lfile->SetWriteLifeTimeHint(Env::WLTH_SHORT);

  FileTypeSet tmp_set = immutable_db_options_.checksum_handoff_file_types;
  std::unique_ptr<WritableFileWriter> file_writer(new WritableFileWriter(
      std::move(lfile), log_fname, opt_file_options, immutable_db_options_.clock,
      io_tracer_, nullptr /* stats */, Histograms::HISTOGRAM_ENUM_MAX,
      immutable_db_options_.listeners, nullptr,
      tmp_set.Contains(FileType::kWalFile),
      tmp_set.Contains(FileType::kWalFile)));

  current_writer_.reset(new log::Writer(
      std::move(file_writer), number,
      immutable_db_options_.recycle_log_file_num > 0,
      immutable_db_options_.manual_wal_flush,
      immutable_db_options_.wal_compression));

  current_file_.number = number;
  current_file_.path = log_fname;
  current_file_.min_seq = 0;
  current_file_.max_seq = 0;

  if (write_options && write_options->sync) {
    IOOptions opts;
    io_s = WritableFileWriter::PrepareIOOptions(*write_options, opts);
    if (!io_s.ok()) {
      return io_s;
    }
    io_s = current_writer_->file()->Sync(opts,
                                         immutable_db_options_.use_fsync);
    if (!io_s.ok()) {
      return io_s;
    }
    if (!log_dir_synced_ && wal_dir_ != nullptr) {
      io_s = wal_dir_->FsyncWithDirOptions(
          IOOptions(), nullptr,
          DirFsyncOptions(DirFsyncOptions::FsyncReason::kNewFileSynced));
      if (!io_s.ok()) {
        return io_s;
      }
      log_dir_synced_ = true;
    }
  }

  return Status::OK();
}

void WriteCacheWAL::DeleteObsolete() {
  if (oldest_live_seq_ == 0) {
    while (!closed_files_.empty()) {
      auto& file = closed_files_.front();
      fs_->DeleteFile(file.path, IOOptions(), nullptr)
          .PermitUncheckedError();
      ++deleted_file_count_;
      closed_files_.pop_front();
    }
    return;
  }
  while (!closed_files_.empty()) {
    const auto& file = closed_files_.front();
    if (file.max_seq >= oldest_live_seq_) {
      break;
    }
    fs_->DeleteFile(file.path, IOOptions(), nullptr)
        .PermitUncheckedError();
    ++deleted_file_count_;
    closed_files_.pop_front();
  }
}

std::string WriteCacheWAL::MakeWalFileName(uint64_t number) const {
  char buf[100];
  snprintf(buf, sizeof(buf), "%06llu.wlog",
           static_cast<unsigned long long>(number));
  return immutable_db_options_.GetWalDir() + "/" + buf;
}

uint64_t WriteCacheWAL::FindNextLogNumber() const {
  uint64_t max_number = 0;
  std::vector<std::string> files;
  if (!fs_->GetChildren(immutable_db_options_.GetWalDir(), IOOptions(), &files,
                        nullptr)
           .ok()) {
    return 1;
  }
  for (const auto& file : files) {
    const std::string suffix = ".wlog";
    if (file.size() <= suffix.size()) {
      continue;
    }
    if (file.compare(file.size() - suffix.size(), suffix.size(), suffix) != 0) {
      continue;
    }
    const std::string number_str =
        file.substr(0, file.size() - suffix.size());
    char* end = nullptr;
    const unsigned long long num = std::strtoull(number_str.c_str(), &end, 10);
    if (end == number_str.c_str() || *end != '\0') {
      continue;
    }
    if (num > max_number) {
      max_number = num;
    }
  }
  return max_number + 1;
}

}  // namespace ROCKSDB_NAMESPACE
