#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QStackedWidget>
#include "template.h"
#include "templateeditor.h"

class TextDocument;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void newFile();
    void openFile();
    void saveFile();
    void setFontBold();
    void setFontItalic();
    void setFontUnderline();
    void setFontFamily(const QString &family);
    void setFontSize(int size);
    void setTextAlignment(Qt::Alignment alignment);
    void insertBulletList();
    void insertNumberedList();
    void newTemplate();
    void applyCurrentFormatToField();

private:
    QStackedWidget *centralStack;
    QTextEdit *textEdit;
    TemplateEditor *templateEditor;
    TextDocument *document;
    Template *currentTemplate;

    QFont m_currentFont;
    QColor m_currentColor;

    void createMenuBar();
    void createToolBars();
    void updateWindowTitle();
    void connectDocumentSignals();
    void applyCharFormat(const QTextCharFormat &format);
    void applyBlockFormat(const QTextBlockFormat &format);
};

#endif
