# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Manual pages (section 1) for the GUI and dt-* tools.

set(DIRTOO_MAN_PAGES
  dirtoo.1
  dt-copy.1
  dt-move.1
  dt-rename.1
  dt-rm.1
  dt-rmdir.1
  dt-mkdir.1
  dt-mkfile.1
  dt-symlink.1
  dt-swap.1
  dt-filter.1
  dt-mediainfo.1
  dt-archiveinfo.1
)

find_program(GZIP_EXECUTABLE gzip)
if(GZIP_EXECUTABLE)
  set(DIRTOO_MAN_GZ)
  foreach(_man ${DIRTOO_MAN_PAGES})
    set(_src "${CMAKE_CURRENT_SOURCE_DIR}/${_man}")
    set(_dst "${CMAKE_CURRENT_BINARY_DIR}/${_man}.gz")
    add_custom_command(
      OUTPUT "${_dst}"
      COMMAND ${GZIP_EXECUTABLE} -9 -c "${_src}" > "${_dst}"
      DEPENDS "${_src}"
      COMMENT "Compressing man page ${_man}"
      VERBATIM
    )
    list(APPEND DIRTOO_MAN_GZ "${_dst}")
  endforeach()
  add_custom_target(dirtoo_man ALL DEPENDS ${DIRTOO_MAN_GZ})
  install(FILES ${DIRTOO_MAN_GZ}
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
    COMPONENT documentation
  )
else()
  install(FILES ${DIRTOO_MAN_PAGES}
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
    COMPONENT documentation
  )
endif()
