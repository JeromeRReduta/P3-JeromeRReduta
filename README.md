# Project 3: Memory Allocator

See: https://www.cs.usfca.edu/~mmalensek/cs326/assignments/project-3.html 

This is a memory allocator, built from scratch.

# Memory Allocator?

In order for the computer to remember things, it maps info into certain regions of its physical memory. This is a very oversimplified view of what allocating memory is.

This program is more or less a simple custom memory allocator. It maps memory to a file and unmaps as needed (except in the case of Linux utilities).


# How to Compile and Use

```bash
make
LD_PRELOAD=$(pwd)/allocator.so command_name
```
