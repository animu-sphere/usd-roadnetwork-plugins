// SPDX-License-Identifier: Apache-2.0
#include "openstrata/rn/Validation.h"

#include "PlanViewEnd.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace openstrata::rn {

namespace {

// The code is the bracketed prefix of the message, so the catalog
// cross-check (tests/test_diagnostics_catalog.py) finds every code raised.
Diagnostic Fail(const char* bracketed, const std::string& detail) {
  std::string code(bracketed);
  code = code.substr(1, code.size() - 2);
  return Diagnostic{code, std::string(bracketed) + " " + detail};
}

std::string Quote(const std::string& id) {
  return "\"" + id + "\"";
}

std::string RoadName(const Road& road) {
  return "road " + Quote(road.id);
}

std::string SectionName(const Road& road, std::size_t index) {
  return RoadName(road) + " lane section " + std::to_string(index);
}

std::string LaneKeyName(const LaneKey& key) {
  return "road " + Quote(key.roadId) + " lane section " + std::to_string(key.sectionIndex) +
         " lane " + std::to_string(key.laneNumber);
}

std::string LaneEndName(const LaneEnd& end) {
  return LaneKeyName(end.lane) + (end.end == ContactPoint::Start ? " (start)" : " (end)");
}

// Independent of the global locale, so messages are deterministic.
std::string Number(double value) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out.precision(9);
  out << value;
  return out.str();
}

struct Index {
  std::map<std::string, const Road*> roads;
  std::map<std::string, const Junction*> junctions;
};

// ---------------------------------------------------------------- identity

std::optional<Diagnostic> CheckIdentity(const RoadNetworkData& data, Index& index) {
  for (std::size_t i = 0; i < data.roads.size(); ++i) {
    const Road& road = data.roads[i];
    if (road.id.empty()) {
      return Fail("[RN-E101]", "road at position " + std::to_string(i) + " has an empty id");
    }
    if (!index.roads.emplace(road.id, &road).second) {
      return Fail("[RN-E102]", "road id " + Quote(road.id) + " is not unique");
    }
  }
  for (std::size_t i = 0; i < data.junctions.size(); ++i) {
    const Junction& junction = data.junctions[i];
    if (junction.id.empty()) {
      return Fail("[RN-E101]", "junction at position " + std::to_string(i) + " has an empty id");
    }
    if (!index.junctions.emplace(junction.id, &junction).second) {
      return Fail("[RN-E102]", "junction id " + Quote(junction.id) + " is not unique");
    }
  }
  for (const Junction& junction : data.junctions) {
    std::set<std::string> ids;
    for (std::size_t i = 0; i < junction.connections.size(); ++i) {
      const Connection& connection = junction.connections[i];
      if (connection.id.empty()) {
        return Fail("[RN-E101]", "junction " + Quote(junction.id) + " connection at position " +
                                     std::to_string(i) + " has an empty id");
      }
      if (!ids.insert(connection.id).second) {
        return Fail("[RN-E102]", "junction " + Quote(junction.id) + " connection id " +
                                     Quote(connection.id) + " is not unique");
      }
    }
  }
  return std::nullopt;
}

// -------------------------------------------------------------- references

bool LaneExists(const Index& index, const LaneKey& key) {
  const auto road = index.roads.find(key.roadId);
  if (road == index.roads.end() || key.sectionIndex >= road->second->sections.size()) {
    return false;
  }
  const std::vector<Lane>& lanes = road->second->sections[key.sectionIndex].lanes;
  return std::any_of(lanes.begin(), lanes.end(),
      [&](const Lane& lane) { return lane.number == key.laneNumber; });
}

std::optional<Diagnostic> CheckRoadLink(const Index& index, const Road& road,
    const std::optional<RoadLink>& link, const char* which) {
  if (!link) {
    return std::nullopt;
  }
  const bool isRoad = link->kind == RoadLink::Kind::Road;
  const bool found = isRoad ? index.roads.count(link->id) != 0 : index.junctions.count(link->id) != 0;
  if (!found) {
    return Fail("[RN-E111]", RoadName(road) + " " + which + " names " +
                                 (isRoad ? "road " : "junction ") + Quote(link->id) +
                                 ", which does not exist");
  }
  return std::nullopt;
}

