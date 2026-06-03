#ifndef TEXTDOCUMENT_H
#define TEXTDOCUMENT_H

#include <QObject>
#include <QString>

class TextDocument : public QObject
{
    Q_OBJECT
public:
    explicit TextDocument(QObject *parent = nullptr);

    QString content() const;
    void setContent(const QString &text);
    bool isModified() const;

    bool loadFromFile(const QString &filePath);
    bool saveToFile(const QString &filePath);
    QString filePath() const { return m_filePath; }
    void setFilePath(const QString &path) { m_filePath = path; }

    void newDocument();

signals:
    void contentChanged(const QString &newContent);
    void modifiedChanged(bool modified);

private:
    QString m_content;
    QString m_filePath;
    bool m_modified;

    void setModified(bool modified);
};

#endif