#pragma once
#include "multiboot.h"
namespace desktop {
    bool init(const MultibootInfo* info);
    bool active();
    void invalidate();
    void input(char c);
    void render();
    void show_terminal();
    void edit(const char* path);
}
