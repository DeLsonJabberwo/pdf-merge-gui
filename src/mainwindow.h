#pragma once

#include "export.h"
#include "source.h"
#include <QFutureWatcher>
#include <QMainWindow>

class AssemblyModel;
class DocumentTree;
class Preview;
class QLabel;
class QPushButton;
class QCheckBox;
class QStackedWidget;
class QSplitter;
class QAction;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    void importFiles(const QStringList& paths);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void buildMenus();
    void browse();
    void refreshAssembly();
    void exportDocument();
    void applyTheme();
    void setTheme(const QString& theme);

    AssemblyModel* assembly_;
    DocumentTree* tree_ = nullptr;
    Preview* preview_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QSplitter* splitter_ = nullptr;
    QLabel* pageLabel_ = nullptr;
    QLabel* countLabel_ = nullptr;
    QLabel* zoomLabel_ = nullptr;
    QPushButton* exportButton_ = nullptr;
    QCheckBox* openAfter_ = nullptr;
    QAction* exportAction_ = nullptr;
    QAction* removeAction_ = nullptr;
    QFutureWatcher<ExportResult>* exportWatcher_;
    std::shared_ptr<ExportProgress> progress_;
    std::vector<Page> exportSources_;
    QString theme_;
    bool closeAfterExport_ = false;
};
