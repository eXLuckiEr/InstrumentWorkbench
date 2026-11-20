#pragma once
// Internal: the GPU layer of PlotView. Compiled only when Qt Shader Tools are available
// (IWP_PLOT_RHI). A QRhiWidget that draws a ready list of coloured triangles; all the plotting
// logic stays in PlotView, this class knows nothing about signals or axes.
#include <QColor>
#include <QRhiWidget>
#include <memory>
#include <vector>

#include "plotframe.h"

class QRhiBuffer;
class QRhiGraphicsPipeline;
class QRhiShaderResourceBindings;
class QRhi;

namespace iwp::detail {

class PlotGpuLayer : public QRhiWidget
{
    Q_OBJECT
public:
    explicit PlotGpuLayer(QWidget *parent = nullptr);
    ~PlotGpuLayer() override;

    // Replaces the picture. Takes effect with the next update().
    void setGeometryData(std::vector<Vertex> &&vertices, const QColor &clear);

signals:
    void failed();   // no usable GPU path: the owner falls back to the QPainter renderer

protected:
    void initialize(QRhiCommandBuffer *cb) override;
    void render(QRhiCommandBuffer *cb) override;
    void releaseResources() override;

private:
    void resetResources();

    QRhi *m_rhi = nullptr;
    std::unique_ptr<QRhiBuffer> m_vbuf;
    std::unique_ptr<QRhiBuffer> m_ubuf;
    std::unique_ptr<QRhiShaderResourceBindings> m_srb;
    std::unique_ptr<QRhiGraphicsPipeline> m_pipeline;
    quint32 m_vbufSize = 0;
    std::vector<Vertex> m_vertices;
    QColor m_clear;
    bool m_failed = false;
};

} // namespace iwp::detail
