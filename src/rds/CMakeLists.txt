add_executable(rdsd rdsd.c)
target_include_directories(rdsd PRIVATE ../libradio)
target_link_libraries(rdsd PRIVATE radio ${AVAHI_LIBRARIES} fftw3f_threads ${FFTW3F_LIBRARIES} ${BSD_LIBRARIES} m ${CMAKE_THREAD_LIBS_INIT})

include(GNUInstallDirs)
install(TARGETS rdsd DESTINATION sbin)
