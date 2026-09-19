#include "documenttree.h"
#include "assemblymodel.h"

#include <QDrag>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QPainter>
#include <QPolygonF>
#include <QScrollBar>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QUrl>

namespace {
class RowDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        auto size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(40);
        return size;
    }
};
}

DocumentTree::DocumentTree(AssemblyModel* model, QWidget* parent)
    : QTreeView(parent), assembly_(model)
{
    setModel(model);
    setHeaderHidden(true);
    setIndentation(22);
    setUniformRowHeights(true);
    setAnimated(false);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setDropIndicatorShown(false);
    setAcceptDrops(true);
    setDragEnabled(true);
    setAutoScroll(true);
    setAutoScrollMargin(36);
    setAutoExpandDelay(650);
    setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::DoubleClicked);
    setExpandsOnDoubleClick(false);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTextElideMode(Qt::ElideMiddle);
    setItemDelegate(new RowDelegate(this));
    setAccessibleName(tr("Output document order"));
    setToolTip(tr("Drag pages onto a group to insert them. Drag near the left edge to place them outside a group."));

    connect(model, &QAbstractItemModel::modelAboutToBeReset, this, &DocumentTree::rememberView);
    connect(model, &QAbstractItemModel::modelReset, this, &DocumentTree::restoreView);
    connect(this, &QTreeView::expanded, this, &DocumentTree::updateContentHeight);
    connect(this, &QTreeView::collapsed, this, &DocumentTree::updateContentHeight);
}

void DocumentTree::rememberView()
{
    selection_ = currentIndex().data(Qt::UserRole).toUuid();
    scroll_ = verticalScrollBar()->value();
    for (int row = 0; row < assembly_->rowCount(); ++row) {
        const auto index = assembly_->index(row, 0);
        const auto id = index.data(Qt::UserRole).toUuid();
        if (isExpanded(index))
            expanded_.insert(id);
        else
            expanded_.remove(id);
    }
}

void DocumentTree::restoreView()
{
    for (int row = 0; row < assembly_->rowCount(); ++row) {
        const auto index = assembly_->index(row, 0);
        if (!assembly_->item(index)->group)
            continue;
        const auto id = index.data(Qt::UserRole).toUuid();
        if (expanded_.contains(id))
            expand(index);
    }
    setCurrentIndex(assembly_->find(selection_));
    verticalScrollBar()->setValue(scroll_);
    updateContentHeight();
}

void DocumentTree::updateContentHeight()
{
    qint64 rows = assembly_->rowCount();
    for (int row = 0; row < assembly_->rowCount(); ++row) {
        const auto index = assembly_->index(row, 0);
        if (isExpanded(index))
            rows += assembly_->rowCount(index);
    }
    setMaximumHeight(static_cast<int>(qMin<qint64>(16'000'000, qMax<qint64>(1, rows) * 40 + 4)));
}

void DocumentTree::drawRow(QPainter* painter, const QStyleOptionViewItem& option,
                           const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QRectF row = QRectF(0, option.rect.top(), viewport()->width(), option.rect.height())
                           .adjusted(1.5, 1.5, -1.5, -1.5);
    const bool selected = selectionModel()->isSelected(index);
    const bool focused = hasFocus() && currentIndex() == index;
    painter->setPen(focused ? QPen(palette().color(QPalette::Highlight), 1) : QPen(Qt::NoPen));
    painter->setBrush(selected ? property("selectionColor").value<QColor>()
                              : option.state.testFlag(QStyle::State_MouseOver)
                                  ? palette().color(QPalette::Window) : QColor(Qt::transparent));
    painter->drawRoundedRect(row, 4, 4);

    // Paint the label without a second selection/focus background. Qt's usual
    // drawRow path paints the branch area and the item as separate panels.
    auto content = option;
    content.rect = visualRect(index);
    content.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus | QStyle::State_MouseOver);
    content.backgroundBrush = Qt::NoBrush;
    content.showDecorationSelected = false;
    itemDelegateForIndex(index)->paint(painter, content, index);

    // Draw the disclosure indicator directly: the stylesheet can suppress
    // PE_IndicatorBranch even when State_Children is set.
    if (model()->hasChildren(index)) {
        const bool rtl = layoutDirection() == Qt::RightToLeft;
        const qreal x = rtl ? content.rect.right() + indentation() / 2.0
                            : content.rect.left() - indentation() / 2.0;
        const qreal y = option.rect.top() + option.rect.height() / 2.0;
        QPolygonF arrow;
        if (isExpanded(index)) {
            arrow << QPointF(x - 4, y - 2) << QPointF(x + 4, y - 2) << QPointF(x, y + 3);
        } else {
            const qreal direction = rtl ? -1 : 1;
            arrow << QPointF(x - 2 * direction, y - 4)
                  << QPointF(x - 2 * direction, y + 4)
                  << QPointF(x + 3 * direction, y);
        }
        painter->setPen(Qt::NoPen);
        painter->setBrush(palette().color(QPalette::PlaceholderText));
        painter->drawPolygon(arrow);
    }
    painter->restore();
}

