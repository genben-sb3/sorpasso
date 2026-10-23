#include "ultragravity/test_driver.h"

#include <algorithm>
#include <cstdint>

namespace ultragravity {

TestDriver::TestDriver(const VehicleParams& k)
    : gears_(static_cast<int>(k.drivetrain.gearbox.ratios.size())), shift_pause_(k.drivetrain.gearbox.shift_time + 0.05) {}

InputPacket TestDriver::packet(const TestWorld& world) {
    InputPacket p;
    p.gear_request = static_cast<std::int8_t>(gear_);
    if (time_ < engage) {
        p.clutch = 1.0;
    } else if (time_ < engage + clutch_launch) {
        p.throttle = throttle();
        p.clutch = 1.0 - (time_ - engage) / clutch_launch;
    } else {
        const double since_shift = time_ - shifted_; // s
        if (gear_ < gears_ && since_shift > shift_pause_ + clutch_shift &&
            world.last_step().animation.engine_speed > shift_speed) {
            ++gear_;
            shifted_ = time_;
            p.gear_request = static_cast<std::int8_t>(gear_);
            p.clutch = 1.0;
        } else if (since_shift < shift_pause_) {
            p.clutch = 1.0;
        } else {
            p.throttle = throttle();
            p.clutch = std::max(1.0 - (since_shift - shift_pause_) / clutch_shift, 0.0);
        }
    }
    time_ += TestWorld::time_step;
    return p;
}

} // namespace ultragravity
