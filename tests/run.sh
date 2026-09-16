#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined -Isrc src/RobStride05.cpp tests/protocol_test.cpp -o build/protocol_test
./build/protocol_test
