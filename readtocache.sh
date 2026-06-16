#!/bin/bash

# Baseline (128)
sync
echo 3 > /proc/sys/vm/drop_caches

echo "[128MiB_Cache]"

sleep 2

./db_bench --threads=8 --cache_index_and_filter_blocks=true --bloom_bits=10 --partition_index=true \
--partition_index_and_filters=true --num=$((1024*1024*1024)) --reads=$((2*1024*1024)) --use_direct_reads=true \
--key_size=48 --value_size=43 --db=/home/lee/db_bench --use_existing_db=1 \
--cache_size=$((128*1024*1024)) --benchmarks=readtocache  > temp.txt

echo "[128MiB_Cache]" >> rocksdb_1.txt
cat temp.txt | grep "readtocache" >> rocksdb_1.txt

# 512MiB_Cache
sync
echo 3 > /proc/sys/vm/drop_caches

echo "[512MiB_Cache]"

sleep 2

./db_bench --threads=8 --cache_index_and_filter_blocks=true --bloom_bits=10 --partition_index=true \
--partition_index_and_filters=true --num=$((1024*1024*1024)) --reads=$((2*1024*1024)) --use_direct_reads=true \
--key_size=48 --value_size=43 --db=/home/lee/db_bench --use_existing_db=1 \
--cache_size=$((512*1024*1024)) --benchmarks=readtocache  > temp.txt

echo "[512MiB_Cache]" >> rocksdb_1.txt
cat temp.txt | grep "readtocache" >> rocksdb_1.txt

# 1GiB_Cache
sync
echo 3 > /proc/sys/vm/drop_caches

echo "[1GiB_Cache]"

sleep 2

./db_bench --threads=8 --cache_index_and_filter_blocks=true --bloom_bits=10 --partition_index=true \
--partition_index_and_filters=true --num=$((1024*1024*1024)) --reads=$((2*1024*1024)) --use_direct_reads=true \
--key_size=48 --value_size=43 --db=/home/lee/db_bench --use_existing_db=1 \
--cache_size=$((1024*1024*1024)) --benchmarks=readtocache  > temp.txt

echo "[1GiB_Cache]" >> rocksdb_1.txt
cat temp.txt | grep "readtocache" >> rocksdb_1.txt

# 5GiB_Cache
sync
echo 3 > /proc/sys/vm/drop_caches

echo "[5GiB_Cache]"

sleep 2

./db_bench --threads=8 --cache_index_and_filter_blocks=true --bloom_bits=10 --partition_index=true \
--partition_index_and_filters=true --num=$((1024*1024*1024)) --reads=$((2*1024*1024)) --use_direct_reads=true \
--key_size=48 --value_size=43 --db=/home/lee/db_bench --use_existing_db=1 \
--cache_size=$((5*1024*1024*1024)) --benchmarks=readtocache  > temp.txt

echo "[5GiB_Cache]" >> rocksdb_1.txt
cat temp.txt | grep "readtocache" >> rocksdb_1.txt

rm temp.txt

cat rocksdb_1.txt
