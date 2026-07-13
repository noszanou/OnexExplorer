#include "NosVfxConverter.h"
#include <QJsonArray>
#include <QtEndian>
#include <cmath>
#include <cstring>

namespace {

    const int MAX_OBJECT_COUNT = 4096; // above that the item is a model, not a VFX config

    class VfxReader {
    public:
        explicit VfxReader(const QByteArray &data) : d(data) {}

        bool ok = true;
        int pos = 0;

        bool consumedAll() const { return pos == d.size(); }

        quint8 u8() {
            if (!ok || pos + 1 > d.size()) {
                ok = false;
                return 0;
            }
            return (quint8) d.at(pos++);
        }
        qint8 i8() { return (qint8) u8(); }
        quint16 u16() { return take<quint16>(); }
        qint16 i16() { return (qint16) take<quint16>(); }
        quint32 u32() { return take<quint32>(); }
        qint32 i32() { return (qint32) take<quint32>(); }

        float f32() {
            quint32 bits = take<quint32>();
            float value;
            memcpy(&value, &bits, 4);
            return value;
        }

        QByteArray bytes(int amount) {
            if (!ok || amount < 0 || pos + amount > d.size()) {
                ok = false;
                return QByteArray();
            }
            QByteArray result = d.mid(pos, amount);
            pos += amount;
            return result;
        }

        float halfMid() {
            int raw = i16();
            if (raw < 0)
                raw = -(raw + 32768);
            return raw / 32767.0f;
        }

        float singleMid() {
            int raw = i8();
            if (raw < 0)
                raw = -(raw + 128);
            return raw / 127.0f;
        }

        bool fits(quint32 count, int bytesPerValue) const {
            return (qint64) count * (2 + bytesPerValue) <= (qint64)(d.size() - pos);
        }

    private:
        template<typename T>
        T take() {
            if (!ok || pos + (int) sizeof(T) > d.size()) {
                ok = false;
                return T(0);
            }
            T value = qFromLittleEndian<T>(reinterpret_cast<const uchar *>(d.constData()) + pos);
            pos += sizeof(T);
            return value;
        }

        const QByteArray &d;
    };

    class VfxWriter {
    public:
        QByteArray data;

        void u8(quint8 value) { data.append((char) value); }
        void i8(qint8 value) { data.append((char) value); }
        void u16(quint16 value) { put<quint16>(value); }
        void i16(qint16 value) { put<quint16>((quint16) value); }
        void u32(quint32 value) { put<quint32>(value); }
        void i32(qint32 value) { put<quint32>((quint32) value); }

        void f32(float value) {
            quint32 bits;
            memcpy(&bits, &value, 4);
            put<quint32>(bits);
        }

        void bytes(const QByteArray &value) { data.append(value); }

        void halfMid(float value) {
            value = clamp(value);
            int raw = value >= 0 ? qRound(value * 32767.0) : qRound(-value * 32767.0 - 32768.0);
            i16((qint16) qBound(-32768, raw, 32767));
        }

        void singleMid(float value) {
            value = clamp(value);
            int raw = value >= 0 ? qRound(value * 127.0) : qRound(-value * 127.0 - 128.0);
            i8((qint8) qBound(-128, raw, 127));
        }

    private:
        static float clamp(float value) { return value < -1.0f ? -1.0f : (value > 1.0f ? 1.0f : value); }

        template<typename T>
        void put(T value) {
            char buffer[sizeof(T)];
            qToLittleEndian<T>(value, reinterpret_cast<uchar *>(buffer));
            data.append(buffer, sizeof(T));
        }
    };

    quint32 toU32(const QJsonValue &value) {
        return (quint32)(qint64) value.toDouble();
    }

    QJsonValue floatToJson(float value) {
        if (value == 0.0f && std::signbit(value))
            return QJsonValue(QString("-0"));
        return QJsonValue((double) value);
    }

