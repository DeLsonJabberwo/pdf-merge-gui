#include "mainwindow.h"
#include "assemblymodel.h"
#include "documenttree.h"
#include "preview.h"

#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyleHints>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrentRun>

namespace {
QPushButton* button(const QString& text, const QString& accessibleName = {})
{
    auto* result = new QPushButton(text);
    result->setCursor(Qt::PointingHandCursor);
    result->setAccessibleName(accessibleName.isEmpty() ? text : accessibleName);
    return result;
}

QLabel* muted(const QString& text)
{
    auto* label = new QLabel(text);
    label->setObjectName(QStringLiteral("muted"));
    return label;
}

QString pdfError(QPdfDocument::Error error)
{
    switch (error) {
    case QPdfDocument::Error::IncorrectPassword:
        return QObject::tr("The password is incorrect.");
    case QPdfDocument::Error::UnsupportedSecurityScheme:
        return QObject::tr("This PDF uses unsupported encryption.");
    case QPdfDocument::Error::InvalidFileFormat:
        return QObject::tr("This file is not a readable PDF.");
    default:
        return QObject::tr("The PDF could not be opened.");
    }
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), assembly_(new AssemblyModel(this)),
      exportWatcher_(new QFutureWatcher<ExportResult>(this))
{
    setWindowTitle(tr("PDF Merge"));
    setMinimumSize(760, 520);
    resize(1100, 760);
    setAcceptDrops(true);
    buildUi();
    buildMenus();

    QSettings settings;
    theme_ = settings.value(QStringLiteral("theme"), QStringLiteral("system")).toString();
    restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());
    splitter_->restoreState(settings.value(QStringLiteral("splitter")).toByteArray());
    openAfter_->setChecked(settings.value(QStringLiteral("openAfterExport"), false).toBool());
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, &MainWindow::applyTheme);
    connect(assembly_, &AssemblyModel::assemblyChanged, this, &MainWindow::refreshAssembly);
    connect(assembly_->undoStack(), &QUndoStack::cleanChanged, this, [this](bool clean) {
        setWindowTitle(clean ? tr("PDF Merge") : tr("PDF Merge • Modified"));
    });
    applyTheme();
    refreshAssembly();
}

MainWindow::~MainWindow()
{
    if (progress_)
        progress_->cancelled.store(true);
    exportWatcher_->waitForFinished();
}

