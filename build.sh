#!/bin/bash

# Zephyr Build Script
# This script sets up the environment and builds the K2-Zephyr project

echo "Setting up Zephyr environment..."
cd ~/zephyrproject
source .venv/bin/activate
source zephyr/zephyr-env.sh

echo "Building K2-Zephyr project..."
cd ~/zephyrproject/K2-Zephyr
west build -p -b nucleo_f767zi

echo "Build complete! Flash with: west flash"