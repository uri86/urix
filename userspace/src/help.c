/**
 * Licensed under MIT License - URIX project
 * help.c - the help print command
 */
#include "urix.h"

int main()
{
    println("Built-in commands:");
    println("  cd <dir> - change directory");
    println("  exit     - exit shell");
    println("  pid      - show PID");
    println("");
    println("External commands (in /bin):");
    println("  help                    - this help");
    println("  clr                     - clear screen");
    println("  proct                   - process table (root)");
    println("  pmmstat                 - physical memory stats (root)");
    println("  kmlcstat                - kernel allocator stats (root)");
    println("  prntlg                  - print kernel log (root)");
    println("  prntpcb <pid>           - print PCB (root)");
    println("  sudo <cmd> [args...]    - execute as root");
    println("  ls [-la] [path]         - list directory");
    println("  cat [-n] <file> [...]   - print file(s)");
    println("  touch <file> [...]      - create empty file(s)");
    println("  mkdir [-p] <dir> [...]  - create directory");
    println("  rm [-rf] <path> [...]   - remove file/directory");
    println("  cp <src> <dst>          - copy file");
    println("  mv <src> <dst>          - move/rename file");
    println("  echo [-n] [args...]     - print arguments");
    println("  pwd                     - print working directory");
    return 0;
}
