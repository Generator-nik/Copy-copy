#include "mainwindow.h"
#include "textdocument.h"
#include "templateeditor.h"
#include <QStatusBar>
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
#include <QFileInfo>
#include <QDebug>
#include <QPdfWriter>
#include <QUrl>
#include <QPainter>
#include <QTemporaryDir>
#include <QProcess>
#include <QStandardPaths>
#include <QUuid>
#include <QDir>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QProgressDialog>

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
    resize(950, 650);
}

MainWindow::~MainWindow()
{
    if (currentTemplate) {
        delete currentTemplate;
    }
}

void MainWindow::connectDocumentSignals()
{
    connect(textEdit, &QTextEdit::textChanged, [this]() {
        document->setContent(textEdit->toPlainText());
    });
    connect(document, &TextDocument::contentChanged, textEdit, &QTextEdit::setPlainText);
    connect(document, &TextDocument::modifiedChanged, this, &MainWindow::updateWindowTitle);
}

void MainWindow::updateWindowTitle()
{
    QString title = "copycopy";
    if (centralStack->currentIndex() == 1) {
        title += " - [Конструктор Шаблонов]";
        if (currentTemplate) {
            title += " (" + currentTemplate->name() + ")";
            int page = templateEditor->currentPageIndex();
            if (page >= 0) title += QString(" - стр. %1").arg(page+1);
        }
    } else {
        QString title = "copycopy";
        if (centralStack->currentIndex() == 1) {
            title += " - [Конструктор Шаблонов]";
            if (currentTemplate) title += " (" + currentTemplate->name() + ")";
        } else {
            if (!document->filePath().isEmpty())
                title += " - " + QFileInfo(document->filePath()).fileName();
            else
                title += " - [Новый документ]";
            if (document->isModified())
                title += " *";
        }
    }
    setWindowTitle(title);
}

void MainWindow::toggleViewMode()
{
    int nextIndex = (centralStack->currentIndex() == 0) ? 1 : 0;
    centralStack->setCurrentIndex(nextIndex);
    if (nextIndex == 1 && !currentTemplate) {
        newTemplate();
    }
    toggleViewAction->setText(nextIndex == 0 ? "Режим: Шаблоны" : "Режим: Текст");
    updateWindowTitle();
}

