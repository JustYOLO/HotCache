#!/bin/bash

sudo chown lee /home/lee/db_bench

filename="zipf.txt"

function fill_db {
  rm -rf /home/lee/db_bench/*
  sync
  echo 3 > /proc/sys/vm/drop_caches  
  sudo chown lee /home/lee/db_bench

  echo "fill db"
  sleep 2

  ./db_bench --benchmarks=fillseq \
             --db=/home/lee/db_bench \
             --value_size=1000 \
             --num=$((50*1024*1024)) \
--use_direct_reads=true --use_direct_io_for_flush_and_compaction=true --disable_wal=true \
             --seed=912912
  sync

  echo "compact db"

  ./db_bench --benchmarks=compact \
  --db=/home/lee/db_bench \
--use_direct_reads=true --use_direct_io_for_flush_and_compaction=true --disable_wal=true \
  --use_existing_db=true

  sync
}

fill_db

echo "readseq"
echo "[readseq]" > "./zipf/readseq_${filename}"

./db_bench --benchmarks="readseq,stats" --statistics \
--db=/home/lee/db_bench \
--use_existing_db=true \
--num=$((50*1024*1024)) >> "./zipf/readseq_${filename}"

fill_db

echo "readrandom"
echo "[readrandom]" > "./zipf/readrandom_${filename}"

./db_bench --benchmarks="readrandom,stats" --statistics \
--db=/home/lee/db_bench \
--use_existing_db=true \
--num=$((50*1024*1024)) >> "./zipf/readrandom_${filename}"

fill_db

echo "YCSB Workload A"
echo "[YCSB Workload A]" > "./zipf/YCSB_A_${filename}"

./db_bench --benchmarks="ycsbwklda,stats" --statistics \
--db=/home/lee/db_bench \
--use_existing_db=true \
--num=$((50*1024*1024)) \
--duration=1800 >> "./zipf/YCSB_A_${filename}"

fill_db

echo "YCSB Workload B"
echo "[YCSB Workload B]" > "./zipf/YCSB_B_${filename}"
./db_bench --benchmarks="ycsbwkldb,stats" --statistics \
--db=/home/lee/db_bench \
--use_existing_db=true \
--num=$((50*1024*1024)) \
--duration=1800 >> "./zipf/YCSB_B_${filename}"

fill_db

echo "YCSB Workload C"
echo "[YCSB Workload C]" > "./zipf/YCSB_C_${filename}"
./db_bench --benchmarks="ycsbwkldc,stats" --statistics \
--db=/home/lee/db_bench \
--use_existing_db=true \
--num=$((50*1024*1024)) \
--duration=1800 >> "./zipf/YCSB_C_${filename}"

fill_db

echo "YCSB Workload D"
echo "[YCSB Workload D]" > "./zipf/YCSB_D_${filename}"
./db_bench --benchmarks="ycsbwkldd,stats" --statistics \
--db=/home/lee/db_bench \
--use_existing_db=true \
--num=$((50*1024*1024)) \
--duration=1800 >> "./zipf/YCSB_D_${filename}"

fill_db

echo "YCSB Workload E"
echo "[YCSB Workload E]" > "./zipf/YCSB_E_${filename}"
./db_bench --benchmarks="ycsbwklde,stats" --statistics \
--db=/home/lee/db_bench \
--use_existing_db=true \
--num=$((50*1024*1024)) \
--duration=1800 >> "./zipf/YCSB_E_${filename}"


fill_db

echo "YCSB Workload F"
echo "[YCSB Workload F]" > "./zipf/YCSB_F_${filename}"
./db_bench --benchmarks="ycsbwkldf,stats" --statistics \
--db=/home/lee/db_bench \
--use_existing_db=true \
--num=$((50*1024*1024)) \
--duration=1800 >> "./zipf/YCSB_F_${filename}"

