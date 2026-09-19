#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("PDF Merge"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QCoreApplication::setOrganizationName(QStringLiteral("PDFMerge"));
    QApplication::setStyle(QStringLiteral("Fusion"));
    QApplication::setDesktopFileName(QStringLiteral("org.pdfmerge"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Arrange PDF pages and export one document."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("files"), QStringLiteral("PDF files to add."),
                                 QStringLiteral("[files…]"));
    parser.process(app);

    MainWindow window;
    window.show();
    if (!parser.positionalArguments().isEmpty()) {
        QTimer::singleShot(0, &window, [&window, paths = parser.positionalArguments()] {
            window.importFiles(paths);
        });
    }
    return app.exec();
}
