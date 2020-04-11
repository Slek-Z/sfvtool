#! /bin/bash

if [ ! -d "build" ]; then
  mkdir build
fi
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make "$@"
if [ $? -eq 0 ]; then
  echo "Build done. To install, run sudo make install in "$(pwd)
fi