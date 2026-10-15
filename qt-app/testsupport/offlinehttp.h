#ifndef OFFLINEHTTP_H
#define OFFLINEHTTP_H

#include "sequencednam.h"
#include "transport/httpclient.h"

// Test-only seam client that can never reach a real backend: every request is
// recorded by SequencedNam (requestCount(), lastUrl, urls) and then stays in
// flight — never answered — until aborted. Inject it into every seam owner a
// test constructs (ViewModel / AccessControlHub) unless the test injects its
// own CapturingNam / SequencedNam:
//
//     OfflineHttp vmHttp;
//     DatabaseViewModel vm(nullptr, &vmHttp.http);
//
// Declare it BEFORE the owner so it outlives it. Enforced by
// tst_transportseamguard::ownersInTestsUseFakeManagers.
struct OfflineHttp
{
    OfflineHttp() { nam.setStallWhenEmpty(true); }

    SequencedNam nam;
    HttpClient http{nullptr, &nam};   // declared after nam: constructed after it
};

#endif // OFFLINEHTTP_H
