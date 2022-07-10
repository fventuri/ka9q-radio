add_executable(setfilt setfilt.c)
target_include_directories(setfilt PRIVATE ../libradio)
target_link_libraries(setfilt PRIVATE radio ${BSD_LIBRARIES} m)

include(GNUInstallDirs)
install(TARGETS setfilt)
