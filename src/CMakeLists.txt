find_package (PkgConfig REQUIRED)

pkg_check_modules (GLIB REQUIRED glib-2.0)
pkg_check_modules (WNCK REQUIRED libwnck-3.0)

add_definitions (-DWNCK_I_KNOW_THIS_IS_UNSTABLE
                 -D_POSIX_SOURCE
                 -D_DEFAULT_SOURCE)

add_compile_options (${GLIB_CFLAGS} ${WNCK_CFLAGS})

file (GLOB SOURCES "*.[ch]")
add_executable (${PROJECT_NAME} ${SOURCES})

# pkg-config recipe for libwnck may be a bit overzealos; trim linked libs to essentials
set (CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed -Wl,--no-undefined")

target_link_libraries (${PROJECT_NAME} PRIVATE ${GLIB_LDFLAGS} ${WNCK_LDFLAGS})

install (TARGETS ${PROJECT_NAME} DESTINATION bin)
