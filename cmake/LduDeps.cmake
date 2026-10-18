# Abhängigkeiten gegen deps.lock.json (Muster Signierungstool/cmake/SwstDeps.cmake).
#
#   ldu::doctest   third_party/doctest/doctest.h 2.4.11 (vendort, SHA-256 geprüft)
#
# Sonst nichts: Netz (Winsock), Konsole und Zeitgeber kommen aus dem Windows-SDK.

include_guard(GLOBAL)

set(LDU_LOCK_FILE "${LDU_SOURCE_ROOT}/deps.lock.json")
if(NOT EXISTS "${LDU_LOCK_FILE}")
    message(FATAL_ERROR "deps.lock.json fehlt: ${LDU_LOCK_FILE}")
endif()
file(READ "${LDU_LOCK_FILE}" LDU_LOCK_JSON)
set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${LDU_LOCK_FILE}")

# Eine vendorte Datei gegen ihren SHA-256: ein abweichender Header wäre ein
# unbemerkter Versionswechsel.
function(_ldu_check_vendored lock_key rel_path)
    string(JSON _expected GET "${LDU_LOCK_JSON}" vendored ${lock_key} sha256 "${rel_path}")
    set(_file "${LDU_SOURCE_ROOT}/${rel_path}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "Vendorte Datei fehlt: ${_file} (deps.lock.json → vendored.${lock_key})")
    endif()
    file(SHA256 "${_file}" _actual)
    if(NOT _actual STREQUAL _expected)
        message(FATAL_ERROR "Vendorte Datei weicht von deps.lock.json ab: ${rel_path}\n  erwartet ${_expected}\n  gefunden ${_actual}")
    endif()
endfunction()

_ldu_check_vendored(doctest "third_party/doctest/doctest.h")
_ldu_check_vendored(doctest "licenses/doctest/LICENSE.txt")

add_library(ldu_doctest INTERFACE)
add_library(ldu::doctest ALIAS ldu_doctest)
target_include_directories(ldu_doctest SYSTEM INTERFACE "${LDU_SOURCE_ROOT}/third_party/doctest")