void MainWindow::createMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("&Файл");

    QAction *newAction = new QAction("&Новый файл", this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::newFile);
    fileMenu->addAction(newAction);

    QAction *openAction = new QAction("&Открыть файл...", this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);
    fileMenu->addAction(openAction);

    QAction *saveAction = new QAction("&Сохранить файл", this);
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveFile);
    fileMenu->addAction(saveAction);

    fileMenu->addSeparator();

    QAction *exitAction = new QAction("В&ыход", this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(exitAction);

    fileMenu->addSeparator();

    QAction *exportPdfAction = new QAction("Экспорт в &PDF...", this);
    connect(exportPdfAction, &QAction::triggered, this, &MainWindow::exportToPdf);
    fileMenu->addAction(exportPdfAction);

    QAction *exportDocxAction = new QAction("Экспорт в &DOCX (Word)...", this);
    connect(exportDocxAction, &QAction::triggered, this, &MainWindow::exportToDocx);
    fileMenu->addAction(exportDocxAction);

    QMenu *templateMenu = menuBar()->addMenu("&Шаблон");

    QAction *newTemplateAction = new QAction("&Новый шаблон", this);
    connect(newTemplateAction, &QAction::triggered, this, &MainWindow::newTemplate);
    templateMenu->addAction(newTemplateAction);

    QAction *importPdfAction = new QAction("&Импорт из PDF...", this);
    connect(importPdfAction, &QAction::triggered, [this]() {
        if (!currentTemplate) {
            QMessageBox::warning(this, "Ошибка", "Сначала создайте или откройте шаблон.");
            return;
        }
        QString pdfPath = QFileDialog::getOpenFileName(this, "Выберите PDF-файл",
                                                       QString(), "PDF (*.pdf)");
        if (pdfPath.isEmpty()) return;

        if (importPdfToTemplate(pdfPath, 150)) {
            templateEditor->setTemplate(currentTemplate);
            templateEditor->setCurrentPage(0);
            QMessageBox::information(this, "Успех",
                                     QString("Импортировано %1 страниц.").arg(currentTemplate->pageCount()));
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось импортировать PDF.");
        }
    });
    templateMenu->addAction(importPdfAction);

    QAction *bgAction = new QAction("&Загрузить фон...", this);
    connect(bgAction, &QAction::triggered, [this]() {
        if (!currentTemplate) {
            QMessageBox::warning(this, "Ошибка", "Сначала создайте или откройте шаблон.");
            return;
        }
        int pageIndex = templateEditor->currentPageIndex();
        if (pageIndex < 0) return;

        QString path = QFileDialog::getOpenFileName(this, "Выберите фон",
                                                    QString(), "Изображения (*.png *.jpg *.jpeg)");
        if (!path.isEmpty()) {
            TemplatePage *page = currentTemplate->page(pageIndex);
            if (page) {
                page->backgroundPath = path;
                templateEditor->setBackgroundImage(path);
            }
        }
    });
    templateMenu->addAction(bgAction);

    templateMenu->addSeparator();

    QAction *saveTemplateAction = new QAction("&Сохранить шаблон...", this);
    connect(saveTemplateAction, &QAction::triggered, [this]() {
        if (!currentTemplate) {
            QMessageBox::warning(this, "Ошибка", "Сначала создайте или откройте шаблон.");
            return;
        }
        QString path = QFileDialog::getSaveFileName(this, "Сохранить шаблон",
                                                    QString(), "JSON (*.json)");
        if (!path.isEmpty()) {
            if (!currentTemplate->saveToJson(path)) {
                QMessageBox::critical(this, "Ошибка", "Не удалось сохранить файл шаблона.");
            } else {
                statusBar()->showMessage("Шаблон успешно сохранён", 3000);
            }
        }
    });
    templateMenu->addAction(saveTemplateAction);

    QAction *loadTemplateAction = new QAction("&Загрузить шаблон...", this);
    connect(loadTemplateAction, &QAction::triggered, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Загрузить шаблон",
                                                    QString(), "JSON (*.json)");
        if (path.isEmpty()) return;

        Template *loadedTemplate = new Template("");
        if (loadedTemplate->loadFromJson(path)) {
            if (currentTemplate) delete currentTemplate;
            currentTemplate = loadedTemplate;
            templateEditor->setTemplate(currentTemplate);
            centralStack->setCurrentIndex(1);
            toggleViewAction->setText("Режим: Текст");

            if (!currentTemplate->backgroundPath().isEmpty()) {
                templateEditor->setBackgroundImage(currentTemplate->backgroundPath());
            }
            updateWindowTitle();
        } else {
            delete loadedTemplate;
            QMessageBox::critical(this, "Ошибка", "Не удалось загрузить или распарсить файл шаблона.");
        }
    });
    templateMenu->addAction(loadTemplateAction);
}

