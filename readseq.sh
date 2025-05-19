#!/bin/bash

# Baseline (1MiB)
sync
echo 3 > /proc/sys/vm/drop_caches

echo "[1MiB_Cache]"

sleep 2

./db_bench --threads=8 --cache_index_and_filter_blocks=true --bloom_bits=10 --partition_index=true \
--partition_index_and_filters=true --num=$((1024*1024*1024)) --reads=$((1024*1024*1024)) --use_direct_reads=true \
--key_size=48 --value_size=43 --db=/home/lee/db_bench --use_existing_db=1 \
--cache_size=$((1*1024*1024)) --benchmarks=readseq  > temp.txt

echo "[1MiB_Cache]" >> rocksdb_1.txt
cat temp.txt | grep "readseq" >> rocksdb_1.txt

# 64MiB_Cache
sync
echo 3 > /proc/sys/vm/drop_caches

echo "[64MiB_Cache]"

sleep 2

./db_bench --threads=8 --cache_index_and_filter_blocks=true --bloom_bits=10 --partition_index=true \
--partition_index_and_filters=true --num=$((1024*1024*1024)) --reads=$((1024*1024*1024)) --use_direct_reads=true \
--key_size=48 --value_size=43 --db=/home/lee/db_bench --use_existing_db=1 \
--cache_size=$((64*1024*1024)) --benchmarks=readseq  > temp.txt

echo "[64MiB_Cache]" >> rocksdb_1.txt
cat temp.txt | grep "readseq" >> rocksdb_1.txt


# 128MiB_Cache
sync
echo 3 > /proc/sys/vm/drop_caches

echo "[128MiB_Cache]"

sleep 2

./db_bench --threads=8 --cache_index_and_filter_blocks=true --bloom_bits=10 --partition_index=true \
--partition_index_and_filters=true --num=$((1024*1024*1024)) --reads=$((1024*1024*1024)) --use_direct_reads=true \
--key_size=48 --value_size=43 --db=/home/lee/db_bench --use_existing_db=1 \
--cache_size=$((128*1024*1024)) --benchmarks=readseq  > temp.txt

echo "[128MiB_Cache]" >> rocksdb_1.txt
cat temp.txt | grep "readseq" >> rocksdb_1.txt

# 512MiB_Cache
sync
echo 3 > /proc/sys/vm/drop_caches

echo "[512MiB_Cache]"

sleep 2

./db_bench --threads=8 --cache_index_and_filter_blocks=true --bloom_bits=10 --partition_index=true \
--partition_index_and_filters=true --num=$((1024*1024*1024)) --reads=$((1024*1024*1024)) --use_direct_reads=true \
--key_size=48 --value_size=43 --db=/home/lee/db_bench --use_existing_db=1 \
--cache_size=$((512*1024*1024)) --benchmarks=readseq  > temp.txt

echo "[512MiB_Cache]" >> rocksdb_1.txt
cat temp.txt | grep "readseq" >> rocksdb_1.txt

# 1GiB_Cache
sync
echo 3 > /proc/sys/vm/drop_caches

echo "[1GiB_Cache]"

sleep 2

./db_bench --threads=8 --cache_index_and_filter_blocks=true --bloom_bits=10 --partition_index=true \
--partition_index_and_filters=true --num=$((1024*1024*1024)) --reads=$((1024*1024*1024)) --use_direct_reads=true \
--key_size=48 --value_size=43 --db=/home/lee/db_bench --use_existing_db=1 \
--cache_size=$((1024*1024*1024)) --benchmarks=readseq  > temp.txt

echo "[1GiB_Cache]" >> rocksdb_1.txt
cat temp.txt | grep "readseq" >> rocksdb_1.txt

# 5GiB
sync
echo 3 > /proc/sys/vm/drop_caches

echo "[5GiB_Cache]"

sleep 2

./db_bench --threads=8 --cache_index_and_filter_blocks=true --bloom_bits=10 --partition_index=true \
--partition_index_and_filters=true --num=$((1024*1024*1024)) --reads=$((1024*1024*1024)) --use_direct_reads=true \
--key_size=48 --value_size=43 --db=/home/lee/db_bench --use_existing_db=1 \
--cache_size=$((5*1024*1024*1024)) --benchmarks=readseq  > temp.txt

echo "[5GiB_Cache]" >> rocksdb_1.txt
cat temp.txt | grep "readseq" >> rocksdb_1.txt

cat rocksdb_1.txt
