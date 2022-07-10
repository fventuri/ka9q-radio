add_executable(airspyd airspyd.c)
pkg_check_modules(LIBAIRSPY REQUIRED libairspy)
target_include_directories(airspyd PRIVATE ${LIBAIRSPY_INCLUDE_DIRS} ../libradio ../common)
target_link_libraries(airspyd PRIVATE radio ${LIBAIRSPY_LIBRARIES} ${AVAHI_LIBRARIES} ${BSD_LIBRARIES} iniparser m ${CMAKE_THREAD_LIBS_INIT})

add_executable(airspyhfd airspyhfd.c)
pkg_check_modules(LIBAIRSPYHF REQUIRED libairspyhf)
target_include_directories(airspyhfd PRIVATE ${LIBAIRSPYHF_INCLUDE_DIRS} ../libradio ../common)
target_link_libraries(airspyhfd PRIVATE radio ${LIBAIRSPYHF_LIBRARIES} ${AVAHI_LIBRARIES} ${BSD_LIBRARIES} iniparser m ${CMAKE_THREAD_LIBS_INIT})

include(GNUInstallDirs)
install(TARGETS airspyd airspyhfd DESTINATION sbin)
