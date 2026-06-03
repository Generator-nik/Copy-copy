#include "template.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFontInfo>

Template::Template() {}
Template::Template(const QString &name) : m_name(name) {}

QString Template::name() const { return m_name; }
void Template::setName(const QString &name) { m_name = name; }

QString Template::backgroundPath() const { return m_backgroundPath; }
void Template::setBackgroundPath(const QString &path) { m_backgroundPath = path; }

void Template::addField(const TemplateField &field) {
    m_fields.append(field);
}

void Template::removeField(const QString &fieldName) {
    for (int i = 0; i < m_fields.size(); ++i) {
        if (m_fields[i].name == fieldName) {
            m_fields.removeAt(i);
            return;
        }
    }
}

void Template::removeFieldByIndex(int index) {
    if (index >= 0 && index < m_fields.size()) {
        m_fields.removeAt(index);
    }
}

TemplateField *Template::field(const QString &fieldName) {
    for (int i = 0; i < m_fields.size(); ++i) {
        if (m_fields[i].name == fieldName) {
            return &m_fields[i];
        }
    }
    return nullptr;
}

TemplateField *Template::fieldAt(int index) {
    if (index >= 0 && index < m_fields.size()) {
        return &m_fields[index];
    }
    return nullptr;
}

QVector<TemplateField> Template::fields() const {
    return m_fields;
}

bool Template::saveToJson(const QString &filePath) const {
    QJsonObject root;
    root["name"] = m_name;
    root["background"] = m_backgroundPath;

    QJsonArray fieldsArray;
    for (const auto &field : m_fields) {
        QJsonObject fieldObj;
        fieldObj["name"] = field.name;
        fieldObj["x"] = field.geometry.x();
        fieldObj["y"] = field.geometry.y();
        fieldObj["width"] = field.geometry.width();
        fieldObj["height"] = field.geometry.height();
        fieldObj["type"] = field.type;
        fieldObj["defaultValue"] = field.defaultValue;

        QJsonObject fontObj;
        fontObj["family"] = field.font.family();
        fontObj["size"] = field.font.pointSize();
        fontObj["bold"] = field.font.bold();
        fontObj["italic"] = field.font.italic();
        fontObj["underline"] = field.font.underline();
        fieldObj["font"] = fontObj;

        QJsonArray optionsArray;
        for (const auto &opt : field.options) {
            optionsArray.append(opt);
        }
        fieldObj["options"] = optionsArray;

        fieldsArray.append(fieldObj);
    }
    root["fields"] = fieldsArray;

    QJsonDocument doc(root);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(doc.toJson());
    file.close();
    return true;
}

bool Template::loadFromJson(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();
    m_name = root["name"].toString();
    m_backgroundPath = root["background"].toString();

    m_fields.clear();
    QJsonArray fieldsArray = root["fields"].toArray();
    for (const auto &val : fieldsArray) {
        QJsonObject fieldObj = val.toObject();
        TemplateField field;
        field.name = fieldObj["name"].toString();
        int x = fieldObj["x"].toInt();
        int y = fieldObj["y"].toInt();
        int w = fieldObj["width"].toInt();
        int h = fieldObj["height"].toInt();
        field.geometry = QRect(x, y, w, h);
        field.type = fieldObj["type"].toString();
        field.defaultValue = fieldObj["defaultValue"].toString();

        QJsonObject fontObj = fieldObj["font"].toObject();
        QFont font;
        if (!fontObj.isEmpty()) {
            font.setFamily(fontObj["family"].toString());
            font.setPointSize(fontObj["size"].toInt());
            font.setBold(fontObj["bold"].toBool());
            font.setItalic(fontObj["italic"].toBool());
            font.setUnderline(fontObj["underline"].toBool());
        } else {
            font = QFont("Arial", 12);
        }
        field.font = font;

        QJsonArray optionsArray = fieldObj["options"].toArray();
        for (const auto &opt : optionsArray) {
            field.options.append(opt.toString());
        }

        m_fields.append(field);
    }

    return true;
}
