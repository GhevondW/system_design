#pragma once

#include <cstdint>

#include "engine/event.h"

namespace engine {

class SimulationClock {
public:
    SimulationClock() = default;

    [[nodiscard]] Timestamp Now() const;
    void AdvanceTo(Timestamp time);
    void Reset();

private:
    Timestamp current_time_ = 0;
};

}  // namespace engine
