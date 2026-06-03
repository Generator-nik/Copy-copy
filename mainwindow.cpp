#include "mainwindow.h"
#include "textdocument.h"
#include "templateeditor.h"
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextCursor>
#include <QFontComboBox>
#include <QComboBox>
#include <QTextList>
#include <QTextImageFormat>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>
#include <QStackedWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , centralStack(new QStackedWidget(this))
    , textEdit(new QTextEdit(this))
    , templateEditor(new TemplateEditor(this))
    , document(new TextDocument(this))
    , currentTemplate(nullptr)
{
    m_currentFont = QFont("Arial", 12);
    m_currentColor = Qt::black;

    centralStack->addWidget(textEdit);
    centralStack->addWidget(templateEditor);
    centralStack->setCurrentIndex(0);

    setCentralWidget(centralStack);
    createMenuBar();
    createToolBars();
    connectDocumentSignals();

    document->newDocument();
    setWindowTitle("copycopy");
    resize(800, 600);
}

MainWindow::~MainWindow() {}

void MainWindow::connectDocumentSignals()
{
    connect(textEdit, &QTextEdit::textChanged, [this]() {
        document->setContent(textEdit->toPlainText());
    });

    connect(document, &TextDocument::modifiedChanged, this, &MainWindow::updateWindowTitle);
}

void MainWindow::updateWindowTitle()
{
    QString title = "copycopy";
    if (!document->filePath().isEmpty())
        title += " - " + QFileInfo(document->filePath()).fileName();
    else
        title += " - [Новый документ]";
    if (document->isModified())
        title += " *";
    setWindowTitle(title);
}

void MainWindow::createMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("&Файл");

    QAction *newAction = new QAction("&Новый", this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::newFile);
    fileMenu->addAction(newAction);

    QAction *openAction = new QAction("&Открыть...", this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);
    fileMenu->addAction(openAction);

    QAction *saveAction = new QAction("&Сохранить", this);
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveFile);
    fileMenu->addAction(saveAction);

    fileMenu->addSeparator();

    QAction *exitAction = new QAction("В&ыход", this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(exitAction);

    QMenu *templateMenu = menuBar()->addMenu("&Шаблон");

    QAction *newTemplateAction = new QAction("&Новый шаблон", this);
    connect(newTemplateAction, &QAction::triggered, this, &MainWindow::newTemplate);
    templateMenu->addAction(newTemplateAction);

    QAction *bgAction = new QAction("&Загрузить фон...", this);
    connect(bgAction, &QAction::triggered, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Выберите фон",
                        QString(), "Изображения (*.png *.jpg *.jpeg)");
        if (!path.isEmpty()) {
            templateEditor->setBackgroundImage(path);
            if (currentTemplate) {
                currentTemplate->setBackgroundPath(path);
            }
        }
    });
    templateMenu->addAction(bgAction);

    templateMenu->addSeparator();

    QAction *saveTemplateAction = new QAction("&Сохранить шаблон...", this);
    connect(saveTemplateAction, &QAction::triggered, [this]() {
        if (!currentTemplate) {
            QMessageBox::warning(this, "Ошибка", "Сначала создайте шаблон.");
            return;
        }
        QString path = QFileDialog::getSaveFileName(this, "Сохранить шаблон",
                        QString(), "JSON (*.json)");
        if (!path.isEmpty()) {
            currentTemplate->saveToJson(path);
        }
    });
    templateMenu->addAction(saveTemplateAction);

    QAction *loadTemplateAction = new QAction("&Загрузить шаблон...", this);
    connect(loadTemplateAction, &QAction::triggered, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Загрузить шаблон",
                        QString(), "JSON (*.json)");
        if (path.isEmpty()) return;

        if (currentTemplate) delete currentTemplate;
        currentTemplate = new Template("");
        currentTemplate->loadFromJson(path);
        templateEditor->setTemplate(currentTemplate);
        centralStack->setCurrentIndex(1);

        if (!currentTemplate->backgroundPath().isEmpty()) {
            templateEditor->setBackgroundImage(currentTemplate->backgroundPath());
        }
    });
    templateMenu->addAction(loadTemplateAction);
}