void MainWindow::buildUi()
{
    stack_ = new QStackedWidget;
    setCentralWidget(stack_);
    auto* empty = button(QString{}, tr("Add PDF files"));
    empty->setObjectName(QStringLiteral("emptyState"));
    auto* emptyLayout = new QVBoxLayout(empty);
    emptyLayout->addStretch();
    auto* plus = new QLabel(QStringLiteral("+"));
    plus->setObjectName(QStringLiteral("emptyPlus"));
    plus->setAlignment(Qt::AlignCenter);
    auto* prompt = new QLabel(tr("Drop PDFs here"));
    prompt->setObjectName(QStringLiteral("emptyPrompt"));
    prompt->setAlignment(Qt::AlignCenter);
    auto* hint = muted(tr("or click to browse"));
    hint->setAlignment(Qt::AlignCenter);
    for (auto* label : {plus, prompt, hint}) {
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
        emptyLayout->addWidget(label);
    }
    emptyLayout->addStretch();
    connect(empty, &QPushButton::clicked, this, &MainWindow::browse);
    stack_->addWidget(empty);

    splitter_ = new QSplitter(Qt::Horizontal);
    splitter_->setChildrenCollapsible(false);
    auto* main = new QWidget;
    auto* mainLayout = new QVBoxLayout(main);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    auto* toolbar = new QWidget;
    toolbar->setObjectName(QStringLiteral("toolbar"));
    auto* tools = new QHBoxLayout(toolbar);
    tools->setContentsMargins(16, 10, 16, 10);
    tools->setSpacing(8);
    auto* zoomOut = button(QStringLiteral("−"), tr("Zoom out"));
    auto* zoomIn = button(QStringLiteral("+"), tr("Zoom in"));
    zoomOut->setFixedWidth(34);
    zoomIn->setFixedWidth(34);
    zoomLabel_ = muted(QStringLiteral("100%"));
    zoomLabel_->setAlignment(Qt::AlignCenter);
    zoomLabel_->setMinimumWidth(54);
    auto* fit = button(tr("Fit width"));
    tools->addWidget(zoomOut);
    tools->addWidget(zoomLabel_);
    tools->addWidget(zoomIn);
    tools->addSpacing(8);
    tools->addWidget(fit);
    tools->addStretch();
    mainLayout->addWidget(toolbar);
    preview_ = new Preview;
    mainLayout->addWidget(preview_, 1);
    auto* navigation = new QHBoxLayout;
    navigation->setContentsMargins(16, 8, 16, 8);
    pageLabel_ = muted(QString{});
    auto* previous = button(QStringLiteral("‹"), tr("Previous page"));
    auto* next = button(QStringLiteral("›"), tr("Next page"));
    previous->setFixedWidth(34);
    next->setFixedWidth(34);
    navigation->addWidget(pageLabel_);
    navigation->addStretch();
    navigation->addWidget(previous);
    navigation->addWidget(next);
    mainLayout->addLayout(navigation);
    connect(zoomOut, &QPushButton::clicked, this, [this] { preview_->zoomBy(1.0 / 1.2); });
    connect(zoomIn, &QPushButton::clicked, this, [this] { preview_->zoomBy(1.2); });
    connect(fit, &QPushButton::clicked, preview_, &Preview::fitWidth);
    connect(previous, &QPushButton::clicked, this, [this] { preview_->stepPage(-1); });
    connect(next, &QPushButton::clicked, this, [this] { preview_->stepPage(1); });
    connect(preview_, &Preview::currentPageChanged, this,
            [this, previous, next](int page, int total) {
        pageLabel_->setText(tr("Page %1 of %2").arg(page).arg(total));
        previous->setEnabled(page > 1);
        next->setEnabled(page < total);
    });
    connect(preview_, &Preview::zoomChanged, this, [this](int percent, bool) {
        zoomLabel_->setText(tr("%1%").arg(percent));
    });

    auto* sidebar = new QWidget;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setMinimumWidth(260);
    auto* sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(14, 18, 14, 16);
    sideLayout->setSpacing(12);
    auto* heading = new QLabel(tr("Documents"));
    QFont headingFont = heading->font();
    headingFont.setWeight(QFont::DemiBold);
    heading->setFont(headingFont);
    sideLayout->addWidget(heading);
    tree_ = new DocumentTree(assembly_);
    sideLayout->addWidget(tree_, 1);
    auto* add = button(tr("＋ Add PDFs"));
    add->setObjectName(QStringLiteral("addButton"));
    sideLayout->addWidget(add);
    sideLayout->addStretch();
    auto* footer = new QWidget;
    footer->setObjectName(QStringLiteral("exportFooter"));
    auto* footerLayout = new QVBoxLayout(footer);
    footerLayout->setContentsMargins(0, 16, 0, 0);
    footerLayout->setSpacing(12);
    countLabel_ = muted(QString{});
    footerLayout->addWidget(countLabel_);
    openAfter_ = new QCheckBox(tr("Open after export"));
    footerLayout->addWidget(openAfter_);
    exportButton_ = button(tr("Export PDF"));
    exportButton_->setObjectName(QStringLiteral("primary"));
    exportButton_->setMinimumHeight(42);
    footerLayout->addWidget(exportButton_);
    sideLayout->addWidget(footer);
    connect(add, &QPushButton::clicked, this, &MainWindow::browse);
    connect(exportButton_, &QPushButton::clicked, this, &MainWindow::exportDocument);
    connect(tree_, &DocumentTree::filesDropped, this, &MainWindow::importFiles);
    connect(tree_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& index) {
        preview_->jumpTo(assembly_->firstPageId(index));
        if (removeAction_)
            removeAction_->setEnabled(index.isValid());
    });
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree_, &QWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const auto index = tree_->indexAt(position);
        if (!index.isValid())
            return;
        tree_->setCurrentIndex(index);
        QMenu menu(this);
        if (assembly_->item(index)->group) {
            menu.addAction(tr("Rename group"), this, [this, index] { tree_->edit(index); });
        } else if (index.parent().isValid()) {
            menu.addAction(tr("Move outside group"), this,
                           [this, index] { assembly_->makeStandalone(index); });
        }
        auto* up = menu.addAction(tr("Move up"), this,
                                 [this, index] { assembly_->moveBy(index, -1); });
        up->setEnabled(index.row() > 0);
        auto* down = menu.addAction(tr("Move down"), this,
                                   [this, index] { assembly_->moveBy(index, 1); });
        down->setEnabled(index.row() + 1 < assembly_->rowCount(index.parent()));
        menu.addSeparator();
        menu.addAction(removeAction_);
        menu.exec(tree_->viewport()->mapToGlobal(position));
    });
    splitter_->addWidget(main);
    splitter_->addWidget(sidebar);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 0);
    splitter_->setSizes({780, 320});
    stack_->addWidget(splitter_);
    statusBar()->setSizeGripEnabled(false);
}

