cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake"
        CACHE PATH "Vita toolchain file")
  else()
    message(FATAL_ERROR "VITASDK is not set.")
  endif()
endif()

project(vita_cap_test C)

include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Vita Cap Test")
set(VITA_TITLEID  "CAPT00001")
set(VITA_VERSION  "01.00")

set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -s VERSION=${VITA_VERSION}")
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -s CONTENT_ID=HB0001-${VITA_TITLEID}_00-0000000000000000")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -std=gnu11")

# Reserve 64 KiB before the RW segment. Some code sizes otherwise make
# vita-elf-create fail with "segment 1 overlaps".
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--defsym=__sce_headroom=0x10000")

find_package(SDL2 REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(ARCHIVE REQUIRED libarchive)
pkg_check_modules(WEBP REQUIRED libwebp)
pkg_check_modules(OPUS REQUIRED opus)
pkg_check_modules(VORBIS REQUIRED vorbis)
pkg_check_modules(FLAC REQUIRED flac)

include_directories(
  ${SDL2_MIXER_INCLUDE_DIRS}
  ${CURL_INCLUDE_DIRS}
  ${ARCHIVE_INCLUDE_DIRS}
  ${WEBP_INCLUDE_DIRS}
  ${OPUS_INCLUDE_DIRS}
  ${VORBIS_INCLUDE_DIRS}
  ${FLAC_INCLUDE_DIRS}
)

link_directories(
  ${SDL2_MIXER_LIBRARY_DIRS}
  ${CURL_LIBRARY_DIRS}
  ${ARCHIVE_LIBRARY_DIRS}
  ${WEBP_LIBRARY_DIRS}
  ${OPUS_LIBRARY_DIRS}
  ${VORBIS_LIBRARY_DIRS}
  ${FLAC_LIBRARY_DIRS}
)

add_executable(vita_cap_test
  src/main.c
)

target_link_libraries(vita_cap_test
  vita2d

  # I/O, app services, display, input
  SceDisplay_stub
  SceGxm_stub
  SceSysmodule_stub
  SceCtrl_stub
  ScePgf_stub
  ScePvf_stub
  SceCommonDialog_stub
  SceAppMgr_stub
  SceAppUtil_stub
  SceIofilemgr_stub
  SceLibKernel_stub

  # Audio
  SceAudio_stub

  # Networking
  SceNet_stub
  SceNetCtl_stub
  SceHttp_stub
  SceSsl_stub

  # Third-party libraries installed with vdpm
  SDL2::SDL2
  ${SDL2_MIXER_LIBRARIES}
  ${CURL_LIBRARIES}
  ${ARCHIVE_LIBRARIES}
  ${WEBP_LIBRARIES}
  ${OPUS_LIBRARIES}
  ${VORBIS_LIBRARIES}
  ${FLAC_LIBRARIES}
  mpg123

  freetype
  png
  jpeg
  z
  m
)

# This test writes to ux0:data and uses the network, so build it UNSAFE.
vita_create_self(vita_cap_test.self vita_cap_test UNSAFE)

vita_create_vpk(vita_cap_test.vpk ${VITA_TITLEID} vita_cap_test.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE sce_sys/icon0.png sce_sys/icon0.png
  FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)
