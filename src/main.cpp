#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "viewport.h"


int main(void)
{
    M2V::Viewport viewport;
    auto& commit = viewport.BeginTransaction();
    viewport.Summit(commit);
    return 0;
}


#include <emscripten/bind.h>
EMSCRIPTEN_BINDINGS(Magic2DViewer) {
    emscripten::class_<M2V::Viewport>("Viewport")
        .constructor<>()
        .function("OnResize", &M2V::Viewport::OnResize)
        .function("OnScale",  &M2V::Viewport::OnScale);
}
