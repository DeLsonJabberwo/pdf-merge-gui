#pragma once

#include <QTreeView>
#include <QSet>
#include <QUuid>

class AssemblyModel;

class DocumentTree : public QTreeView {
    Q_OBJECT
public:
    explicit DocumentTree(AssemblyModel* model, QWidget* parent = nullptr);

signals:
    void filesDropped(const QStringList& paths);

protected:
    void drawRow(QPainter* painter, const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;
    void startDrag(Qt::DropActions actions) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    struct Target {
        QModelIndex parent;
        int row = 0;
        QRect marker;
        bool inside = false;
    };
    Target dropTarget(const QPoint& position) const;
    void rememberView();
    void restoreView();
    void updateContentHeight();

    AssemblyModel* assembly_;
    Target target_;
    bool showTarget_ = false;
    QSet<QUuid> expanded_;
    QUuid selection_;
    int scroll_ = 0;
};
