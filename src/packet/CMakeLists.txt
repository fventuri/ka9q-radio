add_executable(packetd packetd.c ax25.c)
target_include_directories(packetd PRIVATE ../libradio)
target_link_libraries(packetd PRIVATE radio ${AVAHI_LIBRARIES} fftw3f_threads ${FFTW3F_LIBRARIES} ${BSD_LIBRARIES} m ${CMAKE_THREAD_LIBS_INIT})

add_executable(aprs aprs.c ax25.c)
target_include_directories(aprs PRIVATE ../libradio)
target_link_libraries(aprs PRIVATE radio ${BSD_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} m)

add_executable(aprsfeed aprsfeed.c ax25.c)
target_include_directories(aprsfeed PRIVATE ../libradio)
target_link_libraries(aprsfeed PRIVATE radio ${BSD_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} m)

include(GNUInstallDirs)
install(TARGETS packetd DESTINATION sbin)
install(TARGETS aprs aprsfeed)
