# Source files
set(fcitx_thai_sources
  thai-config.c
  thai.c
  )
  
include(FindPkgConfig)
pkg_check_modules(THAI "libthai" REQUIRED)

include_directories(${THAI_INCLUDE_DIRS})
link_directories(${THAI_LIBRARY_DIRS})

# See document in FcitxMacro.cmake and the example .fxaddon file
# doc/fcitx-example.fxaddon in the fcitx repo for more information.
fcitx_add_addon_full(thai
  DESC # Use configdesc file
  SCAN # Generate api header
  SCAN_PRIV # Generate addfunction header
  IM_CONFIG thai.conf
  # HEADER_DIR alternative_name # change dir name your header is installed
  # LIB_NAME lib_name # module file name (without extension)
  # HEADERS headers.h # extra public headers to install
  # EXTRA_PO extra_files # extra files to translate
  LINK_LIBS ${THAI_LIBRARIES} # external libraies to link
  SOURCES ${fcitx_thai_sources})
