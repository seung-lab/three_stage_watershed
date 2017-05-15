#!/bin/bash
while [ ! -f /tmp/finished.txt ]
do
    /mnt/data01/run_3.sh $*
done
rm /tmp/finished.txt
