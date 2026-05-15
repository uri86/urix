# URIX

## Dependencies

1. [brew](https://brew.sh)
2. make

> [!NOTE]
> make sure you have the dependencies installed, otherwise the compilation will fail. 

> [!IMPORTANT]
> This project only compiles on **macOS**.

## Initial set up on Mac

Before compiling, you will need to download the required toolchain using brew. Keep in mind that **x86_64-elf-gcc** is the only compiler to be used for this project.

You will need to install the following packages:

1. `x86_64-elf-gcc`
2. `x86_64-elf-binutils`
3. `i686-elf-grub`
4. `xquartz`

*(You may also need an emulator like `qemu` to run the generated ISO).*

After the initial packages setup, clone the repository to your local machine and navigate into the project directory:

```bash
git clone https://github.com/uri86/urix.git
cd urix
```

Next, you need to make the script that generates the embedded programs executable. This script runs automatically during the build process, but requires the correct permissions first:

```bash
chmod +x userspace/gen_embedded.sh
```

## Compiling

> [!NOTE]
> The Makefiles handle the compilation of the kernel, the shared libc, and the userspace programs (which are automatically generated into an embedded C array via `gen_embedded.sh` before the kernel is linked).

To build the project and generate the bootable image, run:

```bash
make iso
```

This will generate a file named `urix.iso`. Use that to run the OS using a virtual environment.