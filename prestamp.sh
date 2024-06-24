#!/bin/bash
timestamp=$(date -u +'D%FT%T')
echo "prestamp" >> prestamp.txt
echo "#define TIMESTAMP \"$1.$2.$3.$timestamp\"" > ../timestamp.h
