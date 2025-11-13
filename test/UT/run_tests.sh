#!/bin/bash
# Convenience script to build and run unit tests

cd ../../build

# Build the test executable
echo "Building imediaplayer_ut..."
make imediaplayer_ut -j4
if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

# Set library path for running tests
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH

# Run tests with provided arguments (or default to thread+inc+utils)
if [ $# -eq 0 ]; then
    echo "Running tests (thread, inc, utils modules)..."
    ./imediaplayer_ut --module=thread --module=inc --module=utils
else
    echo "Running tests with custom arguments: $@"
    ./imediaplayer_ut "$@"
fi

exit $?
