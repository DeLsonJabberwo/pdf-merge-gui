#include "../src/export.h"

#include <QDebug>
#include <QImage>
#include <QPageSize>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QTemporaryDir>

// Run inside the deployed application so it exercises exactly the libraries
// and plugins that users receive, rather than a separate test executable.
int checkPackage()
{
    QTemporaryDir directory;
    if (!directory.isValid()) {
        qCritical() << "Could not create the package-check directory.";
        return 1;
    }
    const auto input = directory.filePath(QStringLiteral("input.pdf"));
    const auto output = directory.filePath(QStringLiteral("output.pdf"));
    {
        QPdfWriter writer(input);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        if (!painter.isActive())
            return 1;
        const QRect page(0, 0, writer.width(), writer.height());
        painter.fillRect(page, Qt::red);
        if (!writer.newPage())
            return 1;
        painter.fillRect(page, Qt::blue);
    }

    auto progress = std::make_shared<ExportProgress>();
    const auto result = exportPdf({{input, {}, 1}, {input, {}, 0}}, output, progress);
    if (!result.error.isEmpty() || result.cancelled) {
        qCritical() << "Package export check failed:" << result.error;
        return 1;
    }

    QPdfDocument document;
    if (document.load(output) != QPdfDocument::Error::None || document.pageCount() != 2) {
        qCritical() << "Could not reopen the exported PDF.";
        return 1;
    }
    for (int page = 0; page != 2; ++page) {
        const auto image = document.render(page, QSize(100, 140));
        if (image.isNull())
            return 1;
        const auto color = image.pixelColor(50, 70);
        if ((page == 0 && (color.blue() < 200 || color.red() > 50))
            || (page == 1 && (color.red() < 200 || color.blue() > 50))) {
            qCritical() << "Package rendering/page-order check failed on page" << page;
            return 1;
        }
    }
    qInfo() << "Package check passed: PDF creation, export, page order, and rendering.";
    return 0;
}
