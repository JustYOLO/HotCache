#!/bin/bash

num=$((10*1024*1024))
key_size=16
value_size=100

options="--num=$num --key_size=$key_size --value_size=$value_size --writes=$num"

# Baseline (RocksDB Default = 2)
sync
echo 3 > /proc/sys/vm/drop_caches

echo "[Memtable: 2] # RocksDB Default"

sleep 2

./db_bench --threads=16 --bloom_bits=10 --use_direct_reads=true --use_direct_io_for_flush_and_compaction=true --max_write_buffer_number=2 \
${options} \
--ttl_seconds=$((60*60*24*30*12)) --partition_index=true --partition_index_and_filters=true \
--db=/home/lee/db_bench --use_existing_db=false --benchmarks="fillrandom,stats" --statistics > 2_Memtable.txt

echo "[Memtable: 2] # RocksDB Default" >> Memtable.txt
cat 2_Memtable.txt | grep fillrandom >> Memtable.txt

rm /home/lee/db_bench/*

# Memtable = 4
sync
echo 3 > /proc/sys/vm/drop_caches

echo "[Memtable: 4]"

sleep 2

./db_bench --threads=16 --bloom_bits=10 --use_direct_reads=true --use_direct_io_for_flush_and_compaction=true --max_write_buffer_number=4 \
${options} \
--ttl_seconds=$((60*60*24*30*12)) --partition_index=true --partition_index_and_filters=true \
--db=/home/lee/db_bench --use_existing_db=false --benchmarks="fillrandom,stats" --statistics > 4_Memtable.txt

echo "[Memtable: 4]" >> Memtable_result.txt

cat 4_Memtable.txt | grep fillrandom >> Memtable_result.txt

rm /home/lee/db_bench/*

# Memtable = 8
sync
echo 3 > /proc/sys/vm/drop_caches

echo "[Memtable: 8]"

sleep 2

./db_bench --threads=16 --bloom_bits=10 --use_direct_reads=true --use_direct_io_for_flush_and_compaction=true --max_write_buffer_number=8 \
${options} \
--ttl_seconds=$((60*60*24*30*12)) --partition_index=true --partition_index_and_filters=true \
--db=/home/lee/db_bench --use_existing_db=false --benchmarks="fillrandom,stats" --statistics > 8_Memtable.txt

echo "[Memtable: 8]" >> Memtable_result.txt

cat 8_Memtable.txt | grep fillrandom >> Memtable_result.txt

rm /home/lee/db_bench/*

# Memtable = 16
sync
echo 3 > /proc/sys/vm/drop_caches

echo "[Memtable: 16]"

sleep 2

./db_bench --threads=16 --bloom_bits=10 --use_direct_reads=true --use_direct_io_for_flush_and_compaction=true --max_write_buffer_number=16 \
${options} \
--ttl_seconds=$((60*60*24*30*12)) --partition_index=true --partition_index_and_filters=true \
--db=/home/lee/db_bench --use_existing_db=false --benchmarks="fillrandom,stats" --statistics > 16_Memtable.txt

echo "[Memtable: 16]" >> Memtable_result.txt

cat 16_Memtable.txt | grep fillrandom >> Memtable_result.txt

rm /home/lee/db_bench/*

# Memtable = 32
sync
echo 3 > /proc/sys/vm/drop_caches

echo "[Memtable: 32]"

sleep 2

./db_bench --threads=16 --bloom_bits=10 --use_direct_reads=true --use_direct_io_for_flush_and_compaction=true --max_write_buffer_number=32 \
${options} \
--ttl_seconds=$((60*60*24*30*12)) --partition_index=true --partition_index_and_filters=true \
--db=/home/lee/db_bench --use_existing_db=false --benchmarks="fillrandom,stats" --statistics > 32_Memtable.txt

echo "[Memtable: 32]" >> Memtable_result.txt

cat 32_Memtable.txt | grep fillrandom >> Memtable_result.txt

rm /home/lee/db_bench/*
