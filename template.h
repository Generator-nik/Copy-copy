#ifndef TEMPLATE_H
#define TEMPLATE_H

#include <QString>
#include <QVector>
#include <QRect>
#include <QStringList>
#include <QFont>

struct TemplateField
{
    QString name;
    QRect geometry;
    QString type;
    QString defaultValue;
    QStringList options;
    QFont font;
};

struct TemplatePage
{
    QString backgroundPath;
    QVector<TemplateField> fields;
};

class Template
{
public:
    Template();
    explicit Template(const QString &name);

    QString name() const;
    void setName(const QString &name);

    int pageCount() const;
    void addPage(const TemplatePage &page);
    void insertPage(int index, const TemplatePage &page);
    void removePage(int index);
    TemplatePage *page(int index);
    const TemplatePage *page(int index) const;
    QVector<TemplatePage> pages() const;

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
    QVector<TemplatePage> m_pages;
};

#endif