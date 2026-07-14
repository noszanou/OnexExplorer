#include "NosMapConverter.h"
#include <QJsonArray>
#include <cstring>

namespace {
    constexpr int FLOATS_OFFSET = 0x1F;
    constexpr int AMBIENT_COLOR_OFFSET = 0x5F;
    constexpr int OBJECT_COLOR_OFFSET = 0x64;
    constexpr int MAP_COLOR_OFFSET = 0x67;
    constexpr int CAMERA_ROTATION_OFFSET = 0x6D;
    constexpr int FOG_OFFSET = 0x77;
    constexpr int MODEL_COUNT_OFFSET = 0x85;
    constexpr int MODEL_IDS_OFFSET = 0x87;
    constexpr int OBJECT_POSITION_OFFSET = 0x21;

    float readFloat(const QByteArray &d, int offset) {
        float f;
        memcpy(&f, d.constData() + offset, 4);
        return f;
    }

    void appendFloat(QByteArray &d, float f) {
        char raw[4];
        memcpy(raw, &f, 4);
        d.append(raw, 4);
    }

    qint16 readInt16(const QByteArray &d, int offset) {
        qint16 v;
        memcpy(&v, d.constData() + offset, 2);
        return v;
    }

    void appendInt16(QByteArray &d, qint16 v) {
        char raw[2];
        memcpy(raw, &v, 2);
        d.append(raw, 2);
    }

    qint32 readInt32(const QByteArray &d, int offset) {
        qint32 v;
        memcpy(&v, d.constData() + offset, 4);
        return v;
    }

    void appendInt32(QByteArray &d, qint32 v) {
        char raw[4];
        memcpy(raw, &v, 4);
        d.append(raw, 4);
    }

    int objectSize(int type) {
        switch (type) {
            case 0x00: return 0x13;
            case 0x01: return 0x45;
            case 0x02: return 0x4B;
            case 0x03: return 0x58;
            default: return -1;
        }
    }

    QJsonArray readVec3(const QByteArray &d, int offset) {
        QJsonArray a;
        for (int i = 0; i < 3; i++)
            a.append(readFloat(d, offset + i * 4));
        return a;
    }

    void appendVec3(QByteArray &d, const QJsonArray &a) {
        for (int i = 0; i < 3; i++)
            appendFloat(d, (float) a.at(i).toDouble());
    }

    void writeVec3At(QByteArray &d, int offset, const QJsonArray &a) {
        QByteArray tmp;
        appendVec3(tmp, a);
        memcpy(d.data() + offset, tmp.constData(), 12);
    }

    QJsonObject readColor(const QByteArray &d, int offset) {
        QJsonObject c;
        c["R"] = (int) (uchar) d.at(offset + 2);
        c["G"] = (int) (uchar) d.at(offset + 1);
        c["B"] = (int) (uchar) d.at(offset);
        return c;
    }

    void appendColor(QByteArray &d, const QJsonObject &c) {
        d.append((char) c["B"].toInt());
        d.append((char) c["G"].toInt());
        d.append((char) c["R"].toInt());
    }

    QString hexOf(const QByteArray &d, int offset, int length) {
        return QString(d.mid(offset, length).toHex());
    }

    QByteArray fromHexField(const QJsonObject &jo, const char *key) {
        return QByteArray::fromHex(jo[key].toString().toLocal8Bit());
    }
}

QJsonObject NosMapConverter::toJson(const QByteArray &content) {
    QJsonObject jo = decode(content);
    if (!jo.isEmpty() && fromJson(jo) == content)
        return jo;
    QJsonObject raw;
    raw["MapConfig"] = false;
    raw["RawData"] = QString(content.toHex());
    return raw;
}

