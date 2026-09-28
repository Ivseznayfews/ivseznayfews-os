#pragma once
#include "idt.h"

namespace isr {
    void init();
    void install_handlers();
}
