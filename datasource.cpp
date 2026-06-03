#include "datasource.h"
#include <QFile>
#include <QTextStream>

DataSource::DataSource() {}

DataSource::DataSource(const QString &name) : m_name(name) {}

QString DataSource::name() const { return m_name; }

void DataSource::setName(const QString &name) { m_name = name; }

QStringList DataSource::items() const { return m_items; }

void DataSource::setItems(const QStringList &items) { m_items = items; }

void DataSource::addItem(const QString &item) {
    if (!m_items.contains(item)) {
        m_items.append(item);
    }
}

void DataSource::removeItem(const QString &item) {
    m_items.removeAll(item);
}

bool DataSource::loadFromCsv(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream in(&file);
    m_items.clear();
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            m_items.append(line);
        }
    }
    file.close();
    return true;
}

bool DataSource::saveToCsv(const QString &filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    for (const auto &item : m_items) {
        out << item << "\n";
    }
    file.close();
    return true;
}
