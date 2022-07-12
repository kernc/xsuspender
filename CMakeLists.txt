cmake_minimum_required (VERSION 2.8.12 FATAL_ERROR)

project (xsuspender C)
set (PROJECT_VERSION 1.1)

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release)
endif ()

set (CMAKE_C_STANDARD 99)
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")  # CMake<=3.0

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -DG_ENABLE_DEBUG")
set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

set (example_dir "share/doc/${PROJECT_NAME}/examples")

add_definitions (-DPROJECT_NAME="${PROJECT_NAME}"
                 -DPROJECT_VERSION="${PROJECT_VERSION}"
                 -DEXAMPLE_CONF="${CMAKE_INSTALL_PREFIX}/${example_dir}/${PROJECT_NAME}.conf")

# For CMAKE_INSTALL_SYSCONFDIR below
include (GNUInstallDirs)

install (FILES       data/${PROJECT_NAME}.conf
         DESTINATION ${example_dir})
install (FILES       data/${PROJECT_NAME}.desktop
         DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/xdg/autostart)
install (FILES       doc/${PROJECT_NAME}.1
         DESTINATION share/man/man1)

add_subdirectory (src)

# Tests, if X is running
if (NOT $ENV{DISPLAY} EQUAL "")
    set_property (GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)  # Avoid cruft CTest build targets
    include (CTest)
    add_test (TestHelp src/${PROJECT_NAME} --help)
    set_tests_properties (TestHelp PROPERTIES PASS_REGULAR_EXPRESSION "Usage:\n  ${PROJECT_NAME}.*")
endif ()

# `make package_source`
set (CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set (CPACK_PACKAGE_EXECUTABLES "${PROJECT_NAME}")
set (CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set (CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
set (CPACK_SOURCE_IGNORE_FILES "/\\\\.git/;.*build.*;/\\\\..*")
set (CPACK_SOURCE_GENERATOR "TXZ")
include (CPack)
