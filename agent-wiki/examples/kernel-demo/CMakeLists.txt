cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake"
        CACHE PATH "Vita toolchain file")
  else()
    message(FATAL_ERROR "VITASDK is not set.")
  endif()
endif()

project(vita_kernel_demo C)

include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Kernel Demo")
set(VITA_TITLEID  "KRNL00001")
set(VITA_VERSION  "01.00")

set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -s VERSION=${VITA_VERSION}")
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -s CONTENT_ID=HB0001-${VITA_TITLEID}_00-0000000000000000")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -std=gnu11")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--defsym=__sce_headroom=0x10000")

# ---------------------------------------------------------------------------
# Kernel plugin (.skprx)
# ---------------------------------------------------------------------------
add_executable(kernel_demo
  kernel/main.c
)

target_include_directories(kernel_demo PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel
)

target_link_libraries(kernel_demo
  taihenForKernel_stub
  SceSysmemForDriver_stub
)

set_target_properties(kernel_demo PROPERTIES
  LINK_FLAGS "-nostdlib"
)

vita_create_self(kernel_demo.skprx kernel_demo
  CONFIG kernel/exports.yml
  UNSAFE
)

# ---------------------------------------------------------------------------
# Generate a userland stub library from the kernel plugin's exports.
# This produces libvita_kernel_demo_stub.a (named after the module).
# ---------------------------------------------------------------------------
vita_create_stubs(kernel_demo_stubs kernel_demo kernel/exports.yml KERNEL)

link_directories(${CMAKE_CURRENT_BINARY_DIR})

# ---------------------------------------------------------------------------
# Userland app (.self / .vpk)
# ---------------------------------------------------------------------------
add_executable(vita_kernel_demo
  app/main.c
)

add_dependencies(vita_kernel_demo kernel_demo_stubs)

target_link_libraries(vita_kernel_demo
  vita2d

  SceDisplay_stub
  SceGxm_stub
  SceSysmodule_stub
  SceCtrl_stub
  ScePgf_stub
  ScePvf_stub
  SceCommonDialog_stub
  SceAppMgr_stub
  SceLibKernel_stub
  SceIofilemgr_stub

  taihen_stub
  vita_kernel_demo_stub

  freetype
  png
  jpeg
  z
  m
)

# vita_create_self() stores its flags in cache variables. The kernel plugin
# used CONFIG kernel/exports.yml; clear that so the app gets a normal
# executable module config instead.
unset(VITA_ELF_CREATE_FLAGS CACHE)
unset(VITA_MAKE_FSELF_FLAGS CACHE)
set(VITA_ELF_CREATE_FLAGS "" CACHE STRING "vita-elf-create flags")
set(VITA_MAKE_FSELF_FLAGS "" CACHE STRING "vita-make-fself flags")

vita_create_self(vita_kernel_demo.self vita_kernel_demo UNSAFE)

vita_create_vpk(vita_kernel_demo.vpk ${VITA_TITLEID} vita_kernel_demo.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE sce_sys/icon0.png sce_sys/icon0.png
  FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
  FILE ${CMAKE_CURRENT_BINARY_DIR}/kernel_demo.skprx kernel_demo.skprx
)

# The .skprx is generated, so make sure it exists before packing the VPK.
if(TARGET vita_kernel_demo.vpk-vpk)
  add_dependencies(vita_kernel_demo.vpk-vpk kernel_demo.skprx-self)
else()
  add_dependencies(vita_kernel_demo.vpk kernel_demo.skprx-self)
endif()
