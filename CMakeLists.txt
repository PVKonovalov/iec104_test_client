cmake_minimum_required(VERSION 3.10)
project(iec104_test_client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIB_60870 ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lib60870-C/lib60870-C)

find_package(Threads REQUIRED)

# Disable unneeded lib60870 sub-targets
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS    OFF CACHE BOOL "" FORCE)

add_subdirectory(${LIB_60870})

# lib60870 unconditionally adds -lrt for all UNIX targets; macOS has no librt
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    foreach(_t lib60870 lib60870-shared)
        if (TARGET ${_t})
            foreach(_prop LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
                get_target_property(_libs ${_t} ${_prop})
                if (_libs)
                    list(REMOVE_ITEM _libs "-lrt")
                    set_target_properties(${_t} PROPERTIES ${_prop} "${_libs}")
                endif()
            endforeach()
        endif()
    endforeach()
endif()

add_subdirectory(vendor/cppflags)

add_executable(iec104_test_client main.cpp)

target_include_directories(iec104_test_client PRIVATE
        ${LIB_60870}/config
        ${LIB_60870}/src/inc/api
        ${LIB_60870}/src/inc/internal
        ${LIB_60870}/src/hal/inc
        ${LIB_60870}/src/common/inc
)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    find_library(COCOA_LIBRARY    Cocoa    REQUIRED)
    find_library(SECURITY_LIBRARY Security REQUIRED)
    find_library(RESOLV_LIBRARY   resolv)
    target_link_libraries(iec104_test_client PRIVATE
            Threads::Threads
            lib60870
            cppflags
            ${COCOA_LIBRARY}
            ${SECURITY_LIBRARY}
            $<$<BOOL:${RESOLV_LIBRARY}>:${RESOLV_LIBRARY}>
    )
else()
    target_link_libraries(iec104_test_client PRIVATE
            Threads::Threads
            lib60870
            cppflags
    )
endif()
