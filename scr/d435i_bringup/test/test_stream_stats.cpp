#include "d435i_bringup/stream_stats.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

void require(bool condition)
{
  if (!condition) { throw std::runtime_error("Invalid stream statistics"); }
}

int main()
{
  try {
    d435i_bringup::StreamStats stats;
    require(stats.hz() == 0 && stats.mean_age_ms() == 0);
    require(!stats.active(0.0, 1.0));
    stats.observe(0.0, 0.002);
    require(stats.hz() == 0);
    stats.observe(1.0 / 90.0, 0.004);
    stats.observe(2.0 / 90.0, 0.003);
    require(stats.count == 3 && stats.valid_ages == 3);
    require(std::abs(stats.hz() - 90.0) < 1e-8);
    require(std::abs(stats.mean_age_ms() - 3.0) < 1e-8);
    require(stats.active(0.5, 1.0));
    require(!stats.active(2.0, 1.0));
    require(!stats.active(-1.0, 1.0));
    require(std::abs(stats.max_age_ms - 4.0) < 1e-8);
    require(std::abs(stats.max_gap_ms - 1000.0 / 90.0) < 1e-8);
    stats.observe(3.0 / 90.0, -0.001);
    stats.observe(4.0 / 90.0, std::numeric_limits<double>::quiet_NaN());
    require(stats.count == 5 && stats.valid_ages == 3);
    require(std::abs(stats.mean_age_ms() - 3.0) < 1e-8);
    std::cout << "Stream frequency, gap, freshness and timestamp-age checks passed\n";
    return 0;
  } catch (const std::exception & error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
