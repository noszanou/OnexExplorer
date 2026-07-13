#include "SingleTextFilePreview.h"
#include "ui_SingleTextFilePreview.h"
#ifdef _WIN32
#include <windows.h>
#endif

static UINT codepageFor(const QString &encoding) {
    if (encoding == "EUC-KR")
        return 949;
    if (encoding == "Big5")
        return 950;
    if (encoding == "Windows-1251")
        return 1251;
    if (encoding == "Windows-1252")
        return 1252;
    if (encoding == "Windows-1254")
        return 1254;
    return 1250;
}

static QString decodeBytes(const QByteArray &data, const QString &encoding) {
#ifdef _WIN32
    UINT cp = codepageFor(encoding);
    int len = MultiByteToWideChar(cp, 0, data.constData(), data.size(), nullptr, 0);
    if (len <= 0)
        return QString::fromLocal8Bit(data);
    QString out(len, QChar(0));
    MultiByteToWideChar(cp, 0, data.constData(), data.size(), reinterpret_cast<wchar_t *>(out.data()), len);
    return out;
#else
    Q_UNUSED(encoding);
    return QString::fromLocal8Bit(data);
#endif
}

static QByteArray encodeBytes(const QString &text, const QString &encoding) {
#ifdef _WIN32
    UINT cp = codepageFor(encoding);
    const wchar_t *src = reinterpret_cast<const wchar_t *>(text.constData());
    int len = WideCharToMultiByte(cp, 0, src, text.size(), nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return text.toLocal8Bit();
    QByteArray out(len, Qt::Uninitialized);
    WideCharToMultiByte(cp, 0, src, text.size(), out.data(), len, nullptr, nullptr);
    return out;
#else
    Q_UNUSED(encoding);
    return text.toLocal8Bit();
#endif
}

SingleTextFilePreview::SingleTextFilePreview(QByteArray &item, const QString &encoding, QWidget *parent)
        : QWidget(parent), ui(new Ui::SingleTextFilePreview), encoding(encoding), usesCrLf(item.contains("\r\n")) {
    ui->setupUi(this);
    QString encodeContent = decodeBytes(item, encoding);
    ui->label_encoding->setText(encoding);
    ui->plainTextEdit->appendPlainText(encodeContent);
    ui->plainTextEdit->moveCursor(QTextCursor::Start);
    connect(ui->saveButton, &QPushButton::clicked, this, &SingleTextFilePreview::onSaveClicked);
}

SingleTextFilePreview::~SingleTextFilePreview() {
    delete ui;
}

void SingleTextFilePreview::onReplaced(const QByteArray &text) {
    usesCrLf = text.contains("\r\n");
    ui->plainTextEdit->clear();
    QString encodeContent = decodeBytes(text, encoding);
    ui->plainTextEdit->appendPlainText(encodeContent);
    ui->plainTextEdit->moveCursor(QTextCursor::Start);
}

void SingleTextFilePreview::onSaveClicked() {
    QString text = ui->plainTextEdit->toPlainText();
    if (usesCrLf)
        text.replace("\n", "\r\n");
    emit saveRequested(encodeBytes(text, encoding));
}
