#ifndef D435I_BRINGUP__STREAM_STATS_HPP_
#define D435I_BRINGUP__STREAM_STATS_HPP_

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace d435i_bringup
{
struct StreamStats
{
  uint64_t count{};
  uint64_t valid_ages{};
  double first_receive{};
  double last_receive{};
  double age_sum_ms{};
  double max_age_ms{};
  double max_gap_ms{};

  void observe(double receive_seconds, double age_seconds)
  {
    if (count == 0) {
      first_receive = receive_seconds;
    } else {
      max_gap_ms = std::max(max_gap_ms, (receive_seconds - last_receive) * 1000.0);
    }
    ++count;
    last_receive = receive_seconds;
    // Negative ages indicate timestamp/clock mismatch, not negative latency.
    if (std::isfinite(age_seconds) && age_seconds >= 0) {
      ++valid_ages;
      const double age_ms = age_seconds * 1000.0;
      age_sum_ms += age_ms;
      max_age_ms = std::max(max_age_ms, age_ms);
    }
  }

  double hz() const
  {
    const double duration = last_receive - first_receive;
    return count > 1 && duration > 0 ? (count - 1) / duration : 0.0;
  }

  double mean_age_ms() const
  {
    return valid_ages ? age_sum_ms / valid_ages : 0.0;
  }

  bool active(double now_seconds, double max_silence_seconds) const
  {
    return count > 0 && now_seconds >= last_receive &&
           now_seconds - last_receive <= max_silence_seconds;
  }
};
}  // namespace d435i_bringup

#endif  // D435I_BRINGUP__STREAM_STATS_HPP_
