add_executable(modulate modulate.c)
target_include_directories(modulate PRIVATE ../libradio ../common)
target_link_libraries(modulate PRIVATE radio fftw3f_threads ${FFTW3F_LIBRARIES} m ${CMAKE_THREAD_LIBS_INIT})

include(GNUInstallDirs)
install(TARGETS modulate)
