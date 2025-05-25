#!/bin/bash

# Exit on error
set -e

# Check for required system packages
echo "Checking for required system packages..."
if ! dpkg -l | grep -q libcurl4-openssl-dev; then
    echo "Installing libcurl4-openssl-dev..."
    sudo apt-get update
    sudo apt-get install -y libcurl4-openssl-dev
fi

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