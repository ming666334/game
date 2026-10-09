#pragma once
#include "window.hpp"
#include "cam.hpp"
#include "circuit.hpp"
#include <vector>

class Game
{
private:
    Window win;
    Camera cam;
    CircuitEditor cir;
    std::vector<Event> evs;
    bool running = true;
    Vec2 m_pos;
public:
    void handleEvents(Window& w);
    void draw(Window& w);
    void update(Window& w);
    void create_game(int w, int h, LPCWSTR title);
    void run();
};
