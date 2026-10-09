#pragma once
#include "netlist.hpp"
#include "geometry.hpp"
#include "tools.hpp"
#include "window.hpp"
#include "cam.hpp"
#include "wire.hpp"
#include <vector>
#include <deque>
#include <string>


// 一个"门类型"的定义（E 步再补 netlist 内核指针）
struct GateDef {
    const ComponentType* comp;
    Vec3 color = {0.25f, 0.45f, 0.85f};   // 默认蓝，可每个门不同
    int nin() const {return comp->inputs.size();}
    int nout() const {return comp->outputs.size();}
};

// 图上的节点（用户正在画的门）
struct Node {
    int id;
    int typeId;            // 索引 gateLib
    Vec2 pos;              // 世界坐标(左上角)
    float w = 60, h = 120;
    Rect getBounds() const { return Rect(pos, w, h); }
};

// 一根连线：从某节点输出口 → 某节点输入口
// struct Link {
//     int fromNode, fromPort;   // 输出口
//     int toNode,   toPort;     // 输入口
// };

class CircuitEditor 
{
private:
    // 全局元件库（建一次）
    std::vector<GateDef> gateLib;
    std::deque<ComponentType> compLib;
    // 当前图
    std::vector<Node>  nodes;
    std::vector<Wire>  wires;
    std::vector<Connector> connectors;
    bool drawing = false;
    static constexpr float GRID = 32.0f;   // 一格的世界单位边长
    Vec2 cursorScreen{0,0};
    bool btnDownEdge = false;   // 本帧左键刚按下（只活一帧）
    bool btnUpEdge   = false;   // 本帧左键刚松开（只活一帧）
    size_t drawingFromId = static_cast<size_t>(-1);  // 起点 connector id（真实存在）
    Vec2  drawPreview{0,0};                           // 橡皮筋末点（无 id，松手才建）
    bool fromNew = false;   // 本次起点是按下时新建的(非复用)
    std::vector<std::vector<size_t>> usersOf;        // 反向索引：connId -> 引用它的 wire 下标
    bool dragging = false;
    size_t dragConnId = static_cast<size_t>(-1);
    bool rightDownEdge = false;                 // 右键刚按下
    size_t wireSegmentAt(const Vec2& p) const;  // 命中线中段返回 wire 下标，否则 -1
    void removeConnector(size_t id);            // tombstone + 降级所有引用线
public:
    void init();
    void update(float dt, Camera& cam);
    void draw(Window& w, Camera& cam);
    void handleEvent(const std::vector<Event>& evs, Camera& cam, Window& w);
    int addComposite(ComponentType c, Vec3 col);
    Vec2 pinWorld(const Node& n, int port, bool isInput, int portCount);// 引脚世界坐标：输入在左缘、输出在右缘，竖向均分
    void drawGrid(Window& w, Camera& cam);
    template<typename B>
    bool isOccupied(const B& proposed) const;
    size_t connectorAt(const Vec2& p) const;         // 命中返回 id，否则 (size_t)-1
    size_t addConnector(const Vec2& p);              // 新建并返回 id（同步 usersOf
};

template<typename B>
inline bool CircuitEditor::isOccupied(const B& proposed) const {
    for (const auto& n : nodes)
        if (proposed.intersects(n.getBounds())) return true;
    for (const auto& c : connectors) {
        if (!c.alive) continue;
        if (proposed.intersects(Circle(c.pos, c.r))) return true;
    }
    return false;
}