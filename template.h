#ifndef TEMPLATE_H
#define TEMPLATE_H

#include <QString>
#include <QVector>
#include <QRect>
#include <QStringList>
#include <QFont>
#include <QJsonObject>
#include <QJsonArray>

struct TemplateField
{
    QString name;
    QRect geometry;
    QString type;
    QString defaultValue;
    QStringList options;
    QFont font;
};

class Template
{
public:
    Template();
    explicit Template(const QString &name);

    QString name() const;
    void setName(const QString &name);

    QString backgroundPath() const;
    void setBackgroundPath(const QString &path);

    void addField(const TemplateField &field);
    void removeField(const QString &fieldName);
    void removeFieldByIndex(int index);
    TemplateField *field(const QString &fieldName);
    TemplateField *fieldAt(int index);
    QVector<TemplateField> fields() const;

    bool saveToJson(const QString &filePath) const;
    bool loadFromJson(const QString &filePath);

private:
    QString m_name;
    QString m_backgroundPath;
    QVector<TemplateField> m_fields;
};

#endif
