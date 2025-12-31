#include <rocksdb/env.h>
#include <rocksdb/sst_file_reader.h>

#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// Standard C++17 filesystem
namespace fs = std::filesystem;

struct LevelStats {
  long total_keys = 0;
  long unique_keys = 0;
  long duplicate_keys = 0;  // Keys that appeared more than once
};

// Function to analyze a single level directory
LevelStats AnalyzeLevel(const std::string& level_dir_path) {
  LevelStats stats;
  rocksdb::Options options;
  rocksdb::SstFileReader reader(options);

  // Map to count frequency of each key
  // Key string -> Count
  std::map<std::string, int> key_counts;

  std::cout << "Analyzing " << level_dir_path << "..." << std::endl;

  for (const auto& entry : fs::directory_iterator(level_dir_path)) {
    std::string file_path = entry.path().string();

    // Skip non-sst files
    if (file_path.find(".sst") == std::string::npos) continue;

    // Open the SST file
    rocksdb::Status s = reader.Open(file_path);
    if (!s.ok()) {
      std::cerr << "  Error opening " << file_path << ": " << s.ToString()
                << std::endl;
      continue;
    }

    // Iterate through all keys in this file
    std::unique_ptr<rocksdb::Iterator> iter(
        reader.NewIterator(rocksdb::ReadOptions()));
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
      std::string key = iter->key().ToString();

      key_counts[key]++;
      stats.total_keys++;
    }
  }

  // After processing all files in this level, calculate stats
  stats.unique_keys = key_counts.size();

  for (const auto& pair : key_counts) {
    if (pair.second > 1) {
      stats.duplicate_keys++;
    }
  }

  return stats;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: ./analyze_sst <path_to_sst_archive>" << std::endl;
    return 1;
  }

  std::string archive_root = argv[1];

  // Iterate through Level_0, Level_1, ...
  // We assume folders are named "Level_N"
  for (const auto& entry : fs::directory_iterator(archive_root)) {
    if (!entry.is_directory()) continue;

    std::string dir_name = entry.path().filename().string();
    if (dir_name.find("Level_") == 0) {
      LevelStats s = AnalyzeLevel(entry.path().string());

      std::cout << "--------------------------------" << std::endl;
      std::cout << "Stats for " << dir_name << ":" << std::endl;
      std::cout << "  Total Keys Scanned: " << s.total_keys << std::endl;
      std::cout << "  Unique Keys:        " << s.unique_keys << std::endl;
      std::cout << "  Duplicate Keys:     " << s.duplicate_keys << std::endl;

      double dup_ratio =
          (s.total_keys > 0)
              ? (double)(s.total_keys - s.unique_keys) / s.total_keys * 100.0
              : 0.0;
      std::cout << "  Duplication Rate:   " << dup_ratio << "%" << std::endl;
    }
  }

  return 0;
}