    float floatFromJson(const QJsonValue &value) {
        if (value.isString()) {
            QString text = value.toString();
            return text == "-0" ? -0.0f : text.toFloat();
        }
        return (float) value.toDouble();
    }

    const char *TYPE_NAMES[] = {"Normal", "Targeted", "Randomness"};
    const char *INTERPOLATION_NAMES[] = {"RunOnce", "RunOnceAndToFirst", "Repeat", "OnlyLast"};

    QJsonValue enumToJson(quint8 value, const char *const *names, int amount) {
        return value < amount ? QJsonValue(QString(names[value])) : QJsonValue((int) value);
    }

    quint8 enumFromJson(const QJsonValue &value, const char *const *names, int amount) {
        if (value.isString()) {
            for (int i = 0; i != amount; ++i) {
                if (value.toString() == names[i])
                    return (quint8) i;
            }
        }
        return (quint8) value.toInt();
    }

    QJsonObject coordToJson(float x, float y, float z) {
        QJsonObject jo;
        jo["X"] = floatToJson(x);
        jo["Y"] = floatToJson(y);
        jo["Z"] = floatToJson(z);
        return jo;
    }

    QJsonObject readCoord(VfxReader &r) {
        float x = r.f32(), y = r.f32(), z = r.f32();
        return coordToJson(x, y, z);
    }

    void writeCoord(VfxWriter &w, const QJsonObject &jo) {
        w.f32(floatFromJson(jo["X"]));
        w.f32(floatFromJson(jo["Y"]));
        w.f32(floatFromJson(jo["Z"]));
    }

    QJsonObject readCoordHalfMid(VfxReader &r) {
        float x = r.halfMid(), y = r.halfMid(), z = r.halfMid();
        return coordToJson(x, y, z);
    }

    void writeCoordHalfMid(VfxWriter &w, const QJsonObject &jo) {
        w.halfMid(floatFromJson(jo["X"]));
        w.halfMid(floatFromJson(jo["Y"]));
        w.halfMid(floatFromJson(jo["Z"]));
    }

    QJsonObject readCoordUByte(VfxReader &r) {
        float x = r.u8(), y = r.u8(), z = r.u8();
        return coordToJson(x, y, z);
    }

    void writeCoordUByte(VfxWriter &w, const QJsonObject &jo) {
        w.u8((quint8) jo["X"].toInt());
        w.u8((quint8) jo["Y"].toInt());
        w.u8((quint8) jo["Z"].toInt());
    }

    QJsonObject readQuat(VfxReader &r) {
        QJsonObject jo;
        jo["X"] = floatToJson(r.f32());
        jo["Y"] = floatToJson(r.f32());
        jo["Z"] = floatToJson(r.f32());
        jo["W"] = floatToJson(r.f32());
        return jo;
    }

    void writeQuat(VfxWriter &w, const QJsonObject &jo) {
        w.f32(floatFromJson(jo["X"]));
        w.f32(floatFromJson(jo["Y"]));
        w.f32(floatFromJson(jo["Z"]));
        w.f32(floatFromJson(jo["W"]));
    }

    QJsonObject readQuatHalfMid(VfxReader &r) {
        QJsonObject jo;
        jo["X"] = floatToJson(r.halfMid());
        jo["Y"] = floatToJson(r.halfMid());
        jo["Z"] = floatToJson(r.halfMid());
        jo["W"] = floatToJson(r.halfMid());
        return jo;
    }

    void writeQuatHalfMid(VfxWriter &w, const QJsonObject &jo) {
        w.halfMid(floatFromJson(jo["X"]));
        w.halfMid(floatFromJson(jo["Y"]));
        w.halfMid(floatFromJson(jo["Z"]));
        w.halfMid(floatFromJson(jo["W"]));
    }

    QJsonObject readColor(VfxReader &r) {
        QJsonObject jo;
        jo["Blue"] = r.u8();
        jo["Green"] = r.u8();
        jo["Red"] = r.u8();
        jo["Alpha"] = r.u8();
        return jo;
    }