std::optional<Diagnostic> CheckReferences(const RoadNetworkData& data, const Index& index) {
  for (const Road& road : data.roads) {
    if (auto failure = CheckRoadLink(index, road, road.predecessor, "predecessor")) {
      return failure;
    }
    if (auto failure = CheckRoadLink(index, road, road.successor, "successor")) {
      return failure;
    }
    if (road.junction && index.junctions.count(*road.junction) == 0) {
      return Fail("[RN-E112]", RoadName(road) + " belongs to junction " + Quote(*road.junction) +
                                   ", which does not exist");
    }
  }
  for (const Junction& junction : data.junctions) {
    for (const Connection& connection : junction.connections) {
      const std::string name =
          "junction " + Quote(junction.id) + " connection " + Quote(connection.id);
      for (const std::string* roadId : {&connection.incomingRoad, &connection.connectingRoad}) {
        if (index.roads.count(*roadId) == 0) {
          return Fail("[RN-E113]", name + " names " +
                                       (roadId == &connection.incomingRoad ? "incoming" : "connecting") +
                                       " road " + Quote(*roadId) + ", which does not exist");
        }
      }
      const Road& connecting = *index.roads.at(connection.connectingRoad);
      if (connecting.junction != junction.id) {
        return Fail("[RN-E114]", name + " names connecting road " + Quote(connecting.id) +
                                     ", which does not belong to the junction");
      }
    }
  }
  for (const LaneLink& link : data.laneLinks) {
    for (const LaneEnd* end : {&link.a, &link.b}) {
      if (!LaneExists(index, end->lane)) {
        return Fail("[RN-E115]", "a lane link names " + LaneEndName(*end) +
                                     ", which does not exist");
      }
    }
  }
  return std::nullopt;
}

// ---------------------------------------------------------- lane structure

std::optional<Diagnostic> CheckLaneStructure(const RoadNetworkData& data) {
  for (const Road& road : data.roads) {
    for (std::size_t s = 0; s < road.sections.size(); ++s) {
      const std::vector<Lane>& lanes = road.sections[s].lanes;
      const auto centers = std::count_if(lanes.begin(), lanes.end(),
          [](const Lane& lane) { return lane.number == 0; });
      if (centers != 1) {
        return Fail("[RN-E121]", SectionName(road, s) + " has " + std::to_string(centers) +
                                     " center lanes; it needs exactly one");
      }
      std::vector<std::int32_t> left;
      std::vector<std::int32_t> right;
      for (const Lane& lane : lanes) {
        if (lane.number > 0) {
          left.push_back(lane.number);
        } else if (lane.number < 0) {
          right.push_back(-lane.number);
        }
      }
      for (const bool isLeft : {true, false}) {
        std::vector<std::int32_t>& side = isLeft ? left : right;
        std::sort(side.begin(), side.end());
        const char* which = isLeft ? "left" : "right";
        const std::int32_t sign = isLeft ? 1 : -1;
        for (std::size_t i = 0; i < side.size(); ++i) {
          const auto expected = static_cast<std::int32_t>(i + 1);
          if (i > 0 && side[i] == side[i - 1]) {
            return Fail("[RN-E122]", SectionName(road, s) + " has " + which + " lane " +
                                         std::to_string(sign * side[i]) + " more than once");
          }
          if (side[i] != expected) {
            return Fail("[RN-E122]", SectionName(road, s) + " has no " + which + " lane " +
                                         std::to_string(sign * expected) + ", but has lane " +
                                         std::to_string(sign * side.back()));
          }
        }
      }
    }
  }
  return std::nullopt;
}

// ----------------------------------------------------------------- numeric

std::optional<Diagnostic> NotFinite(const std::string& element, const char* field) {
  return Fail("[RN-E301]", element + " " + field + " is not finite");
}

