#include "preview.h"

#include <QPainter>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

Preview::Preview(QWidget* parent) : QAbstractScrollArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(tr("Merged PDF preview"));
    verticalScrollBar()->setSingleStep(48);
    horizontalScrollBar()->setSingleStep(48);
}

int Preview::currentPage() const
{
    if (rects_.empty())
        return -1;
    const int y = verticalScrollBar()->value() + std::min(viewport()->height() / 3, 180);
    const auto found = std::lower_bound(rects_.begin(), rects_.end(), y,
        [](const QRect& rect, int position) { return rect.bottom() < position; });
    return std::min(static_cast<int>(found - rects_.begin()), static_cast<int>(rects_.size()) - 1);
}

void Preview::setPages(std::vector<Page> pages)
{
    const int current = currentPage();
    const auto anchor = current >= 0 ? pages_[current].id : QUuid{};
    pages_ = std::move(pages);
    layoutPages();
    jumpTo(anchor);
}

void Preview::layoutPages()
{
    rects_.clear();
    double widest = 1;
    for (const auto& page : pages_) {
        const auto size = page.source->document.pagePointSize(page.number);
        widest = std::max(widest, std::clamp(size.width(), 1.0, 14400.0));
    }
    if (fit_)
        zoom_ = std::max(0.05, (viewport()->width() - 64.0) / widest);
    const int canvasWidth = std::max(viewport()->width(), static_cast<int>(widest * zoom_) + 64);
    int y = 28;
    for (const auto& page : pages_) {
        const auto points = page.source->document.pagePointSize(page.number);
        const int width = std::max(1, static_cast<int>(std::clamp(points.width(), 1.0, 14400.0) * zoom_));
        const int height = std::max(1, static_cast<int>(std::clamp(points.height(), 1.0, 14400.0) * zoom_));
        rects_.emplace_back((canvasWidth - width) / 2, y, width, height);
        y += height + 28;
    }
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setRange(0, std::max(0, y - viewport()->height()));
    horizontalScrollBar()->setPageStep(viewport()->width());
    horizontalScrollBar()->setRange(0, std::max(0, canvasWidth - viewport()->width()));
    viewport()->update();
    emit currentPageChanged(currentPage() + 1, static_cast<int>(pages_.size()));
    emit zoomChanged(qRound(zoom_ * 100), fit_);
}

void Preview::jumpTo(const QUuid& pageId)
{
    for (int index = 0; index < static_cast<int>(pages_.size()); ++index) {
        if (pages_[index].id == pageId) {
            verticalScrollBar()->setValue(rects_[index].top() - 28);
            return;
        }
    }
}

void Preview::stepPage(int offset)
{
    if (pages_.empty())
        return;
    const int index = std::clamp(currentPage() + offset, 0, static_cast<int>(pages_.size()) - 1);
    jumpTo(pages_[index].id);
}

void Preview::zoomBy(double factor)
{
    const int current = currentPage();
    fit_ = false;
    zoom_ = std::clamp(zoom_ * factor, 0.2, 3.0);
    layoutPages();
    if (current >= 0)
        jumpTo(pages_[current].id);
}

void Preview::fitWidth()
{
    const int current = currentPage();
    fit_ = true;
    layoutPages();
    if (current >= 0)
        jumpTo(pages_[current].id);
}

void Preview::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    layoutPages();
}

void Preview::scrollContentsBy(int, int)
{
    viewport()->update();
    emit currentPageChanged(currentPage() + 1, static_cast<int>(pages_.size()));
}

void Preview::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        zoomBy(event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15);
        event->accept();
    } else {
        QAbstractScrollArea::wheelEvent(event);
    }
}

QSize Preview::renderSize(const QRect& rect) const
{
    QSizeF size = QSizeF(rect.size()) * devicePixelRatioF();
    // Very large paper formats and high DPI must not consume unbounded memory.
    const double pixels = size.width() * size.height();
    if (pixels > 8'000'000)
        size *= std::sqrt(8'000'000 / pixels);
    return size.toSize().expandedTo(QSize(1, 1));
}

QString Preview::cacheKey(const Page& page, const QSize& size) const
{
    return QStringLiteral("%1/%2/%3x%4").arg(page.source->id.toString())
        .arg(page.number).arg(size.width()).arg(size.height());
}

void Preview::render(const Page& page, const QSize& size, const QString& key)
{
    if (!pending_.isEmpty())
        return;
    auto source = page.source;
    if (!connected_.contains(source->id)) {
        connected_.insert(source->id);
        connect(&source->renderer, &QPdfPageRenderer::pageRendered, this,
                [this](int, QSize, const QImage& image, QPdfDocumentRenderOptions, quint64) {
            // There is at most one outstanding request, including obsolete zoom
            // requests. This bounds the renderer queue while scrolling quickly.
            const auto key = pending_;
            pending_.clear();
            if (!image.isNull()) {
                const int cost = static_cast<int>((image.sizeInBytes() + 1023) / 1024);
                cache_.insert(key, new QImage(image), cost);
            } else {
                cache_.insert(key, new QImage, 1);
            }
            // Keep the sender alive through signal delivery even if its source
            // was removed and the undo history no longer references it.
            auto finishedSource = std::move(renderingSource_);
            QTimer::singleShot(0, this, [finishedSource] {});
            viewport()->update();
        });
    }
    pending_ = key;
    renderingSource_ = source;
    source->renderer.requestPage(page.number, size);
}

void Preview::paintEvent(QPaintEvent*)
{
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), palette().color(QPalette::Window));
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QPoint offset(horizontalScrollBar()->value(), verticalScrollBar()->value());
    const QRect visible(offset, viewport()->size());
    auto first = std::lower_bound(rects_.begin(), rects_.end(), visible.top(),
        [](const QRect& rect, int y) { return rect.bottom() < y; });
    for (auto it = first; it != rects_.end() && it->top() <= visible.bottom(); ++it) {
        const int index = static_cast<int>(it - rects_.begin());
        const auto rect = it->translated(-offset);
        painter.fillRect(rect.translated(0, 2).adjusted(-1, -1, 1, 1), QColor(0, 0, 0, 22));
        painter.fillRect(rect, Qt::white);
        const auto size = renderSize(*it);
        const auto key = cacheKey(pages_[index], size);
        if (const auto* image = cache_.object(key)) {
            if (!image->isNull()) {
                painter.drawImage(rect, *image);
            } else {
                painter.setPen(QColor("#65676E"));
                painter.drawText(rect, Qt::AlignCenter, tr("Could not preview this page"));
            }
        } else {
            painter.setPen(QColor("#65676E"));
            painter.drawText(rect, Qt::AlignCenter, tr("Loading page %1…").arg(index + 1));
            render(pages_[index], size, key);
        }
    }
}
