#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include <iostream>
#include "h2geometry.h"
#include "common.h"


namespace M2V {

using GObjectID = size_t;
using Point = H2G::Point<int>;
using CommonShape = H2G::SHAPE<int>;

class GObject {
public:
    GObjectID GetId() const { return m_id; }

    auto& shape() const { return m_shape; }
    auto& shape()       { return m_shape; }

    static std::unique_ptr<GObject> CreateObject();

private:
    GObject(GObjectID id, CommonShape shape):
        m_id(id), m_shape(shape) {}

    GObjectID m_id;
    CommonShape m_shape;
};

using GObjectPtr = QPtr<GObject>;

}

