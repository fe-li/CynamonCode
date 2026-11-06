# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Kamil Feliszewski

# Reguły instalacji (D044). Pakiet .deb: prefiks /usr (cmake/Packaging.cmake);
# „cmake --install” bez pakietu domyślnie instaluje do /usr/local.

set(CYNAMONCODE_DOCDIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/cynamoncode")

# Katalogi 0755 niezależnie od umask budującego (wymóg pakietów Debiana).
set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

install(TARGETS cynamoncode RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# Wpis w menu i metadane AppStream.
install(FILES "packaging/linux/${CYNAMONCODE_APP_ID}.desktop"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
install(FILES "packaging/linux/${CYNAMONCODE_APP_ID}.metainfo.xml"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/metainfo")

# Ikony w motywie hicolor pod nazwą identyfikatora aplikacji.
foreach(size IN ITEMS 16 24 32 48 64 128 256 512)
    install(FILES "resources/icons/cynamoncode-${size}.png"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${size}x${size}/apps"
        RENAME "${CYNAMONCODE_APP_ID}.png")
endforeach()

# Strona podręcznika z numerem wersji; Debian wymaga kompresji gzip -9n.
configure_file(packaging/linux/cynamoncode.1 "${CMAKE_CURRENT_BINARY_DIR}/cynamoncode.1" @ONLY)
find_program(CYNAMONCODE_GZIP gzip)
if(CYNAMONCODE_GZIP)
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cynamoncode.1.gz"
        COMMAND "${CYNAMONCODE_GZIP}" -9 -n -c "${CMAKE_CURRENT_BINARY_DIR}/cynamoncode.1"
                > "${CMAKE_CURRENT_BINARY_DIR}/cynamoncode.1.gz"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/cynamoncode.1"
        COMMENT "Kompresja strony podręcznika cynamoncode.1"
        VERBATIM)
    add_custom_target(cynamoncode_manpage ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/cynamoncode.1.gz")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cynamoncode.1.gz"
        DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
else()
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cynamoncode.1"
        DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
endif()

# Licencja: plik copyright w formacie Debiana (DEP-5) i pełny tekst GPLv3.
install(FILES packaging/deb/copyright LICENSE DESTINATION "${CYNAMONCODE_DOCDIR}")
