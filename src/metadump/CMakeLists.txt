add_executable(metadump metadump.c dump.c)
target_include_directories(metadump PRIVATE ../libradio)
target_link_libraries(metadump PRIVATE radio ${BSD_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} m)

include(GNUInstallDirs)
install(TARGETS metadump)
