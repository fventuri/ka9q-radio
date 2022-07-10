cmake_minimum_required(VERSION 3.20)
project(ka9q-radio)

# front-ends
option(AIRSPY "Build Airspy front-end" OFF)
option(FUNCUBE "Build FUNcube front-end" OFF)
option(HACKRF "Build HackRF front-end" OFF)
option(RTLSDR "Build RTL-SDR front-end" OFF)
option(SDRPLAY "Build SDRplay front-end" OFF)

add_subdirectory(src)
add_subdirectory(afiles)
