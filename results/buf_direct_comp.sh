#!/bin/bash

sudo chown lee /home/lee/db_bench

function initalize() {
  echo "initalizing"
  rm -rf /home/lee/db_bench/*
  sync
  echo 3 > /proc/sys/vm/drop_caches  
  sudo chown lee /home/lee/db_bench 
}

echo "key: default, value: 1000, num = 10M"

################################## start of seq 

initalize

echo "buffered_seq_64M"
sleep 2

../db_bench --benchmarks="fillseq,stats", --statistics \
           --db=/home/lee/db_bench \
           --value_size=1000 \
           --num=$((10*1024*1024)) \
           --write_buffer_size=67108864 \
           --target_file_size_base=67108864 \
           --seed=912912 > "buffered_seq_64M.txt"
sync

initalize

echo "buffered_seq_512M"
sleep 2

../db_bench --benchmarks="fillseq,stats", --statistics \
           --db=/home/lee/db_bench \
           --value_size=1000 \
           --num=$((10*1024*1024)) \
           --write_buffer_size=536870912 \
           --target_file_size_base=536870912 \
           --seed=912912 > "buffered_seq_512M.txt"
sync

initalize

echo "direct_seq_64M"
sleep 2

../db_bench --benchmarks="fillseq,stats", --statistics \
           --db=/home/lee/db_bench \
           --value_size=1000 \
           --num=$((10*1024*1024)) \
           --write_buffer_size=67108864 \
           --target_file_size_base=67108864 \
           --use_direct_reads=true --use_direct_io_for_flush_and_compaction=true \
           --seed=912912 > "direct_seq_64M.txt"
sync

initalize

echo "direct_seq_512M"
sleep 2

../db_bench --benchmarks="fillseq,stats", --statistics \
           --db=/home/lee/db_bench \
           --value_size=1000 \
           --num=$((10*1024*1024)) \
           --write_buffer_size=536870912 \
           --target_file_size_base=536870912 \
           --use_direct_reads=true --use_direct_io_for_flush_and_compaction=true \
           --seed=912912 > "direct_seq_512M.txt"
sync

################################## start of rand 

initalize

echo "buffered_rand_64M"
sleep 2

../db_bench --benchmarks="fillrandom,stats", --statistics \
           --db=/home/lee/db_bench \
           --value_size=1000 \
           --num=$((10*1024*1024)) \
           --write_buffer_size=67108864 \
           --target_file_size_base=67108864 \
           --seed=912912 > "buffered_rand_64M.txt"
sync

initalize

echo "buffered_seq_512M"
sleep 2

../db_bench --benchmarks="fillrandom,stats", --statistics \
           --db=/home/lee/db_bench \
           --value_size=1000 \
           --num=$((10*1024*1024)) \
           --write_buffer_size=536870912 \
           --target_file_size_base=536870912 \
           --seed=912912 > "buffered_rand_512M.txt"
sync

initalize

echo "direct_seq_64M"
sleep 2

../db_bench --benchmarks="fillrandom,stats", --statistics \
           --db=/home/lee/db_bench \
           --value_size=1000 \
           --num=$((10*1024*1024)) \
           --write_buffer_size=67108864 \
           --target_file_size_base=67108864 \
           --use_direct_reads=true --use_direct_io_for_flush_and_compaction=true \
           --seed=912912 > "direct_rand_64M.txt"
sync

initalize

echo "direct_seq_512M"
sleep 2

../db_bench --benchmarks="fillrandom,stats", --statistics \
           --db=/home/lee/db_bench \
           --value_size=1000 \
           --num=$((10*1024*1024)) \
           --write_buffer_size=536870912 \
           --target_file_size_base=536870912 \
           --use_direct_reads=true --use_direct_io_for_flush_and_compaction=true \
           --seed=912912 > "direct_rand_512M.txt"
sync

################################## start of zipf 

initalize

echo "buffered_zipf_64M"
sleep 2

../db_bench --benchmarks="ycsbwkldw,stats", --statistics \
           --db=/home/lee/db_bench \
           --value_size=1000 \
           --num=$((10*1024*1024)) \
           --write_buffer_size=67108864 \
           --target_file_size_base=67108864 \
           --seed=912912 > "buffered_zipf_64M.txt"
sync

initalize

echo "buffered_zipf_512M"
sleep 2

../db_bench --benchmarks="ycsbwkldw,stats", --statistics \
           --db=/home/lee/db_bench \
           --value_size=1000 \
           --num=$((10*1024*1024)) \
           --write_buffer_size=536870912 \
           --target_file_size_base=536870912 \
           --seed=912912 > "buffered_zipf_512M.txt"
sync

initalize

echo "direct_zipf_64M"
sleep 2

../db_bench --benchmarks="ycsbwkldw,stats", --statistics \
           --db=/home/lee/db_bench \
           --value_size=1000 \
           --num=$((10*1024*1024)) \
           --write_buffer_size=67108864 \
           --target_file_size_base=67108864 \
           --use_direct_reads=true --use_direct_io_for_flush_and_compaction=true \
           --seed=912912 > "direct_zipf_64M.txt"
sync

initalize

echo "direct_zipf_512M"
sleep 2

../db_bench --benchmarks="ycsbwkldw,stats", --statistics \
           --db=/home/lee/db_bench \
           --value_size=1000 \
           --num=$((10*1024*1024)) \
           --write_buffer_size=536870912 \
           --target_file_size_base=536870912 \
           --use_direct_reads=true --use_direct_io_for_flush_and_compaction=true \
           --seed=912912 > "direct_zipf_512M.txt"
sync

