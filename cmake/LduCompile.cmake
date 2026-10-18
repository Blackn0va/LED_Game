# Gemeinsame Compiler- und Linker-Einstellungen für eigenen Code.
#
# Übernommen aus Signierungstool/kraken (Muster CaptionBridgeCPP): C++20, /utf-8 /W4
# /permissive- /EHsc, /WX für eigenen Code, /MD überall, UNICODE, NOMINMAX,
# WIN32_LEAN_AND_MEAN, _WIN32_WINNT=0x0A00.
#
# /utf-8 ist Pflicht: Ohne es übersetzt MSVC deutsche Literale still in CP1252, und die
# Konsole zeigt „Grün“ als „GrÃ¼n“. Warnungen aus fremden Headern (<windows.h>,
# doctest) blendet /external:anglebrackets aus; eigene Header immer mit "…".

include_guard(GLOBAL)

set(LDU_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." CACHE INTERNAL "Wurzel von LichtDuell")
cmake_path(NORMAL_PATH LDU_SOURCE_ROOT)

add_library(ldu_compile_defaults INTERFACE)
add_library(ldu::compile_defaults ALIAS ldu_compile_defaults)
target_compile_features(ldu_compile_defaults INTERFACE cxx_std_20)
target_compile_definitions(ldu_compile_defaults INTERFACE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_compile_options(ldu_compile_defaults INTERFACE
    /utf-8 /permissive- /EHsc /Zc:__cplusplus /Zc:preprocessor)

# Strenge Warnungen nur für eigenen Code, nie global (vendorte Header bleiben außen vor).
function(ldu_strict_warnings target)
    target_compile_options(${target} PRIVATE /W4 /WX /external:anglebrackets /external:W0 /MP)
endfunction()

# Härtung der erzeugten Binärdateien (Control Flow Guard, CET-Schattenstapel, ASLR, DEP).
function(ldu_harden_binary target)
    target_compile_options(${target} PRIVATE /guard:cf)
    target_link_options(${target} PRIVATE /guard:cf /CETCOMPAT /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT)
endfunction()

# Legt eine Modulbibliothek ldu_<name> (Alias ldu::<name>) an.
#   ldu_add_module(<name> SOURCES … [PUBLIC_DEPS …] [PRIVATE_DEPS …])
# Öffentliche Header liegen unter src/<name>/ und werden als "<name>/x.hpp" eingebunden.
function(ldu_add_module name)
    cmake_parse_arguments(ARG "" "" "SOURCES;PUBLIC_DEPS;PRIVATE_DEPS" ${ARGN})
    add_library(ldu_${name} STATIC ${ARG_SOURCES})
    add_library(ldu::${name} ALIAS ldu_${name})
    target_include_directories(ldu_${name} PUBLIC "${LDU_SOURCE_ROOT}/src")
    target_link_libraries(ldu_${name}
        PUBLIC ldu::compile_defaults ${ARG_PUBLIC_DEPS}
        PRIVATE ${ARG_PRIVATE_DEPS})
    ldu_strict_warnings(ldu_${name})
    target_compile_options(ldu_${name} PRIVATE /guard:cf)
    set_target_properties(ldu_${name} PROPERTIES FOLDER "module")
endfunction()

# Legt eine Konsolen-EXE an.
#   ldu_add_executable(<name> SOURCES … [DEPS …] [DESCRIPTION text])
function(ldu_add_executable name)
    cmake_parse_arguments(ARG "" "DESCRIPTION" "SOURCES;DEPS" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE ldu::compile_defaults ${ARG_DEPS})
    ldu_strict_warnings(${name})
    ldu_harden_binary(${name})
    # Abhängige DLLs nur aus System32 und dem Programmordner, nie aus dem Arbeitsordner
    # (CWE-427). Ergänzt SetDefaultDllDirectories in main.cpp, das erst nach dem
    # Laden der statischen Importe greift.
    target_link_options(${name} PRIVATE /DEPENDENTLOADFLAG:0xA00)
    set(_desc "${ARG_DESCRIPTION}")
    if(NOT _desc)
        set(_desc "LichtDuell")
    endif()
    ldu_add_version_resource(${name} "${_desc}")
    set_target_properties(${name} PROPERTIES FOLDER "app")
endfunction()

# Versionsressource (Datei-Eigenschaften im Explorer). Quelle: app/version.rc.in,
# Version aus project().
function(ldu_add_version_resource target description)
    get_target_property(_out ${target} OUTPUT_NAME)
    if(NOT _out)
        set(_out ${target})
    endif()
    set(LDU_RC_DESCRIPTION "${description}")
    set(LDU_RC_ORIGINAL "${_out}.exe")
    set(_rc "${CMAKE_BINARY_DIR}/generated/rc/${target}_version.rc")
    configure_file("${LDU_SOURCE_ROOT}/app/version.rc.in" "${_rc}" @ONLY)
    target_sources(${target} PRIVATE "${_rc}")
endfunction()
