#!/bin/bash

clear

echo -e '\033[32;1m-------------------------------------\n----------------------------------------\n-------------------------------------\n\033[0m'

g++ -o libpool.so -O2 -fpic -std=c++17 lib.cpp -shared -s

if [ $? -ne 0 ]; then
	exit 1
fi

g++ -o user -O2 -std=c++17 user.cpp -L. -lpool -s

if [ $? -ne 0 ]; then
	exit 1
fi

if [ $# -eq 1 ]; then
	wrapper="$1"
else
	wrapper=""
fi

LD_LIBRARY_PATH=. $wrapper ./user
