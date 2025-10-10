#!/bin/bash

git submodule update --init --recursive

# Get the Doxygen version
doxtool="$(command -v doxygen)"
if [[ -z "$doxtool" ]]; then
    echo "Error: Doxygen is not installed."
    exit 1
fi

# Get the dot version
dottool="$(command -v dot)"
if [[ -z "$dottool" ]]; then
    echo "Error: dot is not installed. Please install graphviz"
    exit 1
fi

py3="$(command -v python3)"
if [[ -z "$py3" ]]; then
    echo "Error: python3 is not installed."
    exit 1
fi

dox_version=$(doxygen --version | awk '{print $1}')

# Required version
required_version="1.13.2"

# Check if the version matches
if [[ "$dox_version" == "$required_version" ]]; then
    echo "Doxygen version $dox_version detected. Running doxygen generate..."
    doxygen Doxyfile
    cd html
    echo -e "\033[32mDoxygen can be found at localhost:8000\033[0m"
    python3 -m http.server
else
    echo "Doxygen version $dox_version detected. Expected version: $required_version."
fi

