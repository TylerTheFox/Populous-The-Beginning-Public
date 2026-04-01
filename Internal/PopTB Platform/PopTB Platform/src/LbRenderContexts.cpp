#include <LbRender.h>
#include <LbException.h>

#define RENDERCLASS RenderContext8
#define RENDER_BPP  1
#include "LbRendef_impl.inl"
#undef RENDER_BPP
#undef RENDERCLASS

#define RENDERCLASS RenderContext16
#define RENDER_BPP  2
#include "LbRendef_impl.inl"
#undef RENDER_BPP
#undef RENDERCLASS

#define RENDERCLASS RenderContext24
#define RENDER_BPP  3
#include "LbRendef_impl.inl"
#undef RENDER_BPP
#undef RENDERCLASS

#define RENDERCLASS RenderContext32
#define RENDER_BPP  4
#include "LbRendef_impl.inl"
#undef RENDER_BPP
#undef RENDERCLASS