void MainWindow::buildMenus()
{
    auto* file = menuBar()->addMenu(tr("&File"));
    auto* add = file->addAction(tr("&Add PDFs…"), this, &MainWindow::browse);
    add->setShortcut(QKeySequence::Open);
    exportAction_ = file->addAction(tr("&Export PDF…"), this, &MainWindow::exportDocument);
    exportAction_->setShortcut(QKeySequence::Save);
    file->addSeparator();
    auto* quit = file->addAction(tr("&Quit"), this, &QWidget::close);
    quit->setShortcut(QKeySequence::Quit);
    quit->setMenuRole(QAction::QuitRole);

    auto* edit = menuBar()->addMenu(tr("&Edit"));
    auto* undo = assembly_->undoStack()->createUndoAction(this, tr("&Undo"));
    undo->setShortcut(QKeySequence::Undo);
    auto* redo = assembly_->undoStack()->createRedoAction(this, tr("&Redo"));
    redo->setShortcuts(QKeySequence::Redo);
    edit->addAction(undo);
    edit->addAction(redo);
    edit->addSeparator();
    removeAction_ = edit->addAction(tr("&Remove selected"), this,
                                   [this] { assembly_->remove(tree_->currentIndex()); });
    removeAction_->setShortcut(QKeySequence::Delete);
    removeAction_->setEnabled(false);
    auto* up = edit->addAction(tr("Move up"), this,
                              [this] { assembly_->moveBy(tree_->currentIndex(), -1); });
    up->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
    auto* down = edit->addAction(tr("Move down"), this,
                                [this] { assembly_->moveBy(tree_->currentIndex(), 1); });
    down->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Down));
    auto* outside = edit->addAction(tr("Move outside group"), this,
                                   [this] { assembly_->makeStandalone(tree_->currentIndex()); });
    outside->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));

    auto* view = menuBar()->addMenu(tr("&View"));
    auto* appearance = view->addMenu(tr("Appearance"));
    auto* themes = new QActionGroup(this);
    const auto chosen = QSettings().value(QStringLiteral("theme"), QStringLiteral("system")).toString();
    for (const auto& pair : {std::pair{"system", "Follow system"},
                            std::pair{"light", "Light"}, std::pair{"dark", "Dark"}}) {
        auto* action = appearance->addAction(tr(pair.second));
        action->setCheckable(true);
        action->setChecked(chosen == QLatin1String(pair.first));
        themes->addAction(action);
        connect(action, &QAction::triggered, this, [this, name = QString::fromLatin1(pair.first)] {
            setTheme(name);
        });
    }
    view->addSeparator();
    view->addAction(tr("Zoom in"), this, [this] { preview_->zoomBy(1.2); })
        ->setShortcut(QKeySequence::ZoomIn);
    view->addAction(tr("Zoom out"), this, [this] { preview_->zoomBy(1.0 / 1.2); })
        ->setShortcut(QKeySequence::ZoomOut);
    view->addAction(tr("Fit width"), preview_, &Preview::fitWidth)
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));

    auto* help = menuBar()->addMenu(tr("&Help"));
    auto* about = help->addAction(tr("About PDF Merge"), this, [this] {
        QMessageBox::about(this, tr("About PDF Merge"),
            tr("<b>PDF Merge 0.1</b><p>Arrange pages. Export one PDF.</p>"
               "<p>Built with Qt Widgets, Qt PDF, and qpdf. All processing stays on your computer.</p>"
               "<p>Export preserves page content and copies annotations and form fields. "
               "Document bookmarks, attachments, and digital signatures are not preserved. "
               "Exports are not password protected.</p>"));
    });
    about->setMenuRole(QAction::AboutRole);
}

