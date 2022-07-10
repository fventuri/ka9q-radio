add_executable(tune tune.c)
target_include_directories(tune PRIVATE ../libradio)
target_link_libraries(tune PRIVATE radio ${BSD_LIBRARIES} m)

include(GNUInstallDirs)
install(TARGETS tune)
