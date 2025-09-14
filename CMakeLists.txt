# CMake build configuration for Zephyr RTOS application
# This file tells the build system how to compile our embedded application

# Require minimum CMake version for modern features
cmake_minimum_required(VERSION 3.20.0)

# Find and load the Zephyr RTOS build system
# ZEPHYR_BASE environment variable points to Zephyr installation
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Define our project name (used in build outputs)
project(k2_zephyr_app)

# Tell Zephyr to compile our main source file into the application
# 'app' is Zephyr's standard target name for the main application binary
target_sources(app PRIVATE src/main.c)