    void writeColor(VfxWriter &w, const QJsonObject &jo) {
        w.u8((quint8) jo["Blue"].toInt());
        w.u8((quint8) jo["Green"].toInt());
        w.u8((quint8) jo["Red"].toInt());
        w.u8((quint8) jo["Alpha"].toInt());
    }

    QJsonObject readPatchCount(VfxReader &r) {
        QJsonObject jo;
        jo["Count"] = (qint64) r.u32();
        jo["TimestampsAddress"] = (qint64) r.u32();
        jo["ValuesAddress"] = (qint64) r.u32();
        return jo;
    }

    void writePatchCount(VfxWriter &w, const QJsonObject &jo, int count) {
        w.u32((quint32) count);
        w.u32(toU32(jo["TimestampsAddress"]));
        w.u32(toU32(jo["ValuesAddress"]));
    }

    QJsonObject readObjectHeader(VfxReader &r) {
        QJsonObject jo;
        jo["Ft"] = (qint64) r.u32();
        quint8 type = r.u8();
        jo["Type"] = enumToJson(type, TYPE_NAMES, 3);
        jo["CameraLock"] = r.u8();
        jo["OnlyOnHit"] = r.u8();
        jo["SplitDamage"] = r.u8();
        jo["SourceBlending"] = (qint64) r.u32();
        jo["DestinationBlending"] = (qint64) r.u32();
        jo["Model1Id"] = r.i32();
        jo["Model2Id"] = r.i32();
        jo["OnTarget"] = r.u8();

        if (type == 2) {
            QJsonObject config;
            config["EmittersOffset"] = readCoordHalfMid(r);
            config["EmittersRotation"] = readQuatHalfMid(r);
            config["EmittersDistance"] = readCoordHalfMid(r);
            config["EmittersVector"] = readCoordHalfMid(r);
            config["EmittersCount"] = readCoordUByte(r);
            config["ParticlesRotation"] = readCoordUByte(r);
            config["ParticlesScale"] = readCoordUByte(r);
            config["Gravity"] = r.u8();
            config["FirstTickIntensity"] = r.u8();
            config["Intensity"] = r.u8();
            config["Rate"] = r.u8();
            config["TickDelayMin"] = r.u16();
            config["TickDelayMax"] = r.u16();
            config["ParticleLifespanMin"] = r.u16();
            config["ParticleLifespanMax"] = r.u16();
            jo["Randomness"] = config;
        } else if (type == 0) {
            QJsonObject config;
            config["OriginHeight"] = floatToJson(r.f32());
            config["OriginOffset"] = readCoord(r);
            config["TargetHeight"] = floatToJson(r.f32());
            config["TargetOffset"] = readCoord(r);
            config["DistanceOffset"] = floatToJson(r.f32());
            config["TravelSpeed"] = floatToJson(r.f32());
            config["HeightParabola"] = floatToJson(r.f32());
            config["Unk1"] = r.u16();
            config["PlaneParabola"] = floatToJson(r.singleMid());
            jo["Normal"] = config;
        } else {
            QJsonObject config;
            config["OriginHeight"] = floatToJson(r.f32());
            config["FollowEntityHeight"] = r.u8();
            config["Padding"] = QString(r.bytes(42).toBase64());
            jo["FromAtoB"] = config;
        }

        QJsonObject ticks;
        ticks["Unk"] = r.u16();
        ticks["Length"] = r.u16();
        ticks["Ticks"] = r.u16();
        ticks["Speed"] = r.u16();
        jo["AnimationTicks"] = ticks;

        jo["AnimOffset"] = r.i32();
        jo["AnimDuration"] = (qint64) r.u32();
        jo["Flag0"] = r.u8();
        jo["Interpolation"] = enumToJson(r.u8(), INTERPOLATION_NAMES, 4);
        jo["FollowPreviousObject"] = r.u8();
        jo["HasDepthTest"] = r.u8();
        jo["HasTransform"] = r.u8();
        jo["Flag2"] = r.u8();
        jo["Flag3"] = r.u8();
        jo["HasTextureTransform"] = r.u8();

        jo["PatchRotation"] = readPatchCount(r);
        jo["PatchScale"] = readPatchCount(r);
        jo["PatchTranslation"] = readPatchCount(r);
        jo["PatchColor"] = readPatchCount(r);
        jo["PatchTexture"] = readPatchCount(r);
        jo["PatchTextureRotation"] = readPatchCount(r);
        jo["PatchTextureScale"] = readPatchCount(r);
        jo["PatchTextureTranslate"] = readPatchCount(r);
        return jo;
    }

