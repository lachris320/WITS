import QtQuick
import QtTest
import LOAMS

TestCase {
    name: "ThemeTokens"

    // Bezier probe: a real animation exercising Easing.BezierSpline with
    // Theme.motion.easing, not just an array-shape assertion. Duration 20ms
    // so the test settles quickly; running: false so it only moves when a
    // test calls start() explicitly.
    Item {
        id: bezierProbe
        property real v: 0
        NumberAnimation {
            id: bezierAnim
            objectName: "bezierAnim"
            target: bezierProbe
            property: "v"
            to: 1
            duration: 400
            easing.type: Easing.BezierSpline
            easing.bezierCurve: Theme.motion.easing
        }
    }

    function init() {
        bezierAnim.stop();
        bezierProbe.v = 0;
    }

    function test_spacingScaleExposed() {
        compare(Theme.spacing.lg, 16);
        compare(Theme.radius.card, 16);
    }

    function test_brandAdminResolvesToOpaqueColor() {
        verify(Theme.brand.admin.a === 1.0);
        verify(Theme.brand.admin !== Theme.card);
    }

    function test_kioskTokensExposed() {
        verify(Theme.motion.toastHold >= 1000);
        verify(Theme.brand.onKiosk.a === 1.0);
        compare(Theme.onBrandMuted.toString().length, 7);   // "#RRGGBB"
        verify(Theme.scrim.a > 0 && Theme.scrim.a < 1);
    }

    // --- Phase 4d Task 3: role-based brand/accent tokens, old names as aliases ---

    function test_roleTokensExistAndAliasesMatch() {
        verify(Theme.brand.base !== undefined);
        verify(Theme.accent.base !== undefined);
        // The deprecated alias must resolve to the same colour as the new token.
        compare(Theme.brand.admin.toString(), Theme.brand.base.toString());
        compare(Theme.secondary.toString(),   Theme.accent.base.toString());
    }

    // --- A1: easing extended to the 6-value BezierSpline form ---

    function test_easingIsSixValueBezierForm() {
        compare(Theme.motion.easing.length, 6);
        compare(Theme.motion.easing[0], 0.2);
        compare(Theme.motion.easing[1], 0.8);
        compare(Theme.motion.easing[2], 0.3);
        compare(Theme.motion.easing[3], 1.0);
    }

    // Proves Qt actually accepts the 6-value bezierCurve form end-to-end —
    // not just that the animation eventually reaches its target (a linear
    // fallback would too), but that the CURVE SHAPE is applied. With control
    // points (0.2,0.8)/(0.3,1.0), this curve reaches ~55-85% of its target
    // well before 50% of the duration elapses; a malformed bezierCurve list
    // (e.g. the old 4-value array, which forms zero complete 6-tuples) has
    // been observed to make Qt silently fall back to a ~linear progression
    // instead of warning, so a mid-animation sample well above the linear
    // expectation is the only way to actually catch that failure mode.
    function test_easingBezierSplineAnimatesToTarget() {
        compare(bezierProbe.v, 0);
        bezierAnim.start();
        wait(100);
        verify(bezierProbe.v > 0.5);
        tryCompare(bezierProbe, "v", 1, 1000);
    }

    // --- A2: stagger tokens promoted from magic literals to Theme policy ---

    function test_motionStaggerTokensExposed() {
        compare(Theme.motion.rowStagger, 25);
        compare(Theme.motion.barStagger, 45);
        compare(Theme.motion.staggerCap, 10);
    }

    // --- Part 3 (DRY): shared stagger-delay helper on the nested Theme.motion
    // QtObject. Verifies a function declared ON a nested-QtObject singleton
    // property actually resolves and is callable from QML in Qt 6.11 — not
    // just that the clamp-and-multiply arithmetic is correct.
    function test_staggerDelayClampsIndexToStaggerCapThenMultipliesByStep() {
        // index 15 > staggerCap(10) -> clamped to 10 * step 10 = 100.
        compare(Theme.motion.staggerDelay(15, 10), 100);
        // index 2 <= staggerCap -> 2 * step 25 = 50, unclamped.
        compare(Theme.motion.staggerDelay(2, 25), 50);
        // Lower guard: a delegate's `index` transiently goes to -1 while the
        // view tears its items down during a model reset, and the stagger
        // delay is read from exactly that binding. Without the Math.max(0,
        // ...) in Theme.qml that -1 would reach a PauseAnimation as a
        // NEGATIVE duration; clamp it to no delay instead. This is the only
        // place the negative branch is covered.
        compare(Theme.motion.staggerDelay(-1, 25), 0);
    }
}
