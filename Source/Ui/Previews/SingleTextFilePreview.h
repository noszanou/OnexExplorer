#ifndef SINGLETEXTFILEPREVIEW_H
#define SINGLETEXTFILEPREVIEW_H

#include "../TreeItems/OnexTreeText.h"
#include <QWidget>
#include <iostream>

namespace Ui {
    class SingleTextFilePreview;
}
class SingleTextFilePreview : public QWidget {
Q_OBJECT
public:
    explicit SingleTextFilePreview(QByteArray &item, const QString &encoding = "Windows-1250", QWidget *parent = nullptr);
    ~SingleTextFilePreview() override;
signals:
    void saveRequested(QByteArray data);
private slots:
    void onReplaced(const QByteArray &text);
    void onSaveClicked();
private:
    Ui::SingleTextFilePreview *ui;
    QString encoding;
    bool usesCrLf;
};

#endif // SINGLETEXTFILEPREVIEW_H
