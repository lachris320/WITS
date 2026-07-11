import QtQuick
import QtTest
import LOAMS

TestCase {
    name: "ThemeTokens"

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
}