    void writeObjectHeader(VfxWriter &w, const QJsonObject &jo, const QJsonObject &patch) {
        w.u32(toU32(jo["Ft"]));
        quint8 type = enumFromJson(jo["Type"], TYPE_NAMES, 3);
        w.u8(type);
        w.u8((quint8) jo["CameraLock"].toInt());
        w.u8((quint8) jo["OnlyOnHit"].toInt());
        w.u8((quint8) jo["SplitDamage"].toInt());
        w.u32(toU32(jo["SourceBlending"]));
        w.u32(toU32(jo["DestinationBlending"]));
        w.i32(jo["Model1Id"].toInt());
        w.i32(jo["Model2Id"].toInt());
        w.u8((quint8) jo["OnTarget"].toInt());

        if (type == 2) {
            QJsonObject config = jo["Randomness"].toObject();
            writeCoordHalfMid(w, config["EmittersOffset"].toObject());
            writeQuatHalfMid(w, config["EmittersRotation"].toObject());
            writeCoordHalfMid(w, config["EmittersDistance"].toObject());
            writeCoordHalfMid(w, config["EmittersVector"].toObject());
            writeCoordUByte(w, config["EmittersCount"].toObject());
            writeCoordUByte(w, config["ParticlesRotation"].toObject());
            writeCoordUByte(w, config["ParticlesScale"].toObject());
            w.u8((quint8) config["Gravity"].toInt());
            w.u8((quint8) config["FirstTickIntensity"].toInt());
            w.u8((quint8) config["Intensity"].toInt());
            w.u8((quint8) config["Rate"].toInt());
            w.u16((quint16) config["TickDelayMin"].toInt());
            w.u16((quint16) config["TickDelayMax"].toInt());
            w.u16((quint16) config["ParticleLifespanMin"].toInt());
            w.u16((quint16) config["ParticleLifespanMax"].toInt());
        } else if (type == 0) {
            QJsonObject config = jo["Normal"].toObject();
            w.f32(floatFromJson(config["OriginHeight"]));
            writeCoord(w, config["OriginOffset"].toObject());
            w.f32(floatFromJson(config["TargetHeight"]));
            writeCoord(w, config["TargetOffset"].toObject());
            w.f32(floatFromJson(config["DistanceOffset"]));
            w.f32(floatFromJson(config["TravelSpeed"]));
            w.f32(floatFromJson(config["HeightParabola"]));
            w.u16((quint16) config["Unk1"].toInt());
            w.singleMid(floatFromJson(config["PlaneParabola"]));
        } else {
            QJsonObject config = jo["FromAtoB"].toObject();
            w.f32(floatFromJson(config["OriginHeight"]));
            w.u8((quint8) config["FollowEntityHeight"].toInt());
            QString encoded = config["Padding"].toString();
            QByteArray padding = QByteArray::fromBase64(encoded.toLatin1());
            if (padding.size() < 42)
                padding.append(QByteArray(42 - padding.size(), '\0'));
            padding.truncate(42);
            w.bytes(padding);
        }

        QJsonObject ticks = jo["AnimationTicks"].toObject();
        w.u16((quint16) ticks["Unk"].toInt());
        w.u16((quint16) ticks["Length"].toInt());
        w.u16((quint16) ticks["Ticks"].toInt());
        w.u16((quint16) ticks["Speed"].toInt());

        w.i32(jo["AnimOffset"].toInt());
        w.u32(toU32(jo["AnimDuration"]));
        w.u8((quint8) jo["Flag0"].toInt());
        w.u8(enumFromJson(jo["Interpolation"], INTERPOLATION_NAMES, 4));
        w.u8((quint8) jo["FollowPreviousObject"].toInt());
        w.u8((quint8) jo["HasDepthTest"].toInt());
        w.u8((quint8) jo["HasTransform"].toInt());
        w.u8((quint8) jo["Flag2"].toInt());
        w.u8((quint8) jo["Flag3"].toInt());
        w.u8((quint8) jo["HasTextureTransform"].toInt());

        writePatchCount(w, jo["PatchRotation"].toObject(), patch["Rotations"].toArray().size());
        writePatchCount(w, jo["PatchScale"].toObject(), patch["Scales"].toArray().size());
        writePatchCount(w, jo["PatchTranslation"].toObject(), patch["Translations"].toArray().size());
        writePatchCount(w, jo["PatchColor"].toObject(), patch["Colors"].toArray().size());
        writePatchCount(w, jo["PatchTexture"].toObject(), patch["Textures"].toArray().size());
        writePatchCount(w, jo["PatchTextureRotation"].toObject(), patch["TexRotations"].toArray().size());
        writePatchCount(w, jo["PatchTextureScale"].toObject(), patch["TexScales"].toArray().size());
        writePatchCount(w, jo["PatchTextureTranslate"].toObject(), patch["TexTranslations"].toArray().size());
    }

