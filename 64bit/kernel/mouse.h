#pragma once
namespace mouse {
    bool init();
    bool available();
    bool pressed();
    bool take(int& dx, int& dy, bool& click);
}