void MainWindow::createToolBars()
{
    QToolBar *formatBar = addToolBar("Форматирование");

    QAction *boldAction = new QAction("Жирный", this);
    boldAction->setCheckable(true);
    connect(boldAction, &QAction::toggled, [this](bool checked) {
        m_currentFont.setBold(checked);
        applyCurrentFormatToField();
        textEdit->setFontWeight(checked ? QFont::Bold : QFont::Normal);
    });
    formatBar->addAction(boldAction);

    QAction *italicAction = new QAction("Курсив", this);
    italicAction->setCheckable(true);
    connect(italicAction, &QAction::toggled, [this](bool checked) {
        m_currentFont.setItalic(checked);
        applyCurrentFormatToField();
        textEdit->setFontItalic(checked);
    });
    formatBar->addAction(italicAction);

    QAction *underlineAction = new QAction("Подчёркивание", this);
    underlineAction->setCheckable(true);
    connect(underlineAction, &QAction::toggled, [this](bool checked) {
        m_currentFont.setUnderline(checked);
        applyCurrentFormatToField();
        textEdit->setFontUnderline(checked);
    });
    formatBar->addAction(underlineAction);

    formatBar->addSeparator();

    QAction *alignLeft = new QAction("По левому краю", this);
    connect(alignLeft, &QAction::triggered, [this]() { setTextAlignment(Qt::AlignLeft); });
    formatBar->addAction(alignLeft);

    QAction *alignCenter = new QAction("По центру", this);
    connect(alignCenter, &QAction::triggered, [this]() { setTextAlignment(Qt::AlignCenter); });
    formatBar->addAction(alignCenter);

    QAction *alignRight = new QAction("По правому краю", this);
    connect(alignRight, &QAction::triggered, [this]() { setTextAlignment(Qt::AlignRight); });
    formatBar->addAction(alignRight);

    formatBar->addSeparator();

    QFontComboBox *fontCombo = new QFontComboBox(this);
    fontCombo->setCurrentFont(QFont("Arial"));
    connect(fontCombo, &QFontComboBox::currentFontChanged, [this](const QFont &font) {
        m_currentFont.setFamily(font.family());
        applyCurrentFormatToField();
        textEdit->setFontFamily(font.family());
    });
    formatBar->addWidget(fontCombo);

    QComboBox *sizeCombo = new QComboBox(this);
    sizeCombo->addItems({"8", "9", "10", "11", "12", "14", "16", "18", "20", "24", "28", "32", "48"});
    sizeCombo->setCurrentText("12");
    connect(sizeCombo, &QComboBox::currentTextChanged, [this](const QString &text) {
        int size = text.toInt();
        m_currentFont.setPointSize(size);
        applyCurrentFormatToField();
        textEdit->setFontPointSize(size);
    });
    formatBar->addWidget(sizeCombo);

    formatBar->addSeparator();

    QAction *addFieldAction = new QAction("Добавить поле", this);
    addFieldAction->setCheckable(true);
    formatBar->addAction(addFieldAction);

    QAction *fillAction = new QAction("Заполнить", this);
    fillAction->setCheckable(true);
    formatBar->addAction(fillAction);

    connect(addFieldAction, &QAction::toggled, [this, addFieldAction, fillAction](bool checked) {
        if (checked) {
            fillAction->setChecked(false);
            templateEditor->setFillMode(false);
        }
        templateEditor->setAddFieldMode(checked);
    });

    connect(fillAction, &QAction::toggled, [this, addFieldAction, fillAction](bool checked) {
        if (checked) {
            addFieldAction->setChecked(false);
            templateEditor->setAddFieldMode(false);
        }
        templateEditor->setFillMode(checked);
    });
}

