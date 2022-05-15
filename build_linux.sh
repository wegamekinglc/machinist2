#!/bin/bash -e

export NUM_CORES=$(grep -c processor /proc/cpuinfo)
export MACHINIST_DIR=$PWD
export BUILD_TYPE=Release

echo NUM_CORES: $NUM_CORES
echo MACHINIST_DIR: $MACHINIST_DIR
echo BUILD_TYPE: $BUILD_TYPE

mkdir -p build
(
  cd build || exit
  cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_INSTALL_PREFIX=$DAL_DIR ..
  make clean
  make -j${NUM_CORES}
  make install
)

if [ $? -ne 0 ]; then
  exit 1
fi

echo "Finished building of Machinist"
