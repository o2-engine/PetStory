#!/bin/bash

# Change to the directory where the script is located
cd "$(dirname "$0")"

mkdir -p Build
cmake -G "Xcode" -B Build

read -p "Do you want to open the project? (y/n) " choice
if [[ "$choice" == "y" || "$choice" == "Y" ]]; then
    echo "Opening project..."
    open Build/PetStory.xcodeproj
fi

# Keep the terminal window open
echo "Press any key to close this window..."
read -n 1 