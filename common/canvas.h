#pragma once

namespace Canvas {
    constexpr double MIN_X = -50;
    constexpr double MAX_X = 50;
    constexpr double MIN_Y = -50;
    constexpr double MAX_Y = 50;

    /**
     * @brief Gets the width of the canvas.
     * @return The width.
     */
    inline double get_width() { return MAX_X - MIN_X; }

    /**
     * @brief Gets the height of the canvas.
     * @return The height.
     */
    inline double get_height() { return MAX_Y - MIN_Y; }

    /**
     * @brief Gets the total area of the canvas.
     * @return The total area in units squared.
     */
    inline double get_area() { return get_width() * get_height(); }
} // namespace Canvas