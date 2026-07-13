#ifndef ONEXNSEFFDATA_H
#define ONEXNSEFFDATA_H

#include "../../Converters/NosVfxConverter.h"
#include "OnexTreeZlibItem.h"

class OnexNSeffData : public OnexTreeZlibItem {
Q_OBJECT
public:
    OnexNSeffData(const QString &name, QByteArray content, NosZlibOpener *opener, int id = -1, int creationDate = 0,
                  bool compressed = false);
    OnexNSeffData(QJsonObject jo, NosZlibOpener *opener, const QString &directory);
    ~OnexNSeffData() override;
    QWidget *getPreview() override;
    QString getExportExtension() override;
public slots:
    int afterReplace(QByteArray content) override;
signals:
    void replaceSignal(QByteArray text);
protected:
    int saveAsFile(const QString &path, QByteArray content) override;
    FileInfo *generateInfos() override;
private:
    NosVfxConverter converter;
    QByteArray toJsonText();
    QString getSummary();
};

#endif // ONEXNSEFFDATA_H
