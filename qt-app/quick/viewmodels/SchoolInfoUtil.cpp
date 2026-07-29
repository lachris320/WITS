#include "SchoolInfoUtil.h"

#include <QFileInfo>

namespace SchoolInfoUtil {

QUrl resolveLogoUrl(const QString &path, bool *hasLogo)
{
    const bool exists = !path.isEmpty() && QFileInfo::exists(path);
    *hasLogo = exists;
    return exists ? QUrl::fromLocalFile(path) : QUrl();
}

} // namespace SchoolInfoUtil
