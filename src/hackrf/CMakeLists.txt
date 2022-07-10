add_executable(hackrf hackrf.c)
pkg_check_modules(LIBHACKRF REQUIRED libhackrf)
target_include_directories(hackrf PRIVATE ${LIBHACKRF_INCLUDE_DIRS} ../libradio ../common)
target_link_libraries(hackrf PRIVATE radio ${LIBHACKRF_LINK_LIBRARIES} ${BSD_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} m)

include(GNUInstallDirs)
install(TARGETS hackrf DESTINATION sbin)
