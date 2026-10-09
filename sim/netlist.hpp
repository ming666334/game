// netlist.hpp —— 数字电路仿真内核（header-only）
// 所有类型/行为/仿真都在这里；netlist.cpp 已不需要 main，可只保留 #include。
#ifndef NETLIST_HPP
#define NETLIST_HPP

#include <vector>
#include <string>
#include <iostream>

// ===== 四值逻辑 =====
enum class L { Zero, One, X, Z };

enum class CompKind { Primitive, Sub };

struct Port { std::string name; int width = 1; };
struct ComponentType;                       // 前向声明
struct ComponentInstance { int id; const ComponentType* type; };

struct Net {
    int fromId; std::string fromPort;       // 起点：实例 id + 端口名
    int toId;  std::string toPort;         // 终点：实例 id + 端口名
};

// 元件类型：既可以是原子(带 eval)，也可以是复合(带 sub/subWires)
// 编辑器的“门类型”就直接用它，不再另起结构。
struct ComponentType {
    std::string name;
    std::vector<Port> inputs, outputs;     // 对外端口（编辑器按 index 画、内核按 name 连）
    CompKind kind;                         // Primitive(原子) / Sub(复合)
    std::vector<ComponentInstance> sub;    // 复合元件内部的实例
    std::vector<Net> subWires;            // 复合元件内部的连线
    int instrBytes = 0;                    // 程序 ROM 用（以后）
    int displayColor = 0;                  // LED 用（以后）
    void (*eval)(const std::vector<L>& in, std::vector<L>& out,
                 std::vector<L>& state, bool rising) = nullptr;  // 行为函数指针
};

// 往 t 的连线表里加一条
inline void addWire(ComponentType& t, int f, const std::string& fp,
                    int tt, const std::string& tp) {
    t.subWires.push_back({f, fp, tt, tp});
}

// ===== 原子行为函数 =====
inline void evalVdd(const std::vector<L>& in, std::vector<L>& out,
                     std::vector<L>& state, bool rising) { out[0] = L::One; }

inline void evalGnd(const std::vector<L>& in, std::vector<L>& out,
                     std::vector<L>& state, bool rising) { out[0] = L::Zero; }

inline void evalBuf(const std::vector<L>& in, std::vector<L>& out,
                    std::vector<L>& state, bool rising) { out[0] = in[0]; }

inline void evalClock(const std::vector<L>& in, std::vector<L>& out,
                      std::vector<L>& state, bool rising) {
    if (state.empty()) state.assign(1, L::Zero);
    out[0] = state[0];
}

inline void evalSwitch(const std::vector<L>& in, std::vector<L>& out,
                       std::vector<L>& state, bool rising) {
    if (state.empty()) state.assign(1, L::Zero);
    out[0] = state[0];
}

// 受控开关：NMOS gate=1 时 drain 跟随 source；gate=0 断开(悬空 Z)
// 端口：in[0]=gate in[1]=source ; out[0]=drain
inline void evalNmos(const std::vector<L>& in, std::vector<L>& out,
                     std::vector<L>& state, bool rising) {
    if (in[0] == L::One) out[0] = in[1];
    else out[0] = L::Z;
}

// PMOS 相反：gate=0 时 drain 跟随 source；gate=1 断开
inline void evalPmos(const std::vector<L>& in, std::vector<L>& out,
                     std::vector<L>& state, bool rising) {
    if (in[0] == L::Zero) out[0] = in[1];
    else out[0] = L::Z;
}

// NODE：多驱动节点的“汇总点”，纯透传（真正解析在 simulate 里做）
inline void evalNode(const std::vector<L>& in, std::vector<L>& out,
                     std::vector<L>& state, bool rising) {
    out[0] = in[0];
}

// LED：把输入透传到输出，方便渲染时读取“显示值”
inline void evalLed(const std::vector<L>& in, std::vector<L>& out,
                    std::vector<L>& state, bool rising) {
    out[0] = in[0];
}

inline int portIndex(const std::vector<Port>& port, const std::string& name) {
    for (size_t i = 0; i < port.size(); i++)
        if (port[i].name == name) return (int)i;
    return -1;
}

inline char toChar(L v) {
    switch (v) {
        case L::One:  return '1';
        case L::Zero: return '0';
        case L::X:    return 'X';
        case L::Z:    return 'Z';
    }
    return '?';
}

