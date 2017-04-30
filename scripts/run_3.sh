#!/bin/bash
yell() { echo "$0: $*" >&2;  }
die() { yell "$*"; exit 111;  }
bail() { yell "$*"; exit 0; }
try() { "$@" || die "cannot $*";  }
try_bail() { "$@" && touch /tmp/finished.txt && bail "skip $*"; }

i=$1
j=$2
k=$3

S3PATH="s3://seunglab/watershed_data"

cd /mnt/data01

try_bail aws s3 cp $S3PATH/done/"$i"_"$j"_"$k".txt .

rm input.affinity.data input.chunksizes input.metadata
rm -rf input.chunks

try aws s3 cp $S3PATH/dend/"$i"_"$j"_"$k".tar.bz2 .
try aws s3 cp $S3PATH/seg/"$i"_"$j"_"$k".seg.bz2 .
try aws s3 cp $S3PATH/reorder/"$i"_"$j"_"$k".reorder.tmp.bz2 .

try tar -jxvf "$i"_"$j"_"$k".tar.bz2
try bunzip "$i"_"$j"_"$k".seg.bz2
try bunzip "$i"_"$j"_"$k".reorder.tmp.bz2

mv "$i"_"$j"_"$k".seg input.chunks/$i/$j/$k/.seg
mv "$i"_"$j"_"$k".reorder.tmp input.chunks/$i/$j/$k/reorder.tmp

try julia /mnt/data01/prepare_3.jl $i $j $k
try /mnt/data01/stage_3 --filename=./input --high=0.999987 --low=0.003 --dust=800 --dust_low=0.3 --merge=800
rm input.affinity.data input.chunksizes input.metadata
try julia /mnt/data01/prepare_2.jl
try julia /mnt/data01/finish_3.jl $i $j $k

touch "$i"_"$j"_"$k".txt

try aws s3 cp "$i"_"$j"_"$k".txt $S3PATH/done/

rm *.tar.bz2
