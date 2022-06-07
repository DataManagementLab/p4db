#!/bin/bash

git clone --depth=1 https://github.com/lh3/rmaxcut
pushd rmaxcut
make
popd

git clone --depth=1 https://github.com/MQLib/MQLib
pushd MQLib
git apply ../mqlib.patch
make
popd


git clone --depth=1 https://git.mpi-cbg.de/mosaic/FaspHeuristic.git
pushd FaspHeuristic
mkdir build
cd build
cmake ..
make
popd


git clone --depth=1 https://github.com/DRMacIver/Feedback-Arc-Set
pushd Feedback-Arc-Set
make fas
popd
