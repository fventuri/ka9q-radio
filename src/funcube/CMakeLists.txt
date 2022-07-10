add_library(fcd STATIC fcd.c hid-libusb.c)
pkg_check_modules(LIBUSB-1.0 REQUIRED libusb-1.0)
target_include_directories(fcd PRIVATE ${LIBUSB-1.0_INCLUDE_DIRS} ../libradio)

add_executable(funcubed funcubed.c)
target_include_directories(funcubed PRIVATE ../libradio ../common)
if(${CMAKE_SYSTEM_NAME} MATCHES Darwin)
  set(ICONV_LIBRARIES iconv)
endif(${CMAKE_SYSTEM_NAME} MATCHES Darwin)
target_link_libraries(funcubed PRIVATE radio fcd ${PORTAUDIO_LIBRARIES} ${AVAHI_LIBRARIES} ${LIBUSB-1.0_LIBRARIES} ${BSD_LIBRARIES} ${ICONV_LIBRARIES} m ${CMAKE_THREAD_LIBS_INIT})

include(GNUInstallDirs)
install(TARGETS funcubed DESTINATION sbin)
