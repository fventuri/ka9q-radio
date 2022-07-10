add_executable(monitor monitor.c)
target_include_directories(monitor PRIVATE ../libradio ../common)
target_link_libraries(monitor PRIVATE radio ${OPUS_LIBRARIES} ${PORTAUDIO_LIBRARIES} ${NCURSESW_LIBRARIES} ${BSD_LIBRARIES} m ${CMAKE_THREAD_LIBS_INIT})

include(GNUInstallDirs)
install(TARGETS monitor)
