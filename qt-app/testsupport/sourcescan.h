#ifndef SOURCESCAN_H
#define SOURCESCAN_H

#include <QChar>
#include <QString>

// Test-only helpers for source-grep guards (tst_notokenaliases,
// tst_transportseamguard). Header-only: include it and add
// ${CMAKE_SOURCE_DIR}/testsupport to the target's INCLUDES.
namespace SourceScan {

// Which literal syntax stripComments() understands on top of the shared
// "..." / '...' tracking. QmlJs is the original tst_notokenaliases behaviour;
// Cpp additionally keeps C++11 raw string literals (R"delim(...)delim",
// optionally u8/u/U/L-prefixed) verbatim, since their content may contain
// unbalanced quotes, "//" or "/*".
enum class Dialect { QmlJs, Cpp };

namespace detail {
inline bool isIdentChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_');
}

// If src[quoteAt] == '"' opens a C++ raw string literal, return the index one
// past its closing quote (src.size() if it is unterminated); otherwise -1.
inline qsizetype rawStringEnd(const QString &src, qsizetype quoteAt)
{
    if (quoteAt < 1 || src.at(quoteAt - 1) != QLatin1Char('R'))
        return -1;
    qsizetype k = quoteAt - 2;          // char before the R (or its encoding prefix)
    if (k >= 1 && src.at(k) == QLatin1Char('8') && src.at(k - 1) == QLatin1Char('u'))
        k -= 2;
    else if (k >= 0 && (src.at(k) == QLatin1Char('u') || src.at(k) == QLatin1Char('U')
                        || src.at(k) == QLatin1Char('L')))
        k -= 1;
    if (k >= 0 && isIdentChar(src.at(k)))
        return -1;                      // R is the tail of an identifier, e.g. FOOBAR"..."
    const qsizetype open = src.indexOf(QLatin1Char('('), quoteAt + 1);
    if (open < 0 || open - quoteAt - 1 > 16)
        return -1;
    const QString delim = src.mid(quoteAt + 1, open - quoteAt - 1);
    for (const QChar d : delim) {
        if (d.isSpace() || d == QLatin1Char(')') || d == QLatin1Char('\\') || d == QLatin1Char('"'))
            return -1;
    }
    const QString close = QLatin1Char(')') + delim + QLatin1Char('"');
    const qsizetype end = src.indexOf(close, open + 1);
    return end < 0 ? src.size() : end + close.size();
}
} // namespace detail

// Remove // line-comments (to EOL) and /* */ block-comments, preserving
// newlines so the remaining text still maps to the original line structure.
// String literals ARE tracked: while inside a double- or single-quoted string,
// `//` and `/*` are NOT treated as comment starts but preserved verbatim as
// code. This matters because the tree already has `//` inside strings (e.g.
// 'http://attacker/beacon', "file:///manifests/…" in tst_qml_admin.qml, URLs
// in C++); without string tracking, code trailing an in-string `//` on the
// same line would be silently stripped (and a "*/*" literal could open a fake
// block comment), so a guard would wrongly pass.
inline QString stripComments(const QString &src, Dialect dialect = Dialect::QmlJs)
{
    QString out;
    out.reserve(src.size());
    enum State { Code, LineComment, BlockComment, StringLit };
    State state = Code;
    QChar quote;      // which quote opened the current StringLit
    bool escaped = false;   // previous char was an unescaped backslash
    for (qsizetype i = 0; i < src.size(); ++i) {
        const QChar c = src.at(i);
        const QChar n = (i + 1 < src.size()) ? src.at(i + 1) : QChar();
        switch (state) {
        case Code:
            if (c == QLatin1Char('/') && n == QLatin1Char('/')) {
                state = LineComment;
                ++i;
            } else if (c == QLatin1Char('/') && n == QLatin1Char('*')) {
                state = BlockComment;
                ++i;
            } else if (const qsizetype rawEnd = (dialect == Dialect::Cpp && c == QLatin1Char('"'))
                                                    ? detail::rawStringEnd(src, i) : -1;
                       rawEnd >= 0) {
                out.append(QStringView(src).mid(i, rawEnd - i));
                i = rawEnd - 1;
            } else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
                state = StringLit;
                quote = c;
                escaped = false;
                out.append(c);
            } else {
                out.append(c);
            }
            break;
        case StringLit:
            // String content is code: preserve it verbatim so in-string
            // `//` and `/*` are never mistaken for comments.
            out.append(c);
            if (escaped) {
                escaped = false;
            } else if (c == QLatin1Char('\\')) {
                escaped = true;
            } else if (c == quote) {
                state = Code;
            } else if (c == QLatin1Char('\n')) {
                // Neither QML nor ordinary C++ strings span raw newlines;
                // treat an unterminated quote as ended here rather than
                // swallowing the rest of the file.
                state = Code;
            }
            break;
        case LineComment:
            if (c == QLatin1Char('\n')) {
                state = Code;
                out.append(c);
            }
            break;
        case BlockComment:
            if (c == QLatin1Char('*') && n == QLatin1Char('/')) {
                state = Code;
                ++i;
            }
            break;
        }
    }
    return out;
}

} // namespace SourceScan

#endif // SOURCESCAN_H
