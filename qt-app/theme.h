#ifndef THEME_H
#define THEME_H

#include <QPalette>
#include <QColor>

// Centralized, fixed light theme for the whole application.
//
// On Windows dark mode the OS hands Qt a dark default palette: light
// foreground text on a dark background. The app's stylesheets force a light
// background on inputs (QLineEdit, etc.) but historically set no explicit
// text color, so typed text inherited the OS-dark foreground and rendered
// light-on-light — invisible. Applying this palette app-wide pins every
// foreground/background role to a consistent light scheme, so input text is
// always dark and legible regardless of the OS dark/light setting.
//
// Stylesheets still own backgrounds/borders/padding because a stylesheet rule
// overrides the palette; this palette is the default that fills in everything
// the stylesheets don't paint (e.g. unstyled widgets and the guest window).
// Header-only and dependency-free (only QtGui's QPalette/QColor) so it links
// into both the app and the unit-test target with no extra .cpp.
namespace WitsTheme {

// The single source of truth for the app's fixed light palette. Every relevant
// role is set explicitly so the result is fully theme-independent.
inline QPalette lightPalette()
{
    const QColor darkText("#2C3E50");      // primary foreground (dark slate)
    const QColor disabledText("#95A5A6");  // muted grey for disabled/placeholder

    QPalette palette;

    palette.setColor(QPalette::Window, QColor("#F0F0F0"));
    palette.setColor(QPalette::WindowText, darkText);

    palette.setColor(QPalette::Base, QColor("#FFFFFF"));
    palette.setColor(QPalette::AlternateBase, QColor("#F8F9FA"));
    palette.setColor(QPalette::Text, darkText);

    palette.setColor(QPalette::Button, QColor("#F0F0F0"));
    palette.setColor(QPalette::ButtonText, darkText);

    palette.setColor(QPalette::ToolTipBase, QColor("#FFFFFF"));
    palette.setColor(QPalette::ToolTipText, darkText);

    palette.setColor(QPalette::Highlight, QColor("#4A90E2"));
    palette.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));

    palette.setColor(QPalette::PlaceholderText, disabledText);

    // Keep disabled text legible (a lighter grey, not invisible) under the
    // forced light theme.
    palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);

    return palette;
}

} // namespace WitsTheme

#endif // THEME_H
