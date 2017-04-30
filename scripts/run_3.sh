#!/bin/bash
yell() { echo "$0: $*" >&2;  }
die() { yell "$*"; exit 111;  }
bail() { yell "$*"; exit 0; }
try() { "$@" || die "cannot $*";  }
try_bail() { "$@" && bail "skip $*"; }

i=$1
j=$2
k=$3
cd /mnt/data01
touch "$i"_"$j"_"$k".txt
rm input.affinity.data input.chunksizes input.metadata
rm -rf input.chunks

try_bail aws s3 cp s3://seunglab/watershed_data/done/"$i"_"$j"_"$k".txt .

try aws s3 cp s3://seunglab/watershed_data/dend/"$i"_"$j"_"$k".tar.bz2 .
try aws s3 cp s3://seunglab/watershed_data/seg/"$i"_"$j"_"$k".seg .
try aws s3 cp s3://seunglab/watershed_data/reorder/"$i"_"$j"_"$k".reorder.tmp .

tar -jxvf "$i"_"$j"_"$k".tar.bz2
mv "$i"_"$j"_"$k".seg input.chunks/$i/$j/$k/.seg
mv "$i"_"$j"_"$k".reorder.tmp input.chunks/$i/$j/$k/reorder.tmp

try julia /mnt/data01/prepare_3.jl $i $j $k
try /mnt/data01/stage_3 --filename=./input --high=0.999987 --low=0.003 --dust=800 --dust_low=0.3 --merge=800
rm input.affinity.data input.chunksizes input.metadata
try julia /mnt/data01/prepare_2.jl
try julia /mnt/data01/finish_3.jl $i $j $k
try aws s3 cp "$i"_"$j"_"$k".txt s3://seunglab/watershed_data/done/
rm *.tar.bz2
