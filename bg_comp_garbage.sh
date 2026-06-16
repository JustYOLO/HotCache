#!/usr/bin/env bash
set -euo pipefail

###############################################################################
# RocksDB db_bench benchmark runner
# - Runs 8 benchmarks: 4 workloads × {with_bg_compaction, no_bg_compaction}
# - Captures stdout/stderr into per-run log files
# - Keeps common flags consistent with your template
###############################################################################

# ---- User-configurable paths/knobs ----
DB_ROOT="${DB_ROOT:-./bench_dbs}"          # where db_bench will create DB dirs
OUT_ROOT="${OUT_ROOT:-./bench_results}"    # where logs will be written

# Common flags (from your template)
COMMON_FLAGS=(
  --statistics
  --seed=912912
  --flushstats=1
  --disable_auto_compactions=true
)

# Optional: add more global db_bench options here (env var), e.g.
# EXTRA_COMMON="--db_write_buffer_size=..."
EXTRA_COMMON="${EXTRA_COMMON:-}"

# ---- Workload-specific options ----
opts_fillrandom=( --num=5000000 )
opts_fillzip=( --num=5000000 )

opts_mixgraph=(
  --key_dist_a=0.002312 --key_dist_b=0.3467
  --keyrange_dist_a=14.18 --keyrange_dist_b=-2.917 --keyrange_dist_c=0.0164 --keyrange_dist_d=-0.08082
  --keyrange_num=30
  --value_k=0.2615 --value_sigma=25.45
  --iter_k=2.517 --iter_sigma=14.236
  --mix_get_ratio=0.83 --mix_put_ratio=0.14 --mix_seek_ratio=0.03
  --sine_mix_rate_interval_milliseconds=5000
  --sine_a=1000 --sine_b=0.000073 --sine_d=4500
  --perf_level=2
  --reads=420000000
  --num=50000000
  --key_size=48
  --put_limit=5000000
  --mix_skip_reads=true
)

opts_twittertrace=(
  --twitter_trace_file=/home/lee/twtr/cluster41.sort.10p
  --put_limit=5000000
  --twittertrace_skip_reads=true
)

# ---- Helpers ----
timestamp() { date +"%Y%m%d_%H%M%S"; }

run_one() {
  local workload="$1"     # fillrandom|fillzip|mixgraph|twittertrace
  local mode="$2"         # bg|nobg
  local db_dir="$3"
  local out_file="$4"

  local benchmarks=""
  if [[ "$mode" == "bg" ]]; then
    # With background operations (with compaction) -> include compactall
    benchmarks="${workload},flush,compactall,stats,flushstats"
  else
    # Without background operations -> remove compactall
    benchmarks="${workload},flush,stats,flushstats"
  fi

  mkdir -p "$(dirname "$out_file")"
  mkdir -p "$db_dir"

  echo "===================================================================="
  echo "Workload: $workload | Mode: $mode"
  echo "DB Dir  : $db_dir"
  echo "Out     : $out_file"
  echo "Bench   : $benchmarks"
  echo "===================================================================="

  # shellcheck disable=SC2086
  ./db_bench \
    --db="$db_dir" \
    --benchmarks="$benchmarks" \
    "${COMMON_FLAGS[@]}" \
    ${EXTRA_COMMON} \
    "$@" \
    2>&1 | tee "$out_file"
}

# ---- Main ----
main() {
  mkdir -p "$DB_ROOT" "$OUT_ROOT"

  local ts
  ts="$(timestamp)"
  local run_root="$OUT_ROOT/run_${ts}"
  mkdir -p "$run_root"

  # Each workload uses its own DB dir per mode to avoid state contamination
  local db_fillrandom_bg="$DB_ROOT/fillrandom_bg_${ts}"
  local db_fillrandom_nobg="$DB_ROOT/fillrandom_nobg_${ts}"
  local db_fillzip_bg="$DB_ROOT/fillzip_bg_${ts}"
  local db_fillzip_nobg="$DB_ROOT/fillzip_nobg_${ts}"
  local db_mixgraph_bg="$DB_ROOT/mixgraph_bg_${ts}"
  local db_mixgraph_nobg="$DB_ROOT/mixgraph_nobg_${ts}"
  local db_twitter_bg="$DB_ROOT/twittertrace_bg_${ts}"
  local db_twitter_nobg="$DB_ROOT/twittertrace_nobg_${ts}"

  # fillrandom
  run_one "fillrandom" "bg"   "$db_fillrandom_bg"   "$run_root/fillrandom_bg.log"   "${opts_fillrandom[@]}"
  run_one "fillrandom" "nobg" "$db_fillrandom_nobg" "$run_root/fillrandom_nobg.log" "${opts_fillrandom[@]}"

  # fillzip
  run_one "fillzip" "bg"   "$db_fillzip_bg"   "$run_root/fillzip_bg.log"   "${opts_fillzip[@]}"
  run_one "fillzip" "nobg" "$db_fillzip_nobg" "$run_root/fillzip_nobg.log" "${opts_fillzip[@]}"

  # mixgraph
  run_one "mixgraph" "bg"   "$db_mixgraph_bg"   "$run_root/mixgraph_bg.log"   "${opts_mixgraph[@]}"
  run_one "mixgraph" "nobg" "$db_mixgraph_nobg" "$run_root/mixgraph_nobg.log" "${opts_mixgraph[@]}"

  # twittertrace
  run_one "twittertrace" "bg"   "$db_twitter_bg"   "$run_root/twittertrace_bg.log"   "${opts_twittertrace[@]}"
  run_one "twittertrace" "nobg" "$db_twitter_nobg" "$run_root/twittertrace_nobg.log" "${opts_twittertrace[@]}"

  echo "All benchmarks completed."
  echo "Logs: $run_root"
}

main "$@"