    quint32 patchCount(const QJsonObject &object, const QString &name) {
        return toU32(object[name].toObject()["Count"]);
    }

    QJsonArray readTimestamps(VfxReader &r, quint32 amount) {
        QJsonArray array;
        for (quint32 i = 0; i != amount && r.ok; ++i)
            array.append(r.u16());
        return array;
    }

    void writeTimestamps(VfxWriter &w, const QJsonArray &array) {
        for (auto &&value : array)
            w.u16((quint16) value.toInt());
    }

    bool readTrack(VfxReader &r, QJsonObject &patch, const QJsonObject &object, const QString &countName,
                   const QString &timestampsName, const QString &valuesName, int valueSize,
                   QJsonObject (*readValue)(VfxReader &)) {
        quint32 amount = patchCount(object, countName);
        if (!r.fits(amount, valueSize)) {
            r.ok = false;
            return false;
        }
        patch[timestampsName] = readTimestamps(r, amount);
        QJsonArray values;
        for (quint32 i = 0; i != amount && r.ok; ++i)
            values.append(readValue(r));
        patch[valuesName] = values;
        return r.ok;
    }

    QJsonObject readObjectPatch(VfxReader &r, const QJsonObject &object) {
        QJsonObject patch;
        readTrack(r, patch, object, "PatchRotation", "RotationTs", "Rotations", 16, readQuat);
        readTrack(r, patch, object, "PatchScale", "ScaleTs", "Scales", 12, readCoord);
        readTrack(r, patch, object, "PatchTranslation", "TranslationTs", "Translations", 12, readCoord);
        readTrack(r, patch, object, "PatchColor", "ColorTs", "Colors", 4, readColor);

        quint32 textures = patchCount(object, "PatchTexture");
        if (!r.fits(textures, 4))
            r.ok = false;
        patch["TextureTs"] = readTimestamps(r, textures);
        QJsonArray textureValues;
        for (quint32 i = 0; i != textures && r.ok; ++i)
            textureValues.append(r.i32());
        patch["Textures"] = textureValues;

        readTrack(r, patch, object, "PatchTextureRotation", "TexRotationTs", "TexRotations", 16, readQuat);
        readTrack(r, patch, object, "PatchTextureScale", "TexScaleTs", "TexScales", 12, readCoord);
        readTrack(r, patch, object, "PatchTextureTranslate", "TexTranslateTs", "TexTranslations", 12, readCoord);
        return patch;
    }