QJsonObject NosMapConverter::decode(const QByteArray &content) {
    if (content.size() < MODEL_IDS_OFFSET)
        return QJsonObject();

    QJsonObject jo;
    jo["MapConfig"] = true;
    jo["Header"] = hexOf(content, 0, FLOATS_OFFSET);

    QJsonObject box1, box2;
    box1["Min"] = readVec3(content, FLOATS_OFFSET);
    box1["Max"] = readVec3(content, FLOATS_OFFSET + 12);
    box2["Min"] = readVec3(content, FLOATS_OFFSET + 24);
    box2["Max"] = readVec3(content, FLOATS_OFFSET + 36);
    jo["BoundingBox1"] = box1;
    jo["BoundingBox2"] = box2;
    jo["Center"] = readVec3(content, FLOATS_OFFSET + 48);
    jo["Radius"] = readFloat(content, FLOATS_OFFSET + 60);

    jo["AmbientColor"] = readColor(content, AMBIENT_COLOR_OFFSET);
    jo["Unknown62"] = hexOf(content, 0x62, 2);
    jo["ObjectColor"] = readColor(content, OBJECT_COLOR_OFFSET);
    jo["MapColor"] = readColor(content, MAP_COLOR_OFFSET);
    jo["Unknown6A"] = hexOf(content, 0x6A, 3);
    jo["CameraRotation"] = (int) (uchar) content.at(CAMERA_ROTATION_OFFSET);
    jo["Unknown6E"] = hexOf(content, 0x6E, FOG_OFFSET - 0x6E);
    jo["Fog"] = readInt16(content, FOG_OFFSET);
    jo["Unknown79"] = hexOf(content, 0x79, MODEL_COUNT_OFFSET - 0x79);

    int modelCount = readInt16(content, MODEL_COUNT_OFFSET);
    if (modelCount < 0 || MODEL_IDS_OFFSET + modelCount * 4 > content.size())
        return QJsonObject();
    QJsonArray models;
    for (int i = 0; i < modelCount; i++)
        models.append(readInt32(content, MODEL_IDS_OFFSET + i * 4));
    jo["Models"] = models;

    QJsonArray objects;
    int pos = MODEL_IDS_OFFSET + modelCount * 4;
    while (pos < content.size()) {
        int type = (uchar) content.at(pos);
        int size = objectSize(type);
        if (size < 0 || pos + size > content.size())
            break;
        QJsonObject object;
        object["Type"] = type;
        object["ModelIndex"] = readInt16(content, pos + 1);
        if (size >= OBJECT_POSITION_OFFSET + 12)
            object["Position"] = readVec3(content, pos + OBJECT_POSITION_OFFSET);
        object["Data"] = hexOf(content, pos, size);
        objects.append(object);
        pos += size;
    }
    jo["Objects"] = objects;
    jo["Trailing"] = hexOf(content, pos, content.size() - pos);
    return jo;
}

QByteArray NosMapConverter::fromJson(const QJsonObject &jo) {
    if (!jo["MapConfig"].toBool())
        return fromHexField(jo, "RawData");

    QByteArray out = fromHexField(jo, "Header");
    appendVec3(out, jo["BoundingBox1"].toObject()["Min"].toArray());
    appendVec3(out, jo["BoundingBox1"].toObject()["Max"].toArray());
    appendVec3(out, jo["BoundingBox2"].toObject()["Min"].toArray());
    appendVec3(out, jo["BoundingBox2"].toObject()["Max"].toArray());
    appendVec3(out, jo["Center"].toArray());
    appendFloat(out, (float) jo["Radius"].toDouble());

    appendColor(out, jo["AmbientColor"].toObject());
    out.append(fromHexField(jo, "Unknown62"));
    appendColor(out, jo["ObjectColor"].toObject());
    appendColor(out, jo["MapColor"].toObject());
    out.append(fromHexField(jo, "Unknown6A"));
    out.append((char) jo["CameraRotation"].toInt());
    out.append(fromHexField(jo, "Unknown6E"));
    appendInt16(out, (qint16) jo["Fog"].toInt());
    out.append(fromHexField(jo, "Unknown79"));

    QJsonArray models = jo["Models"].toArray();
    appendInt16(out, (qint16) models.size());
    for (auto model : models)
        appendInt32(out, model.toInt());

    for (auto value : jo["Objects"].toArray()) {
        QJsonObject object = value.toObject();
        QByteArray data = QByteArray::fromHex(object["Data"].toString().toLocal8Bit());
        if (!data.isEmpty()) {
            data[0] = (char) object["Type"].toInt();
            qint16 index = (qint16) object["ModelIndex"].toInt();
            memcpy(data.data() + 1, &index, 2);
            if (object.contains("Position") && data.size() >= OBJECT_POSITION_OFFSET + 12)
                writeVec3At(data, OBJECT_POSITION_OFFSET, object["Position"].toArray());
        }
        out.append(data);
    }
    out.append(fromHexField(jo, "Trailing"));
    return out;
}

bool NosMapConverter::isMapConfig(const QJsonObject &jo) {
    return jo["MapConfig"].toBool();
}
