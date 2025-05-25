#!/bin/bash

# Exit on error
set -e

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Install Conan dependencies
conan install .. --build=missing

# Configure CMake
cmake ..

# Build
cmake --build .

echo "Build completed successfully!" 