void MainWindow::createToolBars()
{
    QToolBar *viewBar = addToolBar("Режим отображения");
    toggleViewAction = new QAction("Режим: Шаблоны", this);
    connect(toggleViewAction, &QAction::triggered, this, &MainWindow::toggleViewMode);
    viewBar->addAction(toggleViewAction);

    QToolBar *formatBar = addToolBar("Форматирование");

    QAction *boldAction = new QAction("Жирный", this);
    boldAction->setCheckable(true);
    connect(boldAction, &QAction::toggled, this, &MainWindow::setFontBold);
    formatBar->addAction(boldAction);

    QAction *italicAction = new QAction("Курсив", this);
    italicAction->setCheckable(true);
    connect(italicAction, &QAction::toggled, this, &MainWindow::setFontItalic);
    formatBar->addAction(italicAction);

    QAction *underlineAction = new QAction("Подчёркивание", this);
    underlineAction->setCheckable(true);
    connect(underlineAction, &QAction::toggled, this, &MainWindow::setFontUnderline);
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
        setFontFamily(font.family());
    });
    formatBar->addWidget(fontCombo);

    QComboBox *sizeCombo = new QComboBox(this);
    sizeCombo->addItems({"8", "9", "10", "11", "12", "14", "16", "18", "20", "24", "28", "32", "48"});
    sizeCombo->setCurrentText("12");
    connect(sizeCombo, &QComboBox::currentTextChanged, [this](const QString &text) {
        setFontSize(text.toInt());
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
            if(centralStack->currentIndex() != 1) toggleViewMode();
        }
        templateEditor->setAddFieldMode(checked);
    });

    connect(fillAction, &QAction::toggled, [this, addFieldAction, fillAction](bool checked) {
        if (checked) {
            addFieldAction->setChecked(false);
            templateEditor->setAddFieldMode(false);
            if(centralStack->currentIndex() != 1) toggleViewMode();
        }
        templateEditor->setFillMode(checked);
    });

    formatBar->addSeparator();

    QAction *zoomInAction = new QAction("Приблизить", this);
    zoomInAction->setIcon(QIcon::fromTheme("zoom-in"));
    connect(zoomInAction, &QAction::triggered, templateEditor, &TemplateEditor::zoomIn);
    formatBar->addAction(zoomInAction);

    QAction *zoomOutAction = new QAction("Отдалить", this);
    zoomOutAction->setIcon(QIcon::fromTheme("zoom-out"));
    connect(zoomOutAction, &QAction::triggered, templateEditor, &TemplateEditor::zoomOut);
    formatBar->addAction(zoomOutAction);

    m_zoomLabel = new QLabel("Масштаб: 100%", this);
    m_zoomLabel->setStyleSheet("QLabel { margin-left: 10px; margin-right: 10px; font-weight: bold; color: #555; }");

    formatBar->addWidget(m_zoomLabel);

    connect(templateEditor, &TemplateEditor::zoomChanged, this, [this](double zoomFactor) {
        int percents = qRound(zoomFactor * 100);
        m_zoomLabel->setText(QString("Масштаб: %1%").arg(percents));
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
    toggleViewAction->setText("Режим: Текст");
    updateWindowTitle();
    m_zoomLabel->setText("Масштаб: 100%");
}

void MainWindow::newFile()
{
    if (document->isModified()) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                                  "Несохранённые изменения", "Текст был изменён. Сохранить изменения?",
                                                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (reply == QMessageBox::Yes) {
            saveFile();
        } else if (reply == QMessageBox::Cancel) {
            return;
        }
    }
    document->newDocument();
    textEdit->clear();
    centralStack->setCurrentIndex(0);
    toggleViewAction->setText("Режим: Шаблоны");
    updateWindowTitle();
}

void MainWindow::openFile()
{
    if (document->isModified()) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                                  "Несохранённые изменения", "Текст был изменён. Сохранить изменения?",
                                                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (reply == QMessageBox::Yes) {
            saveFile();
        } else if (reply == QMessageBox::Cancel) {
            return;
        }
    }

    QString fileName = QFileDialog::getOpenFileName(this, "Открыть файл",
                                                    QString(), "Текстовые файлы (*.txt);;Все файлы (*.*)");
    if (fileName.isEmpty())
        return;

    if (!document->loadFromFile(fileName)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл");
        return;
    }
    centralStack->setCurrentIndex(0);
    toggleViewAction->setText("Режим: Шаблоны");
    updateWindowTitle();
}

