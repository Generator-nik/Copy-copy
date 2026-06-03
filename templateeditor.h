#ifndef TEMPLATEEDITOR_H
#define TEMPLATEEDITOR_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QFont>
#include <QTextEdit>
#include <QMap>
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
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

    int currentPageIndex() const { return m_currentPageIndex; }

    void zoomIn() { m_zoomFactor = qBound(0.2, m_zoomFactor + 0.1, 3.0); emit zoomChanged(m_zoomFactor); update(); }
    void zoomOut() { m_zoomFactor = qBound(0.2, m_zoomFactor - 0.1, 3.0); emit zoomChanged(m_zoomFactor); update(); }
    double zoomFactor() const { return m_zoomFactor; }
    void updateActiveEditorGeometry();

    int fieldAtPos(const QPoint &pos) const;
    bool isInResizeZone(const QPoint &pos, const QRect &displayRect) const;
    QRect toDisplayRect(const QRect &relativeRect) const;
    QRect toRelativeRect(const QRect &displayRect) const;
    void updatePixmapRect();
    QString generateUniqueFieldName() const;
    void updateInlineEditorHighlights();
    void copyField(int index);
    void setCurrentPage(int index);
    TemplateField *fieldAt(int index);

    QString evaluateFormulas(const QString &text) const;
    QString calculateFormula(QString formula) const;
    double parseResult(const QString &str, int &pos, bool &ok) const;
    double parseTerm(const QString &str, int &pos, bool &ok) const;
    double parseFactor(const QString &str, int &pos, bool &ok) const;

    QString evaluateTextExpressions(const QString &text) const;
    QString evaluateTextExpression(const QString &expr) const;
    QString evaluateTextFormula(const QString &formula) const;
    bool parseFunctionCall(const QString &expr, const QString &funcName, QStringList &args) const;
    QString replaceFieldNames(const QString &text) const;
    bool splitArgs(const QString &str, int &pos, QStringList &args) const;
    bool parseCondition(const QString &expr, int &pos) const;
    bool parseOr(const QString &expr, int &pos) const;
    bool parseAnd(const QString &expr, int &pos) const;
    bool parseComparison(const QString &expr, int &pos) const;
    QString parseAtomic(const QString &expr, int &pos) const;
    void skipWhitespace(const QString &expr, int &pos) const;
    QString expandFormulasInString(const QString &str) const;

public slots:
    void applyCurrentFormat(const QFont &font, const QColor &color);
    void addNewPage();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void finishInlineEditing();
    void showContextMenu(const QPoint &pos);
    void prevPage();
    void nextPage();
    void updatePageLabel();

private:
    QHBoxLayout *m_topLayout;
    QPushButton *m_prevBtn;
    QPushButton *m_nextBtn;
    QPushButton *m_addPageBtn;
    QLabel *m_pageLabel;

    TemplateField m_copyBuffer;
    bool m_hasBufferData;

    Template *m_template;
    QPixmap m_background;
    QRect m_pixmapRect;

    int m_currentPageIndex;

    int m_dragIndex;
    enum DragMode { None, Move, Resize };
    DragMode m_dragMode;
    QPoint m_dragStart;
    QRect m_dragStartRect;

    bool m_addingField;
    bool m_isDrawingNewField;
    QPoint m_addStart;
    QRect m_currentRect;

    int m_hoverIndex;
    bool m_fillMode;
    int m_selectedIndex;

    QFont m_currentFont;
    QColor m_currentColor;

    QTextEdit *m_activeEditor;
    int m_editingFieldIndex;
    QMap<int, QColor> m_highlightedFields;

    double m_zoomFactor;

    bool evaluateCondition(const QString &cond) const;

    void generateBlankA4();

signals:
    void zoomChanged(double zoom);
};

#endif