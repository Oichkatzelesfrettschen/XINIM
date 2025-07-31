#!/bin/bash

# Update package lists
sudo apt-get update

# Install apt tools if not already installed
dpkg -s cloc >/dev/null 2>&1 || sudo apt-get install -y cloc
dpkg -s cscope >/dev/null 2>&1 || sudo apt-get install -y cscope
dpkg -s cppcheck >/dev/null 2>&1 || sudo apt-get install -y cppcheck
dpkg -s tree >/dev/null 2>&1 || sudo apt-get install -y tree
dpkg -s python3-pip >/dev/null 2>&1 || sudo apt-get install -y python3-pip

# Install pip tool if not already installed
pip3 show lizard >/dev/null 2>&1 || pip3 install lizard

# Make the script executable
chmod +x tools/setup.sh

echo "--- Tool Status ---"
echo "cloc: $(dpkg -s cloc >/dev/null 2>&1 && echo "Installed" || echo "Not Installed")"
echo "cscope: $(dpkg -s cscope >/dev/null 2>&1 && echo "Installed" || echo "Not Installed")"
echo "cppcheck: $(dpkg -s cppcheck >/dev/null 2>&1 && echo "Installed" || echo "Not Installed")"
echo "tree: $(dpkg -s tree >/dev/null 2>&1 && echo "Installed" || echo "Not Installed")"
echo "python3-pip: $(dpkg -s python3-pip >/dev/null 2>&1 && echo "Installed" || echo "Not Installed")"
echo "lizard: $(pip3 show lizard >/dev/null 2>&1 && echo "Installed" || echo "Not Installed")"
