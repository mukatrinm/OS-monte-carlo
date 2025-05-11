#include "ellipse.h"

bool Ellipse::is_inside(double x_coord, double y_coord) const {
    if (a <= 0 || b <= 0) {
        return false;
    }
    double term_x = (x_coord - cx) / a;
    double term_y = (y_coord - cy) / b;
    return (term_x * term_x + term_y * term_y) <= 1.0;
}