#pragma once
#include "commit.h"
#include "canvas_layer.h"
#include <map>
#include <optional>
#include <string>

namespace M2V {

class Viewport {
public:
    Commit& BeginTransaction();
    void Abort(Commit& commit);
    void Summit(Commit& commit);

    void OnScale(double scaleX, double scaleY);
    void OnTranslate(int deltaX, double deltaY);
    void OnRotate(double degreeCClockwise);
    void OnReset();
    void OnResize(int viewportXSize, int viewportYSize);
    void OnSelect(Point from, Point to);
    void OnDelete();

    Viewport():
        m_freeLayerId(1), m_freeObjectId(1) {}

protected:
    class ViewportOperator {
    public:
    LayerID CreateLayer(const std::string& layerName) {
        return m_viewportPtr->CreateLayer(layerName);
    }
    std::optional<LayerID> FindLayer(const std::string& layerName) const
    {
        return m_viewportPtr->FindLayer(layerName);
    }

    size_t GetLayerZIndex(LayerID layer) const {
        return m_viewportPtr->GetLayerZIndex(layer);
    }

    template<typename ... Args>
    GObjectPtr CreateGObject(Args&& ...args)
    {
        return m_viewportPtr->CreateGObject(std::forward<Args&&>(args)...);
    }

    void DeleteObject(GObjectID objId)
    {
        m_viewportPtr->DeleteObject(objId);
    }

    void CanvasAddObject(LayerID layer, GObjectPtr object)
    {
        m_viewportPtr->CanvasAddObject(layer, object);
    }

    void CanvasRemoveObject(LayerID layer, GObjectPtr object)
    {
        m_viewportPtr->CanvasRemoveObject(layer, object);
    }

    protected:
        friend class Viewport;
        ViewportOperator(Viewport* viewportPtr):
            m_viewportPtr(viewportPtr) {}

    private:
        Viewport* m_viewportPtr;
    };

    LayerID CreateLayer(const std::string& layerName);
    std::optional<LayerID> FindLayer(const std::string& layerName) const;

    size_t GetLayerZIndex(LayerID layer) const {
        for (size_t i=0;i<m_layerStack.size();i++) {
            if (m_layerStack.at(i) == layer) {
                return i;
            }
        }
        MUnreachable();
        return 0;
    }

    template<typename ... Args>
    GObjectPtr CreateGObject(Args&& ...args)
    {
        const auto objectId = m_freeObjectId++;
        std::unique_ptr<GObject> obj = GObject::CreateObject(objectId, std::forward<Args&&>(args)...);
        GObjectPtr ans(obj.get());
        m_objects.insert({objectId, std::move(obj)});
        return ans;
    }

    void DeleteObject(GObjectID objId)
    {
        MASSERT(m_objects.count(objId));
        m_objects.erase(objId);
    }

    void CanvasAddObject(LayerID layer, GObjectPtr object)
    {
        MASSERT(m_layers.count(layer));
        m_layers.at(layer).Add(object);
    }

    void CanvasRemoveObject(LayerID layer, GObjectPtr object)
    {
        MASSERT(m_layers.count(layer));
        m_layers.at(layer).Remove(object);
    }

private:
    std::vector<std::unique_ptr<Commit>> m_undoList;
    std::vector<std::unique_ptr<Commit>> m_redoList;
    LayerID m_freeLayerId;
    GObjectID m_freeObjectId;
    std::map<LayerID, CanvasLayer> m_layers;
    std::vector<LayerID> m_layerStack;
    std::unordered_map<GObjectID, std::unique_ptr<GObject>> m_objects;
};

}
