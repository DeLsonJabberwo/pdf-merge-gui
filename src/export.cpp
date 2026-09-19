#include "export.h"

#include <QSaveFile>
#include <qpdf/Pipeline.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <map>
#include <stdexcept>

namespace {
struct Cancelled {};

void checkCancelled(const ExportProgress& progress)
{
    if (progress.cancelled.load())
        throw Cancelled{};
}

class SavePipeline : public Pipeline {
public:
    SavePipeline(QSaveFile& file, ExportProgress& progress)
        : Pipeline("Qt atomic PDF output", nullptr), file_(file), progress_(progress) {}

    void write(const unsigned char* data, size_t length) override
    {
        checkCancelled(progress_);
        if (file_.write(reinterpret_cast<const char*>(data), static_cast<qint64>(length))
            != static_cast<qint64>(length)) {
            throw std::runtime_error(file_.errorString().toStdString());
        }
    }
    void finish() override { checkCancelled(progress_); }

private:
    QSaveFile& file_;
    ExportProgress& progress_;
};

struct Input {
    QPDF pdf;
    std::unique_ptr<QPDFAcroFormDocumentHelper> forms;
    std::vector<QPDFPageObjectHelper> pages;
};
}

ExportResult exportPdf(const std::vector<ExportPage>& pages, const QString& destination,
                       const std::shared_ptr<ExportProgress>& progress)
{
    ExportResult result;
    try {
        if (pages.empty())
            throw std::runtime_error("There are no pages to export.");

        QSaveFile file(destination);
        // No direct-write fallback: a failed or cancelled export must not leave
        // a partial PDF in place of an existing destination.
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly))
            throw std::runtime_error(file.errorString().toStdString());

        std::map<QString, std::unique_ptr<Input>> inputs;
        QPDF output;
        output.emptyPDF();
        output.setSuppressWarnings(true);
        QPDFPageDocumentHelper outputPages(output);
        QPDFAcroFormDocumentHelper outputForms(output);
        int completed = 0;
        for (const auto& page : pages) {
            checkCancelled(*progress);
            auto& input = inputs[page.path];
            if (!input) {
                input = std::make_unique<Input>();
                input->pdf.setSuppressWarnings(true);
                input->pdf.processFile(page.path.toUtf8().constData(), page.password.toUtf8().constData());
                input->forms = std::make_unique<QPDFAcroFormDocumentHelper>(input->pdf);
                // A newly assembled document cannot preserve cryptographic signatures.
                input->forms->disableDigitalSignatures();
                input->pages = QPDFPageDocumentHelper(input->pdf).getAllPages();
            }
            if (page.number < 0 || page.number >= static_cast<int>(input->pages.size()))
                throw std::runtime_error("A source page is no longer available.");
            auto original = input->pages[page.number];
            outputPages.addPage(original, false);
            outputForms.fixCopiedAnnotations(output.getAllPages().back(),
                                             original.getObjectHandle(), *input->forms);
            progress->percent.store(++completed * 35 / static_cast<int>(pages.size()));
        }

        SavePipeline pipeline(file, *progress);
        QPDFWriter writer(output);
        writer.setOutputPipeline(&pipeline);
        writer.setObjectStreamMode(qpdf_o_generate);
        writer.registerProgressReporter(std::make_shared<QPDFWriter::FunctionProgressReporter>(
            [progress](int percent) {
                checkCancelled(*progress);
                progress->percent.store(35 + percent * 64 / 100);
            }));
        writer.write();
        checkCancelled(*progress);
        for (auto& [path, input] : inputs) {
            for (const auto& warning : input->pdf.getWarnings())
                result.warnings.append(QString::fromUtf8(warning.what()));
        }
        for (const auto& warning : output.getWarnings())
            result.warnings.append(QString::fromUtf8(warning.what()));
        if (!file.commit())
            throw std::runtime_error(file.errorString().toStdString());
        progress->percent.store(100);
    } catch (const Cancelled&) {
        result.cancelled = true;
    } catch (const std::exception& error) {
        result.error = QString::fromUtf8(error.what());
    }
    return result;
}
