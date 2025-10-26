#!/bin/bash

# Build
echo "Building periodic samples..."
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-34
cmake --build build --target bench_memory_periodic_samples -j

# Push and run
echo "Pushing to device and running..."
adb push build/bench_memory_periodic_samples /data/local/tmp/
adb shell chmod +x /data/local/tmp/bench_memory_periodic_samples
adb shell /data/local/tmp/bench_memory_periodic_samples
