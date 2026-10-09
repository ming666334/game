#include "wire.hpp"

Wire::Wire(size_t aId, const Vec2& aPos, size_t bId, const Vec2& bPos) {
    nodeRefs = {aId, bId};
    path     = {aPos, routeElbow(aPos, bPos), bPos};   // 弯点在中
    bendFlag = {1, 0, 1};                                // 端点是节点，中间是弯点
}

bool Wire::isNode(size_t i) const {
    return i < bendFlag.size() && bendFlag[i] != 0;
}

void Wire::insertNodeAt(size_t newId, const Vec2& newPos) {
    const float thr = END_R * END_R;      // 半径20，>半格²(256)，snap后仍能命中
    if (nodeCount() < 2) {addNode(newId, newPos); return;};
    for (size_t i = 0; i < path.size() - 1; i++) {
        if (distance_squared_point_to_segment(newPos, path[i], path[i + 1]) <= thr) {
            path.insert(path.begin() + i + 1, newPos);
            bendFlag.insert(bendFlag.begin() + i + 1, 1);

            size_t ni = nodeIndexOfPath(i);
            nodeRefs.insert(nodeRefs.begin() + ni + 1, newId);
            return;
        }
    }
}

void Wire::addNode(size_t id, const Vec2& pos) {
    nodeRefs.push_back(id);
    path.push_back(pos);
    bendFlag.push_back(1);
}

void Wire::addBend(const Vec2& pos) {
    path.push_back(pos);
    bendFlag.push_back(0);            // nodeRefs 不动
}

size_t Wire::nodeIndexOfPath(size_t k) const {
    size_t ni = 0, count = 0;
    for (size_t i = 0; i <= k; i++) {
        if (bendFlag[i] == 1) {
            ni = count;
            ++count;
        }
    }
    return ni;
}

size_t Wire::pathIndexAtNode(size_t k) const {
    size_t count = 0;
    for (size_t i = 0; i < path.size(); i++ ) {
        if (bendFlag[i] == 1) {
            if (count == k) return i;
            count++;
        }
    }
    return path.size();
}

std::vector<Wire> Wire::splitAt(size_t i) {
    if (i == 0) {
        std::vector<size_t> n(nodeRefs.begin() + 1, nodeRefs.end());
        std::vector<Vec2> p(path.begin() + 1, path.end());
        std::vector<uint8_t> f(bendFlag.begin() + 1, bendFlag.end());
        std::vector<Wire> r{Wire(n, p, f)};
        return r;
    } 
    if (i == path.size() - 1) {
        std::vector<size_t> n(nodeRefs.begin(), nodeRefs.end() - 1);
        std::vector<Vec2> p(path.begin(), path.end() - 1);
        std::vector<uint8_t> f(bendFlag.begin(), bendFlag.end() - 1);
        std::vector<Wire> r{Wire(n, p, f)};
        return r;
    }
    size_t ni = nodeIndexOfPath(i);
    std::vector<size_t> n1(nodeRefs.begin(), nodeRefs.begin() + ni + 1), n2(nodeRefs.begin() + ni + 1, nodeRefs.end());
    std::vector<Vec2> p1(path.begin(), path.begin() + i + 1), p2(path.begin() + i + 1, path.end());
    std::vector<uint8_t> f1(bendFlag.begin(), bendFlag.begin() + i + 1), f2(bendFlag.begin() + i + 1, bendFlag.end());
    std::vector<Wire> r{Wire(n1, p1, f1), Wire(n2, p2, f2)};
    return r;
}

Vec2 Wire::routeElbow(const Vec2& s, const Vec2& m, bool diagFirst) {
    float dx = m.x - s.x, dy = m.y - s.y;
    float adx = dx < 0 ? -dx : dx;        // |dx|
    float ady = dy < 0 ? -dy : dy;        // |dy|
    float sx = dx < 0 ? -1.0f : 1.0f;     // x 方向符号
    float sy = dy < 0 ? -1.0f : 1.0f;     // y 方向符号
    if (adx >= ady) {
        // 水平腿更长：水平段长 (adx-ady)，对角段长 ady
        if (diagFirst)  return Vec2(s.x + sx*ady,        s.y + sy*ady);        // 先对角后水平
        else            return Vec2(s.x + sx*(adx - ady), s.y);                // 先水平后对角
    } else {
        // 垂直腿更长：垂直段长 (ady-adx)，对角段长 adx
        if (diagFirst)  return Vec2(s.x + sx*adx,        s.y + sy*adx);       // 先对角后垂直
        else            return Vec2(s.x,                 s.y + sy*(ady - adx)); // 先垂直后对角
    }
}

void Wire::removeNodeById(size_t id) {
    for (size_t i = 0; i < nodeCount(); i++) {
        if (nodeRefs[i] == id) {
            nodeRefs.erase(nodeRefs.begin() + i);
            size_t ni = pathIndexAtNode(i);
            bendFlag[ni] = 0;
            return;
        }
    }
}

void Wire::drawline(Window& w) {
    if (path.size() < 2) return;
    for (size_t i = 0; i < path.size() - 1; i++) {
        w.renderer.Line2(path[i], path[i + 1], line_color, 18);
    }
}

void Wire::drawEndpoints(Window& w) {  // 被全局连接点表代替
    if (path.empty()) return;
    for (size_t i = 0; i < path.size(); i++) {
        if (!isNode(i)) continue;
        w.renderer.Circle2(path[i], 10, 48, line_color);
        w.renderer.Circle2(path[i], 7, 48, port_color);
    }
}

void Wire::updateConnectorPos(size_t id, const Vec2& newPos) {
    for (size_t k = 0; k < nodeCount(); ++k) {
        if (nodeRefs[k] == id) {
            size_t pi = pathIndexAtNode(k);
            path[pi] = newPos;
            rebuildPath();
            return;
        }
    }
}

void Wire::rebuildPath() {
    if (nodeRefs.empty()) {path.clear(); bendFlag.clear(); return;}
    std::vector<Vec2> nodePos;
    nodePos.reserve(nodeCount());
    for (size_t i = 0; i < path.size(); i++) {
        if (bendFlag[i] == 1) nodePos.push_back(path[i]);
    }
    std::vector<Vec2> np;
    std::vector<uint8_t> nf;
    np.reserve(nodeCount() * 2);
    nf.reserve(nodeCount() * 2);
    for (size_t i = 0; i < nodePos.size(); i++) {
        if (i + 1 < nodePos.size()) {
            np.push_back(nodePos[i]);
            nf.push_back(1);
            np.push_back(routeElbow(nodePos[i], nodePos[i + 1]));
            nf.push_back(0);
        } else {
            np.push_back(nodePos[i]);
            nf.push_back(1);
        }
    }
    path = std::move(np);
    bendFlag = std::move(nf);
}