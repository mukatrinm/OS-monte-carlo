#include "monte_carlo_simulator.h"
#include "common/canvas.h"
#include "common/point.h"
#include <cmath>
#include <iostream>

namespace Server {

    MonteCarloSimulator::MonteCarloSimulator()
        : random_generator_(std::random_device{}()) {}

    void MonteCarloSimulator::add_ellipse(const Ellipse &ellipse) {
        ellipses_.push_back(ellipse);
    }

    MonteCarloResult MonteCarloSimulator::estimate_area() {
        if (ellipses_.empty()) {
            return {0.0, 0.0};
        }

        const double TOTAL_CANVAS_AREA = Canvas::get_area();
        long long total_points_sampled = 0;
        long long points_inside_any_ellipse = 0;

        std::uniform_real_distribution<double> distrib_x(Canvas::MIN_X, Canvas::MAX_X);
        std::uniform_real_distribution<double> distrib_y(Canvas::MIN_Y, Canvas::MAX_Y);

        while (true) {
            for (int i = 0; i < POINTS_PER_BATCH; ++i) {
                Point p = {distrib_x(random_generator_), distrib_y(random_generator_)};
                total_points_sampled++;
                for (const auto &ellipse : ellipses_) {
                    if (ellipse.is_inside(p.x, p.y)) {
                        points_inside_any_ellipse++;
                        break; // Point is covered
                    }
                }
            }

            // If no points hit, continue sampling or determine area is effectively zero.
            if (points_inside_any_ellipse == 0) {
                if (total_points_sampled >= MAX_TOTAL_SAMPLES / 2 && ellipses_.size() > 0) { // Arbitrary large number for "effectively zero"
                    // If many samples and still no hits, likely area is very small or zero
                    break;
                }
                if (total_points_sampled >= MAX_TOTAL_SAMPLES)
                    break; // Safety break
                continue;  // Continue sampling
            }

            double current_estimated_proportion = static_cast<double>(points_inside_any_ellipse) / total_points_sampled;

            if (total_points_sampled < MIN_SAMPLES_FOR_ERROR_CHECK && total_points_sampled < MAX_TOTAL_SAMPLES) {
                continue; // Not enough samples yet to reliably check error
            }

            double relative_error;
            if (current_estimated_proportion <= 1e-9) {              // Proportion is effectively zero
                relative_error = 1.0;                                // High error, continue sampling
            } else if (current_estimated_proportion >= 1.0 - 1e-9) { // Proportion is effectively one
                relative_error = 0.0;                                // No error, effectively covers everything
            } else {
                relative_error = std::sqrt((1.0 - current_estimated_proportion) /
                                           (current_estimated_proportion * total_points_sampled));
            }

            if (relative_error <= TARGET_RELATIVE_ERROR) {
                // std::cout << "Stabilization achieved with relative error: " << relative_error * 100 << "%" << std::endl;
                break; // Stabilization achieved
            }

            if (total_points_sampled >= MAX_TOTAL_SAMPLES) {
                std::cerr << "Warning: Max samples (" << MAX_TOTAL_SAMPLES
                          << ") reached. Using current estimate with relative error: "
                          << relative_error * 100 << "%" << std::endl;
                break;
            }
        }

        // std::cout << "number of points inside any ellipse: " << points_inside_any_ellipse << std::endl;
        // std::cout << "total number of points sampled: " << total_points_sampled << std::endl;

        double final_proportion = (total_points_sampled == 0) ? 0.0 : static_cast<double>(points_inside_any_ellipse) / total_points_sampled;
        double covered_area = final_proportion * TOTAL_CANVAS_AREA;
        double percentage_covered = final_proportion * 100.0;

        return {covered_area, percentage_covered};
    }

    void MonteCarloSimulator::clear_ellipses() {
        ellipses_.clear();
    }

    size_t MonteCarloSimulator::get_ellipse_count() const {
        return ellipses_.size();
    }

} // namespace Server