void DocumentTree::startDrag(Qt::DropActions)
{
    if (!currentIndex().isValid())
        return;
    // The model performs the move itself, so QAbstractItemView must not remove
    // the original row after exec() returns.
    QDrag drag(this);
    drag.setMimeData(assembly_->mimeData({currentIndex()}));
    drag.exec(Qt::MoveAction, Qt::MoveAction);
}

void DocumentTree::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }
    QTreeView::dragEnterEvent(event);
}

DocumentTree::Target DocumentTree::dropTarget(const QPoint& position) const
{
    const auto hovered = indexAt(position);
    if (!hovered.isValid()) {
        auto last = assembly_->index(assembly_->rowCount() - 1, 0);
        if (isExpanded(last) && assembly_->rowCount(last) > 0)
            last = assembly_->index(assembly_->rowCount(last) - 1, 0, last);
        const int y = last.isValid() ? visualRect(last).bottom() + 2 : 2;
        return {{}, assembly_->rowCount(), QRect(12, y, viewport()->width() - 24, 2), false};
    }

    const auto rect = visualRect(hovered);
    const auto* entry = assembly_->item(hovered);
    const bool leftEdge = position.x() < indentation() + 12;
    if (entry->group && !leftEdge && position.y() > rect.top() + 9
        && position.y() < rect.bottom() - 9) {
        return {hovered, assembly_->rowCount(hovered), rect.adjusted(1, 1, -2, -1), true};
    }

    auto owner = hovered.parent();
    int row = hovered.row();
    int y = rect.top();
    if (leftEdge && owner.isValid()) {
        row = owner.row() + 1;
        const auto last = assembly_->index(assembly_->rowCount(owner) - 1, 0, owner);
        y = visualRect(last).bottom();
        owner = {};
    } else if (position.y() >= rect.center().y()) {
        ++row;
        y = rect.bottom();
        if (entry->group && isExpanded(hovered) && assembly_->rowCount(hovered) > 0) {
            const auto last = assembly_->index(assembly_->rowCount(hovered) - 1, 0, hovered);
            y = visualRect(last).bottom();
        }
    }
    const int x = owner.isValid() ? indentation() * 2 : 12;
    return {owner, row, QRect(x, y, viewport()->width() - x - 12, 2), false};
}

void DocumentTree::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        showTarget_ = false;
        event->acceptProposedAction();
    } else {
        QTreeView::dragMoveEvent(event); // Retain Qt's edge autoscrolling.
        target_ = dropTarget(event->position().toPoint());
        showTarget_ = assembly_->canDropMimeData(event->mimeData(), Qt::MoveAction,
                                                target_.row, 0, target_.parent);
        if (showTarget_) {
            event->setDropAction(Qt::MoveAction);
            event->accept();
        } else {
            event->ignore();
        }
    }
    viewport()->update();
}

void DocumentTree::dragLeaveEvent(QDragLeaveEvent* event)
{
    showTarget_ = false;
    viewport()->update();
    QTreeView::dragLeaveEvent(event);
}

void DocumentTree::dropEvent(QDropEvent* event)
{
    showTarget_ = false;
    if (event->mimeData()->hasUrls()) {
        QStringList paths;
        for (const auto& url : event->mimeData()->urls()) {
            if (url.isLocalFile())
                paths.append(url.toLocalFile());
        }
        event->acceptProposedAction();
        emit filesDropped(paths);
    } else {
        const auto target = dropTarget(event->position().toPoint());
        if (assembly_->dropMimeData(event->mimeData(), Qt::MoveAction, target.row, 0, target.parent)) {
            event->setDropAction(Qt::MoveAction);
            event->accept();
        } else {
            event->ignore();
        }
    }
    setState(QAbstractItemView::NoState);
    viewport()->update();
}

void DocumentTree::paintEvent(QPaintEvent* event)
{
    QTreeView::paintEvent(event);
    if (!showTarget_)
        return;
    QPainter painter(viewport());
    const auto accent = palette().color(QPalette::Highlight);
    painter.setPen(QPen(accent, 2));
    if (target_.inside) {
        painter.drawRoundedRect(target_.marker, 4, 4);
    } else {
        painter.drawLine(target_.marker.topLeft(), target_.marker.topRight());
        painter.setBrush(palette().color(QPalette::Base));
        painter.drawRect(QRect(target_.marker.left() - 3, target_.marker.top() - 4, 6, 8));
    }
}
