include(${CMAKE_CURRENT_LIST_DIR}/conan-wrapper.cmake)

function(conan_init)
    # Value for settings.os.
    set(CONAN_OS                ${CMAKE_SYSTEM_NAME})
    string(REPLACE "Darwin" "Macos" CONAN_OS ${CONAN_OS})

    # Value for settings.os_build.
    set(CONAN_HOST_OS       ${CMAKE_HOST_SYSTEM_NAME})
    string(REPLACE "Darwin" "Macos" CONAN_HOST_OS ${CONAN_HOST_OS})

    # Value for settings.arch.
    if (NOT DEFINED CONAN_ARCH)
        if (PLATFORM STREQUAL linux)
            if (CMAKE_CROSSCOMPILING)
                set(CONAN_ARCH  armv8_32)
            else ()
                set(CONAN_ARCH  ${CMAKE_HOST_SYSTEM_PROCESSOR})
            endif ()
        elseif (PLATFORM STREQUAL freertos-arm OR PLATFORM STREQUAL baremetal-arm)
            set(CONAN_ARCH      armv7)
        endif ()
    endif ()
    string(REPLACE "armv7l" "armv8_32" CONAN_ARCH ${CONAN_ARCH})

    # Value for settings.arch_build.
    if (CMAKE_CROSSCOMPILING)
        set(CONAN_HOST_ARCH     x86_64)
    else ()
        set(CONAN_HOST_ARCH     ${CONAN_ARCH})
    endif ()

    # Value for settings.compiler.
    if (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
        set(CONAN_COMPILER      gcc)
    else ()
        set(CONAN_COMPILER      clang)
    endif ()

    # Value for settings.compiler.version.
    string(REPLACE "." ";" VERSION_LIST ${CMAKE_CXX_COMPILER_VERSION})
    list(GET VERSION_LIST 0 CONAN_COMPILER_MAJOR)

    # Value for settings.compiler.libcxx.
    if (CONAN_OS STREQUAL Macos AND CONAN_COMPILER STREQUAL clang)
        set(CONAN_LIBCXX        libc++)
    else ()
        set(CONAN_LIBCXX        libstdc++11)
    endif ()

    # Values for env.CFLAGS, env.CXXFLAGS and env.LDFLAGS.
    string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
    set(CONAN_C_FLAGS           "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${BUILD_TYPE}}")
    set(CONAN_CXX_FLAGS         "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${BUILD_TYPE}}")
    set(CONAN_LINKER_FLAGS      "${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS_${BUILD_TYPE}}")

    message(STATUS "Configuring '${CMAKE_BINARY_DIR}/conan-profile'")
    configure_file(${CMAKE_SOURCE_DIR}/tools/profile.in ${CMAKE_BINARY_DIR}/conan-profile)
endfunction ()

include(CMakeParseArguments)

macro(conan_get)
    set(OPTIONS                 "")
    set(ONE_VALUE_ARGS          "")
    set(MULTI_VALUE_ARGS        REQUIRES OPTIONS ENV)
    cmake_parse_arguments(CONAN_GET "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})

    set(CMAKE_SYSTEM_NAME_TMP   ${CMAKE_SYSTEM_NAME})
    set(CMAKE_SYSTEM_NAME       ${CMAKE_HOST_SYSTEM_NAME})
    conan_cmake_run(
        REQUIRES                ${CONAN_GET_REQUIRES}
        OPTIONS                 ${CONAN_GET_OPTIONS}
        PROFILE                 ${CMAKE_BINARY_DIR}/conan-profile
        BUILD                   missing
        GENERATORS              cmake_find_package cmake_paths
    )
    set(CMAKE_SYSTEM_NAME       ${CMAKE_SYSTEM_NAME_TMP})

    include(${CMAKE_CURRENT_BINARY_DIR}/conan_paths.cmake)
endmacro ()
