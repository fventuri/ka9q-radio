add_executable(pl pl.c)
target_include_directories(pl PRIVATE ../libradio)
target_link_libraries(pl PRIVATE radio fftw3f_threads ${FFTW3F_LIBRARIES} ${BSD_LIBRARIES} m ${CMAKE_THREAD_LIBS_INIT})

include(GNUInstallDirs)
install(TARGETS pl)
