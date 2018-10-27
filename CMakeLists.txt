cmake_minimum_required(VERSION 3.7)
project(fmi-weather)

if(WIN32)
    set(CMAKE_FIND_LIBRARY_PREFIXES "" "lib")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".lib" ".a" ".dll.a")
endif()

set(CMAKE_PREFIX_PATH "../libuo")
set(CMAKE_C_FLAGS -pthread)

add_executable("${CMAKE_PROJECT_NAME}"
    src/fmi_weather.c)

find_package(OpenSSL)

target_include_directories("${CMAKE_PROJECT_NAME}"
    PRIVATE
        src
        ../libuo/uo_hashtbl/include
        ../libuo/uo_queue/include
        ../libuo/uo_cb/include
        ../libuo/uo_err/include
        ../libuo/uo_tcpserv/include
        ../libuo/uo_httpc/include
        ../libuo/uo_sock/include
        ../libuo/uo_mem/include
        "${OPENSSL_INCLUDE_DIR}")

find_library(LIBUO_HASHTBL
    NAMES uo_hashtbl
    HINTS "${CMAKE_PREFIX_PATH}/uo_hashtbl/lib")

find_library(LIBUO_QUEUE
    NAMES uo_queue
    HINTS "${CMAKE_PREFIX_PATH}/uo_queue/lib")

find_library(LIBUO_CB
    NAMES uo_cb
    HINTS "${CMAKE_PREFIX_PATH}/uo_cb/lib")

find_library(LIBUO_ERR
    NAMES uo_err
    HINTS "${CMAKE_PREFIX_PATH}/uo_err/lib")

find_library(LIBUO_TCPSERV
    NAMES uo_tcpserv
    HINTS "${CMAKE_PREFIX_PATH}/uo_tcpserv/lib")

find_library(LIBUO_HTTPC 
    NAMES uo_httpc
    HINTS "${CMAKE_PREFIX_PATH}/uo_httpc/lib")

find_library(LIBUO_SOCK 
    NAMES uo_sock
    HINTS "${CMAKE_PREFIX_PATH}/uo_sock/lib")

target_link_libraries("${CMAKE_PROJECT_NAME}"
    PUBLIC
        "${LIBUO_HTTPC}"
        "${LIBUO_TCPSERV}"
        "${LIBUO_CB}"
        "${LIBUO_HASHTBL}"
        "${LIBUO_QUEUE}"
        "${LIBUO_SOCK}"
        "${LIBUO_ERR}"
        "${OPENSSL_SSL_LIBRARY}"
        "${OPENSSL_CRYPTO_LIBRARY}")

if(WIN32)
    target_link_libraries("${CMAKE_PROJECT_NAME}"
        PUBLIC
            ws2_32
            crypt32)
endif()


set_target_properties("${CMAKE_PROJECT_NAME}"
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY            "${CMAKE_SOURCE_DIR}/bin/release"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE    "${CMAKE_SOURCE_DIR}/bin/release"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG      "${CMAKE_SOURCE_DIR}/bin/debug")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    configure_file(conf/fmi-weather.conf
        "${CMAKE_SOURCE_DIR}/bin/debug/uo_tcpserv.conf" COPYONLY)
    configure_file(keys/fmi-apikey.txt
        "${CMAKE_SOURCE_DIR}/bin/debug/fmi-apikey.txt" COPYONLY)
else()
    configure_file(conf/fmi-weather.conf
        "${CMAKE_SOURCE_DIR}/bin/release/uo_tcpserv.conf" COPYONLY)
    configure_file(keys/fmi-apikey.txt
        "${CMAKE_SOURCE_DIR}/bin/release/fmi-apikey.txt" COPYONLY)
endif()
