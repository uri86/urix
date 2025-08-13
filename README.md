# URIX

A unix like os, built for 64 bit system from the ground up. compatible with linux.

## Dependencies
1. [brew](https://brew.sh)
2. make

**Note:** make sure you have make installed otherwise it won't work.

## Initial set up on Mac
For working and compiling on macOS first run:
```bash
chmod +x ./setup.sh
./setup.sh
```

then run:
```bash
make help
```

## compiling

if you don't want to use setup.sh, you'll need to download (using brew):
1. x86_64-elf-gcc
2. x86_64-elf-binutils
3. i686-elf-grub
4. xquartz

then run: `make iso`

this will generate a file named urix.iso, use that to run the os (using a virtual environment)