void MainWindow::browse()
{
    const auto directory = QSettings().value(QStringLiteral("importDirectory")).toString();
    importFiles(QFileDialog::getOpenFileNames(this, tr("Add PDFs"), directory, tr("PDF files (*.pdf)")));
}

void MainWindow::importFiles(const QStringList& paths)
{
    if (paths.isEmpty() || exportWatcher_->isRunning())
        return;
    std::vector<std::shared_ptr<Source>> sources;
    QStringList errors;
    for (const auto& path : paths) {
        const QFileInfo info(path);
        if (!info.isFile()) {
            errors.append(tr("%1: choose a PDF file, not a folder.").arg(info.fileName()));
            continue;
        }
        auto source = std::make_shared<Source>();
        source->originalPath = info.canonicalFilePath();
        source->name = info.fileName();
        if (!source->storage.isValid() || !QFile::copy(path, source->snapshotPath())) {
            errors.append(tr("%1: could not make a working copy. Check file permissions and free space.")
                          .arg(source->name));
            continue;
        }
        auto error = source->document.load(source->snapshotPath());
        bool skipped = false;
        while (error == QPdfDocument::Error::IncorrectPassword) {
            bool accepted = false;
            const auto password = QInputDialog::getText(this, tr("Password required"),
                tr("Enter the password for %1:").arg(source->name), QLineEdit::Password,
                QString{}, &accepted);
            if (!accepted) {
                skipped = true;
                break;
            }
            source->password = password;
            source->document.close();
            source->document.setPassword(password);
            error = source->document.load(source->snapshotPath());
            if (error == QPdfDocument::Error::IncorrectPassword)
                QMessageBox::information(this, tr("Incorrect password"), tr("Try a different password."));
        }
        if (skipped)
            continue;
        if (error != QPdfDocument::Error::None || source->document.pageCount() == 0) {
            errors.append(tr("%1: %2").arg(source->name,
                error == QPdfDocument::Error::None ? tr("The PDF contains no pages.") : pdfError(error)));
            continue;
        }
        QSettings().setValue(QStringLiteral("importDirectory"), info.absolutePath());
        sources.push_back(std::move(source));
    }
    assembly_->addSources(sources);
    if (!errors.isEmpty()) {
        QMessageBox message(QMessageBox::Warning, tr("Some files could not be added"),
                            errors.join(QStringLiteral("\n\n")), QMessageBox::Ok, this);
        message.setTextFormat(Qt::PlainText);
        message.exec();
    }
    if (!sources.empty())
        statusBar()->showMessage(tr("Added %1 PDF(s)").arg(sources.size()), 4000);
}

void MainWindow::refreshAssembly()
{
    auto pages = assembly_->pages();
    const auto count = pages.size();
    preview_->setPages(std::move(pages));
    stack_->setCurrentIndex(assembly_->rowCount() == 0 ? 0 : 1);
    countLabel_->setText(tr("%1 pages in output").arg(count));
    exportButton_->setEnabled(count > 0 && !exportWatcher_->isRunning());
    exportAction_->setEnabled(exportButton_->isEnabled());
    removeAction_->setEnabled(tree_->currentIndex().isValid());
}

