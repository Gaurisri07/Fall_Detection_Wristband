#!/bin/bash
# Host-native Fall Detector Live Demo Runner for Class Presentation

set -e

echo "Compiling C++ fall detector host-natively..."
g++ -std=c++11 -Iinclude -Isensing src/demo_native.cpp sensing/fall_detector.cpp -o demo_native

echo "Launching Live Presentation Demo..."
./demo_native
