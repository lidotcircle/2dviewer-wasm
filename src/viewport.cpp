#include "viewport.h"
using namespace M2V;

Commit& Viewport::BeginTransaction()
{
    MDEBUG_LOG("begin transaction");
    MASSERT(m_undoList.empty() || m_undoList.back()->done());
    m_undoList.push_back(std::make_unique<Commit>(*this));
    m_redoList.clear();
    return *m_undoList.back();
}
void Viewport::Abort(Commit& commit)
{
    MDEBUG_LOG("abort transaction");
    MASSERT(!m_undoList.empty() && m_undoList.back().get() == &commit);
    m_undoList.pop_back();
}
void Viewport::Summit(Commit& commit)
{
    MDEBUG_LOG("submit transaction");
}

void Viewport::OnScale(double scaleX, double scaleY)
{
    MDEBUG_LOG("on scale");
}
void Viewport::OnTranslate(int deltaX, double deltaY)
{
    MDEBUG_LOG("on translate");
}
void Viewport::OnRotate(double degreeCClockwise)
{
    MDEBUG_LOG("on rotate");
}
void Viewport::OnReset()
{
    MDEBUG_LOG("on reset");
}
void Viewport::OnResize(int viewportXSize, int viewportYSize)
{
    MDEBUG_LOG("on resize");
}
void Viewport::OnSelect(Point from, Point to)
{
    MDEBUG_LOG("on select");
}
void Viewport::OnDelete()
{
    MDEBUG_LOG("on delete");
}

LayerID Viewport::CreateLayer(const std::string& layerName)
{
    const auto layerId = m_freeLayerId++;
    m_layers.insert({layerId, CanvasLayer(layerId, layerName, m_layerStack.size())});
    m_layerStack.push_back(layerId);
    return layerId;
}

std::optional<LayerID> Viewport::FindLayer(const std::string& layerName) const
{
    for (auto& [id, layer]: m_layers) {
        if (layer.GetName() == layerName) {
            return id;
        }
    }
    return std::nullopt;
}

