cmake_minimum_required(VERSION 3.24)
project(PDFMerge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Pdf Concurrent)
# qpdf's version config enforces same-major compatibility. Check our minimum
# separately so an installed qpdf 12 is not rejected by a request for qpdf 11.
find_package(qpdf CONFIG REQUIRED)
if(qpdf_VERSION VERSION_LESS "11.0")
    message(FATAL_ERROR "qpdf 11.0 or newer is required; found ${qpdf_VERSION}")
endif()

qt_add_executable(pdf-merge WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/source.cpp src/source.h
    src/assemblymodel.cpp src/assemblymodel.h
    src/documenttree.cpp src/documenttree.h
    src/preview.cpp src/preview.h
    src/export.cpp src/export.h
    src/mainwindow.cpp src/mainwindow.h
)
target_link_libraries(pdf-merge PRIVATE
    Qt6::Widgets Qt6::Pdf Qt6::Concurrent qpdf::libqpdf
)
if(MSVC)
    target_compile_options(pdf-merge PRIVATE /W4 /utf-8)
else()
    target_compile_options(pdf-merge PRIVATE -Wall -Wextra -Wpedantic)
endif()
set_target_properties(pdf-merge PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "PDF Merge"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.pdfmerge.desktop"
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

include(GNUInstallDirs)
install(TARGETS pdf-merge
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(UNIX AND NOT APPLE)
    install(FILES packaging/org.pdfmerge.desktop DESTINATION share/applications)
    install(FILES packaging/pdf-merge.svg DESTINATION share/icons/hicolor/scalable/apps)
endif()

# Qt's deployment script bundles Qt on Windows/macOS. Package qpdf and its
# runtime dependencies alongside it using the chosen dependency manager.
if(WIN32 OR APPLE)
    qt_generate_deploy_app_script(TARGET pdf-merge OUTPUT_SCRIPT deploy_script)
    install(SCRIPT ${deploy_script})
endif()
