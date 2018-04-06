#!/bin/bash

for solution in solution_*.cpp
do
    echo "=== COMPILING SOLUTION $solution"
    name=${solution/solution_/}
    name=${name/.cpp/}
    clang++ -I$(pwd) -std=c++14 -o program_$name main.cpp gtest/gtest-all.cc -DSOLUTION_FILE="\"$solution\""
done

