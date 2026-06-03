#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialog>
#include <QTextEdit>
#include <QPainter>
#include <QFile>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QResizeEvent>
#include <QFontMetrics>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QSignalBlocker>
#include <QMenu>
#include <QContextMenuEvent>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <QWheelEvent>
#include <QTextDocument>
#include <QTextOption>

#include "templateeditor.h"
#include "datasource.h"

static void skipSpaces(const QString &str, int &pos) {
    while (pos < str.length() && str[pos].isSpace())
        ++pos;
}

void TemplateEditor::prevPage()
{
    if (m_template && m_currentPageIndex > 0)
        setCurrentPage(m_currentPageIndex - 1);
}

void TemplateEditor::nextPage()
{
    if (m_template && m_currentPageIndex < m_template->pageCount() - 1)
        setCurrentPage(m_currentPageIndex + 1);
}

void TemplateEditor::updatePageLabel()
{
    if (m_template)
        m_pageLabel->setText(QString("Страница %1 из %2")
                                 .arg(m_currentPageIndex + 1)
                                 .arg(m_template->pageCount()));
}

void TemplateEditor::setCurrentPage(int index)
{
    if (!m_template || index < 0 || index >= m_template->pageCount())
        return;
    finishInlineEditing();
    m_currentPageIndex = index;

    TemplatePage *page = m_template->page(m_currentPageIndex);
    if (page) {
        if (!page->backgroundPath.isEmpty()) {
            if (!m_background.load(page->backgroundPath))
                m_background = QPixmap();
        } else {
            m_background = QPixmap();
        }

        if (m_background.isNull())
            generateBlankA4();

        updatePixmapRect();
        update();
    }
    updatePageLabel();
}

TemplateEditor::TemplateEditor(QWidget *parent)
    : QWidget(parent), m_template(nullptr), m_hasBufferData(false), m_dragIndex(-1), m_dragMode(None),
    m_addingField(false), m_isDrawingNewField(false), m_hoverIndex(-1), m_fillMode(false),
    m_selectedIndex(-1), m_activeEditor(nullptr), m_editingFieldIndex(-1),
    m_zoomFactor(1.0), m_currentPageIndex(0)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_topLayout = new QHBoxLayout();
    m_topLayout->setContentsMargins(5, 5, 5, 5);

    m_prevBtn = new QPushButton("<", this);
    m_pageLabel = new QLabel("Страница 1", this);
    m_nextBtn = new QPushButton(">", this);
    m_addPageBtn = new QPushButton("+ Новая страница", this);

    m_topLayout->addWidget(m_prevBtn);
    m_topLayout->addWidget(m_pageLabel);
    m_topLayout->addWidget(m_nextBtn);
    m_topLayout->addStretch();
    m_topLayout->addWidget(m_addPageBtn);

    mainLayout->addLayout(m_topLayout);

    setLayout(mainLayout);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::white);
    setPalette(pal);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    m_currentFont = QFont("Arial", 12);
    m_currentColor = Qt::black;

    connect(m_prevBtn, &QPushButton::clicked, this, &TemplateEditor::prevPage);
    connect(m_nextBtn, &QPushButton::clicked, this, &TemplateEditor::nextPage);
    connect(m_addPageBtn, &QPushButton::clicked, this, &TemplateEditor::addNewPage);
}

void TemplateEditor::setTemplate(Template *tmpl)
{
    finishInlineEditing();
    m_template = tmpl;
    m_zoomFactor = 1.0;
    m_dragIndex = -1;
    m_dragMode = None;
    m_addingField = false;
    m_isDrawingNewField = false;
    m_hoverIndex = -1;
    m_fillMode = false;
    m_selectedIndex = -1;
    m_highlightedFields.clear();

    if (m_template && m_template->pageCount() > 0) {
        setCurrentPage(0);
    } else {
        m_background = QPixmap();
        generateBlankA4();
        updatePixmapRect();
        update();
        updatePageLabel();
    }
}

Template *TemplateEditor::currentTemplate() const
{
    return m_template;
}

void TemplateEditor::addNewPage()
{
    if (!m_template) return;
    TemplatePage newPage;
    m_template->addPage(newPage);
    int newIndex = m_template->pageCount() - 1;
    setCurrentPage(newIndex);
}

void TemplateEditor::setBackgroundImage(const QString &path)
{
    if (!m_template) return;
    TemplatePage *page = m_template->page(m_currentPageIndex);
    if (!page) return;

    page->backgroundPath = path;

    if (path.isEmpty() || !m_background.load(path)) {
        m_background = QPixmap();
        if (!path.isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Не удалось загрузить выбранное изображение. Будет использован пустой лист А4.");
        }
    }

    if (m_background.isNull())
        generateBlankA4();

    updatePixmapRect();
    update();
}

