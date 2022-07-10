add_executable(show-pkt show-pkt.c)
target_include_directories(show-pkt PRIVATE ../libradio)
target_link_libraries(show-pkt PRIVATE radio ${NCURSESW_LIBRARIES} ${BSD_LIBRARIES} m)

include(GNUInstallDirs)
install(TARGETS show-pkt)
