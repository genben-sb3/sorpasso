#include "ultragravity/pp_value.h"

#include <cmath>
#include <cstdint>
#include <cstdio>

#include "nlohmann/json.hpp"
#include "ultragravity/test_world.h"

namespace ultragravity {
namespace {

using json = nlohmann::json;

struct Criterion {
    Maneuver maneuver;
    double reference;      // in the unit of the measurement
    bool smaller_is_better;
    double weight;         // 1, sum of all weights: 1
};

constexpr Criterion criteria[] = {
    {Maneuver::acceleration_0_100, 6.0, true, 0.15},
    {Maneuver::acceleration_0_200, 20.0, true, 0.15},
    {Maneuver::skidpad, 1.0, false, 0.25},
    {Maneuver::braking_100_0, 40.0, true, 0.20},
    {Maneuver::top_speed, 250.0, false, 0.25},
};

// The reference drive for the physics version id: full throttle in first gear, then steering, then braking.
constexpr double reference_drop_height = 0.10; // m
constexpr double reference_duration = 6.0;     // s
constexpr double reference_steer_from = 3.0;   // s
constexpr double reference_brake_from = 5.0;   // s
constexpr double reference_steering = 0.2;     // 1
constexpr double reference_brake = 0.5;        // 1

bool whitespace(char c) { return c == ' ' || c == '\n' || c == '\t' || c == '\r'; }

// Position of the block "pp" in the text: from "\"pp\"" to behind the closing brace.
struct Span {
    std::size_t from = 0, to = 0;
};

std::optional<Span> find_pp_block(const std::string& text) {
    // Only search on the top level (depth 1), skip strings.
    int depth = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '"') {
            std::size_t end = i + 1;
            while (end < text.size() && text[end] != '"') end += text[end] == '\\' ? 2 : 1;
            if (depth == 1 && text.compare(i, 4, "\"pp\"") == 0) {
                std::size_t k = end + 1;
                while (k < text.size() && whitespace(text[k])) ++k;
                if (k < text.size() && text[k] == ':') {
                    const std::size_t open = text.find('{', k);
                    if (open == std::string::npos) return std::nullopt;
                    int braces = 0;
                    for (std::size_t j = open; j < text.size(); ++j) {
                        if (text[j] == '{') ++braces;
                        if (text[j] == '}' && --braces == 0) return Span{i, j + 1};
                    }
                    return std::nullopt;
                }
            }
            i = end;
        } else if (c == '{' || c == '[') {
            ++depth;
        } else if (c == '}' || c == ']') {
            --depth;
        }
    }
    return std::nullopt;
}

std::string block_text(const StoredPp& pp) {
    return "\"pp\": { \"value\": " + std::to_string(pp.value) + ", \"physics_version\": \"" + pp.physics_version + "\" }";
}

} // namespace

PpResult compute_pp(const std::vector<Measurement>& measurements) {
    double log_sum = 0.0; // 1
    std::string errors;
    for (const Criterion& k : criteria) {
        const Measurement* found = nullptr;
        for (const Measurement& m : measurements) {
            if (m.maneuver == k.maneuver) found = &m;
        }
        if (!found) {
            errors += std::string("  ") + maneuver_id(k.maneuver) + ": not driven\n";
            continue;
        }
        if (found->failed()) {
            errors += "  " + found->name + ": " + found->reason + '\n';
            continue;
        }
        const double result = *found->result; // in the unit of the measurement
        if (!(result > 0.0)) {
            errors += "  " + found->name + ": result " + std::to_string(result) + " is not usable\n";
            continue;
        }
        const double factor = k.smaller_is_better ? k.reference / result : result / k.reference; // 1
        log_sum += k.weight * std::log(factor);
    }
    PpResult out;
    if (!errors.empty()) {
        out.error = "No PP value, because test maneuvers are missing or failed:\n" + errors;
        return out;
    }
    out.value = static_cast<int>(std::lround(100.0 * std::exp(log_sum)));
    return out;
}

std::string physics_version_id(const VehicleParams& params) {
    TestWorld world(params, reference_drop_height);
    const int ticks = static_cast<int>(std::lround(reference_duration / TestWorld::time_step));
    for (int i = 0; i < ticks; ++i) {
        const double t = i * TestWorld::time_step; // s
        InputPacket input;
        input.tick = static_cast<std::uint32_t>(i);
        input.throttle = 1.0;
        input.gear_request = 1;
        input.steering = t > reference_steer_from ? reference_steering : 0.0;
        input.brake = t > reference_brake_from ? reference_brake : 0.0;
        world.tick(input);
    }
    char buffer[17];
    std::snprintf(buffer, sizeof buffer, "%016llx", static_cast<unsigned long long>(world.checksum()));
    return buffer;
}

std::optional<StoredPp> read_pp(const std::string& json_text) {
    const json root = json::parse(json_text, nullptr, false);
    if (!root.is_object() || !root.contains("pp")) return std::nullopt;
    const json& pp = root["pp"];
    if (!pp.is_object() || !pp.contains("value") || !pp["value"].is_number_integer() || !pp.contains("physics_version") ||
        !pp["physics_version"].is_string()) {
        return std::nullopt;
    }
    return StoredPp{pp["value"].get<int>(), pp["physics_version"].get<std::string>()};
}

std::string set_pp(const std::string& json_text, const StoredPp& pp) {
    const json root = json::parse(json_text, nullptr, false);
    if (!root.is_object()) return {};
    if (const auto span = find_pp_block(json_text)) {
        return json_text.substr(0, span->from) + block_text(pp) + json_text.substr(span->to);
    }
    std::size_t before = json_text.rfind('}');
    while (before > 0 && whitespace(json_text[before - 1])) --before;
    return json_text.substr(0, before) + ",\n  " + block_text(pp) + json_text.substr(before);
}

std::string pp_warning(const std::optional<StoredPp>& stored, const std::string& current_id) {
    if (!stored || stored->physics_version == current_id) return {};
    return "Warning: the stored PP value (" + std::to_string(stored->value) + ", physics version " +
           stored->physics_version + ") is outdated, the current physics version is " + current_id +
           ". Recompute the PP value with --write-pp.\n";
}

} // namespace ultragravity
