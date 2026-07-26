#!/usr/bin/env bash
# Host-native tests for the platform-free parts of Crafty Legend.
# The addon itself is a Windows DLL cross-compiled with MinGW; this harness
# compiles only the modules that have no Windows/ImGui dependencies.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/tests
g++ -std=c++17 -Wall -Wextra -I lib/nlohmann \
    tests/test_character_crafting.cpp src/CharacterCrafting.cpp \
    -o build/tests/test_character_crafting
./build/tests/test_character_crafting
