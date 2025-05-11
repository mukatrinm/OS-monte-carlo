#pragma once

/**
 * @brief Represents an ellipse in a 2D plane.
 */
struct Ellipse {
    double cx, cy; // Center coordinates
    double a, b;   // Axes lengths (horizontal a, vertical b)

    /**
     * @brief Checks if a given point (x, y) is inside or on the boundary of the ellipse.
     * The point (x,y) is inside if ((x-cx)/a)^2 + ((y-cy)/b)^2 <= 1.
     * Assumes a > 0 and b > 0.
     * @param x_coord The x-coordinate of the point.
     * @param y_coord The y-coordinate of the point.
     * @return True if the point is inside or on the ellipse, false otherwise.
     */
    bool is_inside(double x_coord, double y_coord) const;
};