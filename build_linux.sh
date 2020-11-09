#!/bin/bash -e

export num_cores=`grep -c processor /proc/cpuinfo`
export MACHINIST_DIR=$PWD
export BUILD_TYPE=Debug

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_INSTALL_PREFIX=$DAL_DIR ..
make clean
make -j${num_cores}
make install

cd ..
echo "Finished building of Machinist"