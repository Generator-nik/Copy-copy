#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialog>
#include <QTextEdit>
#include "templateeditor.h"
#include "datasource.h"
#include <QPainter>
#include <QFile>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QResizeEvent>
#include <QFontMetrics>

TemplateEditor::TemplateEditor(QWidget *parent)
    : QWidget(parent), m_template(nullptr), m_dragIndex(-1),
      m_addingField(false), m_hoverIndex(-1), m_fillMode(false), m_selectedIndex(-1)
{
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::white);
    setPalette(pal);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    m_currentFont = QFont("Arial", 12);
    m_currentColor = Qt::black;
}

void TemplateEditor::setTemplate(Template *tmpl)
{
    m_template = tmpl;
    m_dragIndex = -1;
    m_addingField = false;
    m_hoverIndex = -1;
    m_fillMode = false;
    m_selectedIndex = -1;
    updatePixmapRect();
    update();
}

Template *TemplateEditor::currentTemplate() const
{
    return m_template;
}

void TemplateEditor::setBackgroundImage(const QString &path)
{
    if (QFile::exists(path)) {
        m_background.load(path);
        if (m_background.isNull()) {
            QMessageBox::warning(this, "Ошибка", "Не удалось загрузить изображение.");
        }
        updatePixmapRect();
        update();
    }
}

void TemplateEditor::setAddFieldMode(bool enable)
{
    m_addingField = enable;
    if (enable) {
        m_fillMode = false;
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

bool TemplateEditor::isAddingField() const
{
    return m_addingField;
}

void TemplateEditor::setFillMode(bool enable)
{
    m_fillMode = enable;
    m_addingField = false;
    m_dragIndex = -1;
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
    QVector<TemplateField> fields = m_template->fields();
    for (int i = fields.size() - 1; i >= 0; --i) {
        QRect displayRect = toDisplayRect(fields[i].geometry);
        if (displayRect.contains(pos)) return i;
    }
    return -1;
}

QRect TemplateEditor::toDisplayRect(const QRect &relativeRect) const
{
    if (m_pixmapRect.isEmpty() || m_background.isNull()) return relativeRect;
    double scaleX = (double)m_pixmapRect.width() / m_background.width();
    double scaleY = (double)m_pixmapRect.height() / m_background.height();
    return QRect(
        m_pixmapRect.x() + (int)(relativeRect.x() * scaleX),
        m_pixmapRect.y() + (int)(relativeRect.y() * scaleY),
        (int)(relativeRect.width() * scaleX),
        (int)(relativeRect.height() * scaleY)
    );
}

QRect TemplateEditor::toRelativeRect(const QRect &displayRect) const
{
    if (m_pixmapRect.isEmpty() || m_background.isNull()) return displayRect;
    double scaleX = (double)m_background.width() / m_pixmapRect.width();
    double scaleY = (double)m_background.height() / m_pixmapRect.height();
    return QRect(
        (int)((displayRect.x() - m_pixmapRect.x()) * scaleX),
        (int)((displayRect.y() - m_pixmapRect.y()) * scaleY),
        (int)(displayRect.width() * scaleX),
        (int)(displayRect.height() * scaleY)
    );
}

void TemplateEditor::updatePixmapRect()
{
    if (m_background.isNull()) {
        m_pixmapRect = QRect();
        return;
    }
    QSize size = m_background.size();
    size.scale(width(), height(), Qt::KeepAspectRatio);
    int x = (width() - size.width()) / 2;
    int y = (height() - size.height()) / 2;
    m_pixmapRect = QRect(x, y, size.width(), size.height());
}

void TemplateEditor::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updatePixmapRect();
    update();
}

void TemplateEditor::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    QPainter painter(this);

    painter.fillRect(rect(), Qt::white);

    if (!m_background.isNull() && m_pixmapRect.isValid()) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawPixmap(m_pixmapRect, m_background);
    }

    if (!m_template) return;

    QVector<TemplateField> fields = m_template->fields();
    for (int i = 0; i < fields.size(); ++i) {
        const TemplateField &field = fields[i];
        QRect displayRect = toDisplayRect(field.geometry);

        if (i == m_selectedIndex) {
            painter.setBrush(QColor(255, 255, 255, 200));
            painter.setPen(QPen(Qt::blue, 2));
            painter.drawRect(displayRect);
        } else if (i == m_hoverIndex) {
            painter.setBrush(QColor(255, 255, 255, 200));
            painter.setPen(QPen(Qt::darkGray, 1));
            painter.drawRect(displayRect);
        }

        QFont font = field.font;
        if (font.family().isEmpty()) {
            font = m_currentFont;
        }
        painter.setFont(font);
        painter.setPen(Qt::black);

        if (field.type == "container") {
            QFont boldFont = font;
            boldFont.setBold(true);
            painter.setFont(boldFont);
            QFontMetrics fm(boldFont);
            int nameY = displayRect.top() + fm.ascent() + 2;

            if (field.options.contains("collapsed")) {
                painter.drawText(displayRect.left() + 2, nameY, field.name + " [...]");
            } else {
                painter.drawText(displayRect.left() + 2, nameY, field.name);
                if (!field.defaultValue.isEmpty()) {
                    painter.setFont(font);
                    QFontMetrics fm2(font);
                    int textY = nameY + fm2.height() + 4;
                    QRect textRect(displayRect.left() + 2, textY, displayRect.width() - 4, displayRect.bottom() - textY);
                    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, field.defaultValue);
                }
            }
        } else {
            QString label = field.name;
            if (!field.defaultValue.isEmpty()) {
                if (!label.isEmpty()) label += "\n";
                label += field.defaultValue;
            }
            QFontMetrics fm(font);
            int textY = displayRect.top() + fm.ascent() + 2;
            painter.drawText(displayRect.left() + 2, textY, label);
        }
    }

    if (m_addingField && m_currentRect.isValid()) {
        painter.setBrush(QColor(0, 0, 255, 40));
        painter.setPen(QPen(Qt::blue, 2, Qt::DashLine));
        painter.drawRect(m_currentRect);
    }
}

