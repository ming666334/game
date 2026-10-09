#include "circuit.hpp"

void CircuitEditor::init() {
    if (!gateLib.empty()) return;
    auto addPrim = [&](const char* name, Vec3 color) {
        GateDef g;
        g.comp = prim(name);
        g.color = color;
        gateLib.push_back(g);
    };
    addPrim("VDD",    tools::rgb(255, 80, 80));   // 0
    addPrim("GND",    tools::rgb(120,120,120));   // 1
    addPrim("SWITCH", tools::rgb( 80,200,120));   // 2
    addPrim("LED",    tools::rgb(255,210, 60));   // 3
    addPrim("CLOCK",  tools::rgb(180,120,255));   // 4
    addPrim("NMOS",   tools::rgb( 90,160,255));   // 5
    addPrim("PMOS",   tools::rgb( 90,160,255));   // 6
}

int CircuitEditor::addComposite(ComponentType c, Vec3 col) {
    compLib.push_back(std::move(c));
    GateDef g;
    g.comp = &compLib.back();
    g.color = col;
    gateLib.push_back(g);
    return (int)gateLib.size() - 1;
}

Vec2 CircuitEditor::pinWorld(const Node& n, int port, bool isInput, int portCount) {
    float x = isInput ? n.pos.x : (n.pos.x + n.w);
    float y = n.pos.y + n.h * (port + 1) / (portCount + 1);
    return {x, y};
}

void CircuitEditor::drawGrid(Window& w, Camera& cam) {
    const float G = GRID;
    int W = w.get_width(), H = w.get_height();
    Vec2 tl = cam.screenToWorld(Vec2(0, 0));
    Vec2 br = cam.screenToWorld(Vec2(W, H));
    float minX = std::min(tl.x, br.x), maxX = std::max(tl.x, br.x);
    float minY = std::min(tl.y, br.y), maxY = std::max(tl.y, br.y);
    int x0 = (int)std::floor(minX / G), x1 = (int)std::ceil(maxX / G);
    int y0 = (int)std::floor(minY / G), y1 = (int)std::ceil(maxY / G);
    Vec3 gc = {0.22f, 0.22f, 0.26f};   // 暗灰，深底场景里不抢戏
    for (int i = x0; i <= x1; ++i)
        w.renderer.Line2(Vec2(i*G, minY), Vec2(i*G, maxY), gc, 1.0f);
    for (int j = y0; j <= y1; ++j)
        w.renderer.Line2(Vec2(minX, j*G), Vec2(maxX, j*G), gc, 1.0f);
}

size_t CircuitEditor::connectorAt(const Vec2& p) const {
    for (size_t i = 0; i < connectors.size(); i++) {
        if (!connectors[i].alive) continue;
        if (Circle(connectors[i].pos, connectors[i].r).contains(p)) return i;
    }
    return (size_t)-1;
}

size_t CircuitEditor::addConnector(const Vec2& p) {
    connectors.push_back({p, END_R});
    usersOf.push_back({});
    return connectors.size() - 1;
}

size_t CircuitEditor::wireSegmentAt(const Vec2& p) const {
    const float thr = END_R * END_R;      // 半径20，覆盖线宽+snap量化
    for (size_t w = 0; w < wires.size(); w++) {
        const auto& path = wires[w].getPath();
        for (size_t i = 0; i + 1 < path.size(); i++) {
            // 该函数返回点到线的带符号距离：0 才代表命中线（原代码有 ! 写反，
            // 且 i < path.size() 会让 path[i+1] 越界）
            if (distance_squared_point_to_segment(p, path[i], path[i + 1]) <= thr) {
                return w;
            }
        }
    }
    return (size_t)-1;
}

void CircuitEditor::removeConnector(size_t id) {
    connectors[id].alive = false;
    for (size_t wi : usersOf[id]) {
        wires[wi].removeNodeById(id);
    }
    usersOf[id].clear();
}

