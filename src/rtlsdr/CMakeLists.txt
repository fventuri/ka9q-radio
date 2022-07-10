add_executable(rtlsdr rtlsdr.c)
pkg_check_modules(LIBRTLSDR REQUIRED librtlsdr)
target_include_directories(rtlsdr PRIVATE ${LIBRTLSDR_INCLUDE_DIRS} ../libradio ../common)
target_link_libraries(rtlsdr PRIVATE radio ${LIBRTLSDR_LINK_LIBRARIES} ${AVAHI_LIBRARIES} ${BSD_LIBRARIES} m ${CMAKE_THREAD_LIBS_INIT})

include(GNUInstallDirs)
install(TARGETS rtlsdr DESTINATION sbin)
