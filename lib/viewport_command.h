#pragma once

namespace M2V {

class ViewportCommand {
public:
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    virtual ~ViewportCommand() = default;
};

}
