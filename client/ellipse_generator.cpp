#include "ellipse_generator.h"

namespace Client {

    EllipseGenerator::EllipseGenerator(unsigned int seed)
        : random_engine_{seed},
          center_dist_{-50, 50}, // Ellipse centers
          axis_dist_{1.0, 25.0}  // Ellipse axes lengths (a, b)
    {}

    Ellipse EllipseGenerator::generate_ellipse() {
        Ellipse e;
        e.cx = center_dist_(random_engine_);
        e.cy = center_dist_(random_engine_);
        e.a = axis_dist_(random_engine_);
        e.b = axis_dist_(random_engine_);

        // e = {0, 0, 3.0, 3.0}; // For testing
        return e;
    }

} // namespace Client