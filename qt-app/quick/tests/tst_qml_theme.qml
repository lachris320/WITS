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
}
