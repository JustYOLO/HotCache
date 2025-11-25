#!/usr/bin/env bash
set -euo pipefail

# --- Configuration ---
ROCKSDB_DIR=~/research/rocksdb
DEST_DIR=~/research/rocksdb/cowork/motivation
NUM=100000000
SEED=912912
COMP_TYPE=none
DB_PARENT=/tmp/rocksdbtest-1000
LOG_PATH="$DB_PARENT/dbbench/LOG"

# Ensure the destination directory exists
mkdir -p "$DEST_DIR"

cd "$ROCKSDB_DIR"

# Helper to run one benchmark
run_bench() {
  local bench_args=$1
  local zipf_arg=$2
  local out_name=$3

  echo ">>> Running: $bench_args ${zipf_arg:--}"
  rm -rf "$DB_PARENT"
  ./db_bench \
    --num="$NUM" \
    --benchmarks="$bench_args" \
    --statistics \
    $zipf_arg \
    --seed="$SEED" \
    --compression_type="$COMP_TYPE"

  cp "$LOG_PATH" "$DEST_DIR/$out_name"
  echo "    → copied to $DEST_DIR/$out_name"
  echo
}

# 1) random fill
run_bench "fillrandom,stats" "" "LOG_rand"

# 2–5) ycsbwkldw with various zipf constants
run_bench "ycsbwkldw,stats" "--zipf_const=0.8"  "LOG_0d8"
run_bench "ycsbwkldw,stats" "--zipf_const=0.9"  "LOG_0d9"
run_bench "ycsbwkldw,stats" "--zipf_const=0.99" "LOG_1"
run_bench "ycsbwkldw,stats" "--zipf_const=1.1"  "LOG_1d1"

echo "All benchmarks complete."

