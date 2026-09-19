# PDF Merge

A native desktop app for arranging PDFs and exporting one document. C++20, Qt 6 Widgets, Qt PDF, and qpdf. All processing happens locally.

## AI Notice

**NOTICE**: This was created using LLM-generated code. This is not meant to be a hand-crafted artisanal codebase. It's meant to be exactly what it is.
I made this real quick because it was annoying me that good tools for this aren't widely available and I have friends that aren't going to use a terminal utility.
If you have some moral quandry with AI-generated code, don't use this. If you want a non-LLM system, I hand-crafted a CLI utility at [DeLsonJabberwo/pdf-merge](https://github.com/DeLsonJabberwo/pdf-merge).
Otherwise, go deal with this problem another way.

## Current implementation

- Drop PDFs onto the window or click to browse. Multiple files can be imported together.
- Expand file groups to arrange their pages. Drag groups to reorder them.
- Drop a page onto a group to append it, or between its children to insert it.
- Drag near the left edge of the list to move a page outside its group. The context menu and `Alt+Left` also do this.
- Rename groups with a double-click or `F2`.
- Remove the selection with `Delete`. Undo and redo cover importing, moving, removing, and renaming.
- Preview the assembled output with continuous scrolling, zoom, and fit-to-width.
- Export through a native save dialog, with progress, cancellation, and optional opening afterward.
- Follow the system's light/dark preference or choose a theme under **View → Appearance**.

The UI uses soft neutral surfaces and a muted red accent. PDF pages keep their original colors in both themes.

## Installation

Release downloads are available from this repository's **Releases** page.

| Platform | Package | Notes |
| --- | --- | --- |
| Linux x86-64 | one-line installer, AppImage, or `.deb` | Ubuntu 22.04 or another distribution with glibc 2.35 or newer |
| Windows x86-64 | Setup `.exe` or portable `.zip` | Windows 10 or 11 |
| macOS Apple Silicon | `pdf-merge-<version>-macos-arm64.dmg` | macOS 12 or newer |
| macOS Intel | `pdf-merge-<version>-macos-x86_64.dmg` | macOS 12 or newer |

Linux users can install the latest release with a single command. It fetches `install-linux.sh` from the most recent published release, verifies its checksum, and installs for the current user:

```sh
curl -fsSL https://github.com/DeLsonJabberwo/pdf-merge-gui/releases/latest/download/install-linux.sh | sh
```

Running the command again updates an existing installation. Run `install-linux.sh --uninstall` to remove it.

Prefer to review before running? Download `install-linux.sh` and `SHA256SUMS` from the same release, check the script, then run:

```sh
chmod +x install-linux.sh
./install-linux.sh
```

You can instead make the AppImage executable and run it directly, or install the Debian package with `sudo apt install ./pdf-merge-*.deb`.

The installers register PDF Merge in the file manager's Open with menu: Explorer on Windows, Finder on macOS, and desktop entries on Linux. Right-click a PDF and choose **Open with → PDF Merge** to add it to a new assembly. Registration never changes your default PDF viewer. The portable `.zip` is unregistered; use **Open with → Choose another app** and select `pdf-merge.exe` from the extracted folder.

The initial packages are unsigned. Windows SmartScreen may require **More info → Run anyway**. On macOS, open the app once with **Control-click → Open**. Published releases include `SHA256SUMS`; the Linux installer checks its downloads against that file.

## Build

Required dependencies:

| Dependency | Minimum | CMake package |
| --- | --- | --- |
| CMake | 3.24 | |
| C++ compiler | C++20 support | |
| Qt | 6.8 | Widgets, Pdf, Concurrent |
| qpdf | 11.0 | `qpdf::libqpdf` |

Qt PDF is a separate Qt module. Installing Qt Base alone is insufficient. Use matching Qt versions for all modules, and matching architectures and compiler toolchains for Qt, qpdf, and the application.

On Arch Linux, the dependency packages are `qt6-base`, `qt6-pdf`, `qpdf`, `cmake`, and `ninja`, plus the C++ toolchain. Wayland sessions also need Qt's Wayland platform plugin, provided by `qt6-wayland`.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/pdf-merge
```

Files can also be passed as arguments:

```sh
./build/pdf-merge first.pdf second.pdf
```

On Windows or macOS, install the dependencies through your Qt installation and dependency manager. Set `CMAKE_PREFIX_PATH` if CMake cannot locate them:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/Qt;/path/to/qpdf"
cmake --build build --config Release
```

With a multi-configuration Windows generator, the executable is typically `build/Release/pdf-merge.exe`. With a single-configuration macOS generator, the application is `build/pdf-merge.app`.

```sh
cmake --install build --config Release --prefix dist
```

The install rules provide a Linux desktop entry and icon and invoke Qt deployment on Windows and macOS. Release packages also include qpdf and the Qt libraries and plugins they need.

## Contributing

Contributions are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for the release process, the code layout, and the manual verification checklist.

## License

Application code is MIT licensed. See `LICENSE`.

Qt and qpdf are separate dependencies with their own licenses. Qt Widgets and the current Qt PDF releases provide LGPLv3 options; qpdf uses Apache-2.0. Qt PDF also includes third-party components. Distributions must include the notices and license materials required by their actual dependency builds. This repository does not vendor those libraries.
