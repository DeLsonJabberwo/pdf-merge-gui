cmake_minimum_required(VERSION 3.24)
project(PDFMerge VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Pdf Concurrent Svg)
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
    Qt6::Widgets Qt6::Pdf Qt6::Concurrent Qt6::Svg qpdf::libqpdf
)
target_compile_definitions(pdf-merge PRIVATE PDFMERGE_VERSION="${PROJECT_VERSION}")
option(PDFMERGE_PACKAGING "Enable release packaging and the package self-check" OFF)
if(PDFMERGE_PACKAGING)
    target_sources(pdf-merge PRIVATE packaging/check.cpp)
    target_compile_definitions(pdf-merge PRIVATE PDFMERGE_PACKAGING)
endif()
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
if(APPLE)
    # Declare PDF document support so Finder offers Open with once the bundle
    # has been launched from its installed location. The template carries the
    # keys Qt's default plist sets, plus the document type declarations.
    set_target_properties(pdf-merge PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in"
    )
endif()

include(GNUInstallDirs)
install(TARGETS pdf-merge
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(UNIX AND NOT APPLE)
    install(FILES packaging/org.pdfmerge.desktop DESTINATION share/applications)
    install(FILES packaging/pdf-merge.svg DESTINATION share/icons/hicolor/scalable/apps)
endif()

# Release builds use qpdf with static zlib/jpeg dependencies. Qt's deployment
# tools handle Qt, but qpdf must be installed explicitly.
if(WIN32 OR APPLE)
    if(WIN32)
        get_target_property(qpdf_type qpdf::libqpdf TYPE)
        if(qpdf_type STREQUAL "SHARED_LIBRARY")
            install(FILES $<TARGET_FILE:qpdf::libqpdf> DESTINATION ${CMAKE_INSTALL_BINDIR})
        endif()
    else()
        install(DIRECTORY "$<TARGET_FILE_DIR:qpdf::libqpdf>/"
            DESTINATION pdf-merge.app/Contents/Frameworks
            FILES_MATCHING PATTERN "libqpdf*.dylib")
    endif()
    qt_generate_deploy_app_script(TARGET pdf-merge OUTPUT_SCRIPT deploy_script)
    install(SCRIPT ${deploy_script})
endif()

if(PDFMERGE_PACKAGING)
    include(packaging/Packaging.cmake)
endif()
