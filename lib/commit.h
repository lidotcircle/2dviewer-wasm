#pragma once
#include "viewport_command.h"
#include <string>
#include <vector>
#include <memory>

namespace M2V {

class Viewport;
class Commit {
public:
    Commit(Viewport& viewport):
        m_viewportPtr(&viewport), m_submitted(false) { }

    bool done() const { return m_submitted; }

    void PushCommand(std::unique_ptr<ViewportCommand>&& cmd)
    {
        m_commands.emplace_back(std::move(cmd));
    }

private:
    std::vector<std::unique_ptr<ViewportCommand>> m_commands;
    Viewport* m_viewportPtr;
    bool m_submitted;
};

}
