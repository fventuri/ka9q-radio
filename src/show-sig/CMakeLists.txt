add_executable(show-sig show-sig.c)
target_include_directories(show-sig PRIVATE ../libradio)
target_link_libraries(show-sig PRIVATE radio ${NCURSESW_LIBRARIES} ${BSD_LIBRARIES} m)

include(GNUInstallDirs)
install(TARGETS show-sig)
