
//
// Copyright (c) 2021-2022 the rbfx project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "../../Precompiled.h"

#include "Clamp.h"

#include "../ParticleGraphLayerInstance.h"
#include "../ParticleGraphSystem.h"
#include "../Span.h"
#include "../UpdateContext.h"
#include "ClampInstance.h"

namespace Urho3D
{
namespace ParticleGraphNodes
{
void Clamp::RegisterObject(ParticleGraphSystem* context)
{
    context->AddReflection<Clamp>();
}

namespace {
static ea::vector<NodePattern> ClampPatterns{
    MakePattern(
        ClampInstance<float, float>()
        , ParticleGraphTypedPin<float>("x")
        , ParticleGraphTypedPin<float>(ParticleGraphPinFlag::Output, "out")
    ),
    MakePattern(
        ClampInstance<Vector2, Vector2>()
        , ParticleGraphTypedPin<Vector2>("x")
        , ParticleGraphTypedPin<Vector2>(ParticleGraphPinFlag::Output, "out")
    ),
    MakePattern(
        ClampInstance<Vector3, Vector3>()
        , ParticleGraphTypedPin<Vector3>("x")
        , ParticleGraphTypedPin<Vector3>(ParticleGraphPinFlag::Output, "out")
    ),
    MakePattern(
        ClampInstance<Vector4, Vector4>()
        , ParticleGraphTypedPin<Vector4>("x")
        , ParticleGraphTypedPin<Vector4>(ParticleGraphPinFlag::Output, "out")
    ),
};
} // namespace

Clamp::Clamp(Context* context)
    : PatternMatchingNode(context, ClampPatterns)
{
}

} // namespace ParticleGraphNodes
} // namespace Urho3D