// 节点解析：收集所有驱动值，按开关级规则合并
// 有 0 又有 1 → 冲突 X；只有 0 → 0；只有 1 → 1；全 Z/空 → Z
inline L resolveNode(const std::vector<L>& vals) {
    bool h0 = false, h1 = false;
    for (L v : vals) {
        if (v == L::Zero) h0 = true;
        else if (v == L::One) h1 = true;
    }
    if (h0 && h1) return L::X;
    if (h0) return L::Zero;
    if (h1) return L::One;
    return L::Z;
}

// ===== 递归仿真核心（含多驱动节点解析）=====
inline void simulate(const ComponentType& top,
                     const std::vector<L>& extIn,
                     std::vector<L>& extOut,
                     std::vector<std::vector<L>>& st,
                     std::vector<std::vector<L>>& in,
                     std::vector<std::vector<L>>& out) {
    int n = (int)top.sub.size();
    for (int i = 0; i < n; i++) {
        auto* t = top.sub[i].type;
        in[i].assign(t->inputs.size(),  L::X);
        out[i].assign(t->outputs.size(), L::X);
    }
    // 多驱动累积器：drive[i][p] = 第 i 实例第 p 输入端口收到的所有驱动值
    std::vector<std::vector<std::vector<L>>> drive(n);
    for (int i = 0; i < n; i++)
        drive[i].assign(top.sub[i].type->inputs.size(), {});

    const int MAX_ITER = 100;
    for (int iter = 0; iter < MAX_ITER; iter++) {
        bool changed = false;
        // 清空累积器
        for (int i = 0; i < n; i++)
            for (size_t p = 0; p < drive[i].size(); p++) drive[i][p].clear();

        // ① 线传播：把每条线的源头输出值，累积到目的输入端口的驱动列表
        for (const auto& w : top.subWires) {
            L v;
            if (w.fromId < 0) {
                int p = portIndex(top.inputs, w.fromPort);
                v = (p >= 0) ? extIn[p] : L::X;
            } else {
                int fi = portIndex(top.sub[w.fromId].type->outputs, w.fromPort);
                v = out[w.fromId][fi];
            }
            if (w.toId < 0) {
                int p = portIndex(top.outputs, w.toPort);
                if (p >= 0 && extOut[p] != v) { extOut[p] = v; changed = true; }
            } else {
                int ti = portIndex(top.sub[w.toId].type->inputs, w.toPort);
                drive[w.toId][ti].push_back(v);
            }
        }
        // ①b 解析每个输入端口（多驱动合并）
        for (int i = 0; i < n; i++) {
            int m = (int)top.sub[i].type->inputs.size();
            for (int p = 0; p < m; p++) {
                L r = resolveNode(drive[i][p]);
                if (in[i][p] != r) { in[i][p] = r; changed = true; }
            }
        }
        // ② 算每个实例
        for (int i = 0; i < n; i++) {
            auto* t = top.sub[i].type;
            if (t->eval) {
                std::vector<L> old = out[i];
                t->eval(in[i], out[i], st[i], false);
                if (out[i] != old) changed = true;
            } else if (t->kind == CompKind::Sub) {
                // 复合：递归下钻。子图用本地 in/out/st
                std::vector<std::vector<L>> subSt(t->sub.size());
                std::vector<std::vector<L>> subIn(t->sub.size()), subOut(t->sub.size());
                simulate(*t, in[i], out[i], subSt, subIn, subOut);
            }
            // 否则：primitive 无 eval（终点），跳过，值已在 in[i]
        }
        if (!changed) break;
    }
}

inline void tick(ComponentType& top,
                 const std::vector<int>& clocks,
                 const std::vector<int>& leds,
                 const std::vector<int>& switches,
                 const std::vector<L>& switchVals,
                 std::vector<std::vector<L>>& st) {
    // ① 时钟翻转
    for (int id : clocks) {
        if (st[id].empty()) st[id].assign(1, L::Zero);
        st[id][0] = (st[id][0] == L::One) ? L::Zero : L::One;
    }
    // ② 注入 switch 值（外部输入）
    for (size_t k = 0; k < switches.size(); k++) {
        int id = switches[k];
        if (st[id].empty()) st[id].assign(1, L::Zero);
        st[id][0] = switchVals[k];
    }
    // ③ 递归仿真
    int n = (int)top.sub.size();
    std::vector<std::vector<L>> in(n), out(n);
    std::vector<L> dummyIn, dummyOut;
    simulate(top, dummyIn, dummyOut, st, in, out);
    // ④ 读 LED（LED 已透传：out[id][0] = 显示值）
    for (int id : leds) {
        auto* t = top.sub[id].type;
        std::cout << "LED #" << id << ": ";
        for (size_t p = 0; p < t->outputs.size(); p++)
            std::cout << t->outputs[p].name << "=" << toChar(out[id][p]) << " ";
        std::cout << "\n";
    }
}

