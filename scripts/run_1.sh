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


try julia /mnt/data01/prepare_1.jl $i $j $k
try /mnt/data01/stage_1 --filename=./input --high=0.999987 --low=0.003 --dust=800 --dust_low=0.3 --merge=800

try tar --exclude=.seg -jcvf "$i"_"$j"_"$k".tar.bz2 input.chunks
mv input.chunks/$i/$j/$k/.seg "$i"_"$j"_"$k".seg
try bzip -1 "$i"_"$j"_"$k".seg
touch "$i"_"$j"_"$k".txt

try aws s3 cp "$i"_"$j"_"$k".tar.bz2 $S3PATH/dend/
try aws s3 cp "$i"_"$j"_"$k".seg.bz2 $S3PATH/seg/
try aws s3 cp "$i"_"$j"_"$k".txt $S3PATH/done/
rm *.seg.bz2
rm *.tar.bz2
rm *.txt
