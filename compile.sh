#!/bin/sh

set -eu

CXX=${CXX:-clang++}
LLVM_Flags=`llvm-config --cxxflags --ldflags --system-libs --libs core native passes`

mkdir -p build

# ${CXX} $* code/unnamed.cpp $LLVM_Flags -ftime-trace -o build/unnamed
${CXX} $* code/unnamed.cpp $LLVM_Flags -o build/unnamed -g
