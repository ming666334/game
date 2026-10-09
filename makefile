# circuit_game 专用 makefile —— 只编本目录的游戏代码，链接引擎静态库
# 用法：在 circuit_game/ 目录里执行  mingw32-make

CXX      = g++
# -MMD -MP：编译时自动生成 .d 依赖文件，任何被包含的 .hpp 改动都会触发重编
CXXFLAGS = -std=c++17 -Wall -Wextra -g -MMD -MP -I../engine -I../engine/core -I../engine/gfx -I../engine/platform -I../engine/scene -I../engine/utils -Isim -Iapp -Igame -Ieditor -Iview
LDFLAGS  = -mwindows -lopengl32 -lgdi32
ENGINE   = ../engine/libengine.a

# 本游戏自己的源文件（只列 circuit_game/ 下的，绝不列 game1/）
GAMESRCS = app/main.cpp game/game.cpp view/cam.cpp editor/circuit.cpp editor/wire.cpp
GAMEOBJS = $(GAMESRCS:.cpp=.o)
DEPS     = $(GAMEOBJS:.o=.d)

all: app.exe

app.exe: $(ENGINE) $(GAMEOBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(GAMEOBJS) $(ENGINE) $(LDFLAGS)

# 引擎库不存在/过期时，递归进 engine/ 编它
# 用 FORCE 强制每次都去 engine/ 核对时间戳，否则 make 看到 libengine.a
# 已存在就会跳过，导致改了 window.cpp 却没重新链进库（典型症状：undefined reference）
$(ENGINE): FORCE
	$(MAKE) -C ../engine
FORCE:

# 自动依赖：引入每个 .o 对应的 .d，实现“改任意头文件都重编”
-include $(DEPS)

clean:
	rm -f $(GAMEOBJS) $(DEPS) app.exe editor/link.o editor/link.d
	$(MAKE) -C ../engine clean
