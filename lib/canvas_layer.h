#pragma once
#include <string>
#include <unordered_map>

#include "gobject.h"


namespace M2V {

using LayerID = size_t;

class CanvasLayer {
public:
    CanvasLayer(LayerID layerId, const std::string& layerName, size_t zindex):
        m_layerId(layerId), m_layerName(layerName),
        m_zindex(zindex), m_dirty(false) {}

    void Add(GObjectPtr obj);
    void Remove(GObjectID objId);
    void Remove(GObjectPtr obj) { Remove(obj->GetId()); };

    auto& GetName() const { return m_layerName; }

    bool dirty() const { return m_dirty; }

private:
    std::unordered_map<GObjectID,GObjectPtr> m_objects;
    LayerID m_layerId;
    std::string m_layerName;
    size_t m_zindex;
    bool m_dirty;
};

}
