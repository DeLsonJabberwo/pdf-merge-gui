#pragma once

#include <QString>
#include <QStringList>
#include <atomic>
#include <memory>
#include <vector>

struct ExportPage {
    QString path;
    QString password;
    int number;
};

struct ExportProgress {
    std::atomic<bool> cancelled{false};
    std::atomic<int> percent{0};
};

struct ExportResult {
    QString error;
    QStringList warnings;
    bool cancelled = false;
};

ExportResult exportPdf(const std::vector<ExportPage>& pages, const QString& destination,
                       const std::shared_ptr<ExportProgress>& progress);
