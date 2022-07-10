add_executable(stereod stereod.c)
target_include_directories(stereod PRIVATE ../libradio)
target_link_libraries(stereod PRIVATE radio ${AVAHI_LIBRARIES} fftw3f_threads ${FFTW3F_LIBRARIES} ${BSD_LIBRARIES} m ${CMAKE_THREAD_LIBS_INIT})

include(GNUInstallDirs)
install(TARGETS stereod DESTINATION sbin)