void TemplateEditor::setAddFieldMode(bool enable)
{
    finishInlineEditing();
    m_addingField = enable;
    m_isDrawingNewField = false;
    if (enable) {
        m_fillMode = false;
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
    m_currentRect = QRect();
    update();
}

bool TemplateEditor::isAddingField() const
{
    return m_addingField;
}

void TemplateEditor::setFillMode(bool enable)
{
    finishInlineEditing();
    m_fillMode = enable;
    m_addingField = false;
    m_isDrawingNewField = false;
    m_dragIndex = -1;
    m_dragMode = None;
    setCursor(enable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    update();
}

bool TemplateEditor::isFillMode() const
{
    return m_fillMode;
}

int TemplateEditor::fieldAtPos(const QPoint &pos) const
{
    if (!m_template) return -1;
    TemplatePage *page = m_template->page(m_currentPageIndex);
    if (!page) return -1;
    const QVector<TemplateField> &fields = page->fields;
    for (int i = fields.size() - 1; i >= 0; --i) {
        QRect displayRect = toDisplayRect(fields[i].geometry);
        if (displayRect.contains(pos)) return i;
    }
    return -1;
}

TemplateField *TemplateEditor::fieldAt(int index)
{
    if (!m_template) return nullptr;
    TemplatePage *page = m_template->page(m_currentPageIndex);
    if (!page) return nullptr;
    if (index < 0 || index >= page->fields.size()) return nullptr;
    return &page->fields[index];
}

bool TemplateEditor::isInResizeZone(const QPoint &pos, const QRect &displayRect) const
{
    QRect resizeZone(displayRect.right() - 16, displayRect.bottom() - 16, 16, 16);
    return resizeZone.contains(pos);
}

QRect TemplateEditor::toDisplayRect(const QRect &relativeRect) const
{
    if (m_background.isNull()) return relativeRect;
    int x = m_pixmapRect.left() + (relativeRect.left() * m_pixmapRect.width() / 1000);
    int y = m_pixmapRect.top() + (relativeRect.top() * m_pixmapRect.height() / 1000);
    int w = relativeRect.width() * m_pixmapRect.width() / 1000;
    int h = relativeRect.height() * m_pixmapRect.height() / 1000;
    return QRect(x, y, w, h);
}

QRect TemplateEditor::toRelativeRect(const QRect &displayRect) const
{
    if (m_background.isNull() || m_pixmapRect.width() == 0 || m_pixmapRect.height() == 0)
        return displayRect;
    int x = (displayRect.left() - m_pixmapRect.left()) * 1000 / m_pixmapRect.width();
    int y = (displayRect.top() - m_pixmapRect.top()) * 1000 / m_pixmapRect.height();
    int w = displayRect.width() * 1000 / m_pixmapRect.width();
    int h = displayRect.height() * 1000 / m_pixmapRect.height();
    return QRect(x, y, w, h);
}

void TemplateEditor::updatePixmapRect()
{
    if (m_background.isNull()) {
        generateBlankA4();
    }
    if (m_background.isNull()) return;
    double margin = 0.9;
    double scaleX = (double)width() * margin / m_background.width();
    double scaleY = (double)height() * margin / m_background.height();
    double fitScale = qMin(scaleX, qMin(scaleY, 1.0));
    if (fitScale <= 0) fitScale = 0.5;
    int w = qRound(m_background.width() * fitScale * m_zoomFactor);
    int h = qRound(m_background.height() * fitScale * m_zoomFactor);
    int x = (width() - w) / 2;
    int y = (height() - h) / 2;
    m_pixmapRect = QRect(x, y, w, h);
}

void TemplateEditor::generateBlankA4()
{
    int a4Width = 840;
    int a4Height = 1188;
    QPixmap blank(a4Width, a4Height);
    blank.fill(Qt::white);
    QPainter painter(&blank);
    painter.setPen(QPen(QColor(160, 160, 160), 6));
    painter.drawRect(3, 3, a4Width - 6, a4Height - 6);
    painter.end();
    m_background = blank;
}

void TemplateEditor::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updatePixmapRect();
    updateActiveEditorGeometry();
    update();
}

QString TemplateEditor::generateUniqueFieldName() const
{
    if (!m_template) return "F1";
    TemplatePage *page = m_template->page(m_currentPageIndex);
    if (!page) return "F1";
    int maxId = 0;
    QRegularExpression re("^F(\\d+)$");
    for (const auto &field : page->fields) {
        QRegularExpressionMatch match = re.match(field.name);
        if (match.hasMatch()) {
            int id = match.captured(1).toInt();
            if (id > maxId) maxId = id;
        }
    }
    return QString("F%1").arg(maxId + 1);
}

void TemplateEditor::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    updatePixmapRect();
    painter.fillRect(rect(), QColor(230, 230, 230));
    if (!m_background.isNull()) {
        painter.drawPixmap(m_pixmapRect, m_background);
    }

    if (!m_template) return;

    TemplatePage *page = m_template->page(m_currentPageIndex);
    if (!page) return;
    const QVector<TemplateField> &fields = page->fields;

    auto drawWrappedText = [&painter](const QRect &rect, const QString &text, const QFont &font) {
        QTextDocument doc;
        doc.setDefaultFont(font);
        doc.setPlainText(text);
        doc.setTextWidth(rect.width());
        QTextOption opt = doc.defaultTextOption();
        opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        doc.setDefaultTextOption(opt);
        painter.save();
        painter.translate(rect.topLeft());
        doc.drawContents(&painter, QRectF(0, 0, rect.width(), rect.height()));
        painter.restore();
    };

    for (int i = 0; i < fields.size(); ++i) {
        if (i == m_editingFieldIndex && m_activeEditor) {
            QRect displayRect = toDisplayRect(fields[i].geometry);
            painter.setPen(QPen(Qt::darkCyan, 2, Qt::SolidLine));
            painter.setBrush(QColor(0, 255, 255, 10));
            painter.drawRect(displayRect);
            continue;
        }

        const TemplateField &field = fields[i];
        QRect displayRect = toDisplayRect(field.geometry);
        QRect innerRect = displayRect.adjusted(2, 2, -2, -2);

        if (m_highlightedFields.contains(i)) {
            QColor variableColor = m_highlightedFields[i];
            painter.setPen(QPen(variableColor, 2, Qt::SolidLine));
            QColor fillColor = variableColor;
            fillColor.setAlpha(35);
            painter.setBrush(fillColor);
            painter.drawRect(displayRect);
        } else if (i == m_selectedIndex && !m_fillMode) {
            painter.setPen(QPen(Qt::red, 2, Qt::DashLine));
            painter.setBrush(QColor(255, 0, 0, 20));
            painter.drawRect(displayRect);
            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::red);
            painter.drawRect(QRect(displayRect.right() - 6, displayRect.bottom() - 6, 6, 6));
        } else if (i == m_hoverIndex && !m_fillMode) {
            painter.setPen(QPen(Qt::blue, 1, Qt::DashLine));
            painter.setBrush(QColor(0, 0, 255, 15));
            painter.drawRect(displayRect);
        } else {
            painter.setPen(QPen(Qt::gray, 1, Qt::DotLine));
            painter.setBrush(QColor(128, 128, 128, 10));
            painter.drawRect(displayRect);
        }

        QString displayContent = evaluateFormulas(field.defaultValue);
        displayContent = evaluateTextExpressions(displayContent);
        double pageScale = m_background.isNull() ? 1.0 : ((double)m_pixmapRect.height() / m_background.height());

        QFont scaledFont = field.font;
        if (scaledFont.pointSize() > 0) {
            scaledFont.setPointSizeF(qMax(1.0, field.font.pointSizeF() * pageScale));
        } else if (scaledFont.pixelSize() > 0) {
            scaledFont.setPixelSize(qMax(4, qRound(field.font.pixelSize() * pageScale)));
        }

        if (field.type == "container") {
            if (field.options.contains("collapsed")) {
                QFont boldFont = scaledFont;
                boldFont.setBold(true);
                painter.setFont(boldFont);
                painter.setPen(m_currentColor);
                painter.drawText(innerRect, Qt::AlignLeft | Qt::AlignTop, "[...]");
            } else {
                painter.setFont(scaledFont);
                painter.setPen(m_currentColor);
                drawWrappedText(innerRect, displayContent, scaledFont);
            }
        } else {
            painter.setFont(scaledFont);
            painter.setPen(m_currentColor);
            drawWrappedText(innerRect, displayContent, scaledFont);
        }
    }

    if (m_addingField && m_isDrawingNewField && m_currentRect.isValid()) {
        painter.setBrush(QColor(0, 0, 255, 30));
        painter.setPen(QPen(Qt::blue, 1, Qt::DashLine));
        painter.drawRect(m_currentRect);
    }
}

