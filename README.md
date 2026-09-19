# PDF Merge

A native desktop app for arranging PDFs and exporting one document. C++20, Qt 6 Widgets, Qt PDF, and qpdf. All processing happens locally.

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

The install rules provide a Linux desktop entry and icon, and invoke Qt deployment on Windows/macOS. Distributable packages still need dependency verification, including qpdf and its runtime libraries. Platform installers, signing, and macOS notarization are not configured yet.

This initial implementation has not been compiled or run on any platform yet.

## Behavior and export scope

The sidebar defines output order from top to bottom. Groups are editable containers, initially named after their imported PDF. A moved page retains its source filename and original page number. Empty groups remain available as drop targets until removed.

Each import makes a temporary, private copy of the source. The preview and exporter use that copy, so subsequent changes to the original cannot silently change the result. Temporary copies live for as long as the document or its undo history needs them and are removed on normal shutdown. Passwords stay in memory and are not written to application settings.

Export copies PDF page objects rather than rendering them into images. Original page dimensions, text, vectors, and embedded images are preserved. qpdf's annotation-copy helper handles copying annotations and form fields. Interactive forms and internal links still need verification against representative documents.

The initial exporter starts a new document. It does not preserve document bookmarks, document-level attachments, source metadata, or valid digital signatures. Exports are unencrypted, including exports from password-protected inputs. It does not provide OCR, recompression presets, or PDF/A conversion.

Exports use `QSaveFile` with direct-write fallback disabled. Cancellation or an error leaves an existing destination intact. Cancellation is checked between pages and while writing, but parsing an individual input may need to finish first. The app prevents exporting over an active source path.

Importing and loading document metadata currently happen on the UI thread. Large inputs can briefly pause the interface during import. Preview rendering and export run in the background. The preview renders visible pages, limits outstanding render requests to one, and maintains a 64 MiB image cache.

Arrangements are session-only. Exporting saves the resulting PDF, not an editable project. The undo history holds up to 100 operations.

## Code layout

| File | Responsibility |
| --- | --- |
| `src/source.*` | Temporary input copies, Qt PDF documents, renderers |
| `src/assemblymodel.*` | Two-level group/page model, page order, undoable edits |
| `src/documenttree.*` | Sidebar interactions, drop targets, selection preservation |
| `src/preview.*` | Output-order preview, rendering queue, image cache |
| `src/export.*` | Background qpdf assembly and atomic output |
| `src/mainwindow.*` | Window, import/export workflow, menus, themes |
| `src/main.cpp` | Application startup and command-line file import |

## Manual verification

After the first build, use PDFs with visibly different page labels to check:

1. Add two PDFs through the picker and through a file-manager drop. Confirm the preview order.
2. Reorder the groups. Move a page within a group, into another group, and into a standalone position. Confirm the insertion marker matches the resulting location.
3. Undo and redo each operation, including removal and renaming. Check selection and group expansion.
4. Export, reopen the result in another viewer, and verify page order, page sizes, selectable text, images, and rotated pages.
5. Import a password-protected PDF. Try an incorrect password and cancel the password prompt.
6. Try a non-PDF, an unreadable file, and a damaged PDF. Confirm valid files in the same batch still import.
7. Export over an existing output, cancel a large export, and try an unwritable destination. Confirm no partial output replaces the original destination.
8. Exercise light/dark themes, keyboard focus, high-DPI screens, long filenames, and non-ASCII paths.
9. Scroll a large document and change zoom repeatedly. Confirm the UI remains responsive and preview memory stays bounded.
10. Verify annotations, forms with colliding field names, and internal links before relying on those features.

## License

Application code is MIT licensed. See `LICENSE`.

Qt and qpdf are separate dependencies with their own licenses. Qt Widgets and the current Qt PDF releases provide LGPLv3 options; qpdf uses Apache-2.0. Qt PDF also includes third-party components. Distributions must include the notices and license materials required by their actual dependency builds. This repository does not vendor those libraries.
