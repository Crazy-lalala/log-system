#!/bin/bash
script_path=$(cd $(dirname $0); pwd)

rm -rf build
mkdir build;cd build

cmake ..
make -j20