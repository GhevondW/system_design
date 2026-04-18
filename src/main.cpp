#include <iostream>

#include "engine/version.h"

int main() {
    std::cout << "SystemForge v" << engine::Version() << std::endl;
    return 0;
}
