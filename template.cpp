#include "template.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QFontInfo>
#include <QFileInfo>

static QString toRelativePath(const QString &baseFilePath, const QString &absolutePath)
{
    if (absolutePath.isEmpty()) return QString();
    QFileInfo baseInfo(baseFilePath);
    QDir baseDir = baseInfo.absoluteDir();
    QString relative = baseDir.relativeFilePath(absolutePath);
    if (relative.startsWith("..") && QFileInfo(absolutePath).isAbsolute())
        return absolutePath;
    return relative;
}

static QString toAbsolutePath(const QString &baseFilePath, const QString &relativePath)
{
    if (relativePath.isEmpty()) return QString();
    QFileInfo baseInfo(baseFilePath);
    if (QFileInfo(relativePath).isAbsolute())
        return relativePath;
    QDir baseDir = baseInfo.absoluteDir();
    return baseDir.absoluteFilePath(relativePath);
}

Template::Template()
{
    m_pages.append(TemplatePage());
}

Template::Template(const QString &name) : m_name(name)
{
    m_pages.append(TemplatePage());
}

QString Template::name() const { return m_name; }
void Template::setName(const QString &name) { m_name = name; }

int Template::pageCount() const { return m_pages.size(); }

void Template::addPage(const TemplatePage &page) { m_pages.append(page); }
void Template::insertPage(int index, const TemplatePage &page) { m_pages.insert(index, page); }
void Template::removePage(int index) { if (index >= 0 && index < m_pages.size()) m_pages.removeAt(index); }

TemplatePage *Template::page(int index)
{
    if (index >= 0 && index < m_pages.size()) return &m_pages[index];
    return nullptr;
}

const TemplatePage *Template::page(int index) const
{
    if (index >= 0 && index < m_pages.size()) return &m_pages[index];
    return nullptr;
}

QVector<TemplatePage> Template::pages() const { return m_pages; }

QString Template::backgroundPath() const
{
    return m_pages.isEmpty() ? QString() : m_pages[0].backgroundPath;
}

void Template::setBackgroundPath(const QString &path)
{
    if (!m_pages.isEmpty()) m_pages[0].backgroundPath = path;
}

void Template::addField(const TemplateField &field)
{
    if (!m_pages.isEmpty()) m_pages[0].fields.append(field);
}

void Template::removeField(const QString &fieldName)
{
    if (m_pages.isEmpty()) return;
    auto &fields = m_pages[0].fields;
    for (int i = 0; i < fields.size(); ++i) {
        if (fields[i].name == fieldName) {
            fields.removeAt(i);
            return;
        }
    }
}

void Template::removeFieldByIndex(int index)
{
    if (m_pages.isEmpty()) return;
    auto &fields = m_pages[0].fields;
    if (index >= 0 && index < fields.size()) fields.removeAt(index);
}

TemplateField *Template::field(const QString &fieldName)
{
    if (m_pages.isEmpty()) return nullptr;
    auto &fields = m_pages[0].fields;
    for (int i = 0; i < fields.size(); ++i) {
        if (fields[i].name == fieldName) return &fields[i];
    }
    return nullptr;
}

TemplateField *Template::fieldAt(int index)
{
    if (m_pages.isEmpty()) return nullptr;
    auto &fields = m_pages[0].fields;
    if (index >= 0 && index < fields.size()) return &fields[index];
    return nullptr;
}

QVector<TemplateField> Template::fields() const
{
    if (m_pages.isEmpty()) return QVector<TemplateField>();
    return m_pages[0].fields;
}

bool Template::saveToJson(const QString &filePath) const
{
    QJsonObject root;
    root["name"] = m_name;
    QJsonArray pagesArray;
    for (const auto &page : m_pages) {
        QJsonObject pageObj;
        pageObj["background"] = toRelativePath(filePath, page.backgroundPath);
        QJsonArray fieldsArray;
        for (const auto &field : page.fields) {
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
            for (const auto &opt : field.options) optionsArray.append(opt);
            fieldObj["options"] = optionsArray;

            fieldsArray.append(fieldObj);
        }
        pageObj["fields"] = fieldsArray;
        pagesArray.append(pageObj);
    }
    root["pages"] = pagesArray;

    QJsonDocument doc(root);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(doc.toJson());
    file.close();
    return true;
}

bool Template::loadFromJson(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return false;

    QJsonObject root = doc.object();
    m_name = root["name"].toString();

    m_pages.clear();
    QJsonArray pagesArray = root["pages"].toArray();
    if (pagesArray.isEmpty()) {
        TemplatePage page;
        page.backgroundPath = toAbsolutePath(filePath, root["background"].toString());
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
            for (const auto &opt : optionsArray) field.options.append(opt.toString());

            page.fields.append(field);
        }
        m_pages.append(page);
    } else {
        for (const auto &pageVal : pagesArray) {
            QJsonObject pageObj = pageVal.toObject();
            TemplatePage page;
            page.backgroundPath = toAbsolutePath(filePath, pageObj["background"].toString());
            QJsonArray fieldsArray = pageObj["fields"].toArray();
            for (const auto &val : fieldsArray) {
                QJsonObject fieldObj = val.toObject();
                TemplateField field;
                field.name = fieldObj["name"].toString();
                field.geometry = QRect(fieldObj["x"].toInt(), fieldObj["y"].toInt(),
                                       fieldObj["width"].toInt(), fieldObj["height"].toInt());
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
                for (const auto &opt : optionsArray) field.options.append(opt.toString());
                page.fields.append(field);
            }
            m_pages.append(page);
        }
    }
    if (m_pages.isEmpty()) m_pages.append(TemplatePage());
    return true;
}