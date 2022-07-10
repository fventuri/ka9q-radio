add_executable(wspr-decoded wspr-decoded.c)
target_include_directories(wspr-decoded PRIVATE ../libradio)
target_link_libraries(wspr-decoded PRIVATE radio fftw3f_threads ${FFTW3F_LIBRARIES} ${BSD_LIBRARIES} m ${CMAKE_THREAD_LIBS_INIT})

include(GNUInstallDirs)
install(TARGETS wspr-decoded)