std::optional<Diagnostic> CheckCubicFinite(const std::string& element, const PiecewiseCubic& cubic) {
  for (std::size_t i = 0; i < cubic.records.size(); ++i) {
    const CubicRecord& r = cubic.records[i];
    for (double value : {r.s0, r.a, r.b, r.c, r.d}) {
      if (!std::isfinite(value)) {
        return Fail("[RN-E301]", element + " record " + std::to_string(i) +
                                     " has a value that is not finite");
      }
    }
  }
  return std::nullopt;
}

std::optional<Diagnostic> CheckSourceFinite(const RoadNetworkData& data) {
  if (data.source.offset) {
    const SourceOffset& o = *data.source.offset;
    for (double value : {o.x, o.y, o.z, o.hdg}) {
      if (!std::isfinite(value)) {
        return Fail("[RN-E301]", "the source offset has a value that is not finite");
      }
    }
  }
  return std::nullopt;
}

std::optional<Diagnostic> CheckFinite(const Road& road) {
  if (!std::isfinite(road.length)) {
    return NotFinite(RoadName(road), "length");
  }
  for (std::size_t i = 0; i < road.referenceLine.segments.size(); ++i) {
    const PlanViewSegment& g = road.referenceLine.segments[i];
    const Arc* arc = std::get_if<Arc>(&g.shape);
    for (double value : {g.s, g.x, g.y, g.hdg, g.length, arc ? arc->curvature : 0.0}) {
      if (!std::isfinite(value)) {
        return Fail("[RN-E301]", RoadName(road) + " plan-view segment " + std::to_string(i) +
                                     " has a value that is not finite");
      }
    }
  }
  if (auto failure = CheckCubicFinite(RoadName(road) + " elevation", road.elevation)) {
    return failure;
  }
  if (auto failure = CheckCubicFinite(RoadName(road) + " lane offset", road.laneOffset)) {
    return failure;
  }
  for (std::size_t i = 0; i < road.speeds.size(); ++i) {
    if (!std::isfinite(road.speeds[i].s) || !std::isfinite(road.speeds[i].maxMetresPerSecond)) {
      return Fail("[RN-E301]", RoadName(road) + " speed record " + std::to_string(i) +
                                   " has a value that is not finite");
    }
  }
  for (std::size_t s = 0; s < road.sections.size(); ++s) {
    if (!std::isfinite(road.sections[s].s)) {
      return NotFinite(SectionName(road, s), "s");
    }
    for (const Lane& lane : road.sections[s].lanes) {
      const std::string name = SectionName(road, s) + " lane " + std::to_string(lane.number) + " width";
      if (auto failure = CheckCubicFinite(name, lane.width)) {
        return failure;
      }
    }
  }
  return std::nullopt;
}

std::optional<Diagnostic> CheckLengths(const Road& road) {
  if (!(road.length > 0.0)) {
    return Fail("[RN-E302]", RoadName(road) + " length " + Number(road.length) + " is not positive");
  }
  for (std::size_t i = 0; i < road.referenceLine.segments.size(); ++i) {
    const double length = road.referenceLine.segments[i].length;
    if (!(length > 0.0)) {
      return Fail("[RN-E302]", RoadName(road) + " plan-view segment " + std::to_string(i) +
                                   " length " + Number(length) + " is not positive");
    }
  }
  return std::nullopt;
}

std::optional<Diagnostic> CheckStations(const Road& road) {
  constexpr double tolerance = ValidationTolerances::kStationMetres;
  const std::vector<PlanViewSegment>& segments = road.referenceLine.segments;
  if (segments.empty()) {
    return Fail("[RN-E303]", RoadName(road) + " has no plan-view segment");
  }
  double expected = 0.0;
  for (std::size_t i = 0; i < segments.size(); ++i) {
    if (std::abs(segments[i].s - expected) > tolerance) {
      return Fail("[RN-E303]", RoadName(road) + " plan-view segment " + std::to_string(i) +
                                   " starts at s = " + Number(segments[i].s) + ", expected " +
                                   Number(expected));
    }
    expected = segments[i].s + segments[i].length;
  }
  if (std::abs(expected - road.length) > tolerance) {
    return Fail("[RN-E303]", RoadName(road) + " plan view ends at s = " + Number(expected) +
                                 ", but the road length is " + Number(road.length));
  }

  if (road.sections.empty()) {
    return Fail("[RN-E304]", RoadName(road) + " has no lane section");
  }
  if (std::abs(road.sections.front().s) > tolerance) {
    return Fail("[RN-E304]", SectionName(road, 0) + " starts at s = " +
                                 Number(road.sections.front().s) + ", expected 0");
  }
  for (std::size_t i = 1; i < road.sections.size(); ++i) {
    if (!(road.sections[i].s > road.sections[i - 1].s)) {
      return Fail("[RN-E304]", SectionName(road, i) + " starts at s = " +
                                   Number(road.sections[i].s) +
                                   ", not after the previous section");
    }
  }
  return std::nullopt;
}

