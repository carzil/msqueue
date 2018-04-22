#!/bin/bash

for solution in solution_lock*.cpp
do
    echo "=== COMPILING SOLUTION $solution"
    name=${solution/solution_/}
    name=${name/.cpp/}
    g++ -I$(pwd) -latomic -fsanitize=address -g -std=c++14 -o program_$name main.cpp gtest/gtest-all.cc -DSOLUTION_FILE="\"$solution\"" -lpthread
done

