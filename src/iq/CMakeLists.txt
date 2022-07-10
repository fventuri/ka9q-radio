add_executable(iqplay iqplay.c)
target_include_directories(iqplay PRIVATE ../libradio)
target_link_libraries(iqplay PRIVATE radio ${AVAHI_LIBRARIES} ${BSD_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} m)

add_executable(iqrecord iqrecord.c ../radio/decode_status.c)
target_include_directories(iqrecord PRIVATE ../libradio)
target_link_libraries(iqrecord PRIVATE radio ${BSD_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} m)

include(GNUInstallDirs)
install(TARGETS iqplay iqrecord)
