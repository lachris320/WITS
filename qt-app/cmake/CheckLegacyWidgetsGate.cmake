# Legacy Widgets freeze self-check (S1 spec §3 "Legacy Widgets source").
#
#   cmake -DOPTION=<LOAMS_BUILD_LEGACY_WIDGETS value>
#         -DHAS_WITS=<0|1> -DHAS_RFIDKBD=<0|1> -DHAS_RESPONSIVE=<0|1>
#         -P CheckLegacyWidgetsGate.cmake
#
# The HAS_* values are $<TARGET_EXISTS:...> generator expressions evaluated by
# the real generated build system, so this asserts what was actually
# configured: the frozen WITS target and its two Widgets-only test targets
# exist exactly when the option is ON (default OFF).
if(OPTION)
    set(_expect 1)
else()
    set(_expect 0)
endif()
set(_bad "")
foreach(_name HAS_WITS HAS_RFIDKBD HAS_RESPONSIVE)
    if(NOT "${${_name}}" STREQUAL "${_expect}")
        list(APPEND _bad "${_name}=${${_name}}")
    endif()
endforeach()
if(_bad)
    message(FATAL_ERROR
        "LOAMS_BUILD_LEGACY_WIDGETS='${OPTION}' expects ${_expect} for every "
        "legacy target, got: ${_bad}")
endif()
message(STATUS "legacy widgets gate OK (LOAMS_BUILD_LEGACY_WIDGETS='${OPTION}')")