void CircuitEditor::update(float dt, Camera& cam) {
    if (nodes.empty()) {
        nodes.push_back({0, 5, Vec2(30, 30)});
        nodes.push_back({1, 6, Vec2(30, 500)});
    }

    Vec2 cur_world = cam.screenToWorld(cursorScreen);
    Vec2 cur = tools::snapToGrid(cur_world, GRID);
    bool in = Circle(cur, END_R + 2).contains(cur_world);

    if (rightDownEdge) {
        rightDownEdge = false;
        if (in) {
            size_t hit = connectorAt(cur_world);
            if (hit != (size_t)-1) {
                removeConnector(hit);
            }
        }
    }

    if (btnDownEdge && !drawing && !dragging) {
        fromNew = false;                          // 每次按下重新判定，避免上次残留
        if (in) {
            size_t hitC = connectorAt(cur_world);
            if (hitC != (size_t)-1) {
                drawingFromId = hitC;
            } else {
                drawingFromId = addConnector(cur);
                size_t hitW = wireSegmentAt(cur_world);
                if (hitW != (size_t)-1) {
                    // 在新连接点上插入：newId 是连接器 id（drawingFromId），
                    // usersOf 存的是 wire 下标（hitW），不要把两个搞反
                    wires[hitW].insertNodeAt(drawingFromId, cur);
                    usersOf[drawingFromId].push_back(hitW);
                } else {
                    fromNew = true;
                }
            }
            drawPreview = cur;
            drawing = true;
        }
    }
    if (drawing) drawPreview = cur;
    if (dragging) {
        connectors[dragConnId].pos = cur;
        for (auto& wi : usersOf[dragConnId]) {
            wires[wi].updateConnectorPos(dragConnId, cur);
        }
    }
    if (btnUpEdge) {
        if (drawing) {
            drawing = false;
            if (drawPreview != connectors[drawingFromId].pos) {
                size_t endId = connectorAt(drawPreview);
                if (endId == (size_t)-1) endId = addConnector(drawPreview);
                size_t wi = wires.size();
                wires.push_back(Wire(drawingFromId, connectors[drawingFromId].pos,
                                    endId, connectors[endId].pos));
                usersOf[drawingFromId].push_back(wi);
                usersOf[endId].push_back(wi);
            } else {
                if (fromNew) connectors[drawingFromId].alive = false;
            }
            drawingFromId = (size_t)-1;
        } else if (dragging) {
            dragConnId = (size_t)-1;
            dragging = false;
        }
    }
    btnDownEdge = btnUpEdge = false;
}

void CircuitEditor::draw(Window& w, Camera& cam) {
    // drawGrid(w, cam);
    // float s = cam.scale;
    for (auto& n : nodes) {
        const GateDef g = gateLib[n.typeId];
        w.renderer.Rect2(n.pos, n.w, n.h, g.color);

        Vec2 tsp = {(float)n.w/2 + n.pos.x + 4, (float)n.h/2 + n.pos.y - 4};
        w.text.drawTextCentered(g.comp->name.c_str(), tsp, tools::rgb(255, 255, 255), 4.0f, 0.5f, n.w > n.h ? 0.0f : -PI/2);

        for (int i = 0; i < g.nin(); i++) {
            w.renderer.Circle2(pinWorld(n, i, true, g.nin()), END_R, 48, tools::rgb(0, 0, 255));
        }
        for (int i = 0; i < g.nout(); i++) {
            w.renderer.Circle2(pinWorld(n, i, false, g.nout()), END_R, 48, tools::rgb(255, 0, 0));
        }
    }
    for (Wire& l : wires) {
        l.drawline(w);
    }
    if (drawing) {
        Vec2 s = connectors[drawingFromId].pos, m = drawPreview;
        Vec2 e = Wire::routeElbow(s, m);
        w.renderer.Line2(s, e, tools::rgb(0, 0, 255), 18);
        w.renderer.Line2(e, m, tools::rgb(0, 0, 255), 18);
        w.renderer.Circle2(drawPreview, END_R, 48, tools::rgb(0, 0, 255));
        w.renderer.Circle2(drawPreview, END_R - 3, 48, tools::rgb(0, 0, 0));
    }
    for (const auto& c : connectors) {
        if (!c.alive) continue;
        w.renderer.Circle2(c.pos, END_R, 48, tools::rgb(0, 0, 255));
        w.renderer.Circle2(c.pos, END_R - 3, 48, tools::rgb(0, 0, 0));
    }
}

void CircuitEditor::handleEvent(const std::vector<Event>& evs, Camera& cam, Window& w) {
    for (const Event& e : evs) {
        if (e.type == EventType::MOUSEBUTTONDOWN) {
            cursorScreen = e.pos;
            if (e.button == 1) {
                btnDownEdge = true;
                size_t hitC = connectorAt(cam.screenToWorld(cursorScreen));
                if (w.isKeysDown(VK_MENU) && hitC != (size_t)-1) {
                    dragging = true;
                    dragConnId = hitC;
                }
            } else if (e.button == 3) {
                rightDownEdge = true;
            }
        } else if (e.type == EventType::MOUSEMOTION) {
            cursorScreen = e.pos;
        } else if (e.type == EventType::MOUSEBUTTONUP) {
            if (e.button == 1) {
                cursorScreen = e.pos; 
                btnUpEdge = true;
            }
        }
    }
}