# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Kamil Feliszewski

# Windows (D050, D053): instalator NSIS i przenośny .zip.
# Instalator dodaje skrót w menu Start i wpisy „Otwórz za pomocą” dla typów
# tekstowych — bez zmiany domyślnych programów; odinstalowanie usuwa wszystko.

set(CPACK_GENERATOR "NSIS;ZIP")
set(CPACK_PACKAGE_FILE_NAME "CynamonCode-${PROJECT_VERSION}-windows-x64")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "CynamonCode")
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "CynamonCode")
set(CPACK_PACKAGE_EXECUTABLES "cynamoncode;CynamonCode")
set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")
set(CPACK_NSIS_PACKAGE_NAME "CynamonCode ${PROJECT_VERSION}")
set(CPACK_NSIS_DISPLAY_NAME "CynamonCode")
set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}/resources/icons/cynamoncode.ico")
set(CPACK_NSIS_MUI_UNIICON "${PROJECT_SOURCE_DIR}/resources/icons/cynamoncode.ico")
set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\cynamoncode.exe")
set(CPACK_NSIS_URL_INFO_ABOUT "${PROJECT_HOMEPAGE_URL}")
set(CPACK_NSIS_HELP_LINK "https://github.com/fe-li/CynamonCode")
set(CPACK_NSIS_CONTACT "${CPACK_PACKAGE_CONTACT}")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_MODIFY_PATH OFF)
set(CPACK_NSIS_MANIFEST_DPI_AWARE ON)
set(CPACK_NSIS_BRANDING_TEXT "CynamonCode ${PROJECT_VERSION}")
set(CPACK_NSIS_UNINSTALL_NAME "Odinstaluj CynamonCode")

# „Otwórz za pomocą”: aplikacja w Software\Classes\Applications i na listach
# OpenWithList rozszerzeń. SHCTX = gałąź zgodna z trybem instalacji (wszyscy
# użytkownicy albo bieżący). Ukośniki podwójnie: CPack zapisuje wartość jeszcze raz.
set(_cynamoncode_extensions
    txt log md ini cfg conf csv
    c h cpp hpp cc cxx hh hxx
    py pyw java js mjs json html htm css php sql xml sh bash)
set(_app "Software\\\\Classes\\\\Applications\\\\cynamoncode.exe")
set(_exe "$INSTDIR\\\\bin\\\\cynamoncode.exe")
set(_install "")
set(_uninstall "")
string(APPEND _install "WriteRegStr SHCTX '${_app}' 'FriendlyAppName' 'CynamonCode'\n")
string(APPEND _install "WriteRegStr SHCTX '${_app}\\\\DefaultIcon' '' '${_exe},0'\n")
string(APPEND _install
    "WriteRegStr SHCTX '${_app}\\\\shell\\\\open\\\\command' '' '\\\"${_exe}\\\" \\\"%1\\\"'\n")
foreach(_ext IN LISTS _cynamoncode_extensions)
    string(APPEND _install "WriteRegStr SHCTX '${_app}\\\\SupportedTypes' '.${_ext}' ''\n")
    set(_list "Software\\\\Classes\\\\.${_ext}\\\\OpenWithList\\\\cynamoncode.exe")
    string(APPEND _install "WriteRegStr SHCTX '${_list}' '' ''\n")
    string(APPEND _uninstall "DeleteRegKey SHCTX '${_list}'\n")
endforeach()
string(APPEND _uninstall "DeleteRegKey SHCTX '${_app}'\n")
# Odświeżenie skojarzeń w Eksploratorze (SHCNE_ASSOCCHANGED).
set(_notify "System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'\n")
string(APPEND _install "${_notify}")
string(APPEND _uninstall "${_notify}")
set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "${_install}")
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "${_uninstall}")
