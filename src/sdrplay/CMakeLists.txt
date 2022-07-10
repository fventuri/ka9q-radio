add_executable(sdrplayd sdrplayd.c)
find_package(LibSDRplay REQUIRED)
target_include_directories(sdrplayd PRIVATE ${LIBSDRPLAY_INCLUDE_DIRS} ../libradio ../common)
target_link_libraries(sdrplayd PRIVATE radio ${LIBSDRPLAY_LIBRARIES} ${AVAHI_LIBRARIES} ${BSD_LIBRARIES} iniparser m ${CMAKE_THREAD_LIBS_INIT})

include(GNUInstallDirs)
install(TARGETS sdrplayd DESTINATION sbin)
