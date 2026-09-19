#pragma once

#include "source.h"
#include <QAbstractScrollArea>
#include <QCache>
#include <QSet>
#include <vector>

class Preview : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit Preview(QWidget* parent = nullptr);
    void setPages(std::vector<Page> pages);
    void jumpTo(const QUuid& pageId);
    void stepPage(int offset);
    void zoomBy(double factor);
    void fitWidth();

signals:
    void currentPageChanged(int page, int total);
    void zoomChanged(int percent, bool fit);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void layoutPages();
    int currentPage() const;
    void render(const Page& page, const QSize& size, const QString& key);
    QSize renderSize(const QRect& rect) const;
    QString cacheKey(const Page& page, const QSize& size) const;

    std::vector<Page> pages_;
    std::vector<QRect> rects_;
    QCache<QString, QImage> cache_{64 * 1024}; // KiB, bounded independently of page count.
    QSet<QUuid> connected_;
    std::shared_ptr<Source> renderingSource_;
    QString pending_;
    double zoom_ = 1.0;
    bool fit_ = true;
};