void TemplateEditor::applyCurrentFormat(const QFont &font, const QColor &color)
{
    m_currentFont = font;
    m_currentColor = color;

    if (m_selectedIndex != -1 && m_template) {
        TemplateField *field = fieldAt(m_selectedIndex);
        if (field) {
            field->font = font;
        }
    }
    update();
}

void TemplateEditor::mousePressEvent(QMouseEvent *event)
{
    if (!m_template) return;
    if (event->button() == Qt::RightButton) return;
    if (event->button() != Qt::LeftButton) return;

    if (m_activeEditor) {
        int clicked = fieldAtPos(event->pos());
        if (clicked != -1 && clicked != m_editingFieldIndex) {
            TemplatePage *page = m_template->page(m_currentPageIndex);
            if (page && m_editingFieldIndex >= 0 && m_editingFieldIndex < page->fields.size()) {
                if (page->fields[m_editingFieldIndex].type != "container") {
                    m_activeEditor->insertPlainText(page->fields[clicked].name);
                    m_activeEditor->setFocus();
                    return;
                }
            }
        } else if (clicked == -1) {
            finishInlineEditing();
        }
    }

    if (m_fillMode) {
        int clicked = fieldAtPos(event->pos());
        if (clicked == -1) {
            finishInlineEditing();
        }
        return;
    }

    int clicked = fieldAtPos(event->pos());

    if (clicked != -1) {
        m_selectedIndex = clicked;
        m_dragIndex = clicked;
        m_dragStart = event->pos();
        TemplateField *f = fieldAt(clicked);
        if (f) m_dragStartRect = toDisplayRect(f->geometry);
        if (isInResizeZone(event->pos(), m_dragStartRect)) {
            m_dragMode = Resize;
            setCursor(Qt::SizeFDiagCursor);
        } else {
            m_dragMode = Move;
            setCursor(Qt::SizeAllCursor);
        }
        m_isDrawingNewField = false;
        update();
    } else {
        if (m_addingField) {
            m_isDrawingNewField = true;
            m_addStart = event->pos();
            m_currentRect = QRect(m_addStart, QSize(0, 0));
            m_selectedIndex = -1;
            m_dragIndex = -1;
            m_dragMode = None;
            update();
        } else {
            m_selectedIndex = -1;
            m_dragIndex = -1;
            m_dragMode = None;
            setCursor(Qt::ArrowCursor);
            update();
        }
    }
}

void TemplateEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_template || m_fillMode) return;

    if (m_dragIndex != -1) {
        int dx = event->pos().x() - m_dragStart.x();
        int dy = event->pos().y() - m_dragStart.y();

        TemplateField *field = fieldAt(m_dragIndex);
        if (field) {
            if (m_dragMode == Move) {
                QPoint delta = event->pos() - m_dragStart;
                QRect newRect = m_dragStartRect.translated(delta);
                if (newRect.left() < m_pixmapRect.left())
                    newRect.moveLeft(m_pixmapRect.left());
                if (newRect.right() > m_pixmapRect.right())
                    newRect.moveRight(m_pixmapRect.right());
                if (newRect.top() < m_pixmapRect.top())
                    newRect.moveTop(m_pixmapRect.top());
                if (newRect.bottom() > m_pixmapRect.bottom())
                    newRect.moveBottom(m_pixmapRect.bottom());
                field->geometry = toRelativeRect(newRect);
                update();
            } else if (m_dragMode == Resize) {
                int newWidth = qMax(20, m_dragStartRect.width() + dx);
                int newHeight = qMax(20, m_dragStartRect.height() + dy);
                QRect newDisplayRect(m_dragStartRect.x(), m_dragStartRect.y(), newWidth, newHeight);
                field->geometry = toRelativeRect(newDisplayRect);
            }
        }
        update();
    } else if (m_addingField && m_isDrawingNewField) {
        QRect newRect = QRect(m_addStart, event->pos()).normalized();
        int left = qMax(newRect.left(), m_pixmapRect.left());
        int top = qMax(newRect.top(), m_pixmapRect.top());
        int right = qMin(newRect.right(), m_pixmapRect.right());
        int bottom = qMin(newRect.bottom(), m_pixmapRect.bottom());
        m_currentRect = QRect(QPoint(left, top), QPoint(right, bottom));
        update();
    } else {
        int hover = fieldAtPos(event->pos());
        if (hover != m_hoverIndex) {
            m_hoverIndex = hover;
            update();
        }
        if (hover != -1) {
            QRect displayRect = toDisplayRect(fieldAt(hover)->geometry);
            if (isInResizeZone(event->pos(), displayRect))
                setCursor(Qt::SizeFDiagCursor);
            else
                setCursor(Qt::SizeAllCursor);
        } else {
            setCursor(m_addingField ? Qt::CrossCursor : Qt::ArrowCursor);
        }
    }
}

void TemplateEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    if (!m_template || m_fillMode) return;

    if (m_addingField && m_isDrawingNewField) {
        m_isDrawingNewField = false;
        if (m_currentRect.isValid() && m_currentRect.width() > 10 && m_currentRect.height() > 10) {
            TemplateField newField;
            newField.name = generateUniqueFieldName();
            newField.geometry = toRelativeRect(m_currentRect);
            newField.type = "text";
            newField.font = m_currentFont;
            TemplatePage *page = m_template->page(m_currentPageIndex);
            if (page) {
                page->fields.append(newField);
                m_selectedIndex = page->fields.size() - 1;
            }
        }
        m_currentRect = QRect();
        update();
    } else if (m_dragIndex != -1) {
        m_dragIndex = -1;
        m_dragMode = None;
        int hover = fieldAtPos(event->pos());
        if (hover != -1) {
            QRect displayRect = toDisplayRect(fieldAt(hover)->geometry);
            setCursor(isInResizeZone(event->pos(), displayRect) ? Qt::SizeFDiagCursor : Qt::SizeAllCursor);
        } else {
            setCursor(m_addingField ? Qt::CrossCursor : Qt::ArrowCursor);
        }
        update();
    }
    m_currentRect = QRect();
}

void TemplateEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_template || event->button() != Qt::LeftButton) return;

    int idx = fieldAtPos(event->pos());
    if (idx == -1) return;

    TemplateField *field = fieldAt(idx);
    if (!field) return;

    auto runInlineEditor = [this, idx](TemplateField *f) {
        finishInlineEditing();
        m_editingFieldIndex = idx;
        QRect geometryRect = toDisplayRect(f->geometry);
        double pageScale = m_background.isNull() ? 1.0 : ((double)m_pixmapRect.height() / m_background.height());
        QFont editorFont = f->font;
        if (editorFont.pointSize() > 0) {
            editorFont.setPointSizeF(qMax(1.0, f->font.pointSizeF() * pageScale));
        } else if (editorFont.pixelSize() > 0) {
            editorFont.setPixelSize(qMax(4, qRound(f->font.pixelSize() * pageScale)));
        }
        m_activeEditor = new QTextEdit(this);
        m_activeEditor->setGeometry(geometryRect);
        m_activeEditor->setFont(editorFont);
        m_activeEditor->setPlainText(f->defaultValue);
        m_activeEditor->setStyleSheet("QTextEdit { background: rgba(255, 255, 255, 230); border: 1px solid #008b8b; }");
        m_activeEditor->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_activeEditor, &QTextEdit::customContextMenuRequested, this, &TemplateEditor::showContextMenu);
        m_activeEditor->installEventFilter(this);
        bool isNumber = (f->type == "number");
        connect(m_activeEditor, &QTextEdit::textChanged, [this, isNumber]() {
            if (m_activeEditor) {
                QSignalBlocker blocker(m_activeEditor);
                QTextCursor cursor = m_activeEditor->textCursor();
                int currentPos = cursor.position();
                QString origText = m_activeEditor->toPlainText();
                QString filteredText = origText;
                if (isNumber) {
                    filteredText.remove(QRegularExpression("[^0-9.,\\-+\\s]"));
                    filteredText.replace(',', '.');
                }
                if (origText != filteredText) {
                    m_activeEditor->setPlainText(filteredText);
                    cursor.setPosition(qMin(currentPos, filteredText.length()));
                    m_activeEditor->setTextCursor(cursor);
                }
            }
            this->updateInlineEditorHighlights();
        });
        m_activeEditor->show();
        m_activeEditor->setFocus();
        updateInlineEditorHighlights();
        update();
    };

    if (m_fillMode) {
        if (field->type == "dropdown") {
            QStringList userVisibleOptions;
            for (const QString &opt : field->options) {
                if (!opt.startsWith("path:"))
                    userVisibleOptions << opt;
            }
            if (userVisibleOptions.isEmpty()) {
                QMessageBox::warning(this, "Внимание", "Список выбора пуст. Задайте элементы в режиме редактирования шаблона.");
                return;
            }
            bool ok;
            QString selected = QInputDialog::getItem(this, "Выбор значения",
                                                     QString("Выберите значение для поля %1:").arg(field->name), userVisibleOptions, 0, false, &ok);
            if (ok) {
                field->defaultValue = selected;
                update();
            }
        } else {
            runInlineEditor(field);
        }
        return;
    }

    if (field->type == "container") {
        if (field->options.contains("collapsed"))
            field->options.removeAll("collapsed");
        else
            field->options.append("collapsed");
        update();
        return;
    }

    QDialog propDlg(this);
    propDlg.setWindowTitle(QString("Свойства поля: %1").arg(field->name));
    propDlg.setMinimumWidth(450);

    QVBoxLayout *mainLayout = new QVBoxLayout(&propDlg);

    QHBoxLayout *typeLayout = new QHBoxLayout();
    typeLayout->addWidget(new QLabel("Тип поля:", &propDlg));
    QComboBox *typeCombo = new QComboBox(&propDlg);
    typeCombo->addItems({"text", "dropdown", "container", "number"});
    typeCombo->setCurrentText(field->type);
    typeLayout->addWidget(typeCombo, 1);
    mainLayout->addLayout(typeLayout);
    QFrame *line = new QFrame(&propDlg);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    QGroupBox *dropdownGroup = new QGroupBox("Настройки выпадающего списка", &propDlg);
    QVBoxLayout *groupLayout = new QVBoxLayout(dropdownGroup);

    QLineEdit *pathEdit = new QLineEdit(&propDlg);
    pathEdit->setReadOnly(true);
    pathEdit->setPlaceholderText("Файл .csv не выбран или не создан");

    QString currentCsvPath;
    for (const QString &opt : field->options) {
        if (opt.startsWith("path:")) {
            currentCsvPath = opt.mid(5);
            pathEdit->setText(currentCsvPath);
            break;
        }
    }

    QHBoxLayout *pathLayout = new QHBoxLayout();
    pathLayout->addWidget(pathEdit, 1);
    QPushButton *browseBtn = new QPushButton("Обзор / Создать...", &propDlg);
    pathLayout->addWidget(browseBtn);
    groupLayout->addLayout(pathLayout);

    QPushButton *editItemsBtn = new QPushButton("Редактировать элементы списка...", &propDlg);
    editItemsBtn->setEnabled(!currentCsvPath.isEmpty());
    groupLayout->addWidget(editItemsBtn);

    mainLayout->addWidget(dropdownGroup);

    auto updateGroupVisibility = [dropdownGroup](const QString &type) {
        dropdownGroup->setVisible(type == "dropdown");
    };
    updateGroupVisibility(field->type);
    connect(typeCombo, &QComboBox::currentTextChanged, updateGroupVisibility);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &propDlg);
    mainLayout->addWidget(buttonBox);

    connect(browseBtn, &QPushButton::clicked, [this, &propDlg, pathEdit, editItemsBtn, &currentCsvPath]() {
        QString path = QFileDialog::getSaveFileName(&propDlg, "Выбрать существующий или создать новый CSV файл",
                                                    QString(), "CSV файлы (*.csv);;Все файлы (*.*)");
        if (!path.isEmpty()) {
            QFile file(path);
            if (!file.exists()) {
                if (file.open(QIODevice::WriteOnly | QIODevice::Text))
                    file.close();
                else {
                    QMessageBox::warning(&propDlg, "Ошибка", "Не удалось создать новый файл по указанному пути.");
                    return;
                }
            }
            currentCsvPath = path;
            pathEdit->setText(path);
            editItemsBtn->setEnabled(true);
        }
    });

    struct DropdownData { QStringList items; } mutableData;
    for (const QString &opt : field->options) {
        if (!opt.startsWith("path:"))
            mutableData.items << opt;
    }

    connect(editItemsBtn, &QPushButton::clicked, [this, &propDlg, &currentCsvPath, &mutableData]() {
        if (mutableData.items.isEmpty() && QFile::exists(currentCsvPath)) {
            DataSource ds;
            if (ds.loadFromCsv(currentCsvPath))
                mutableData.items = ds.items();
        }
        QDialog listDlg(&propDlg);
        listDlg.setWindowTitle("Редактор элементов списка");
        listDlg.setMinimumSize(350, 300);
        QVBoxLayout *lLayout = new QVBoxLayout(&listDlg);
        QListWidget *listWidget = new QListWidget(&listDlg);
        listWidget->addItems(mutableData.items);
        lLayout->addWidget(listWidget);
        QHBoxLayout *bLayout = new QHBoxLayout();
        QPushButton *addBtn = new QPushButton("Добавить", &listDlg);
        QPushButton *editBtn = new QPushButton("Изменить", &listDlg);
        QPushButton *delBtn = new QPushButton("Удалить", &listDlg);
        bLayout->addWidget(addBtn);
        bLayout->addWidget(editBtn);
        bLayout->addWidget(delBtn);
        lLayout->addLayout(bLayout);
        QDialogButtonBox *lBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &listDlg);
        lLayout->addWidget(lBox);

        connect(addBtn, &QPushButton::clicked, [&listDlg, listWidget]() {
            bool ok;
            QString text = QInputDialog::getText(&listDlg, "Новый элемент", "Введите значение:", QLineEdit::Normal, "", &ok);
            if (ok && !text.trimmed().isEmpty())
                listWidget->addItem(text.trimmed());
        });
        connect(editBtn, &QPushButton::clicked, [&listDlg, listWidget]() {
            QListWidgetItem *curr = listWidget->currentItem();
            if (!curr) return;
            bool ok;
            QString text = QInputDialog::getText(&listDlg, "Редактирование", "Измените значение:", QLineEdit::Normal, curr->text(), &ok);
            if (ok && !text.trimmed().isEmpty())
                curr->setText(text.trimmed());
        });
        connect(delBtn, &QPushButton::clicked, [listWidget]() {
            delete listWidget->currentItem();
        });
        connect(lBox, &QDialogButtonBox::accepted, &listDlg, &QDialog::accept);
        connect(lBox, &QDialogButtonBox::rejected, &listDlg, &QDialog::reject);

        if (listDlg.exec() == QDialog::Accepted) {
            mutableData.items.clear();
            for (int i = 0; i < listWidget->count(); ++i)
                mutableData.items << listWidget->item(i)->text();
            QFile file(currentCsvPath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                for (const QString &item : mutableData.items)
                    out << item << "\n";
                file.close();
            } else {
                QMessageBox::warning(&propDlg, "Ошибка", "Не удалось сохранить изменения в файл.");
            }
        }
    });

    connect(buttonBox, &QDialogButtonBox::accepted, &propDlg, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &propDlg, &QDialog::reject);

    if (propDlg.exec() == QDialog::Accepted) {
        QString oldType = field->type;
        field->type = typeCombo->currentText();
        if (field->type == "dropdown") {
            QStringList finalOptions;
            if (!currentCsvPath.isEmpty())
                finalOptions << QString("path:%1").arg(currentCsvPath);
            if (mutableData.items.isEmpty() && QFile::exists(currentCsvPath)) {
                DataSource ds;
                if (ds.loadFromCsv(currentCsvPath))
                    mutableData.items = ds.items();
            }
            finalOptions << mutableData.items;
            field->options = finalOptions;
        } else {
            field->options.clear();
            if (oldType != field->type)
                runInlineEditor(field);
        }
    } else {
        if (field->type != "dropdown" && field->type != "container")
            runInlineEditor(field);
    }
    update();
}

