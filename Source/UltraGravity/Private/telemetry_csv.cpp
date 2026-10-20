#include "ultragravity/telemetry_csv.h"

#include <array>
#include <charconv>
#include <ostream>
#include <string>
#include <string_view>

namespace ultragravity {
namespace {

constexpr std::array<std::string_view, wheel_count> wheel_suffixes{"fl", "fr", "rl", "rr"};

// Writes fields separated by commas and ends the row.
class Row {
public:
    explicit Row(std::ostream& out) : out_(out) {}
    ~Row() { out_ << '\n'; }

    void text(std::string_view t) {
        separate();
        out_ << t;
    }
    void number(double value) {
        separate();
        std::array<char, 32> buffer;
        const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        out_.write(buffer.data(), result.ptr - buffer.data());
    }
    void integer(long long value) {
        separate();
        out_ << value;
    }

private:
    void separate() {
        if (!first_) out_ << ',';
        first_ = false;
    }

    std::ostream& out_;
    bool first_ = true;
};

void header_per_wheel(Row& row, const std::string& name, const std::string& unit) {
    for (std::string_view wheel : wheel_suffixes) row.text(name + "_" + std::string(wheel) + "[" + unit + "]");
}

void values_per_wheel(Row& row, const std::array<double, wheel_count>& values) {
    for (double w : values) row.number(w);
}

} // namespace

void write_telemetry_header(std::ostream& out) {
    Row row(out);
    row.text("tick[1]");
    row.text("speed[m/s]");
    row.text("engine_speed[rad/s]");
    row.text("gear[1]");
    row.text("steering[1]");
    row.text("throttle[1]");
    row.text("brake[1]");
    row.text("clutch[1]");
    row.text("handbrake[1]");
    header_per_wheel(row, "slip", "1");
    header_per_wheel(row, "slip_angle", "rad");
    header_per_wheel(row, "wheel_load", "N");
    header_per_wheel(row, "force_longitudinal", "N");
    header_per_wheel(row, "force_lateral", "N");
}

void write_telemetry_row(std::ostream& out, const InputPacket& input, const StepResult& result) {
    Row row(out);
    row.integer(input.tick);
    row.number(result.telemetry.speed);
    row.number(result.state.engine_speed);
    row.integer(result.state.gear);
    row.number(input.steering);
    row.number(input.throttle);
    row.number(input.brake);
    row.number(input.clutch);
    row.number(input.handbrake);
    values_per_wheel(row, result.telemetry.slip);
    values_per_wheel(row, result.telemetry.slip_angle);
    values_per_wheel(row, result.telemetry.wheel_load);
    values_per_wheel(row, result.telemetry.force_longitudinal);
    values_per_wheel(row, result.telemetry.force_lateral);
}

} // namespace ultragravity
