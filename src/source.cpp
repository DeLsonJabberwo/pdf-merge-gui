#include "source.h"

Source::Source()
{
    renderer.setRenderMode(QPdfPageRenderer::RenderMode::MultiThreaded);
    renderer.setDocument(&document);
}

QString Source::snapshotPath() const
{
    return storage.filePath(QStringLiteral("source.pdf"));
}
