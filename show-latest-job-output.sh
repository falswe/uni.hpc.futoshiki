#!/bin/bash
#
# Finds the most recently modified job output file (*.sh.o*)
# in the current directory and displays its contents.
#
set -e # Exit immediately if a command exits with a non-zero status.

# Find the most recently modified file matching the pattern.
# `ls -t` sorts by modification time. `head -n 1` gets the latest.
latest_output=$(ls -t ./*.sh.o* 2>/dev/null | head -n 1)

if [ -z "$latest_output" ]; then
    echo "No job output files (*.sh.o*) found."
    exit 1
fi

echo "--- Displaying content of: $latest_output ---"
cat "$latest_output"