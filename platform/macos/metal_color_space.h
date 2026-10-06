#pragma once
#import <QuartzCore/CAMetalLayer.h>
#include <SDL.h>

// Palette bytes are encoded sRGB. Tag presentation, rather than switching to
// an sRGB pixel format (which would also change GPU transfer/blending rules).
// Software/OpenGL fallbacks have no Metal layer and keep their existing path.
inline void setMetalSRGBColorSpace(SDL_Renderer* renderer)
{
    CAMetalLayer* layer = (__bridge CAMetalLayer*)SDL_RenderGetMetalLayer(renderer);
    if (!layer) return;
    CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    layer.colorspace = colorSpace;
    CGColorSpaceRelease(colorSpace);
}
