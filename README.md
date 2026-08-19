<h1 align="center">TextTools</h1>
<p align="center">
  <img src="Logo_txt.ico" alt="TextTools Logo" width="128" height="128">
</p>
<p align="center">
  A blazing-fast, lightweight Windows utility for processing large text files and Combolists natively using Win32 API.
</p>

## Features
TextTools is designed to process massive text files efficiently with multi-threaded performance. 

- **Add & Remove Files**: Drag and drop support for `.txt` files.
- **Combine Files**: Merge multiple files into one.
- **Remove Duplicates**: Strip duplicate lines seamlessly.
- **Clean Invalid**: Keep only lines formatted as `email:password`. 
- **Extract Domain**: Extract specific domains (e.g., `@gmail.com`) from files.
- **Split by Lines**: Split massive files into smaller chunks of exact line counts.
- **Sort Files**: Alphabetical (A-Z, Z-A) and Length-based (Shortest, Longest) sorting.

## Performance
- **Native Win32 C++**: Fully hardware accelerated with near zero overhead. No heavy runtimes like Electron or .NET.
- **Multi-threaded Processing**: Avoids freezing the UI while handling gigabytes of text.
- **Dark Mode Support**: Beautiful flat dark-mode UI customized via Subclassing.

## Build from Source
The project relies strictly on the native Windows API and requires MinGW to compile.

1. Install `MinGW` (GCC for Windows).
2. Run `build.bat` in the root directory.
3. The executable `TextTools.exe` will be built instantly!

## Download
Check out the [Releases](../../releases) page for the latest pre-built portable executable.

<hr>
<p align="center">Developed for extreme speed and efficiency.</p>