void MainWindow::newTemplate()
{
    if (currentTemplate) {
        delete currentTemplate;
    }

    currentTemplate = new Template("Новый шаблон");
    templateEditor->setTemplate(currentTemplate);
    centralStack->setCurrentIndex(1);
}

void MainWindow::newFile()
{
    if (document->isModified()) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                                  "Несохранённые изменения",
                                                                  "Текст был изменён. Сохранить изменения?",
                                                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (reply == QMessageBox::Yes) {
            saveFile();
        } else if (reply == QMessageBox::Cancel) {
            return;
        }
    }
    document->newDocument();
    updateWindowTitle();
}

void MainWindow::openFile()
{
    if (document->isModified()) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                                  "Несохранённые изменения",
                                                                  "Текст был изменён. Сохранить изменения?",
                                                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (reply == QMessageBox::Yes) {
            saveFile();
        } else if (reply == QMessageBox::Cancel) {
            return;
        }
    }

    QString fileName = QFileDialog::getOpenFileName(this,
                                                    "Открыть файл",
                                                    QString(),
                                                    "Текстовые файлы (*.txt);;Все файлы (*.*)");
    if (fileName.isEmpty())
        return;

    if (!document->loadFromFile(fileName)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл");
        return;
    }
    updateWindowTitle();
}

void MainWindow::saveFile()
{
    document->setContent(textEdit->toPlainText());

    QString fileName = document->filePath();
    if (fileName.isEmpty()) {
        fileName = QFileDialog::getSaveFileName(this,
                                                "Сохранить файл",
                                                QString(),
                                                "Текстовые файлы (*.txt);;Все файлы (*.*)");
        if (fileName.isEmpty())
            return;
    }
    if (!document->saveToFile(fileName)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось сохранить файл");
    }
    updateWindowTitle();
}

void MainWindow::applyCharFormat(const QTextCharFormat &format)
{
    QTextCursor cursor = textEdit->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);
    cursor.mergeCharFormat(format);
}

void MainWindow::setFontBold()
{
    QTextCharFormat format;
    format.setFontWeight(textEdit->fontWeight() == QFont::Bold ? QFont::Normal : QFont::Bold);
    applyCharFormat(format);
}

void MainWindow::setFontItalic()
{
    QTextCharFormat format;
    format.setFontItalic(!textEdit->fontItalic());
    applyCharFormat(format);
}

void MainWindow::setFontUnderline()
{
    QTextCharFormat format;
    format.setFontUnderline(!textEdit->fontUnderline());
    applyCharFormat(format);
}

void MainWindow::setFontFamily(const QString &family)
{
    QTextCharFormat format;
    format.setFontFamily(family);
    applyCharFormat(format);
}

void MainWindow::setFontSize(int size)
{
    QTextCharFormat format;
    format.setFontPointSize(size);
    applyCharFormat(format);
}

void MainWindow::applyBlockFormat(const QTextBlockFormat &format)
{
    QTextCursor cursor = textEdit->textCursor();
    cursor.mergeBlockFormat(format);
}

void MainWindow::setTextAlignment(Qt::Alignment alignment)
{
    QTextBlockFormat format;
    format.setAlignment(alignment);
    applyBlockFormat(format);
}

void MainWindow::insertBulletList()
{
    QTextCursor cursor = textEdit->textCursor();
    QTextListFormat listFormat;
    listFormat.setStyle(QTextListFormat::ListDisc);
    cursor.createList(listFormat);
}

void MainWindow::insertNumberedList()
{
    QTextCursor cursor = textEdit->textCursor();
    QTextListFormat listFormat;
    listFormat.setStyle(QTextListFormat::ListDecimal);
    cursor.createList(listFormat);
}

void MainWindow::applyCurrentFormatToField()
{
    templateEditor->applyCurrentFormat(m_currentFont, m_currentColor);
}
