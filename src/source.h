#pragma once

#include <QPdfDocument>
#include <QPdfPageRenderer>
#include <QTemporaryDir>
#include <QUuid>
#include <memory>

// Each import owns a private copy. Preview and export always read the same bytes,
// even if the original file is changed or removed while the app is open.
struct Source {
    Source();

    QUuid id = QUuid::createUuid();
    QString originalPath;
    QString name;
    QString password;
    QTemporaryDir storage;
    QPdfDocument document;
    QPdfPageRenderer renderer;

    QString snapshotPath() const;
};

struct Page {
    QUuid id = QUuid::createUuid();
    std::shared_ptr<Source> source;
    int number = 0;
};