void TemplateEditor::updateActiveEditorGeometry()
{
    if (m_activeEditor && m_editingFieldIndex != -1) {
        TemplateField *field = fieldAt(m_editingFieldIndex);
        if (field) {
            m_activeEditor->setGeometry(toDisplayRect(field->geometry));
        }
    }
}

void TemplateEditor::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0)
            m_zoomFactor = qBound(0.2, m_zoomFactor + 0.1, 3.0);
        else
            m_zoomFactor = qBound(0.2, m_zoomFactor - 0.1, 3.0);
        emit zoomChanged(m_zoomFactor);
        updatePixmapRect();
        updateActiveEditorGeometry();
        update();
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

void TemplateEditor::contextMenuEvent(QContextMenuEvent *event)
{
    showContextMenu(event->pos());
}

void TemplateEditor::showContextMenu(const QPoint &pos)
{
    if (!m_template || m_fillMode) return;
    QMenu menu(this);
    if (m_activeEditor && m_editingFieldIndex != -1) {
        QAction *undoAct = menu.addAction("Отменить (Undo)");
        QAction *redoAct = menu.addAction("Повторить (Redo)");
        menu.addSeparator();
        QAction *cutAct = menu.addAction("Вырезать текст");
        QAction *copyTextAct = menu.addAction("Копировать текст");
        QAction *pasteTextAct = menu.addAction("Вставить текст");

        undoAct->setEnabled(m_activeEditor->document()->isUndoAvailable());
        redoAct->setEnabled(m_activeEditor->document()->isRedoAvailable());
        cutAct->setEnabled(m_activeEditor->textCursor().hasSelection());
        copyTextAct->setEnabled(m_activeEditor->textCursor().hasSelection());

        QAction *selected = menu.exec(QCursor::pos());
        if (!selected) return;
        if (selected == undoAct) m_activeEditor->undo();
        else if (selected == redoAct) m_activeEditor->redo();
        else if (selected == cutAct) m_activeEditor->cut();
        else if (selected == copyTextAct) m_activeEditor->copy();
        else if (selected == pasteTextAct) m_activeEditor->paste();
        return;
    }

    int clicked = fieldAtPos(pos);
    if (clicked == -1) {
        QAction *pasteFieldAct = menu.addAction("Вставить поле сюда");
        pasteFieldAct->setEnabled(m_hasBufferData);
        QAction *selected = menu.exec(QCursor::pos());
        if (selected == pasteFieldAct && m_hasBufferData) {
            TemplateField newField = m_copyBuffer;
            newField.name = generateUniqueFieldName();
            newField.geometry = toRelativeRect(QRect(pos, m_copyBuffer.geometry.size()));
            TemplatePage *page = m_template->page(m_currentPageIndex);
            if (page) {
                page->fields.append(newField);
                m_selectedIndex = page->fields.size() - 1;
                update();
            }
        }
        return;
    }

    m_selectedIndex = clicked;
    update();

    QAction *copyFieldAct = menu.addAction("Копировать поле");
    QAction *cutFieldAct = menu.addAction("Вырезать поле");
    QAction *deleteFieldAct = menu.addAction("Удалить поле");

    QAction *selectedAction = menu.exec(QCursor::pos());
    if (selectedAction == copyFieldAct) {
        m_copyBuffer = *fieldAt(clicked);
        m_hasBufferData = true;
    } else if (selectedAction == cutFieldAct) {
        m_copyBuffer = *fieldAt(clicked);
        m_hasBufferData = true;
        TemplatePage *page = m_template->page(m_currentPageIndex);
        if (page) page->fields.removeAt(clicked);
        m_selectedIndex = -1;
        update();
    } else if (selectedAction == deleteFieldAct) {
        TemplatePage *page = m_template->page(m_currentPageIndex);
        if (page) page->fields.removeAt(clicked);
        m_selectedIndex = -1;
        m_dragIndex = -1;
        m_hoverIndex = -1;
        update();
    }
}

