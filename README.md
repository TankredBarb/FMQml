# FM: Fast, Smooth, Modern File Manager

FM is a high-performance, cross-platform file manager prototype built with **C++20, Qt 6, and QML**. It combines a robust, multi-threaded C++ backend with a fluid, modern interface designed for premium user experience.

## Key Features

*   **⚡ Blazing Performance:** Asynchronous scanning and batch loading ensure the UI remains responsive even in directories with tens of thousands of files.
*   **✨ Total Smoothness:** Custom layout transitions (`displaced` animations) and inertial scrolling provide a "premium" feel.
*   **🚀 Smart Updates:** Incremental UI updates using sorted insertions—files and folders appear exactly where they should without jarring full-list resets.
*   **🎨 Dynamic Theming:** Fully adaptive UI with distinct Light and Dark modes, featuring high-contrast accents and evocative, professional hover states.
*   **🛠 Advanced Operations:** Robust background operation queue (copy/move/delete), Undo/Redo history, and intelligent folder creation.
*   **🖥 Native Integration:** Native system icons and thumbnails for images, seamlessly fetching from the host OS.

## Quick Start

### Prerequisites
*   **Qt 6.7+**
*   **CMake 3.21+**
*   **C++20 Compiler**

### Building
1. Clone the repository and configure:
   ```powershell
   cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.7.0/msvc2019_64"
   ```
   *(Update the path to match your specific Qt installation)*

2. Build and run:
   ```powershell
   cmake --build build --config Debug
   .\build\Debug\fm.exe
   ```

## Development Vision
We are striving for "Total Smoothness." Our roadmap focuses on perfecting shared element transitions, pre-fetching logic for instant navigation, and deep OS-level integration.

---
*Built with passion for speed and aesthetics.*