void MainWindow::exportDocument()
{
    if (exportWatcher_->isRunning())
        return;
    const auto pages = assembly_->pages();
    if (pages.empty())
        return;
    QSettings settings;
    const auto directory = settings.value(QStringLiteral("exportDirectory"),
        settings.value(QStringLiteral("importDirectory"))).toString();
    QString destination = QFileDialog::getSaveFileName(this, tr("Export PDF"),
        directory + QStringLiteral("/Merged.pdf"), tr("PDF files (*.pdf)"));
    if (destination.isEmpty())
        return;
    if (!destination.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
        destination += QStringLiteral(".pdf");
        // The dialog did not ask about this path if it added a different suffix.
        if (QFileInfo::exists(destination) && QMessageBox::question(this, tr("Replace PDF?"),
            tr("%1 already exists. Replace it?").arg(QFileInfo(destination).fileName()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
    }
    const auto target = QFileInfo(destination).canonicalFilePath();
    for (const auto& page : pages) {
        if (!target.isEmpty() && target == page.source->originalPath) {
            QMessageBox::information(this, tr("Choose another filename"),
                                     tr("Export to a new file to keep your source PDF intact."));
            return;
        }
    }
    settings.setValue(QStringLiteral("exportDirectory"), QFileInfo(destination).absolutePath());
    settings.setValue(QStringLiteral("openAfterExport"), openAfter_->isChecked());
    std::vector<ExportPage> output;
    output.reserve(pages.size());
    for (const auto& page : pages)
        output.push_back({page.source->snapshotPath(), page.source->password, page.number});
    exportSources_ = pages; // Keep temporary inputs alive on the GUI thread.
    progress_ = std::make_shared<ExportProgress>();
    auto* dialog = new QProgressDialog(tr("Assembling PDF…"), tr("Cancel"), 0, 100, this);
    dialog->setWindowTitle(tr("Export PDF"));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setMinimumDuration(0);
    dialog->setAutoClose(false);
    dialog->setAutoReset(false);
    dialog->setValue(0);
    connect(dialog, &QProgressDialog::canceled, this, [progress = progress_] {
        progress->cancelled.store(true);
    });
    auto* timer = new QTimer(dialog);
    connect(timer, &QTimer::timeout, dialog, [dialog, progress = progress_] {
        dialog->setValue(progress->percent.load());
        if (progress->percent.load() >= 35)
            dialog->setLabelText(QObject::tr("Writing PDF…"));
    });
    timer->start(100);
    const bool openAfter = openAfter_->isChecked();
    connect(exportWatcher_, &QFutureWatcher<ExportResult>::finished, dialog,
            [this, dialog, timer, destination, openAfter] {
        timer->stop();
        dialog->hide();
        dialog->deleteLater();
        const auto result = exportWatcher_->result();
        exportSources_.clear();
        progress_.reset();
        refreshAssembly();
        if (!result.error.isEmpty()) {
            QMessageBox message(QMessageBox::Critical, tr("Export failed"), result.error,
                                QMessageBox::Ok, this);
            message.setTextFormat(Qt::PlainText);
            message.exec();
        } else if (result.cancelled) {
            statusBar()->showMessage(tr("Export cancelled"), 4000);
        } else {
            assembly_->undoStack()->setClean();
            statusBar()->showMessage(tr("Exported %1").arg(QFileInfo(destination).fileName()), 8000);
            if (!result.warnings.isEmpty()) {
                QMessageBox message(QMessageBox::Warning, tr("Export completed with PDF warnings"),
                    tr("The PDF was saved, but some source data needed repair. Check the exported document."),
                    QMessageBox::Ok, this);
                message.setDetailedText(result.warnings.join(QStringLiteral("\n")));
                message.exec();
            }
            if (openAfter)
                QDesktopServices::openUrl(QUrl::fromLocalFile(destination));
        }
        if (closeAfterExport_) {
            closeAfterExport_ = false;
            close();
        }
    });
    exportWatcher_->setFuture(QtConcurrent::run([output = std::move(output), destination, progress = progress_] {
        return exportPdf(output, destination, progress);
    }));
    exportButton_->setEnabled(false);
    exportAction_->setEnabled(false);
}

void MainWindow::setTheme(const QString& theme)
{
    theme_ = theme;
    QSettings().setValue(QStringLiteral("theme"), theme);
    applyTheme();
}

void MainWindow::applyTheme()
{
    const bool dark = theme_ == QStringLiteral("dark")
        || (theme_ == QStringLiteral("system") && qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    const QString background = dark ? QStringLiteral("#242528") : QStringLiteral("#ECEDEF");
    const QString surface = dark ? QStringLiteral("#2D2E32") : QStringLiteral("#F8F8F9");
    const QString text = dark ? QStringLiteral("#ECECEF") : QStringLiteral("#292A2E");
    const QString secondary = dark ? QStringLiteral("#AAADB5") : QStringLiteral("#65676E");
    const QString border = dark ? QStringLiteral("#41434A") : QStringLiteral("#DADCE1");
    const QString accent = dark ? QStringLiteral("#DE858C") : QStringLiteral("#B64D55");
    const QString selection = dark ? QStringLiteral("#483339") : QStringLiteral("#F0E0E2");
    const QString accentText = dark ? QStringLiteral("#242528") : QStringLiteral("#FFFFFF");
    tree_->setProperty("selectionColor", QColor(selection));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(background));
    palette.setColor(QPalette::WindowText, QColor(text));
    palette.setColor(QPalette::Base, QColor(surface));
    palette.setColor(QPalette::AlternateBase, QColor(background));
    palette.setColor(QPalette::Text, QColor(text));
    palette.setColor(QPalette::Button, QColor(surface));
    palette.setColor(QPalette::ButtonText, QColor(text));
    palette.setColor(QPalette::Highlight, QColor(accent));
    palette.setColor(QPalette::HighlightedText, QColor(accentText));
    palette.setColor(QPalette::ToolTipBase, QColor(surface));
    palette.setColor(QPalette::ToolTipText, QColor(text));
    palette.setColor(QPalette::PlaceholderText, QColor(secondary));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(secondary));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(secondary));
    qApp->setPalette(palette);
    qApp->setStyleSheet(QStringLiteral(R"(
        QMainWindow, QStackedWidget { background: %1; }
        QWidget#sidebar, QWidget#toolbar { background: %2; }
        QLabel { background: transparent; }
        QLabel#muted { color: %4; }
        QPushButton { background: %2; color: %3; border: 1px solid %5;
            border-radius: 6px; padding: 7px 12px; }
        QPushButton:hover { border-color: %6; }
        QPushButton:pressed { background: %7; }
        QPushButton:focus { border: 2px solid %6; padding: 6px 11px; }
        QPushButton:disabled { color: %4; background: %1; border-color: %5; }
        QPushButton#primary { background: %6; color: %8; border-color: %6; font-weight: 600; }
        QPushButton#primary:hover { background: %9; }
        QPushButton#primary:disabled { background: %5; color: %4; border-color: %5; }
        QPushButton#primary:focus { border: 2px solid %3; }
        QPushButton#addButton { color: %6; border-style: dashed; }
        QPushButton#emptyState { background: %1; border: 2px solid transparent; }
        QPushButton#emptyState:focus { border-color: %6; }
        QLabel#emptyPlus { color: %6; font-size: 72px; font-weight: 300; }
        QLabel#emptyPrompt { color: %3; font-size: 20px; font-weight: 500; }
        QTreeView { background: %2; border: none; outline: none; }
        QTreeView::item { padding: 4px; border: 1px solid transparent; background: transparent; color: %3; }
        QTreeView QLineEdit { background: %2; color: %3; border: 1px solid %6; }
        QWidget#exportFooter { border-top: 1px solid %5; }
        QSplitter::handle { background: %5; width: 1px; }
        QStatusBar { color: %4; background: %2; border-top: 1px solid %5; }
        QMenuBar, QMenu { background: %2; color: %3; }
        QMenuBar::item:selected, QMenu::item:selected { background: %7; color: %3; }
        QToolTip { background: %2; color: %3; border: 1px solid %5; padding: 4px; }
    )").arg(background, surface, text, secondary, border, accent, selection, accentText,
            QColor(accent).lighter(dark ? 110 : 108).name()));
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls() && !exportWatcher_->isRunning())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    QStringList paths;
    for (const auto& url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            paths.append(url.toLocalFile());
    }
    if (!paths.isEmpty()) {
        event->acceptProposedAction();
        importFiles(paths);
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (exportWatcher_->isRunning()) {
        progress_->cancelled.store(true);
        closeAfterExport_ = true;
        event->ignore();
        return;
    }
    if (!assembly_->undoStack()->isClean() && !assembly_->pages().empty()) {
        if (QMessageBox::question(this, tr("Close without exporting?"),
            tr("Your current page arrangement has not been exported."),
            QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Discard) {
            event->ignore();
            return;
        }
    }
    QSettings settings;
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.setValue(QStringLiteral("splitter"), splitter_->saveState());
    settings.setValue(QStringLiteral("openAfterExport"), openAfter_->isChecked());
    event->accept();
}
