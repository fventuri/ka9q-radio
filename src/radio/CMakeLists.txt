add_executable(radiod main.c audio.c fm.c wfm.c linear.c radio.c rtcp.c radio_status.c modes.c decode_status.c)
target_include_directories(radiod PRIVATE ../libradio)
target_link_libraries(radiod PRIVATE radio ${AVAHI_LIBRARIES} fftw3f_threads ${FFTW3F_LIBRARIES} ${BSD_LIBRARIES} iniparser ${CMAKE_THREAD_LIBS_INIT} m)

include(GNUInstallDirs)
install(TARGETS radiod DESTINATION sbin)
