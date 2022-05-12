#!/bin/bash

SOURCE=${BASH_SOURCE[0]}
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
  SOURCE=$(readlink "$SOURCE")
  [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )

pushd $DIR

S_OUT_BIN=vulkan_example
S_OUT_DIR=./out
S_INCLUDE_DIRECTORIES="-I$VULKAN_SDK/include"
S_LINK_DIRECTORIES="-L$VULKAN_SDK/lib -L/usr/lib/x86_64-linux-gnu"
S_COMPILE_FLAGS="-D_DEBUG -g"
S_LINK_FLAGS="-lstdc++ -lvulkan -lxcb -lX11 -lX11-xcb -lxkbcommon"
S_SOURCES="main.cpp"

if [ -d $S_OUT_DIR ]; then
    rm $S_OUT_DIR/$S_OUT_BIN
else
    mkdir $S_OUT_DIR
fi

glslc -o $S_OUT_DIR/simple.frag.spv simple.frag
glslc -o $S_OUT_DIR/simple.vert.spv simple.vert

# source ../scripts/semper_build.sh
gcc $S_SOURCES --debug -std=c++17 $S_COMPILE_FLAGS $S_INCLUDE_DIRECTORIES $S_LINK_DIRECTORIES $S_LINK_FLAGS -o $S_OUT_DIR/$S_OUT_BIN
popd