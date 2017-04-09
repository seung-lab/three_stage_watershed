#!/bin/bash
yell() { echo "$0: $*" >&2;  }
die() { yell "$*"; exit 111;  }
try() { "$@" || die "cannot $*";  }

i=$1
j=$2
k=$3
cd /mnt/data01
rm input.affinity.data input.chunksizes input.metadata
rm -rf input.chunks

try julia /mnt/data01/prepare_1.jl $i $j $k
try /mnt/data01/stage_1 --filename=./input --high=0.999982 --low=0.05 --dust=100 --dust_low=0.3 --merge=800

tar --exclude=.seg -jcvf "$i"_"$j"_"$k".tar.bz2 input.chunks
mv input.chunks/$i/$j/$k/.seg "$i"_"$j"_"$k".seg
touch "$i"_"$j"_"$k".txt

try aws s3 cp "$i"_"$j"_"$k".tar.bz2 s3://seunglab/watershed_data/dend/
try aws s3 cp "$i"_"$j"_"$k".seg s3://seunglab/watershed_data/seg/
try aws s3 cp "$i"_"$j"_"$k".txt s3://seunglab/watershed_data/done/
