#pragma once

#include "common/ellipse.h"
#include <random>

namespace Client {

    /**
     * @brief Generates random ellipses for the client.
     */
    class EllipseGenerator {
    public:
        /**
         * @brief Constructor.
         * @param seed The seed for the random number generator for deterministic output.
         */
        explicit EllipseGenerator(unsigned int seed);

        /**
         * @brief Generates a new random ellipse.
         * Center coordinates (cx, cy) are within [-50, 50].
         * Axes lengths (a, b) are within [1, 25].
         * @return A randomly generated Ellipse.
         */
        Ellipse generate_ellipse();

    private:
        std::mt19937 random_engine_;
        std::uniform_real_distribution<double> center_dist_;
        std::uniform_real_distribution<double> axis_dist_;
    };

} // namespace Client