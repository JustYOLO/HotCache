#!/bin/bash

# Configuration
DB_DIR="/tmp/rocks"
PUSH_CMD="$HOME/push.sh"
DB_BENCH="./db_bench"

# Helper function to send notifications
send_push() {
    local message="$1"
    if [ -x "$PUSH_CMD" ]; then
        "$PUSH_CMD" "$message"
    else
        echo "[Warn] Push script not executable or found at $PUSH_CMD"
        echo "Message was: $message"
    fi
}

# Function to run a single test case
run_experiment() {
    local test_name="$1"
    local extra_flags="$2"
    local log_file="${test_name}.log"

    echo "========================================"
    echo "Starting Experiment: $test_name"
    echo "========================================"

    # 1. CLEANUP
    # Remove old DB to ensure every benchmark starts from the exact same state
    if [ -d "$DB_DIR" ]; then
        echo "Cleaning up old database at $DB_DIR..."
        rm -rf "$DB_DIR"
    fi

    # 2. FILL PHASE
    echo "Phase 1: Filling Database..."
    $DB_BENCH --benchmarks="fillrandom,stats" \
        --use_direct_io_for_flush_and_compaction=true \
        --use_direct_reads=true \
        --statistics=1 \
        --perf_level=3 \
        --cache_size=268435456 \
        --key_size=48 \
        --value_size=43 \
        --num=50000000 \
        --db="$DB_DIR" >> "${test_name}_fill.log" 2>&1

    if [ $? -ne 0 ]; then
        echo "Error: Fill phase failed."
        send_push "Error: DB Fill failed for $test_name. Check ${test_name}_fill.log."
        return 1
    fi

    # 3. BENCHMARK PHASE
    echo "Phase 2: Running Benchmark ($test_name)..."
    $DB_BENCH --benchmarks="mixgraph,stats" \
        --use_direct_io_for_flush_and_compaction=true \
        --use_direct_reads=true \
        --cache_size=268435456 \
        --key_dist_a=0.002312 \
        --key_dist_b=0.3467 \
        --keyrange_dist_a=14.18 \
        --keyrange_dist_b=-2.917 \
        --keyrange_dist_c=0.0164 \
        --keyrange_dist_d=-0.08082 \
        --keyrange_num=30 \
        --value_k=0.2615 \
        --value_sigma=25.45 \
        --iter_k=2.517 \
        --iter_sigma=14.236 \
        --mix_get_ratio=0.83 \
        --mix_put_ratio=0.14 \
        --mix_seek_ratio=0.03 \
        --sine_mix_rate_interval_milliseconds=5000 \
        --sine_a=1000 \
        --sine_b=0.000073 \
        --sine_d=4500 \
        --perf_level=2 \
        --reads=420000000 \
        --num=50000000 \
        --key_size=48 \
        --db="$DB_DIR" \
        --use_existing_db=true \
        --statistics=1 \
        $extra_flags > "$log_file" 2>&1

    # Check exit code of the benchmark
    if [ $? -eq 0 ]; then
        echo "Success."
        send_push "Program finished successfully. Log saved to $log_file ($test_name)."
    else
        echo "Failed."
        send_push "Error: Benchmark run failed for $test_name. Please check $log_file for details."
    fi
}

# --- Execution ---

# Option 1: Original
run_experiment "original" "--use_write_cache=false --max_write_buffer_number=3"

# Option 2: LRU
run_experiment "LRU" "--use_write_cache=true --write_cache_policy=lru --max_write_buffer_number=2"

# Option 3: LFU
run_experiment "LFU" "--use_write_cache=true --write_cache_policy=lfu --max_write_buffer_number=2"
