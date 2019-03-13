#include "network/fragment_input_stream.h"

namespace lightstep {
//--------------------------------------------------------------------------------------------------
// ComputeFragmentPosition
//--------------------------------------------------------------------------------------------------
std::tuple<int, int, int> ComputeFragmentPosition(
    std::initializer_list<const FragmentInputStream*> fragment_input_streams,
    int n) noexcept {
  int fragment_input_stream_index = 0;
  for (auto fragment_input_stream : fragment_input_streams) {
    int fragment_index = 0;
    auto f = [&n, &fragment_index](void* /*data*/, int size) {
      if (n < size) {
        return false;
      }
      ++fragment_index;
      n -= size;
      return true;
    };
    if (!fragment_input_stream->ForEachFragment(f)) {
      return std::make_tuple(fragment_input_stream_index, fragment_index, n);
    }
    ++fragment_input_stream_index;
  }
  return {static_cast<int>(fragment_input_streams.size()), 0, 0};
}
}  // namespace lightstep