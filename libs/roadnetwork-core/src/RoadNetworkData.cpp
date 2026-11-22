// SPDX-License-Identifier: Apache-2.0
#include "openstrata/rn/RoadNetworkData.h"

#include <algorithm>
#include <tuple>
#include <utility>

namespace openstrata::rn {

namespace {

// std::string compares by char_traits<char>::compare, which orders like
// memcmp on unsigned bytes: the byte order ADR 0001 section 3 requires.
auto Tie(const LaneEnd& end) {
  return std::tie(end.lane.roadId, end.lane.sectionIndex, end.lane.laneNumber, end.end);
}

bool operator<(const LaneLink& lhs, const LaneLink& rhs) {
  if (lhs.a != rhs.a) {
    return lhs.a < rhs.a;
  }
  return lhs.b < rhs.b;
}

bool operator==(const LaneLink& lhs, const LaneLink& rhs) {
  return lhs.a == rhs.a && lhs.b == rhs.b;
}

} // namespace

bool operator<(const LaneEnd& lhs, const LaneEnd& rhs) noexcept {
  return Tie(lhs) < Tie(rhs);
}

bool operator==(const LaneEnd& lhs, const LaneEnd& rhs) noexcept {
  return Tie(lhs) == Tie(rhs);
}

bool operator!=(const LaneEnd& lhs, const LaneEnd& rhs) noexcept {
  return !(lhs == rhs);
}

void CanonicalizeLaneLinks(std::vector<LaneLink>& links) {
  for (LaneLink& link : links) {
    if (link.b < link.a) {
      std::swap(link.a, link.b);
    }
  }
  std::sort(links.begin(), links.end(),
      [](const LaneLink& lhs, const LaneLink& rhs) { return lhs < rhs; });
  links.erase(std::unique(links.begin(), links.end(),
                  [](const LaneLink& lhs, const LaneLink& rhs) { return lhs == rhs; }),
      links.end());
}

} // namespace openstrata::rn
