set(PDFMERGE_PACKAGE_ARCH "" CACHE STRING "Release architecture used in asset names")
set(PDFMERGE_PACKAGE_RESOURCES "" CACHE PATH "Prepared icons and dependency notices")
if(NOT PDFMERGE_PACKAGE_ARCH OR NOT EXISTS "${PDFMERGE_PACKAGE_RESOURCES}/licenses")
    message(FATAL_ERROR "Packaging requires PDFMERGE_PACKAGE_ARCH and PDFMERGE_PACKAGE_RESOURCES")
endif()

if(APPLE)
    set(icon "${PDFMERGE_PACKAGE_RESOURCES}/pdf-merge.icns")
    target_sources(pdf-merge PRIVATE "${icon}")
    set_source_files_properties("${icon}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(pdf-merge PROPERTIES MACOSX_BUNDLE_ICON_FILE pdf-merge.icns)
    set(notice_dir "pdf-merge.app/Contents/Resources/licenses")
elseif(WIN32)
    configure_file("${CMAKE_CURRENT_LIST_DIR}/windows.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/windows.rc" @ONLY)
    target_sources(pdf-merge PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/windows.rc")
    set(notice_dir "licenses")
else()
    set(notice_dir "share/licenses/pdf-merge")
endif()
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION "${notice_dir}" RENAME PDFMerge.txt)
install(DIRECTORY "${PDFMERGE_PACKAGE_RESOURCES}/licenses/" DESTINATION "${notice_dir}")

set(CPACK_PACKAGE_NAME "pdf-merge")
set(CPACK_PACKAGE_VENDOR "PDF Merge")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Arrange PDF pages and export one document")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "PDF Merge")
set(CPACK_PACKAGE_EXECUTABLES "pdf-merge" "PDF Merge")
set(CPACK_MONOLITHIC_INSTALL ON)
# Preserve quotes, backslashes, and NSIS variables when CPack writes these
# settings into CPackConfig.cmake.
set(CPACK_VERBATIM_VARIABLES ON)
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_PACKAGE_FILE_NAME "pdf-merge-${PROJECT_VERSION}-windows-${PDFMERGE_PACKAGE_ARCH}")
    set(CPACK_NSIS_PACKAGE_NAME "PDF Merge")
    set(CPACK_NSIS_DISPLAY_NAME "PDF Merge")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\pdf-merge.exe")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "PDFMerge")
    # Register PDF Merge with Explorer's Open with menu. Keep the commands in
    # raw NSIS files so CPack cannot corrupt their nested quotes and backslashes.
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "!include \"${CMAKE_CURRENT_LIST_DIR}/windows-install.nsh\"")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
        "!include \"${CMAKE_CURRENT_LIST_DIR}/windows-uninstall.nsh\"")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGE_FILE_NAME "pdf-merge-${PROJECT_VERSION}-macos-${PDFMERGE_PACKAGE_ARCH}")
    set(CPACK_DMG_VOLUME_NAME "PDF Merge")
    set(CPACK_DMG_FORMAT "UDZO")
    # Ad-hoc signing is needed for Apple Silicon. This is not Developer ID
    # signing or notarization; releases are still unsigned for Gatekeeper.
    install(CODE [[
        execute_process(COMMAND codesign --force --deep --sign -
            "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/pdf-merge.app"
            COMMAND_ERROR_IS_FATAL ANY)
    ]])
endif()
include(CPack)
