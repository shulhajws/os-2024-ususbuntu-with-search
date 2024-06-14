# UsusBuntu OS
A Protected Mode x86 simple 32-bit Operating System. A project for IF2230 Operating System, Informatics Engineering ITB.

## Project Description
Milestone 1 focuses on the GDT, writing text to a framebuffer and booting sequence, interrupt, keyboard I/O, and filesystem operation

Milestone 2 focuses on implementing paging for implementing user mode and shell, along with some command for actuating the filesystem operation. Some creativity and additionals is added for the shell specification like the command `clear` to clear the screen, `touch` to create some dummy file, and `help` that show some command that is available. A little splash text is also added when try to running the OS.

Milestone 3 focuses on the additional feature for OS to do multitasking process with scheduling. The scheduling used is FCFS in the form of queue.

## Author

| NIM      | Nama                        | Github Account           |
|----------|-----------------------------|--------------------------|
| 13522003 | Shafiq Irvansyah            | [@shafiqIrv](https://github.com/shafiqIrv) |
| 13522005 | Ahmad Naufal Ramadan        | [@SandWithCheese](https://github.com/SandWithCheese) |
| 13522057 | Moh Fairuz Alauddin Yahya   | [@fairuzald](https://github.com/fairuzald) |
| 13522087 | Shulha                      | [@shulhajws](https://github.com/shulhajws) |
| 13522097 | Ellijah Darrelshane S.      | [@HenryofSkalitz1202](https://github.com/HenryofSkalitz1202) |

## Project Directory Structure
```
bin/
other/
src/
├── header/
│   ├── cpu/
│   │   ├── gdt.h
│   │   ├── idt.h
│   │   ├── interrupt.h
│   │   ├── portio.h
│   ├── driver/
│   │   ├── disk.h
│   │   ├── framebuffer.h
│   │   ├── keyboard.h
│   ├── filesystem/
│   │   ├── fat32.h
│   ├── memory/
│   │   ├── paging.h
│   ├── process/
│   │   ├── process.h
│   ├── scheduler/
│   ├── stdlib/
│       ├── string.h
│       ├── command.h
│       ├── kernel-entrypoint.h
├── output/
├── stdlib/
    ├── string.c
    ├── cmos.c
    ├── cmos.h
    ├── command.c
    ├── crt0.s
    ├── disk.c
    ├── external-inserter.c
    ├── fat32.c
    ├── framebuffer.c
    ├── gdt.c
    ├── idt.c
    ├── interrupt.c
    ├── intsetup.s
    ├── kernel-entrypoint.s
    ├── kernel.c
    ├── keyboard.c
    ├── linker.ld
    ├── menu.lst
    ├── paging.c
    ├── portio.c
    ├── process.c
    ├── rtc.c
    ├── scheduler.c
    ├── user-linker.ld
    ├── user-shell.c
test/
├── kaguya.txt
├── tes.txt
.gdb_history
.gitignore
makefile
README.md
```

## Running The OS
- Clone this repository and make sure to be in the right directory
- Run the makefile using make command, all of the dependencies will automatically be compiled, and a kernel window will pop up using the QEMU Emulator.
- To create a disk image, use `make disk` command, this will create a new disk image and delete the last one.
- To insert the shell, use `make insert-shell` command, this will insert a shell into the user space, and also some text files.
