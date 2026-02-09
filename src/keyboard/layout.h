#pragma once

#include <QtCore>
#include <vector>

namespace appload::vk {
class KeyData : public QObject{
    Q_OBJECT
    Q_PROPERTY(QString label READ label CONSTANT)
    Q_PROPERTY(int code READ code CONSTANT)
    Q_PROPERTY(QString altLabel READ altLabel CONSTANT)
    Q_PROPERTY(int altCode READ altCode CONSTANT)
    Q_PROPERTY(int width READ width CONSTANT)
    Q_PROPERTY(bool isModifier READ isModifier CONSTANT)
public:
    QString label() const { return _label; }
    QString altLabel() const { return _altLabel; }
    int code() const { return _code; }
    int altCode() const { return _altCode; }
    int width() const { return _width; }
    bool isModifier() const { return _isModifier; }
    explicit KeyData(QObject *parent = nullptr): QObject(parent) {}

    KeyData(QString label, int code, QString altLabel, int altCode, int width, int isModifier, QObject *parent):
        QObject(parent),
        _label(label),
        _code(code),
        _altLabel(altLabel),
        _altCode(altCode),
        _width(width),
        _isModifier(isModifier) {}

    KeyData(const appload::vk::KeyData& copy):
        QObject(copy.parent()),
        _label(copy._label),
        _code(copy._code),
        _altLabel(copy._altLabel),
        _altCode(copy._altCode),
        _width(copy._width),
        _isModifier(copy._isModifier) {}


private:
    QString _label;
    int _code;
    QString _altLabel;
    int _altCode;
    int _width;
    bool _isModifier;
};

class Layout : public QObject{
    Q_OBJECT
    // No `NOTIFY'. Once initialized, the Layout object is static.
    Q_PROPERTY(int columns READ columns CONSTANT)
    Q_PROPERTY(int rows READ rows CONSTANT)
public:
    explicit Layout(QObject *parent = 0): QObject(parent) {};
    void load(const QByteArray &source);

    Q_INVOKABLE const KeyData *key(int col, int row) const;
    Q_INVOKABLE int columnsInRow(int row) const;
    int columns() const;
    int rows() const;
private:
    std::vector<std::vector<KeyData>> _layout;
};

}