#include "engine/clock.h"

namespace engine {

Timestamp SimulationClock::Now() const {
    return current_time_;
}

void SimulationClock::AdvanceTo(Timestamp time) {
    if (time > current_time_) {
        current_time_ = time;
    }
}

void SimulationClock::Reset() {
    current_time_ = 0;
}

}  // namespace engine
