# CheckQuickTestMainFailure.cmake — ctest driver for tst_quicktestmain_missingfile
# (registered in quick/CMakeLists.txt). Run as:
#
#   cmake -DEXE=<test exe> -DNAME=<target name> -DWORKDIR=<dir> -P CheckQuickTestMainFailure.cmake
#
# EXE is a QuickTest main generated from tests/QuickTestMain.cpp.in for a .qml
# path that does not exist. The generated main must refuse it loudly: exit 1
# with "<NAME>: QuickTest file not found: <path>" on stderr, BEFORE
# quick_test_main runs. If it instead hands the bad path (or none) to Qt, Qt
# silently scans the working directory for tst_*.qml. WORKDIR is the QuickTest
# source dir on purpose, so such a regression runs the real QML suite, prints
# Qt's "Start testing of" banner and (normally) exits 0 — and this check fails.

cmake_minimum_required(VERSION 3.16)

foreach(_v EXE NAME WORKDIR)
    if(NOT DEFINED ${_v} OR "${${_v}}" STREQUAL "")
        message(FATAL_ERROR "CheckQuickTestMainFailure.cmake: -D${_v}=... is required")
    endif()
endforeach()
if(NOT EXISTS "${EXE}")
    message(FATAL_ERROR "${NAME}: test executable not found: ${EXE} (build the target first)")
endif()

# Own TIMEOUT (< ctest's 120) so we kill the child; ctest killing only us orphans it on Windows.
execute_process(COMMAND "${EXE}"
    WORKING_DIRECTORY "${WORKDIR}"
    TIMEOUT 100
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)

set(_want_rc 1)
set(_want_err "${NAME}: QuickTest file not found:")
set(_runner_banner "Start testing of")

set(_problems "")
if(NOT "${rc}" STREQUAL "${_want_rc}")
    list(APPEND _problems "exit code is '${rc}', expected exactly ${_want_rc}")
endif()
string(FIND "${err}" "${_want_err}" _pos)
if(_pos EQUAL -1)
    list(APPEND _problems "stderr lacks \"${_want_err}\"")
endif()
string(FIND "${out}${err}" "${_runner_banner}" _pos)
if(NOT _pos EQUAL -1)
    list(APPEND _problems "Qt's test runner started (\"${_runner_banner}\" in output)")
endif()

if(_problems)
    string(REPLACE ";" "\n  - " _problems "${_problems}")
    message(FATAL_ERROR "${NAME}: the generated QuickTest main did not fail loudly "
        "on a missing .qml:\n  - ${_problems}\n"
        "--- exit code ---\n${rc}\n--- stdout ---\n${out}\n--- stderr ---\n${err}")
endif()
message(STATUS "${NAME}: missing .qml rejected as expected (exit ${rc}): ${err}")
