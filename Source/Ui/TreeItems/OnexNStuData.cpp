#include "OnexNStuData.h"
#include "../Previews/SingleTextFilePreview.h"
#include <QJsonArray>
#include <QJsonDocument>

OnexNStuData::OnexNStuData(const QString &name, QByteArray content, NosZlibOpener *opener, int id, int creationDate,
                           bool compressed) : OnexTreeZlibItem(name, opener, content, id, creationDate, compressed) {
}

OnexNStuData::OnexNStuData(QJsonObject jo, NosZlibOpener *opener, const QString &directory)
        : OnexTreeZlibItem(jo["ID"].toString(), opener) {
    setId(jo["ID"].toInt(), true);
    setCreationDate(jo["Date"].toString(), true);
    setCompressed(jo["isCompressed"].toBool(), true);
    onReplace(directory + jo["path"].toString());
}

OnexNStuData::~OnexNStuData() = default;

static const int PREVIEW_LIMIT = 512 * 1024;

QWidget *OnexNStuData::getPreview() {
    if (!hasParent())
        return nullptr;
    QByteArray json = toJsonText();
    if (json.size() > PREVIEW_LIMIT) {
        QByteArray note = QString("%1\n\nJSON is %2 bytes - too large for the preview.\nUse export/import to edit this entry.")
                .arg(getSummary()).arg(json.size()).toUtf8();
        auto *summaryPreview = new SingleTextFilePreview(note);
        summaryPreview->setSaveEnabled(false);
        return summaryPreview;
    }
    auto *textPreview = new SingleTextFilePreview(json);
    connect(this, SIGNAL(replaceSignal(QByteArray)), textPreview, SLOT(onReplaced(QByteArray)));
    connect(textPreview, SIGNAL(saveRequested(QByteArray)), this, SLOT(afterReplace(QByteArray)));
    return textPreview;
}

QString OnexNStuData::getExportExtension() {
    return ".json";
}

int OnexNStuData::saveAsFile(const QString &path, QByteArray content) {
    return OnexTreeItem::saveAsFile(path, toJsonText());
}

int OnexNStuData::afterReplace(QByteArray content) {
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

FileInfo *OnexNStuData::generateInfos() {
    auto *infos = OnexTreeZlibItem::generateInfos();
    if (hasParent())
        infos->addStringLineEdit("Content", getSummary())->setEnabled(false);
    return infos;
}

QByteArray OnexNStuData::toJsonText() {
    return QJsonDocument(converter.toJson(getContent())).toJson();
}

QString OnexNStuData::getSummary() {
    QJsonObject jo = converter.toJson(getContent());
    if (!NosMapConverter::isMapConfig(jo))
        return QString("not a map config (%1 raw bytes)").arg(getContentSize());
    return QString("Map config: %1 model(s), %2 object(s)")
            .arg(jo["Models"].toArray().size())
            .arg(jo["Objects"].toArray().size());
}
