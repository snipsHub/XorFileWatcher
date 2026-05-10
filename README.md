# XorFileWatcher
XorFileWatcher 
A Qt-based utility that monitors a folder for new files, applies XOR encryption/decryption using an 8-byte key, and moves the processed files to an output folder.

## Features

- File monitoring (one-shot or timer-based polling)
- XOR encryption/decryption with 16-character hex key (8 bytes)
- Configurable file mask (e.g., `*.bin`, `*.txt`, exact filename)
- Duplicate file handling: overwrite or auto-rename with counter
- Optional deletion of source files after processing
- Progress bar and detailed log window
- Multithreaded processing (`QRunnable` + `QThreadPool`)

## Requirements

- Qt5 (5.12+) or Qt6
- C++17 compiler (MinGW, MSVC, GCC)
- CMake 3.16+

## Build

```bash
git clone https://github.com/snipsHub/XorFileWatcher.git
cd XorFileWatcher
mkdir build && cd build
cmake ..
cmake --build .