    void writeObjectPatch(VfxWriter &w, const QJsonObject &patch) {
        writeTimestamps(w, patch["RotationTs"].toArray());
        for (auto &&value : patch["Rotations"].toArray())
            writeQuat(w, value.toObject());
        writeTimestamps(w, patch["ScaleTs"].toArray());
        for (auto &&value : patch["Scales"].toArray())
            writeCoord(w, value.toObject());
        writeTimestamps(w, patch["TranslationTs"].toArray());
        for (auto &&value : patch["Translations"].toArray())
            writeCoord(w, value.toObject());
        writeTimestamps(w, patch["ColorTs"].toArray());
        for (auto &&value : patch["Colors"].toArray())
            writeColor(w, value.toObject());
        writeTimestamps(w, patch["TextureTs"].toArray());
        for (auto &&value : patch["Textures"].toArray())
            w.i32(value.toInt());
        writeTimestamps(w, patch["TexRotationTs"].toArray());
        for (auto &&value : patch["TexRotations"].toArray())
            writeQuat(w, value.toObject());
        writeTimestamps(w, patch["TexScaleTs"].toArray());
        for (auto &&value : patch["TexScales"].toArray())
            writeCoord(w, value.toObject());
        writeTimestamps(w, patch["TexTranslateTs"].toArray());
        for (auto &&value : patch["TexTranslations"].toArray())
            writeCoord(w, value.toObject());
    }

    QJsonObject rawItem(const QByteArray &content) {
        QJsonObject jo;
        jo["RawItem"] = QString(content.toBase64());
        return jo;
    }
}

QJsonObject NosVfxConverter::toJson(const QByteArray &content) {
    VfxReader r(content);

    QJsonObject header;
    header["VfxId"] = r.i32();
    header["Unk1"] = (qint64) r.u32();
    header["Unk2"] = (qint64) r.u32();
    quint16 objectCount = (quint16) r.i16();
    header["Unk3"] = (qint64) r.u32();
    header["UnkId2"] = r.i16();
    header["Unk4"] = (qint64) r.u32();

    if (!r.ok || objectCount > MAX_OBJECT_COUNT)
        return rawItem(content);

    QJsonArray objects;
    for (int i = 0; i != objectCount && r.ok; ++i)
        objects.append(readObjectHeader(r));

    QJsonArray patches;
    for (int i = 0; i != objectCount && r.ok; ++i)
        patches.append(readObjectPatch(r, objects.at(i).toObject()));

    if (!r.ok || !r.consumedAll())
        return rawItem(content);

    QJsonObject jo;
    jo["Header"] = header;
    jo["Objects"] = objects;
    jo["Patches"] = patches;
    return jo;
}

QByteArray NosVfxConverter::fromJson(const QJsonObject &jo) {
    if (!isVfx(jo)) {
        QString encoded = jo["RawItem"].toString();
        return QByteArray::fromBase64(encoded.toLatin1());
    }

    QJsonObject header = jo["Header"].toObject();
    QJsonArray objects = jo["Objects"].toArray();
    QJsonArray patches = jo["Patches"].toArray();

    VfxWriter w;
    w.i32(header["VfxId"].toInt());
    w.u32(toU32(header["Unk1"]));
    w.u32(toU32(header["Unk2"]));
    w.i16((qint16) objects.size());
    w.u32(toU32(header["Unk3"]));
    w.i16((qint16) header["UnkId2"].toInt());
    w.u32(toU32(header["Unk4"]));

    for (int i = 0; i != objects.size(); ++i)
        writeObjectHeader(w, objects.at(i).toObject(), patches.at(i).toObject());
    for (int i = 0; i != objects.size(); ++i)
        writeObjectPatch(w, patches.at(i).toObject());

    return w.data;
}

bool NosVfxConverter::isVfx(const QJsonObject &jo) {
    return jo.contains("Header") && jo.contains("Objects");
}