inline void printNetlist(const ComponentType& t) {
    std::cout << "元件类型: " << t.name
              << (t.kind == CompKind::Sub ? " (复合)" : " (原子)") << "\n";
    std::cout << "  输入: ";
    for (auto& p : t.inputs) std::cout << p.name << " ";
    std::cout << "\n  输出: ";
    for (auto& p : t.outputs) std::cout << p.name << " ";
    std::cout << "\n  内部实例:\n";
    for (auto& inst : t.sub)
        std::cout << "    #" << inst.id << " " << inst.type->name << "\n";
    std::cout << "  连线:\n";
    for (auto& w : t.subWires) {
        std::string f = w.fromId < 0 ? "EXT:" + w.fromPort
                                     : "#" + std::to_string(w.fromId) + "." + w.fromPort;
        std::string tt = w.toId < 0 ? "EXT:" + w.toPort
                                    : "#" + std::to_string(w.toId) + "." + w.toPort;
        std::cout << "    " << f << " -> " << tt << "\n";
    }
}

// ===== 共享元件库（原子类型）=====
// 只放 primitive：primitive 不含指向其它类型的指针，可安全拷贝进 static vector。
// 复合门由编辑器引用这里的指针来搭建（见 circuit.hpp 的 GateDef）。
inline const std::vector<ComponentType>& primLib() {
    static std::vector<ComponentType> lib;
    if (!lib.empty()) return lib;

    ComponentType vdd, gnd, nmos, pmos, node, clk, led, sw;

    vdd.name = "VDD";  vdd.kind  = CompKind::Primitive; vdd.outputs.push_back({"v", 1});
    gnd.name = "GND";  gnd.kind  = CompKind::Primitive; gnd.outputs.push_back({"g", 1});
    nmos.name = "NMOS"; nmos.kind = CompKind::Primitive;
    nmos.inputs.push_back({"gate", 1}); nmos.inputs.push_back({"source", 1});
    nmos.outputs.push_back({"drain", 1});
    pmos.name = "PMOS"; pmos.kind = CompKind::Primitive;
    pmos.inputs.push_back({"gate", 1}); pmos.inputs.push_back({"source", 1});
    pmos.outputs.push_back({"drain", 1});
    node.name = "NODE"; node.kind = CompKind::Primitive;
    node.inputs.push_back({"i", 1}); node.outputs.push_back({"o", 1});
    clk.name = "CLOCK"; clk.kind = CompKind::Primitive; clk.outputs.push_back({"y", 1});
    led.name = "LED"; led.kind = CompKind::Primitive;
    led.inputs.push_back({"d", 1}); //led.outputs.push_back({"x", 1});
    sw.name = "SWITCH"; sw.kind = CompKind::Primitive;
    sw.inputs.push_back({"s", 1}); sw.outputs.push_back({"y", 1});
    // buf.name = "BUF"; buf.kind = CompKind::Primitive;
    // buf.inputs.push_back({"a", 1}); buf.outputs.push_back({"y", 1});

    vdd.eval = evalVdd;  gnd.eval = evalGnd;
    nmos.eval = evalNmos; pmos.eval = evalPmos; node.eval = evalNode;
    clk.eval = evalClock; led.eval = evalLed;
    sw.eval = evalSwitch; // buf.eval = evalBuf;

    lib.push_back(vdd); lib.push_back(gnd); lib.push_back(nmos);
    lib.push_back(pmos); lib.push_back(node); lib.push_back(clk);
    lib.push_back(led); lib.push_back(sw); // lib.push_back(buf);
    return lib;
}

// 按名字取原子类型指针（编辑器搭建复合门时用）
inline const ComponentType* prim(const std::string& name) {
    for (auto& c : primLib()) if (c.name == name) return &c;
    return nullptr;
}

#endif // NETLIST_HPP
