#!/bin/bash
# setup.sh - Development environment setup for URIX on macOS

set -e

echo "  Setting up URIX development environment on macOS..."

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo "  Homebrew is not installed. Please install it first:"
    echo "   /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
    exit 1
fi

echo "  Homebrew found"

# Install required packages
echo "  Installing required packages..."

# Check and install cross-compiler
if ! command -v x86_64-elf-gcc &> /dev/null; then
    echo "Installing x86_64-elf-gcc..."
    brew install x86_64-elf-gcc
else
    echo "  x86_64-elf-gcc already installed"
fi

# Check and install binutils
if ! command -v x86_64-elf-ld &> /dev/null; then
    echo "Installing x86_64-elf-binutils..."
    brew install x86_64-elf-binutils
else
    echo "  x86_64-elf-binutils already installed"
fi

# Check and install QEMU
if ! command -v qemu-system-x86_64 &> /dev/null; then
    echo "Installing QEMU..."
    brew install qemu
else
    echo "  QEMU already installed"
fi

# Check and install GRUB tools
if ! command -v i686-elf-grub &> /dev/null; then
    echo "Installing i686-elf-grub..."
    brew install i686-elf-grub
else
    echo "  i686-elf-grub already installed"
fi

# Check if XQuartz is available (for QEMU display)
if ! command -v xquartz &> /dev/null && ! ls /Applications/Utilities/XQuartz.app &> /dev/null 2>&1; then
    echo "   XQuartz not found. You may want to install it for QEMU display:"
    echo "   brew install --cask xquartz"
    echo "   (This is optional - you can run QEMU in text mode)"
fi

echo ""
echo "  Setup complete! Your development environment is ready."
echo ""
echo "Quick start:"
echo "  1. Run: make iso"
echo "  2. Run: make run"
echo ""
echo "Available make targets:"
echo "  make all       - Build kernel"
echo "  make iso       - Create bootable ISO"
echo "  make run       - Run in QEMU"
echo "  make clean     - Clean build files"
echo "  make help      - Show all targets"
echo ""
echo "Happy kernel hacking!"