// Measures a car: drives the test maneuvers and prints target, result and
// deviation as a table.
//
//   tune <vehicle.json> [--telemetry <directory>]
//
// With --telemetry it writes the telemetry of each test maneuver as <maneuver>.csv.
// Exit code 0 when all test maneuvers succeeded, 1 for failed ones, 2 for
// wrong usage or an unreadable vehicle file.
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "ultragravity/test_maneuver.h"
#include "ultragravity/vehicle_file.h"

using namespace ultragravity;

int main(int argc, char** argv) {
    std::string path;
    std::string telemetry_directory;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--telemetry") == 0 && i + 1 < argc) {
            telemetry_directory = argv[++i];
        } else if (path.empty() && argv[i][0] != '-') {
            path = argv[i];
        } else {
            path.clear();
            break;
        }
    }
    if (path.empty()) {
        std::cerr << "Usage: tune <vehicle.json> [--telemetry <directory>]\n";
        return 2;
    }

    const LoadResult loaded = load_vehicle_file(path);
    if (loaded.error) {
        std::cerr << path << ": " << (loaded.error->field.empty() ? "" : loaded.error->field + " ")
                  << loaded.error->message << '\n';
        return 2;
    }

    std::vector<Measurement> measurements;
    for (Maneuver maneuver : all_maneuvers) {
        std::ofstream file;
        if (!telemetry_directory.empty()) {
            file.open(telemetry_directory + "/" + maneuver_id(maneuver) + ".csv", std::ios::binary);
            if (!file) {
                std::cerr << "cannot write a telemetry file into " << telemetry_directory << '\n';
                return 2;
            }
        }
        measurements.push_back(run_test_maneuver(maneuver, loaded.params, file.is_open() ? &file : nullptr));
    }

    std::cout << format_table(loaded.params.name, measurements);
    const bool all_succeeded =
        std::none_of(measurements.begin(), measurements.end(), [](const Measurement& m) { return m.failed(); });
    return all_succeeded ? 0 : 1;
}
