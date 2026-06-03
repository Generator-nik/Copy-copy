#include "textdocument.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>

TextDocument::TextDocument(QObject *parent)
    : QObject(parent)
    , m_modified(false)
{
}

QString TextDocument::content() const
{
    return m_content;
}

void TextDocument::setContent(const QString &text)
{
    if (m_content == text)
        return;
    m_content = text;
    setModified(true);
    emit contentChanged(m_content);
}

bool TextDocument::isModified() const
{
    return m_modified;
}

void TextDocument::setModified(bool modified)
{
    if (m_modified == modified)
        return;
    m_modified = modified;
    emit modifiedChanged(m_modified);
}

bool TextDocument::loadFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    m_content = in.readAll();
    file.close();

    m_filePath = filePath;
    setModified(false);
    emit contentChanged(m_content);
    return true;
}

bool TextDocument::saveToFile(const QString &filePath)
{
    QString path = filePath.isEmpty() ? m_filePath : filePath;
    if (path.isEmpty())
        return false;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << m_content;
    file.close();

    m_filePath = path;
    setModified(false);
    return true;
}

void TextDocument::newDocument()
{
    m_content.clear();
    m_filePath.clear();
    setModified(false);
    emit contentChanged(m_content);
}