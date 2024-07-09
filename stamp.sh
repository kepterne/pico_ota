#!/bin/bash
timestamp=$TIMESTAMP
rm $1.$2.$3-*.bin
cp pico_ota.bin $1.$2.$3-$timestamp.bin 
