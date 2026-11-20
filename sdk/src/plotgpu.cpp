#include "plotgpu.h"

#include <QFile>
#include <algorithm>
#include <utility>
#include <QMatrix4x4>
// QRhi is a private-ish Qt Gui API; where its headers live differs between Qt layouts.
#if __has_include(<rhi/qrhi.h>)
#  include <rhi/qrhi.h>
#elif __has_include(<QtGui/rhi/qrhi.h>)
#  include <QtGui/rhi/qrhi.h>
#elif __has_include(<QtGui/private/qrhi_p.h>)
#  include <QtGui/private/qrhi_p.h>
#else
#  error "QRhi headers not found: check that Qt6::GuiPrivate is linked and the private headers are installed"
#endif

namespace iwp::detail {

static QShader loadShader(const QString &name)
{
    QFile f(name);
    return f.open(QIODevice::ReadOnly) ? QShader::fromSerialized(f.readAll()) : QShader();
}

PlotGpuLayer::PlotGpuLayer(QWidget *parent) : QRhiWidget(parent)
{
    setSampleCount(4);   // smooth edges of the sparse (polyline) mode
    setAttribute(Qt::WA_TransparentForMouseEvents);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    connect(this, &QRhiWidget::renderFailed, this, [this] {
        m_failed = true;
        emit failed();
    });
#endif
}

PlotGpuLayer::~PlotGpuLayer() = default;

void PlotGpuLayer::setGeometryData(std::vector<Vertex> &&vertices, const QColor &clear)
{
    m_vertices = std::move(vertices);
    m_clear = clear;
}

void PlotGpuLayer::resetResources()
{
    m_pipeline.reset();
    m_srb.reset();
    m_ubuf.reset();
    m_vbuf.reset();
    m_vbufSize = 0;
}

void PlotGpuLayer::releaseResources()
{
    resetResources();
    m_rhi = nullptr;
}

void PlotGpuLayer::initialize(QRhiCommandBuffer *)
{
    // Called at least once before render() and again whenever the render target changes
    // (resize, new window, other sample count): everything that depends on it is rebuilt.
    resetResources();
    m_rhi = rhi();
    if (!m_rhi) {
        if (!m_failed) {
            m_failed = true;
            emit failed();
        }
        return;
    }

    m_ubuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64));
    m_ubuf->create();

    m_srb.reset(m_rhi->newShaderResourceBindings());
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage,
                                                 m_ubuf.get()),
    });
    m_srb->create();

    const QShader vs = loadShader(QStringLiteral(":/iwp/plot/plot.vert.qsb"));
    const QShader fs = loadShader(QStringLiteral(":/iwp/plot/plot.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) {
        qWarning("iwp: plot shaders are missing from the SDK resources, using the CPU renderer");
        m_failed = true;
        emit failed();
        return;
    }

    m_pipeline.reset(m_rhi->newGraphicsPipeline());
    m_pipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vs},
        {QRhiShaderStage::Fragment, fs},
    });
    QRhiVertexInputLayout layout;
    layout.setBindings({{quint32(sizeof(Vertex))}});
    layout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},
        {0, 1, QRhiVertexInputAttribute::Float4, quint32(2 * sizeof(float))},
    });
    m_pipeline->setVertexInputLayout(layout);
    m_pipeline->setShaderResourceBindings(m_srb.get());
    m_pipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    m_pipeline->setSampleCount(renderTarget()->sampleCount());
    if (!m_pipeline->create()) {
        qWarning("iwp: could not create the plot graphics pipeline, using the CPU renderer");
        m_pipeline.reset();
        m_failed = true;
        emit failed();
    }
}

void PlotGpuLayer::render(QRhiCommandBuffer *cb)
{
    if (!m_rhi || !m_pipeline)
        return;

    QRhiResourceUpdateBatch *updates = m_rhi->nextResourceUpdateBatch();
    const QSize px = renderTarget()->pixelSize();

    // pixel coordinates (origin top-left, y down) -> clip space of whatever backend runs
    QMatrix4x4 mvp = m_rhi->clipSpaceCorrMatrix();
    mvp.ortho(0.0f, float(px.width()), float(px.height()), 0.0f, -1.0f, 1.0f);
    updates->updateDynamicBuffer(m_ubuf.get(), 0, 64, mvp.constData());

    const quint32 bytes = quint32(m_vertices.size() * sizeof(Vertex));
    if (bytes > 0) {
        if (!m_vbuf || m_vbufSize < bytes) {
            m_vbufSize = std::max<quint32>(bytes + bytes / 2, 64 * 1024);
            m_vbuf.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, m_vbufSize));
            m_vbuf->create();
        }
        updates->updateDynamicBuffer(m_vbuf.get(), 0, bytes, m_vertices.data());
    }

    cb->beginPass(renderTarget(), m_clear.isValid() ? m_clear : QColor(Qt::black), {1.0f, 0},
                  updates);
    if (bytes > 0) {
        cb->setGraphicsPipeline(m_pipeline.get());
        cb->setViewport(QRhiViewport(0, 0, float(px.width()), float(px.height())));
        cb->setShaderResources();
        const QRhiCommandBuffer::VertexInput binding(m_vbuf.get(), 0);
        cb->setVertexInput(0, 1, &binding);
        cb->draw(quint32(m_vertices.size()));
    }
    cb->endPass();
}

} // namespace iwp::detail
