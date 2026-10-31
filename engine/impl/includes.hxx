#pragma once

//
// engine build graph.
//
// one translation unit, entry.cxx. include order is the architecture:
// platform, the public surface, then each module. modules are headers.
// hydro links the resulting library and only includes impl/engine.hxx.
//

#include <impl/host.hxx>
#include <impl/engine.hxx>

#include <workspace/utility/logger.hxx>
#include <workspace/utility/crash.hxx>
#include <workspace/utility/math.hxx>

#include <workspace/core/render/window.hxx>
#include <workspace/core/render/device.hxx>
#include <workspace/core/render/swap.hxx>
#include <workspace/core/render/target.hxx>
#include <workspace/core/render/shader.hxx>
#include <workspace/core/render/mesh.hxx>

#include <workspace/core/scene/draw.hxx>
#include <workspace/core/scene/viewport.hxx>
#include <workspace/core/scene/sort.hxx>
#include <workspace/core/scene/light.hxx>
#include <workspace/core/scene/scene.hxx>
#include <workspace/core/scene/screen.hxx>
#include <workspace/core/scene/composer.hxx>
#include <workspace/core/bench.hxx>
#include <workspace/core/runtime.hxx>
