add_executable(control control.c ../radio/modes.c bandplan.c ../radio/decode_status.c)
target_include_directories(control PRIVATE ../libradio ../common)
target_link_libraries(control PRIVATE radio fftw3f_threads ${FFTW3F_LIBRARIES} ${NCURSESW_LIBRARIES} iniparser ${BSD_LIBRARIES} m ${CMAKE_THREAD_LIBS_INIT})

include(GNUInstallDirs)
install(TARGETS control)
