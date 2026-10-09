#include "game.hpp"
#include <windows.h>

void Game::create_game(int w, int h, LPCWSTR title) {
    bool created = win.create(w, h, title, SW_SHOWNORMAL, true);
    if (!created) {
        MessageBoxA(NULL, "Window::create() failed — see any earlier GL/window error popup.",
                    "Fatal", MB_OK | MB_ICONERROR);
        running = false;
        return;
    }
    cir.init();
}

void Game::draw(Window& w) {
    w.renderer.fill(tools::rgb(0,0,0));
    cir.draw(w, cam);
}

void Game::update(Window& w) {
    cam.update(w.getDeltaTime());
    Mat4 view = multiply(scale2D(cam.scale, cam.scale),
                        translate2D(-cam.pos.x, -cam.pos.y));
    w.renderer.setView(view);
    w.text.setView(view);
    cir.update(w.getDeltaTime(), cam);
}

void Game::handleEvents(Window& w) {
    w.getEvents(evs);
    for (Event& e : evs) {
        if (e.type == EventType::QUIT || (e.type == EventType::KEYDOWN && e.key == VK_ESCAPE)) {
            running = false;
        }
    }
    cir.handleEvent(evs, cam, w);
    evs.clear();
}

void Game::run() {
    SetProcessDPIAware();
    create_game(0, 0, L"图灵完备");
    win.setPixelMode(true);
    win.setUpdate([&](Window& w) {update(w);});
    win.setDraw([&](Window& w) {draw(w);});
    cam.setUpdate([&](Camera& cam, float dt) {cam_update(cam, win, dt);});
    while (running) {
        win.updateDeltaTime();
        win.pollEvents();
        win.getMousePos(m_pos);
        handleEvents(win);
        win.update();
        win.draw();
    }
}