void TemplateEditor::copyField(int index)
{
    if (!m_template || index < 0) return;
    TemplateField *src = fieldAt(index);
    if (!src) return;
    TemplateField newField = *src;
    newField.name = generateUniqueFieldName();
    newField.geometry.translate(20, 20);
    TemplatePage *page = m_template->page(m_currentPageIndex);
    if (page) {
        page->fields.append(newField);
        m_selectedIndex = page->fields.size() - 1;
        update();
    }
}

bool TemplateEditor::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_activeEditor && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            m_editingFieldIndex = -1;
            finishInlineEditing();
            return true;
        }
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) &&
            !(keyEvent->modifiers() & Qt::ShiftModifier)) {
            if (m_editingFieldIndex != -1 && m_template) {
                TemplateField *f = fieldAt(m_editingFieldIndex);
                if (f && f->type != "container") {
                    finishInlineEditing();
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TemplateEditor::finishInlineEditing()
{
    if (!m_activeEditor) return;
    if (m_editingFieldIndex != -1 && m_template) {
        TemplateField *field = fieldAt(m_editingFieldIndex);
        if (field)
            field->defaultValue = m_activeEditor->toPlainText();
    }
    m_activeEditor->deleteLater();
    m_activeEditor = nullptr;
    m_editingFieldIndex = -1;
    m_highlightedFields.clear();
    setFocus();
    update();
}

void TemplateEditor::updateInlineEditorHighlights()
{
    if (!m_activeEditor || !m_template) return;
    m_highlightedFields.clear();
    QString text = m_activeEditor->toPlainText();
    static const QVector<QColor> excelPalette = {
        QColor(0, 120, 215), QColor(16, 124, 16), QColor(180, 0, 0),
        QColor(135, 100, 184), QColor(227, 111, 30), QColor(0, 164, 239)
    };
    int colorIdx = 0;
    QRegularExpression re("F(\\d+)");
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString fieldName = match.captured(0);
        TemplatePage *page = m_template->page(m_currentPageIndex);
        if (!page) continue;
        const auto &fieldsList = page->fields;
        for (int i = 0; i < fieldsList.size(); ++i) {
            if (fieldsList[i].name == fieldName) {
                if (!m_highlightedFields.contains(i)) {
                    m_highlightedFields[i] = excelPalette[colorIdx % excelPalette.size()];
                    colorIdx++;
                }
                break;
            }
        }
    }
    update();
}

void TemplateEditor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete && m_selectedIndex != -1 && m_template && !m_fillMode) {
        TemplatePage *page = m_template->page(m_currentPageIndex);
        if (page && m_selectedIndex >= 0 && m_selectedIndex < page->fields.size())
            page->fields.removeAt(m_selectedIndex);
        m_selectedIndex = -1;
        m_dragIndex = -1;
        m_hoverIndex = -1;
        m_dragMode = None;
        update();
    } else if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_C) {
        if (m_selectedIndex != -1 && m_template) {
            TemplateField *f = fieldAt(m_selectedIndex);
            if (f) m_copyBuffer = *f;
            m_hasBufferData = true;
        }
    } else if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_V) {
        if (m_hasBufferData)
            copyField(m_selectedIndex);
    } else {
        QWidget::keyPressEvent(event);
    }
}

QString TemplateEditor::evaluateFormulas(const QString &text) const
{
    QString result = text;
    int startIdx = -1;
    while ((startIdx = result.indexOf('~')) != -1) {
        int endIdx = result.indexOf('~', startIdx + 1);
        if (endIdx == -1) break;
        QString formula = result.mid(startIdx + 1, endIdx - startIdx - 1);
        QString evaluated = calculateFormula(formula);
        result.replace(startIdx, endIdx - startIdx + 1, evaluated);
    }
    return result;
}

QString TemplateEditor::calculateFormula(QString formula) const
{
    if (!m_template) return "0";
    TemplatePage *page = m_template->page(m_currentPageIndex);
    if (!page) return "0";
    QVector<TemplateField> fields = page->fields;
    std::sort(fields.begin(), fields.end(), [](const TemplateField &a, const TemplateField &b) {
        return a.name.length() > b.name.length();
    });
    for (const auto &field : fields) {
        if (field.name.isEmpty()) continue;
        if (formula.contains(field.name)) {
            double totalValue = 0.0;
            QStringList numbers = field.defaultValue.split(QRegularExpression("[\\s,;\n\r\t]+"), Qt::SkipEmptyParts);
            for (const QString &numStr : numbers) {
                bool ok;
                QString cleanNum = numStr;
                cleanNum.replace(',', '.');
                double val = cleanNum.toDouble(&ok);
                if (ok) totalValue += val;
            }
            formula.replace(field.name, QString::number(totalValue, 'f', 6));
        }
    }
    formula.remove(' ');
    bool ok = false;
    int pos = 0;
    double res = parseResult(formula, pos, ok);
    if (ok && pos == formula.length())
        return QString::number(res, 'g', 10);
    else
        return "[Ошибка расчета]";
}

double TemplateEditor::parseResult(const QString &str, int &pos, bool &ok) const
{
    double result = parseTerm(str, pos, ok);
    while (pos < str.length()) {
        QChar op = str[pos];
        if (op != '+' && op != '-') break;
        pos++;
        double nextTerm = parseTerm(str, pos, ok);
        if (op == '+') result += nextTerm;
        else result -= nextTerm;
    }
    return result;
}

