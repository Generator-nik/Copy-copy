#ifndef TEMPLATEEDITOR_H
#define TEMPLATEEDITOR_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QFont>
#include "template.h"

class TemplateEditor : public QWidget
{
    Q_OBJECT

public:
    explicit TemplateEditor(QWidget *parent = nullptr);

    void setTemplate(Template *tmpl);
    Template *currentTemplate() const;
    void setBackgroundImage(const QString &path);

    void setAddFieldMode(bool enable);
    bool isAddingField() const;

    void setFillMode(bool enable);
    bool isFillMode() const;

public slots:
    void applyCurrentFormat(const QFont &font, const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    TemplateField m_copyBuffer;

    Template *m_template;
    QPixmap m_background;
    QRect m_pixmapRect;

    int m_dragIndex;
    QPoint m_dragStart;
    QRect m_dragStartRect;

    bool m_addingField;
    QPoint m_addStart;
    QRect m_currentRect;

    int m_hoverIndex;
    bool m_fillMode;
    int m_selectedIndex;

    QFont m_currentFont;
    QColor m_currentColor;

    int fieldAtPos(const QPoint &pos) const;
    QFont fontForRect(const QRect &rect, const QFont &baseFont) const;
    QRect toDisplayRect(const QRect &relativeRect) const;
    QRect toRelativeRect(const QRect &displayRect) const;
    void updatePixmapRect();
};

#endif
