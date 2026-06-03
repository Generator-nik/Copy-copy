#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QStackedWidget>
#include <QLabel>
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
    void setFontBold(bool checked);
    void setFontItalic(bool checked);
    void setFontUnderline(bool checked);
    void setFontFamily(const QString &family);
    void setFontSize(int size);
    void setTextAlignment(Qt::Alignment alignment);
    void insertBulletList();
    void insertNumberedList();
    void newTemplate();
    void toggleViewMode();
    void exportToPdf();
    void exportToDocx();

private:
    QStackedWidget *centralStack;
    QTextEdit *textEdit;
    TemplateEditor *templateEditor;
    TextDocument *document;
    Template *currentTemplate;
    QFont m_currentFont;
    QColor m_currentColor;

    QAction *toggleViewAction;

    void createMenuBar();
    void createToolBars();
    void updateWindowTitle();
    void connectDocumentSignals();
    void applyCharFormat(const QTextCharFormat &format);
    void applyBlockFormat(const QTextBlockFormat &format);
    void applyCurrentFormatToField();

    void generateHtml(const QString &path);
    bool importPdfToTemplate(const QString &pdfPath, int dpi = 150);

    QLabel *m_zoomLabel;
};

#endif