double TemplateEditor::parseTerm(const QString &str, int &pos, bool &ok) const
{
    double result = parseFactor(str, pos, ok);
    while (pos < str.length()) {
        QChar op = str[pos];
        if (op != '*' && op != '/') break;
        pos++;
        double nextFactor = parseFactor(str, pos, ok);
        if (op == '*') result *= nextFactor;
        else {
            if (nextFactor == 0) { ok = false; return 0; }
            result /= nextFactor;
        }
    }
    return result;
}

double TemplateEditor::parseFactor(const QString &str, int &pos, bool &ok) const
{
    if (pos >= str.length()) { ok = false; return 0; }
    bool negative = false;
    if (str[pos] == '-') {
        negative = true;
        pos++;
    } else if (str[pos] == '+') {
        pos++;
    }
    double result = 0;
    if (str.mid(pos).startsWith("sin(")) {
        pos += 4;
        double arg = parseResult(str, pos, ok);
        if (pos < str.length() && str[pos] == ')') { pos++; result = std::sin(arg); } else ok = false;
    }
    else if (str.mid(pos).startsWith("cos(")) {
        pos += 4;
        double arg = parseResult(str, pos, ok);
        if (pos < str.length() && str[pos] == ')') { pos++; result = std::cos(arg); } else ok = false;
    }
    else if (str.mid(pos).startsWith("round(")) {
        pos += 6;
        double arg = parseResult(str, pos, ok);
        if (pos < str.length() && str[pos] == ')') { pos++; result = std::round(arg); } else ok = false;
    }
    else if (str.mid(pos).startsWith("pow(")) {
        pos += 4;
        double base = parseResult(str, pos, ok);
        if (pos < str.length() && str[pos] == ',') {
            pos++;
            double exponent = parseResult(str, pos, ok);
            if (pos < str.length() && str[pos] == ')') { pos++; result = std::pow(base, exponent); } else ok = false;
        } else ok = false;
    }
    else if (str.mid(pos).startsWith("max(")) {
        pos += 4;
        double arg1 = parseResult(str, pos, ok);
        if (pos < str.length() && str[pos] == ',') {
            pos++;
            double arg2 = parseResult(str, pos, ok);
            if (pos < str.length() && str[pos] == ')') { pos++; result = std::max(arg1, arg2); } else ok = false;
        } else ok = false;
    }
    else if (str.mid(pos).startsWith("min(")) {
        pos += 4;
        double arg1 = parseResult(str, pos, ok);
        if (pos < str.length() && str[pos] == ',') {
            pos++;
            double arg2 = parseResult(str, pos, ok);
            if (pos < str.length() && str[pos] == ')') { pos++; result = std::min(arg1, arg2); } else ok = false;
        } else ok = false;
    }
    else if (str[pos] == '(') {
        pos++;
        result = parseResult(str, pos, ok);
        if (pos < str.length() && str[pos] == ')') {
            pos++;
        } else {
            ok = false;
        }
    }
    else {
        int start = pos;
        while (pos < str.length() && (str[pos].isDigit() || str[pos] == '.'))
            pos++;
        QString numStr = str.mid(start, pos - start);
        result = numStr.toDouble(&ok);
    }
    return negative ? -result : result;
}

QString TemplateEditor::evaluateTextExpressions(const QString &text) const
{
    QString result = text;
    int start = 0;
    while (true) {
        int left = result.indexOf('*', start);
        if (left == -1) break;
        int right = result.indexOf('*', left + 1);
        if (right == -1) break;

        QString inner = result.mid(left + 1, right - left - 1);
        QString evaluated = evaluateTextExpression(inner);
        result.replace(left, right - left + 1, evaluated);
        start = left + evaluated.length();
    }
    return result;
}

QString TemplateEditor::evaluateTextExpression(const QString &expr) const
{
    QString withFields = replaceFieldNames(expr);
    return evaluateTextFormula(withFields);
}

QString TemplateEditor::replaceFieldNames(const QString &text) const
{
    if (!m_template) return text;
    TemplatePage *page = m_template->page(m_currentPageIndex);
    if (!page) return text;

    QString result = text;
    QVector<TemplateField> fields = page->fields;
    std::sort(fields.begin(), fields.end(),
              [](const TemplateField &a, const TemplateField &b) {
                  return a.name.length() > b.name.length();
              });

    for (const TemplateField &field : fields) {
        if (field.name.isEmpty()) continue;
        QString pattern = QString("\\b%1\\b").arg(QRegularExpression::escape(field.name));
        QRegularExpression re(pattern);
        QString value = field.defaultValue;
        if (field.type == "number" && value.isEmpty()) value = "0";
        if (field.type != "number") {
            QString escaped = value;
            escaped.replace("\\", "\\\\").replace("\"", "\\\"");
            value = "\"" + escaped + "\"";
        }

        result.replace(re, value);
    }
    return result;
}

static int findMatchingParen(const QString &str, int openPos)
{
    if (openPos < 0 || openPos >= str.length() || str[openPos] != '(')
        return -1;
    int count = 1;
    bool inQuote = false;
    for (int i = openPos + 1; i < str.length(); ++i) {
        QChar ch = str[i];
        if (ch == '"') inQuote = !inQuote;
        if (!inQuote) {
            if (ch == '(') count++;
            else if (ch == ')') {
                count--;
                if (count == 0) return i;
            }
        }
    }
    return -1;
}

bool TemplateEditor::parseFunctionCall(const QString &expr, const QString &funcName, QStringList &args) const
{
    QString trimmed = expr.trimmed();
    if (!trimmed.startsWith(funcName, Qt::CaseInsensitive))
        return false;

    int pos = funcName.length();
    while (pos < trimmed.length() && trimmed[pos].isSpace()) {
        pos++;
    }

    if (pos >= trimmed.length() || trimmed[pos] != '(')
        return false;

    pos++;
    int startPos = pos;
    int depth = 1;
    int endPos = -1;
    while (pos < trimmed.length() && depth > 0) {
        if (trimmed[pos] == '(') {
            depth++;
        } else if (trimmed[pos] == ')') {
            depth--;
            if (depth == 0) {
                endPos = pos;
                break;
            }
        }
        pos++;
    }

    if (endPos == -1 || depth > 0)
        return false;
    QString body = trimmed.mid(startPos, endPos - startPos);
    args.clear();
    int last = 0;
    int currentDepth = 0;
    for (int j = 0; j < body.length(); ++j) {
        QChar ch = body[j];
        if (ch == '(') {
            currentDepth++;
        } else if (ch == ')') {
            currentDepth--;
        } else if (ch == ',' && currentDepth == 0) {
            args << body.mid(last, j - last).trimmed();
            last = j + 1;
        }
    }
    args << body.mid(last).trimmed();

    return true;
}

