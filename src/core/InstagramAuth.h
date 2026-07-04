#pragma once

#include <QByteArray>
#include <QString>

namespace InstagramAuth {

QByteArray sessionCookieHeader();
QString savedSessionCookieHeader();
bool hasSavedSession();
bool rememberSessionCookieHeader(const QString &cookieHeader);
bool clearSavedSession();
QByteArray cookieValue(const QByteArray &cookie, const QByteArray &name);
bool hasRequiredSessionMarkers(const QByteArray &cookie);

} // namespace InstagramAuth