void TemplateEditor::applyCurrentFormat(const QFont &font, const QColor &color)
{
    m_currentFont = font;
    m_currentColor = color;

    if (m_selectedIndex != -1 && m_template) {
        TemplateField *field = m_template->fieldAt(m_selectedIndex);
        if (field) {
            field->font = font;
        }
    }
    update();
}

void TemplateEditor::mousePressEvent(QMouseEvent *event)
{
    if (!m_template || m_fillMode) return;

    if (m_addingField) {
        m_addStart = event->pos();
        m_currentRect = QRect();
    } else {
        int clicked = fieldAtPos(event->pos());
        if (clicked != -1) {
            m_selectedIndex = clicked;
            m_dragIndex = clicked;
            m_dragStart = event->pos();
            m_dragStartRect = toDisplayRect(m_template->fields()[clicked].geometry);
            setCursor(Qt::SizeAllCursor);
            update();
        } else {
            m_selectedIndex = -1;
            m_dragIndex = -1;
            setCursor(Qt::ArrowCursor);
            update();
        }
    }
}

void TemplateEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_template || m_fillMode) return;

    if (m_addingField) {
        int x = qMin(m_addStart.x(), event->pos().x());
        int y = qMin(m_addStart.y(), event->pos().y());
        int w = qAbs(event->pos().x() - m_addStart.x());
        int h = qAbs(event->pos().y() - m_addStart.y());
        m_currentRect = QRect(x, y, w, h);
        update();
    } else if (m_dragIndex != -1) {
        int dx = event->pos().x() - m_dragStart.x();
        int dy = event->pos().y() - m_dragStart.y();
        QRect newDisplayRect = m_dragStartRect.translated(dx, dy);

        TemplateField *field = m_template->fieldAt(m_dragIndex);
        if (field) {
            field->geometry = toRelativeRect(newDisplayRect);
        }
        update();
    } else {
        int hover = fieldAtPos(event->pos());
        if (hover != m_hoverIndex) {
            m_hoverIndex = hover;
            update();
        }
    }
}

void TemplateEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_template || m_fillMode) return;

    if (m_addingField && m_currentRect.isValid() && m_currentRect.width() > 10 && m_currentRect.height() > 10) {
        bool ok = false;
        QString fieldName = QInputDialog::getText(this, "Новое поле", "Введите имя поля (можно пустое):",
                                                  QLineEdit::Normal, "", &ok);
        if (ok) {
            TemplateField newField;
            newField.name = fieldName;
            newField.geometry = toRelativeRect(m_currentRect);
            newField.type = "text";
            newField.font = m_currentFont;
            m_template->addField(newField);
        }
        m_currentRect = QRect();
        update();
    } else if (m_dragIndex != -1) {
        m_dragIndex = -1;
        setCursor(Qt::ArrowCursor);
        update();
    }

    m_currentRect = QRect();
}

void TemplateEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_template) return;

    int idx = fieldAtPos(event->pos());
    if (idx == -1) return;

    TemplateField *field = m_template->fieldAt(idx);
    if (!field) return;

    if (m_fillMode) {
        if (field->type == "dropdown") {
            bool ok;
            QString selected = QInputDialog::getItem(this, "Заполнить поле",
                "Выберите значение:", field->options, 0, false, &ok);
            if (ok) {
                field->defaultValue = selected;
                update();
            }
        } else if (field->type == "container") {
            QDialog *dialog = new QDialog(this);
            dialog->setWindowTitle("Заполнить: " + field->name);
            dialog->resize(500, 400);

            QVBoxLayout *layout = new QVBoxLayout(dialog);
            QLabel *label = new QLabel("Введите текст:");
            QTextEdit *textEdit = new QTextEdit();
            textEdit->setPlainText(field->defaultValue);
            QPushButton *okButton = new QPushButton("OK");
            QPushButton *cancelButton = new QPushButton("Отмена");

            layout->addWidget(label);
            layout->addWidget(textEdit);

            QHBoxLayout *buttonLayout = new QHBoxLayout();
            buttonLayout->addStretch();
            buttonLayout->addWidget(okButton);
            buttonLayout->addWidget(cancelButton);
            layout->addLayout(buttonLayout);

            connect(okButton, &QPushButton::clicked, dialog, &QDialog::accept);
            connect(cancelButton, &QPushButton::clicked, dialog, &QDialog::reject);

            if (dialog->exec() == QDialog::Accepted) {
                field->defaultValue = textEdit->toPlainText();
                update();
            }
            delete dialog;
        } else {
            bool ok;
            QString text = QInputDialog::getText(this, "Заполнить поле",
                "Введите значение:", QLineEdit::Normal, field->defaultValue, &ok);
            if (ok) {
                field->defaultValue = text;
                update();
            }
        }
        return;
    }

    if (event->modifiers() & Qt::ControlModifier) {
        bool ok;
        QString newName = QInputDialog::getText(this, "Переименовать поле", "Новое имя:",
                                                QLineEdit::Normal, field->name, &ok);
        if (ok && !newName.isEmpty()) {
            field->name = newName;
        }
    } else {
        if (field->type == "container") {
            if (field->options.contains("collapsed")) {
                field->options.removeAll("collapsed");
            } else {
                field->options.append("collapsed");
            }
            update();
            return;
        }

        QStringList types = {"text", "formula", "dropdown", "container"};
        bool ok;
        QString newType = QInputDialog::getItem(this, "Тип поля", "Выберите тип:",
                                                types, types.indexOf(field->type), false, &ok);
        if (ok && !newType.isEmpty()) {
            field->type = newType;

            if (newType == "dropdown") {
                QString csvPath = QFileDialog::getOpenFileName(this, "Выберите CSV-файл со списком",
                                                               QString(), "CSV (*.csv)");
                if (!csvPath.isEmpty()) {
                    DataSource ds;
                    if (ds.loadFromCsv(csvPath)) {
                        field->options = ds.items();
                        QMessageBox::information(this, "Готово",
                            QString("Загружено %1 вариантов:\n%2")
                                .arg(field->options.size())
                                .arg(field->options.join(", ")));
                    } else {
                        QMessageBox::warning(this, "Ошибка", "Не удалось загрузить CSV.");
                    }
                }
            }
        }

        QString newDefault = QInputDialog::getText(this, "Значение по умолчанию",
                                                   "Введите значение:", QLineEdit::Normal,
                                                   field->defaultValue, &ok);
        if (ok) {
            field->defaultValue = newDefault;
        }
    }

    update();
}

void TemplateEditor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete && m_selectedIndex != -1 && m_template && !m_fillMode) {
        m_template->removeFieldByIndex(m_selectedIndex);
        m_selectedIndex = -1;
        m_dragIndex = -1;
        m_hoverIndex = -1;
        update();
    } else if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_C) {
        if (m_selectedIndex != -1 && m_template) {
            m_copyBuffer = m_template->fields()[m_selectedIndex];
        }
    } else if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_V) {
        if (!m_copyBuffer.name.isEmpty() || !m_copyBuffer.defaultValue.isEmpty() || m_copyBuffer.geometry.isValid()) {
            TemplateField newField = m_copyBuffer;
            newField.geometry.translate(20, 20);
            m_template->addField(newField);
            m_selectedIndex = m_template->fields().size() - 1;
            update();
        }
    } else {
        QWidget::keyPressEvent(event);
    }
}
