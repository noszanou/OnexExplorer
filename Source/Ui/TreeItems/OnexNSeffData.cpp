#include "OnexNSeffData.h"
#include "../Previews/SingleTextFilePreview.h"
#include <QJsonArray>
#include <QJsonDocument>

OnexNSeffData::OnexNSeffData(const QString &name, QByteArray content, NosZlibOpener *opener, int id, int creationDate,
                             bool compressed) : OnexTreeZlibItem(name, opener, content, id, creationDate, compressed) {
}

OnexNSeffData::OnexNSeffData(QJsonObject jo, NosZlibOpener *opener, const QString &directory)
        : OnexTreeZlibItem(jo["ID"].toString(), opener) {
    setId(jo["ID"].toInt(), true);
    setCreationDate(jo["Date"].toString(), true);
    setCompressed(jo["isCompressed"].toBool(), true);
    onReplace(directory + jo["path"].toString());
}

OnexNSeffData::~OnexNSeffData() = default;

QWidget *OnexNSeffData::getPreview() {
    if (!hasParent())
        return nullptr;
    QByteArray json = toJsonText();
    auto *textPreview = new SingleTextFilePreview(json);
    connect(this, SIGNAL(replaceSignal(QByteArray)), textPreview, SLOT(onReplaced(QByteArray)));
    return textPreview;
}

QString OnexNSeffData::getExportExtension() {
    return ".json";
}

int OnexNSeffData::saveAsFile(const QString &path, QByteArray content) {
    return OnexTreeItem::saveAsFile(path, toJsonText());
}

int OnexNSeffData::afterReplace(QByteArray content) {
    QJsonParseError error{};
    QJsonDocument document = QJsonDocument::fromJson(content, &error);
    if (error.error == QJsonParseError::NoError && document.isObject())
        setContent(converter.fromJson(document.object()));
    else
        setContent(content);

    emit replaceSignal(toJsonText());
    emit replaceInfo(generateInfos());
    return 1;
}

FileInfo *OnexNSeffData::generateInfos() {
    auto *infos = OnexTreeZlibItem::generateInfos();
    if (hasParent())
        infos->addStringLineEdit("Content", getSummary())->setEnabled(false);
    return infos;
}

QByteArray OnexNSeffData::toJsonText() {
    return QJsonDocument(converter.toJson(getContent())).toJson();
}

QString OnexNSeffData::getSummary() {
    QJsonObject jo = converter.toJson(getContent());
    if (!NosVfxConverter::isVfx(jo))
        return QString("not a VFX config (%1 raw bytes)").arg(getContentSize());
    return QString("VFX %1, %2 object(s)")
            .arg(jo["Header"].toObject()["VfxId"].toInt())
            .arg(jo["Objects"].toArray().size());
}
