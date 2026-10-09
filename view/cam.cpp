#include "cam.hpp"
#include "window.hpp"
#include <algorithm>


void cam_update(Camera& c,Window& w, float dt) {
    const float SPEED = 300.0f;

    Vec2 dir(0, 0);
    if (w.isKeysDown('W')) dir.y = -1;
    if (w.isKeysDown('S')) dir.y = 1;
    if (w.isKeysDown('A')) dir.x = -1;
    if (w.isKeysDown('D')) dir.x = 1;

    Vec2 step = dir.normalized() * (SPEED * dt);
    c.pos.x += step.x;
    c.pos.y += step.y;

    int wheel = w.getWheelDelta();          // ← 新增：缩放
    if (wheel != 0) {
        float notches = (float)wheel / 120.0f;        // 每格 120

        Vec2 mouse;
        w.getMousePos(mouse);
        Vec2 worldBefore = c.screenToWorld(mouse);

        c.scale *= std::pow(1.15f, notches);          // 上滚放大，下滚缩小
        c.scale = std::clamp(c.scale, 0.1f, 10.0f);   // 夹住范围

        c.pos.x = worldBefore.x - mouse.x / c.scale;
        c.pos.y = worldBefore.y - mouse.y / c.scale;
    }
}