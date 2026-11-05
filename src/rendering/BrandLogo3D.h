#pragma once
#include "raylib.h"
namespace loblox {
// Own on the graphics thread; destroy before CloseWindow. One cached frame may
// be drawn into several surfaces. Call update before BeginDrawing, only if visible.
class BrandLogo3D {
public:
    BrandLogo3D() = default;
    ~BrandLogo3D();
    BrandLogo3D(const BrandLogo3D&) = delete;
    BrandLogo3D& operator=(const BrandLogo3D&) = delete;
    bool update(float dt, int physicalSize);
    void draw(Rectangle bounds, Texture2D fallback = {}) const;
    Texture2D texture() const { return target_.texture; }
private:
    Model model_{};
    RenderTexture2D target_{};
    Vector3 center_{};
    float radius_=1, angle_=0, elapsed_=1;
    bool tried_=false;
};
}
