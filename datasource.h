#ifndef DATASOURCE_H
#define DATASOURCE_H

#include <QString>
#include <QStringList>

class DataSource
{
public:
    DataSource();
    explicit DataSource(const QString &name);

    QString name() const;
    void setName(const QString &name);

    QStringList items() const;
    void setItems(const QStringList &items);
    void addItem(const QString &item);
    void removeItem(const QString &item);

    bool loadFromCsv(const QString &filePath);
    bool saveToCsv(const QString &filePath) const;

private:
    QString m_name;
    QStringList m_items;
};

#endif
