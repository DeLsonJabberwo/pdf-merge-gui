# Contributing

See the README for installation and build instructions. This document covers the release process, the code layout, and manual verification steps.

## Make a release

The `Package release` GitHub Actions workflow builds and checks every package on its native platform. A manual workflow run leaves the packages as workflow artifacts and does not create a GitHub release.

To prepare a release:

1. Set the version in the `project(PDFMerge VERSION ...)` line in `CMakeLists.txt`.
2. Commit the release changes and push a matching tag, such as `v0.2.0`.
3. Wait for every packaging job to pass.
4. Review and publish the draft release GitHub Actions creates.

If a draft already exists for the tag, the workflow updates it and replaces assets with the same names. Each package contains Qt, qpdf, and third-party license notices. The workflow uses Qt 6.8.3 and qpdf 12.2.0; update the pinned versions, source hashes, and notices together.

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
