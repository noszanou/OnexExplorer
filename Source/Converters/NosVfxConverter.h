#ifndef NOSVFXCONVERTER_H
#define NOSVFXCONVERTER_H

#include <QByteArray>
#include <QJsonObject>

class NosVfxConverter {
public:
    QJsonObject toJson(const QByteArray &content);
    QByteArray fromJson(const QJsonObject &jo);
    static bool isVfx(const QJsonObject &jo);
};

#endif
