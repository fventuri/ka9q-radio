add_executable(pcmcat pcmcat.c)
target_include_directories(pcmcat PRIVATE ../libradio)
target_link_libraries(pcmcat PRIVATE radio m ${BSD_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

add_executable(pcmspawn pcmspawn.c)
target_include_directories(pcmspawn PRIVATE ../libradio)
target_link_libraries(pcmspawn PRIVATE radio m ${BSD_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

add_executable(pcmrecord pcmrecord.c)
target_include_directories(pcmrecord PRIVATE ../libradio)
target_link_libraries(pcmrecord PRIVATE radio m ${BSD_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

add_executable(pcmsend pcmsend.c)
target_include_directories(pcmsend PRIVATE ../libradio)
target_link_libraries(pcmsend PRIVATE radio ${PORTAUDIO_LIBRARIES} ${BSD_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

include(GNUInstallDirs)
install(TARGETS pcmcat pcmspawn pcmrecord pcmsend)
