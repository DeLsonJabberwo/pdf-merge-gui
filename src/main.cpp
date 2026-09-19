#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QEvent>
#include <QTimer>

#ifdef PDFMERGE_PACKAGING
int checkPackage();
#endif

#ifdef Q_OS_MACOS
#include <QFileOpenEvent>
#include <functional>

namespace {

// LaunchServices passes documents opened from Finder to the running
// application as events instead of command-line arguments, so route them
// through the same import path.
class FileOpenFilter : public QObject {
public:
    explicit FileOpenFilter(std::function<void(const QString&)> handler,
                            QObject* parent = nullptr)
        : QObject(parent), handler_(std::move(handler))
    {
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::FileOpen) {
            const QString path = static_cast<QFileOpenEvent*>(event)->file();
            if (!path.isEmpty())
                handler_(path);
        }
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void(const QString&)> handler_;
};

} // namespace
#endif

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("PDF Merge"));
    QCoreApplication::setApplicationVersion(QStringLiteral(PDFMERGE_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("PDFMerge"));
    QApplication::setStyle(QStringLiteral("Fusion"));
    QApplication::setDesktopFileName(QStringLiteral("org.pdfmerge"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Arrange PDF pages and export one document."));
    parser.addHelpOption();
    parser.addVersionOption();
#ifdef PDFMERGE_PACKAGING
    parser.addOption({QStringLiteral("check-package"),
                      QStringLiteral("Check PDF rendering and export, then exit.")});
#endif
    parser.addPositionalArgument(QStringLiteral("files"), QStringLiteral("PDF files to add."),
                                 QStringLiteral("[files…]"));
    parser.process(app);

#ifdef PDFMERGE_PACKAGING
    if (parser.isSet(QStringLiteral("check-package")))
        return checkPackage();
#endif

    MainWindow window;
    window.show();
#ifdef Q_OS_MACOS
    FileOpenFilter fileOpenFilter([&window](const QString& path) {
        window.importFiles({path});
    });
    app.installEventFilter(&fileOpenFilter);
#endif
    if (!parser.positionalArguments().isEmpty()) {
        QTimer::singleShot(0, &window, [&window, paths = parser.positionalArguments()] {
            window.importFiles(paths);
        });
    }
    return app.exec();
}