QString TemplateEditor::evaluateTextFormula(const QString &formula) const
{
    QString expr = formula.trimmed();
    qDebug() << "evaluateTextFormula получила:" << expr;

    QStringList ifArgs;
    if (parseFunctionCall(expr, "IF", ifArgs) && ifArgs.size() == 3) {
        int pos = 0;
        bool condOk = parseCondition(ifArgs[0], pos);
        QString thenPart = ifArgs[1];
        QString elsePart = ifArgs[2];
        thenPart = evaluateTextFormula(thenPart);
        elsePart = evaluateTextFormula(elsePart);
        return condOk ? thenPart : elsePart;
    }
    return expr;
}

bool TemplateEditor::splitArgs(const QString &str, int &pos, QStringList &args) const
{
    if (pos >= str.length() || str[pos] != '(')
        return false;
    pos++;
    int parenLevel = 1;
    int argStart = pos;
    while (pos < str.length() && parenLevel > 0) {
        if (str[pos] == '(') parenLevel++;
        else if (str[pos] == ')') parenLevel--;
        else if (str[pos] == ',' && parenLevel == 1) {
            args.append(str.mid(argStart, pos - argStart).trimmed());
            argStart = pos + 1;
        }
        pos++;
    }
    if (parenLevel != 0) return false;
    args.append(str.mid(argStart, pos - argStart - 1).trimmed());
    return true;
}

void TemplateEditor::skipWhitespace(const QString &expr, int &pos) const
{
    while (pos < expr.length() && expr[pos].isSpace())
        ++pos;
}

bool TemplateEditor::parseCondition(const QString &expr, int &pos) const
{
    return parseOr(expr, pos);
}

bool TemplateEditor::parseOr(const QString &expr, int &pos) const
{
    bool result = parseAnd(expr, pos);
    skipWhitespace(expr, pos);
    while (pos < expr.length()) {
        if (expr.mid(pos, 2) == "||" || expr.mid(pos, 2).toLower() == "or") {
            pos += 2;
            skipWhitespace(expr, pos);
            bool right = parseAnd(expr, pos);
            result = result || right;
            skipWhitespace(expr, pos);
        } else {
            break;
        }
    }
    return result;
}

bool TemplateEditor::parseAnd(const QString &expr, int &pos) const
{
    bool result = parseComparison(expr, pos);
    skipWhitespace(expr, pos);
    while (pos < expr.length()) {
        if (expr.mid(pos, 2) == "&&" || expr.mid(pos, 3).toLower() == "and") {
            int len = (expr.mid(pos, 2) == "&&") ? 2 : 3;
            pos += len;
            skipWhitespace(expr, pos);
            bool right = parseComparison(expr, pos);
            result = result && right;
            skipWhitespace(expr, pos);
        } else {
            break;
        }
    }
    return result;
}

bool TemplateEditor::parseComparison(const QString &expr, int &pos) const
{
    skipWhitespace(expr, pos);
    if (pos < expr.length() && expr[pos] == '(') {
        pos++;
        skipWhitespace(expr, pos);
        bool result = parseCondition(expr, pos);
        skipWhitespace(expr, pos);
        if (pos < expr.length() && expr[pos] == ')') {
            pos++;
        } else {
            return false;
        }
        return result;
    }

    QString left = parseAtomic(expr, pos);
    skipWhitespace(expr, pos);
    if (pos >= expr.length()) {
        return !left.isEmpty();
    }

    QString op;
    if (expr.mid(pos, 2) == "==") { op = "=="; pos += 2; }
    else if (expr.mid(pos, 2) == "!=") { op = "!="; pos += 2; }
    else if (expr.mid(pos, 2) == "<=") { op = "<="; pos += 2; }
    else if (expr.mid(pos, 2) == ">=") { op = ">="; pos += 2; }
    else if (expr.mid(pos, 1) == "<") { op = "<"; pos += 1; }
    else if (expr.mid(pos, 1) == ">") { op = ">"; pos += 1; }
    else {
        return !left.isEmpty();
    }

    skipWhitespace(expr, pos);
    QString right = parseAtomic(expr, pos);
    skipWhitespace(expr, pos);
    auto unquote = [](const QString &s) -> QString {
        QString t = s.trimmed();
        if (t.startsWith('"') && t.endsWith('"') && t.length() >= 2)
            return t.mid(1, t.length() - 2);
        return t;
    };

    QString leftVal = unquote(left);
    QString rightVal = unquote(right);

    bool leftOk = false, rightOk = false;
    double leftNum = leftVal.toDouble(&leftOk);
    double rightNum = rightVal.toDouble(&rightOk);

    if (op == "==") return leftVal == rightVal;
    if (op == "!=") return leftVal != rightVal;
    if (leftOk && rightOk) {
        if (op == "<")  return leftNum < rightNum;
        if (op == ">")  return leftNum > rightNum;
        if (op == "<=") return leftNum <= rightNum;
        if (op == ">=") return leftNum >= rightNum;
    } else {
        if (op == "<")  return leftVal < rightVal;
        if (op == ">")  return leftVal > rightVal;
        if (op == "<=") return leftVal <= rightVal;
        if (op == ">=") return leftVal >= rightVal;
    }
    return false;
}

QString TemplateEditor::parseAtomic(const QString &expr, int &pos) const
{
    skipWhitespace(expr, pos);
    if (pos >= expr.length()) return QString();
    if (expr[pos] == '"') {
        int start = pos;
        pos++;
        while (pos < expr.length() && expr[pos] != '"') {
            pos++;
        }
        QString result = expr.mid(start, pos - start + 1);
        if (pos < expr.length() && expr[pos] == '"') pos++;
        return result;
    }
    int start = pos;
    while (pos < expr.length() && !expr[pos].isSpace() &&
           expr[pos] != '(' && expr[pos] != ')' &&
           !(expr.mid(pos, 2) == "||") && !(expr.mid(pos, 2).toLower() == "or") &&
           !(expr.mid(pos, 2) == "&&") && !(expr.mid(pos, 3).toLower() == "and") &&
           !(expr.mid(pos, 2) == "==") && !(expr.mid(pos, 2) == "!=") &&
           !(expr.mid(pos, 2) == "<=") && !(expr.mid(pos, 2) == ">=") &&
           expr[pos] != '<' && expr[pos] != '>') {
        pos++;
    }
    return expr.mid(start, pos - start);
}