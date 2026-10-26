// Author: Sami Khan
#include "gfx.h"
#ifndef __EMSCRIPTEN__
#define X(ret, name, args) name##_t name = nullptr;
GL_FUNCS(X)
#undef X
bool loadGL(void* (*getProc)(const char*)) {
    bool ok = true;
#define X(ret, name, args) name = (name##_t)getProc(#name); if (!name) { ok = false; }
    GL_FUNCS(X)
#undef X
    return ok;
}
#endif
