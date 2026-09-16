#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined -Isrc src/RobStride05.cpp tests/protocol_test.cpp -o build/protocol_test
./build/protocol_test
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=undefined -Itests/stubs -Isrc src/RobStride05.cpp src/RS05_Single_CAN.cpp tests/controller_test.cpp -o build/controller_test
./build/controller_test
