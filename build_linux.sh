#!/bin/bash -e

export NUM_CORES=$(grep -c processor /proc/cpuinfo)
export BUILD_TYPE=Release

echo NUM_CORES: $NUM_CORES
echo BUILD_TYPE: $BUILD_TYPE

mkdir -p build
(
  cd build || exit
  cmake --preset $BUILD_TYPE-linux ..
  make -j${NUM_CORES}
  make install
)

if [ $? -ne 0 ]; then
  exit 1
fi

echo "Finished building of Machinist"
