#!/bin/bash

c++ -std=c++20 -o cpu_miner cpu_miner.cpp \
    -I/opt/homebrew/include -L/opt/homebrew/lib -lssl -lcrypto -lboost_json-mt

