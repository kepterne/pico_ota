#!/bin/bash
timestamp=$(date -u +'D%FT%T')
echo "#ifndef TIMESTAMP_H" > timestamp.h
echo "#define TIMESTAMP_H" >> timestamp.h
echo "#define TIMESTAMP \"$timestamp\"" >> timestamp.h
echo "#endif" >> timestamp.h
export TIMESTAMP=$timestamp
cd build
make
