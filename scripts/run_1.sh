#!/bin/bash
i = $1
j = $2
k = $3
rm input.affinity.data input.chunksizes input.metadata
rm -rf input.chunks
julia prepare_1.jl ../../affinity $i $j $k >& cutout.log
watershed/stage_1 --filename=./input --high=0.999982 --low=0.05 --dust=100 --dust_low=0.3 --merge=800
tar --exclude=.seg -Jcvf "$i"_"$j"_"$k".tar.xz input.chunks
mv input.chunks/$i/$j/$k/.seg "$i"_"$j"_"$k".seg
xz "$i"_"$j"_"$k".seg
aws s3 cp "$i"_"$j"_"$k".tar.xz s3://seunglab/watershed_data/
aws s3 cp "$i"_"$j"_"$k".seg s3://seunglab/watershed_data/
