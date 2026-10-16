cmake_minimum_required(VERSION 3.16)

# The version of a release: shown in the watch's log, and in the web installer.
set(PROJECT_VER "0.1.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Each board keeps its own lock file of components: a build for another board
# (idf.py -D SDKCONFIG=sdkconfig.<board> ..., see docs/PORTING.md) uses
# dependencies.lock.<board>; the M5Stack StopWatch, the default, dependencies.lock.
if(DEFINED SDKCONFIG)
    get_filename_component(sdkconfig_name "${SDKCONFIG}" NAME)
    if(sdkconfig_name MATCHES "^sdkconfig\\.(.+)$" AND NOT CMAKE_MATCH_1 STREQUAL "m5_stopwatch")
        idf_build_set_property(DEPENDENCIES_LOCK "${CMAKE_CURRENT_LIST_DIR}/dependencies.lock.${CMAKE_MATCH_1}")
    endif()
endif()
project(knoedel_watch)
