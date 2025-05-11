#pragma once

#include "common/ellipse.h"
#include <random>
#include <vector>

namespace Server {

    /**
     * @brief Holds the results of a Monte Carlo simulation.
     */
    struct MonteCarloResult {
        double covered_area;
        double percentage_covered;
    };

    /**
     * @brief Performs Monte Carlo simulation to estimate area covered by ellipses.
     */
    class MonteCarloSimulator {
    public:
        /**
         * @brief Constructor.
         */
        MonteCarloSimulator();

        /**
         * @brief Adds an ellipse to the simulator.
         * @param ellipse The ellipse to add.
         */
        void add_ellipse(const Ellipse &ellipse);

        /**
         * @brief Estimates the total area covered by all added ellipses.
         * Runs trials until the estimated area stabilizes (relative error <= 1%).
         * @return A MonteCarloResult struct with the covered area and percentage.
         */
        MonteCarloResult estimate_area();

        /**
         * @brief Clears all stored ellipses.
         */
        void clear_ellipses();

        /**
         * @brief Gets the number of ellipses currently stored.
         * @return The count of ellipses.
         */
        size_t get_ellipse_count() const;

    private:
        std::vector<Ellipse> ellipses_;
        std::mt19937 random_generator_; // For generating points in simulation

        // Constants for simulation
        static constexpr int POINTS_PER_BATCH = 1000;
        static constexpr double TARGET_RELATIVE_ERROR = 0.01;          // 1%
        static constexpr long long MIN_SAMPLES_FOR_ERROR_CHECK = 5000; // 5000; // Minimum total points before checking error
        static constexpr long long MAX_TOTAL_SAMPLES = 20000000;       // 20000000; // Safety cap for samples
    };

} // namespace Server