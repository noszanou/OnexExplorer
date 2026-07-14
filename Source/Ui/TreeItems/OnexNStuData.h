#ifndef ONEXNSTUDATA_H
#define ONEXNSTUDATA_H

#include "../../Converters/NosMapConverter.h"
#include "OnexTreeZlibItem.h"

class OnexNStuData : public OnexTreeZlibItem {
Q_OBJECT
public:
    OnexNStuData(const QString &name, QByteArray content, NosZlibOpener *opener, int id = -1, int creationDate = 0,
                 bool compressed = false);
    OnexNStuData(QJsonObject jo, NosZlibOpener *opener, const QString &directory);
    ~OnexNStuData() override;
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
    NosMapConverter converter;
    QByteArray toJsonText();
    QString getSummary();
};

#endif // ONEXNSTUDATA_H
