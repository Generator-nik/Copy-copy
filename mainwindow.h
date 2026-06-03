#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QTextCharFormat>
#include <QTextBlockFormat>

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

private:
    QTextEdit *textEdit;
    TextDocument *document;
    void createMenuBar();
    void createToolBars();
    void updateWindowTitle();
    void connectDocumentSignals();
    void applyCharFormat(const QTextCharFormat &format);
    void applyBlockFormat(const QTextBlockFormat &format);

};

#endif