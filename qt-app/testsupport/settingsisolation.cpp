// Settings isolation for the whole test suite.
//
// cmake/WitsTest.cmake compiles this file into EVERY target registered with
// wits_add_qttest(), so isolation is a property of the harness rather than
// something each test has to remember. Two hooks, both automatic:
//
//   1. Q_CONSTRUCTOR_FUNCTION runs before main(), i.e. before any test code
//      (including a QTEST_MAIN initTestCase or a QuickTest setup object) can
//      construct an AppSettings. From that point the process is on a
//      throwaway INI under a QTemporaryDir instead of the real store.
//
//   2. Q_COREAPP_STARTUP_FUNCTION re-checks at QCoreApplication construction
//      and aborts if isolation is somehow not in effect. This is the
//      regression guard: if the seam is ever broken or bypassed, every test
//      binary fails loudly instead of quietly overwriting
//      HKCU\Software\MyCompany\MyApp (which is exactly how the original bug
//      went unnoticed through a green 34/34 suite).
#include <QtGlobal>
#include <QCoreApplication>   // Q_COREAPP_STARTUP_FUNCTION

#include "appsettings.h"

namespace {

void witsIsolateSettingsBeforeMain()
{
    AppSettings::isolateForTesting();
}

void witsVerifySettingsIsolated()
{
    if (!AppSettings::isIsolatedForTesting()) {
        qFatal("wits test harness: AppSettings is NOT isolated — this process "
               "would read and write the real user settings store. Aborting.");
    }
}

}   // namespace

Q_CONSTRUCTOR_FUNCTION(witsIsolateSettingsBeforeMain)
Q_COREAPP_STARTUP_FUNCTION(witsVerifySettingsIsolated)
