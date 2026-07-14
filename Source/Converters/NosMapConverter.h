#ifndef NOSMAPCONVERTER_H
#define NOSMAPCONVERTER_H

#include <QByteArray>
#include <QJsonObject>

class NosMapConverter {
public:
    QJsonObject toJson(const QByteArray &content);
    QByteArray fromJson(const QJsonObject &jo);
    static bool isMapConfig(const QJsonObject &jo);

private:
    QJsonObject decode(const QByteArray &content);
};

#endif