std::optional<Diagnostic> CheckContinuity(const Road& road) {
  const std::vector<PlanViewSegment>& segments = road.referenceLine.segments;
  constexpr double twoPi = 6.283185307179586476925286766559;
  for (std::size_t i = 1; i < segments.size(); ++i) {
    const detail::Pose2 end = detail::SegmentEnd(segments[i - 1]);
    const PlanViewSegment& next = segments[i];
    const double gap = std::hypot(next.x - end.x, next.y - end.y);
    if (gap > ValidationTolerances::kPositionMetres) {
      return Fail("[RN-E305]", RoadName(road) + " plan-view segment " + std::to_string(i) +
                                   " starts " + Number(gap) +
                                   " m from the end of the previous segment");
    }
    const double turn = std::remainder(next.hdg - end.hdg, twoPi);
    if (std::abs(turn) > ValidationTolerances::kHeadingRadians) {
      return Fail("[RN-E306]", RoadName(road) + " plan-view segment " + std::to_string(i) +
                                   " starts with a heading " + Number(turn) +
                                   " rad from the end of the previous segment");
    }
  }
  return std::nullopt;
}

std::optional<Diagnostic> CheckAscending(const std::string& element, const PiecewiseCubic& cubic) {
  for (std::size_t i = 1; i < cubic.records.size(); ++i) {
    if (!(cubic.records[i].s0 > cubic.records[i - 1].s0)) {
      return Fail("[RN-E307]", element + " record " + std::to_string(i) + " starts at s = " +
                                   Number(cubic.records[i].s0) +
                                   ", not after the previous record");
    }
  }
  return std::nullopt;
}

std::optional<Diagnostic> CheckCubics(const Road& road) {
  if (auto failure = CheckAscending(RoadName(road) + " elevation", road.elevation)) {
    return failure;
  }
  if (auto failure = CheckAscending(RoadName(road) + " lane offset", road.laneOffset)) {
    return failure;
  }
  for (std::size_t s = 0; s < road.sections.size(); ++s) {
    for (const Lane& lane : road.sections[s].lanes) {
      const std::string name = SectionName(road, s) + " lane " + std::to_string(lane.number) + " width";
      if (auto failure = CheckAscending(name, lane.width)) {
        return failure;
      }
    }
  }
  return std::nullopt;
}

std::optional<Diagnostic> CheckNumeric(const RoadNetworkData& data) {
  if (auto failure = CheckSourceFinite(data)) {
    return failure;
  }
  using RoadCheck = std::optional<Diagnostic> (*)(const Road&);
  const RoadCheck checks[] = {CheckFinite, CheckLengths, CheckStations, CheckContinuity, CheckCubics};
  for (const Road& road : data.roads) {
    for (RoadCheck check : checks) {
      if (auto failure = check(road)) {
        return failure;
      }
    }
  }
  return std::nullopt;
}

} // namespace

std::optional<Diagnostic> Validate(const RoadNetworkData& data) {
  Index index;
  if (auto failure = CheckIdentity(data, index)) {
    return failure;
  }
  if (auto failure = CheckReferences(data, index)) {
    return failure;
  }
  if (auto failure = CheckLaneStructure(data)) {
    return failure;
  }
  return CheckNumeric(data);
}

} // namespace openstrata::rn
