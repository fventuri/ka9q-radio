add_executable(opusd opusd.c)
target_include_directories(opusd PRIVATE ../libradio)
target_link_libraries(opusd PRIVATE radio ${OPUS_LIBRARIES} ${AVAHI_LIBRARIES} ${BSD_LIBRARIES} m ${CMAKE_THREAD_LIBS_INIT})

add_executable(opussend opussend.c)
target_include_directories(opussend PRIVATE ../libradio)
target_link_libraries(opussend PRIVATE radio ${OPUS_LIBRARIES} ${PORTAUDIO_LIBRARIES} ${BSD_LIBRARIES} m)

include(GNUInstallDirs)
install(TARGETS opusd DESTINATION sbin)
install(TARGETS opussend)