void MainWindow::saveFile()
{
    document->setContent(textEdit->toPlainText());
    QString fileName = document->filePath();
    if (fileName.isEmpty()) {
        fileName = QFileDialog::getSaveFileName(this, "Сохранить файл",
                                                QString(), "Текстовые файлы (*.txt);;Все файлы (*.*)");
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
    if (cursor.hasSelection()) {
        cursor.mergeCharFormat(format);
    } else {
        textEdit->mergeCurrentCharFormat(format);
    }
}

void MainWindow::setFontBold(bool checked)
{
    m_currentFont.setBold(checked);
    applyCurrentFormatToField();
    QTextCharFormat format;
    format.setFontWeight(checked ? QFont::Bold : QFont::Normal);
    applyCharFormat(format);
}

void MainWindow::setFontItalic(bool checked)
{
    m_currentFont.setItalic(checked);
    applyCurrentFormatToField();
    QTextCharFormat format;
    format.setFontItalic(checked);
    applyCharFormat(format);
}

void MainWindow::setFontUnderline(bool checked)
{
    m_currentFont.setUnderline(checked);
    applyCurrentFormatToField();
    QTextCharFormat format;
    format.setFontUnderline(checked);
    applyCharFormat(format);
}

void MainWindow::setFontFamily(const QString &family)
{
    m_currentFont.setFamily(family);
    applyCurrentFormatToField();
    QTextCharFormat format;
    format.setFontFamily(family);
    applyCharFormat(format);
}

void MainWindow::setFontSize(int size)
{
    m_currentFont.setPointSize(size);
    applyCurrentFormatToField();
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
    if (centralStack->currentIndex() == 1) {
        templateEditor->applyCurrentFormat(m_currentFont, m_currentColor);
    }
}


void MainWindow::exportToPdf()
{
    QString path = QFileDialog::getSaveFileName(this, "Экспорт в PDF", QString(), "PDF файлы (*.pdf)");
    if (path.isEmpty()) return;

    if (!currentTemplate) return;

    int originalPageIndex = templateEditor->currentPageIndex();

    QPdfWriter pdfWriter(path);
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setPageMargins(QMarginsF(0, 0, 0, 0));

    QRectF pageRect = pdfWriter.pageLayout().paintRectPixels(pdfWriter.resolution());
    double scaleX = pageRect.width() / 1000.0;
    double scaleY = pageRect.height() / 1000.0;

    QPainter painter(&pdfWriter);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    for (int i = 0; i < currentTemplate->pageCount(); ++i) {
        if (i > 0) pdfWriter.newPage();

        const TemplatePage *page = currentTemplate->page(i);
        if (!page) continue;
        templateEditor->setCurrentPage(i);
        if (!page->backgroundPath.isEmpty()) {
            QPixmap bg(page->backgroundPath);
            if (!bg.isNull())
                painter.drawPixmap(0, 0, (int)pageRect.width(), (int)pageRect.height(), bg);
            else
                painter.fillRect(QRectF(0, 0, pageRect.width(), pageRect.height()), Qt::white);
        } else {
            painter.fillRect(QRectF(0, 0, pageRect.width(), pageRect.height()), Qt::white);
        }

        for (const auto &field : page->fields) {
            QRectF rect(
                field.geometry.x() * scaleX,
                field.geometry.y() * scaleY,
                field.geometry.width() * scaleX,
                field.geometry.height() * scaleY
                );

            QString displayContent = templateEditor->evaluateFormulas(field.defaultValue);
            displayContent = templateEditor->evaluateTextExpressions(displayContent);
            painter.setFont(field.font);
            painter.setPen(Qt::black);

            double padding = 2.0 * scaleX;
            QRectF innerRect = rect.adjusted(padding, padding, -padding, -padding);
            painter.drawText(innerRect, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, displayContent);
        }
    }

    painter.end();
    templateEditor->setCurrentPage(originalPageIndex);
    statusBar()->showMessage("Экспорт в PDF успешно завершен", 3000);
}

void MainWindow::exportToDocx()
{
    QString path = QFileDialog::getSaveFileName(this, "Экспорт в DOCX",
                                                QString(), "Документ Word (*.docx);;HTML файл (*.html)");
    if (path.isEmpty()) return;

    bool isDocx = path.endsWith(".docx", Qt::CaseInsensitive);
    if (isDocx) {
        QString htmlPath = path;
        htmlPath.replace(".docx", ".html");
        generateHtml(htmlPath);
        QFile::rename(htmlPath, path);
    } else {
        generateHtml(path);
    }
    statusBar()->showMessage("Экспорт в Word успешно завершен", 3000);
}


void MainWindow::generateHtml(const QString &path)
{
    if (!currentTemplate) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать файл.");
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "<!DOCTYPE html>\n";
    out << "<html>\n<head>\n";
    out << "<meta charset=\"UTF-8\">\n";
    out << "<title>Экспорт шаблона</title>\n";
    out << "<style>\n";
    out << "@media print {\n";
    out << "  body { margin: 0; padding: 0; }\n";
    out << "  .page { page-break-after: always; }\n";
    out << "  .page:last-child { page-break-after: auto; }\n";
    out << "}\n";
    out << "@page { size: A4; margin: 0mm; }\n";
    out << "body { margin: 0; padding: 0; background: #e0e0e0; }\n";
    out << ".page { \n";
    out << "  position: relative; \n";
    out << "  width: 210mm; \n";
    out << "  min-height: 297mm; \n";
    out << "  margin: 0 auto; \n";
    out << "  background: white; \n";
    out << "  box-shadow: 0 0 5px rgba(0,0,0,0.2); \n";
    out << "  page-break-after: always; \n";
    out << "  page-break-inside: avoid; \n";
    out << "}\n";
    out << ".field { position: absolute; overflow: hidden; word-wrap: break-word; }\n";
    out << "</style>\n";
    out << "</head>\n<body>\n";

    for (int p = 0; p < currentTemplate->pageCount(); ++p) {
        const TemplatePage *page = currentTemplate->page(p);
        if (!page) continue;

        out << "<div class=\"page\">\n";

        if (!page->backgroundPath.isEmpty()) {
            QString bgUrl = QUrl::fromLocalFile(page->backgroundPath).toString();
            out << "<img src=\"" << bgUrl << "\" style=\"position: absolute; top:0; left:0; width:100%; height:100%; z-index:-1;\">\n";
        }

        for (const auto &field : page->fields) {
            double left_mm = field.geometry.x() * 210.0 / 1000.0;
            double top_mm = field.geometry.y() * 297.0 / 1000.0;
            double width_mm = field.geometry.width() * 210.0 / 1000.0;
            double height_mm = field.geometry.height() * 297.0 / 1000.0;

            QString style = QString("left: %1mm; top: %2mm; width: %3mm; height: %4mm;")
                                .arg(left_mm, 0, 'f', 2)
                                .arg(top_mm, 0, 'f', 2)
                                .arg(width_mm, 0, 'f', 2)
                                .arg(height_mm, 0, 'f', 2);
            style += QString(" font-family: %1; font-size: %2pt;")
                         .arg(field.font.family())
                         .arg(field.font.pointSize());
            if (field.font.bold()) style += " font-weight: bold;";
            if (field.font.italic()) style += " font-style: italic;";
            if (field.font.underline()) style += " text-decoration: underline;";
            style += " color: black;";

            out << "<div class=\"field\" style=\"" << style << "\">";
            out << field.defaultValue.toHtmlEscaped().replace("\n", "<br>");
            out << "</div>\n";
        }

        out << "</div>\n";
    }
    out << "</body>\n</html>\n";
    file.close();
}

QString findPdftoppm() {
    QString appDir = QCoreApplication::applicationDirPath();

    QStringList candidates;
    candidates << appDir + "/tools/poppler/pdftoppm.exe"
               << appDir + "/tools/poppler/pdftoppm"
               << appDir + "/pdftoppm.exe"
               << appDir + "/pdftoppm";

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString pathEnv = env.value("PATH");
    QStringList paths = pathEnv.split(QDir::listSeparator());
    for (const QString &p : paths) {
        candidates << p + "/pdftoppm.exe" << p + "/pdftoppm";
    }

    for (const QString &path : candidates) {
        if (QFile::exists(path)) {
            qDebug() << "Найден pdftoppm:" << path;
            return path;
        }
    }

    qDebug() << "pdftoppm не найден. Проверенные пути:" << candidates;
    return QString();
}

bool MainWindow::importPdfToTemplate(const QString &pdfPath, int dpi)
{
    if (!currentTemplate) return false;
    QString pdftoppm = findPdftoppm();
    if (pdftoppm.isEmpty()) {
        QMessageBox::critical(this, "Ошибка",
                              "Утилита pdftoppm не найдена.\n"
                              "Пожалуйста, поместите папку tools/poppler рядом с программой.");
        return false;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) return false;
    QString prefix = QDir(tempDir.path()).filePath("page");

    QProcess process;
    process.start(pdftoppm, QStringList() << "-png" << "-r" << QString::number(dpi) << pdfPath << prefix);
    if (!process.waitForStarted()) return false;

    QProgressDialog progress("Импорт PDF...", "Отмена", 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    if (!process.waitForFinished(60000) || process.exitCode() != 0) {
        QMessageBox::warning(this, "Ошибка", "Не удалось конвертировать PDF.");
        return false;
    }

    QDir dir(tempDir.path());
    QStringList pngFiles = dir.entryList(QStringList() << "page-*.png", QDir::Files, QDir::Name);
    if (pngFiles.isEmpty()) return false;

    QString saveDir = QCoreApplication::applicationDirPath() + "/imported_bg/";
    QDir().mkpath(saveDir);

    while (currentTemplate->pageCount() > 0)
        currentTemplate->removePage(0);

    int pageNum = 1;
    for (const QString &file : pngFiles) {
        QString src = dir.absoluteFilePath(file);
        QString dst = saveDir + QFileInfo(pdfPath).baseName() + QString("_%1.png").arg(pageNum++, 2, 10, QChar('0'));
        if (QFile::copy(src, dst)) {
            TemplatePage newPage;
            newPage.backgroundPath = dst;
            currentTemplate->addPage(newPage);
        }
    }
    progress.close();
    return currentTemplate->pageCount() > 0;
}