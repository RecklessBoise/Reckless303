#pragma once

#include <juce_graphics/juce_graphics.h>

namespace acid
{
// Procedurally renders the case: brushed aluminium streaks, light scratches,
// grime in the corners and rubbed spots, with an aged warm tint.
// widthPx/heightPx are the physical pixel size; the same seed always produces
// the same wear pattern so resizing keeps the scratches in place.
juce::Image createBrushedMetal (int widthPx, int heightPx, juce::uint32 seed = 303);
} // namespace acid
