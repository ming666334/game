#pragma once
#include "math.hpp"
#include "tools.hpp"
#include "window.hpp"
#include "cam.hpp"
#include <vector>

static constexpr float END_R = 12.0f;

struct Connector {          // 一个连接点 = 一个图节点
    Vec2 pos;
    float r = END_R;        // 占位体积，阻挡/吸附都查它
    bool alive = true;      // 是否存在
};

class Wire
{
private:
    std::vector<size_t> nodeRefs;   // 端点+连接点：引用的 connector id（有序）
    std::vector<Vec2>   path;       // 仅绘制几何（routeElbow/A* 生成的折点）
    std::vector<uint8_t>   bendFlag;   // path 中哪些是路由弯点（非节点）
public:
    Vec3 line_color = tools::rgb(0, 0, 255), port_color = tools::rgb(0, 0, 0);
    Wire(size_t aId, const Vec2& aPos, size_t bId, const Vec2& bPos);
    Wire(std::vector<size_t> n, std::vector<Vec2> p, std::vector<uint8_t> f) {nodeRefs = n; path = p; bendFlag = f;}
    size_t nodeCount() const { return nodeRefs.size(); }
    size_t nodeAt(size_t k) const { return nodeRefs[k]; }
    size_t firstNode() const {return nodeRefs.front();}
    size_t lastNode() const {return nodeRefs.back();}
    void addNode(size_t id, const Vec2& pos);
    void addBend(const Vec2& pos);
    size_t nodeIndexOfPath(size_t k) const;
    size_t pathIndexAtNode(size_t k) const;
    bool isNode(size_t i) const;
    void insertNodeAt(size_t newId, const Vec2& newPos);
    std::vector<Wire> splitAt(size_t i);
    const std::vector<Vec2>& getPath() const {return path;};
    static Vec2 routeElbow(const Vec2& s, const Vec2& m, bool diagFirst = false);
    void removeNodeById(size_t id);
    void drawline(Window& w);
    void drawEndpoints(Window& w);  
    void updateConnectorPos(size_t id, const Vec2& newPos);
    void rebuildPath();
};