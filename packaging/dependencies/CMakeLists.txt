cmake_minimum_required(VERSION 3.24)
project(PDFMergeDependencies LANGUAGES C CXX)
include(ExternalProject)

# A separate build keeps dependency fetching out of normal application builds.
# zlib and jpeg are embedded in qpdf; qpdf and Qt remain shared libraries.
set(common_args
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
    -DCMAKE_INSTALL_LIBDIR=lib
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
)
if(APPLE)
    list(APPEND common_args
        -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
        -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
endif()

ExternalProject_Add(zlib
    URL https://zlib.net/fossils/zlib-1.3.1.tar.gz
    URL_HASH SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR ${CMAKE_BINARY_DIR}/sources/zlib
    CMAKE_ARGS ${common_args} -DZLIB_BUILD_EXAMPLES=OFF
)
ExternalProject_Add(jpeg
    URL https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.1.0/libjpeg-turbo-3.1.0.tar.gz
    URL_HASH SHA256=9564c72b1dfd1d6fe6274c5f95a8d989b59854575d4bbee44ade7bc17aa9bc93
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR ${CMAKE_BINARY_DIR}/sources/jpeg
    CMAKE_ARGS ${common_args}
        -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_TURBOJPEG=OFF
        -DWITH_CRT_DLL=ON -DWITH_SIMD=OFF
)
if(WIN32)
    set(zlib_library "${CMAKE_INSTALL_PREFIX}/lib/zlibstatic.lib")
    set(jpeg_library "${CMAKE_INSTALL_PREFIX}/lib/jpeg-static.lib")
else()
    set(zlib_library "${CMAKE_INSTALL_PREFIX}/lib/libz.a")
    set(jpeg_library "${CMAKE_INSTALL_PREFIX}/lib/libjpeg.a")
endif()
ExternalProject_Add(qpdf
    DEPENDS zlib jpeg
    URL https://github.com/qpdf/qpdf/releases/download/v12.2.0/qpdf-12.2.0.tar.gz
    URL_HASH SHA256=b3d1575b2218badc3549d6977524bb0f8c468c6528eebc8967bbe3078cf2cace
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR ${CMAKE_BINARY_DIR}/sources/qpdf
    CMAKE_ARGS ${common_args}
        -DBUILD_SHARED_LIBS=ON -DBUILD_STATIC_LIBS=OFF
        -DUSE_IMPLICIT_CRYPTO=OFF -DREQUIRE_CRYPTO_NATIVE=ON
        -DBUILD_DOC=OFF -DINSTALL_MANUAL=OFF -DINSTALL_EXAMPLES=OFF
        -DPKG_CONFIG_EXECUTABLE=OFF
        -DZLIB_H_PATH=${CMAKE_INSTALL_PREFIX}/include
        -DZLIB_LIB_PATH=${zlib_library}
        -DLIBJPEG_H_PATH=${CMAKE_INSTALL_PREFIX}/include
        -DLIBJPEG_LIB_PATH=${jpeg_library}
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release --target libqpdf
    INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR> --config Release --component lib
    COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR> --config Release --component dev
)
