// SPDX-License-Identifier: Apache-2.0
// ADR 0001 section 6: every validation invariant fails with its RN- code, and the
// lane graph is canonical (section 3).
#include "openstrata/rn/RoadNetworkData.h"
#include "openstrata/rn/Validation.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace {

using namespace openstrata::rn;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
  }
}

Lane MakeLane(std::int32_t number, double width = 3.5) {
  Lane lane;
  lane.number = number;
  lane.type = number == 0 ? "none" : "driving";
  if (number != 0) {
    lane.width.records.push_back(CubicRecord{0.0, width, 0.0, 0.0, 0.0});
  }
  return lane;
}

LaneEnd End(const std::string& road, std::uint32_t section, std::int32_t lane, ContactPoint end) {
  return LaneEnd{LaneKey{road, section, lane}, end};
}

// A valid network: road "1" (a line, then a left-turning arc, in two lane
// sections) leads into junction "J", whose connecting road "2" is a line.
RoadNetworkData MakeValid() {
  RoadNetworkData data;
  data.source.format = "opendrive";
  data.source.versionMajor = 1;
  data.source.versionMinor = 6;
  data.source.offset = SourceOffset{100.0, 200.0, 0.0, 0.0};

  Road road1;
  road1.id = "1";
  road1.length = 20.0;
  road1.successor = RoadLink{RoadLink::Kind::Junction, "J", std::nullopt};
  road1.referenceLine.segments.push_back(PlanViewSegment{0.0, 0.0, 0.0, 0.0, 10.0, Line{}});
  road1.referenceLine.segments.push_back(PlanViewSegment{10.0, 10.0, 0.0, 0.0, 10.0, Arc{0.1}});
  road1.elevation.records.push_back(CubicRecord{0.0, 1.0, 0.01, 0.0, 0.0});
  road1.laneOffset.records.push_back(CubicRecord{0.0, 0.0, 0.0, 0.0, 0.0});
  road1.laneOffset.records.push_back(CubicRecord{10.0, 0.0, 0.0, 0.001, 0.0});
  road1.speeds.push_back(SpeedRecord{0.0, 13.9});
  LaneSection first;
  first.lanes = {MakeLane(1), MakeLane(0), MakeLane(-1)};
  LaneSection second;
  second.s = 10.0;
  second.lanes = {MakeLane(0), MakeLane(-1), MakeLane(-2)};
  second.lanes[1].width.records.push_back(CubicRecord{5.0, 3.5, 0.1, 0.0, 0.0});
  road1.sections = {first, second};

  Road road2;
  road2.id = "2";
  road2.length = 5.0;
  road2.junction = "J";
  road2.predecessor = RoadLink{RoadLink::Kind::Road, "1", ContactPoint::End};
  road2.referenceLine.segments.push_back(PlanViewSegment{0.0, 0.0, 10.0, 0.0, 5.0, Line{}});
  LaneSection only;
  only.lanes = {MakeLane(0), MakeLane(-1)};
  road2.sections = {only};

  data.roads = {road1, road2};

  Junction junction;
  junction.id = "J";
  Connection connection;
  connection.id = "0";
  connection.incomingRoad = "1";
  connection.connectingRoad = "2";
  connection.contact = ContactPoint::Start;
  connection.laneLinks = {{-1, -1}};
  junction.connections = {connection};
  data.junctions = {junction};

  data.laneLinks = {
      LaneLink{End("1", 0, -1, ContactPoint::End), End("1", 1, -1, ContactPoint::Start)},
      LaneLink{End("1", 1, -1, ContactPoint::End), End("2", 0, -1, ContactPoint::Start)},
  };
  return data;
}

void ExpectValid(const RoadNetworkData& data, const std::string& what) {
  const auto failure = Validate(data);
  Check(!failure, what + " is valid" + (failure ? " (got " + failure->message + ")" : ""));
}

void ExpectCode(const std::string& code, const std::string& what,
    const std::function<void(RoadNetworkData&)>& mutate) {
  RoadNetworkData data = MakeValid();
  mutate(data);
  const auto failure = Validate(data);
  if (!failure) {
    Check(false, what + ": expected " + code + ", but the network is valid");
    return;
  }
  Check(failure->code == code, what + ": expected " + code + ", got " + failure->message);
  Check(failure->message.rfind("[" + code + "] ", 0) == 0,
      what + ": the message starts with [" + code + "]: " + failure->message);
}

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

void TestValid() {
  ExpectValid(MakeValid(), "the reference network");
  ExpectValid(RoadNetworkData{}, "an empty network");

  // A gap well inside the tolerances of ADR 0004 section 6 is accepted.
  RoadNetworkData nearly = MakeValid();
  nearly.roads[0].referenceLine.segments[1].x += 0.009;
  nearly.roads[0].referenceLine.segments[1].hdg += 0.0009;
  nearly.roads[0].referenceLine.segments[1].s += 5e-7;
  ExpectValid(nearly, "a network within the plan-view tolerances");

  // A heading that differs by a full turn is continuous.
  RoadNetworkData wrapped = MakeValid();
  wrapped.roads[0].referenceLine.segments[1].hdg += 2.0 * 3.141592653589793;
  ExpectValid(wrapped, "a heading offset by 2 pi");

  // One-sided and reversed lane links are valid input to canonicalization.
  RoadNetworkData lanes = MakeValid();
  std::swap(lanes.laneLinks[0].a, lanes.laneLinks[0].b);
  ExpectValid(lanes, "a lane link stated from its upper end");
}

void TestIdentity() {
  ExpectCode("RN-E101", "empty road id", [](RoadNetworkData& d) { d.roads[1].id.clear(); });
  ExpectCode("RN-E101", "empty junction id", [](RoadNetworkData& d) { d.junctions[0].id.clear(); });
  ExpectCode("RN-E101", "empty connection id",
      [](RoadNetworkData& d) { d.junctions[0].connections[0].id.clear(); });
  ExpectCode("RN-E102", "duplicate road id", [](RoadNetworkData& d) { d.roads[1].id = "1"; });
  ExpectCode("RN-E102", "duplicate junction id",
      [](RoadNetworkData& d) { d.junctions.push_back(d.junctions[0]); });
  ExpectCode("RN-E102", "duplicate connection id", [](RoadNetworkData& d) {
    d.junctions[0].connections.push_back(d.junctions[0].connections[0]);
  });

  // A road and a junction may share an id: uniqueness is within a kind.
  RoadNetworkData shared = MakeValid();
  shared.junctions[0].id = "1";
  shared.roads[0].successor->id = "1";
  shared.roads[1].junction = "1";
  ExpectValid(shared, "a junction that shares an id with a road");
}

void TestReferences() {
  ExpectCode("RN-E111", "successor junction missing",
      [](RoadNetworkData& d) { d.roads[0].successor->id = "nowhere"; });
  ExpectCode("RN-E111", "predecessor road missing",
      [](RoadNetworkData& d) { d.roads[1].predecessor->id = "nowhere"; });
  ExpectCode("RN-E111", "road link of the wrong kind",
      [](RoadNetworkData& d) { d.roads[1].predecessor->kind = RoadLink::Kind::Junction; });
  ExpectCode("RN-E112", "road in a missing junction",
      [](RoadNetworkData& d) { d.roads[0].junction = "nowhere"; });
  ExpectCode("RN-E113", "missing incoming road",
      [](RoadNetworkData& d) { d.junctions[0].connections[0].incomingRoad = "nowhere"; });
  ExpectCode("RN-E113", "missing connecting road",
      [](RoadNetworkData& d) { d.junctions[0].connections[0].connectingRoad = "nowhere"; });
  ExpectCode("RN-E114", "connecting road outside the junction",
      [](RoadNetworkData& d) { d.junctions[0].connections[0].connectingRoad = "1"; });
  ExpectCode("RN-E115", "lane link to a missing road",
      [](RoadNetworkData& d) { d.laneLinks[1].b.lane.roadId = "nowhere"; });
  ExpectCode("RN-E115", "lane link to a missing section",
      [](RoadNetworkData& d) { d.laneLinks[1].b.lane.sectionIndex = 1; });
  ExpectCode("RN-E115", "lane link to a missing lane",
      [](RoadNetworkData& d) { d.laneLinks[0].a.lane.laneNumber = -2; });
}

void TestLaneStructure() {
  ExpectCode("RN-E121", "no center lane",
      [](RoadNetworkData& d) { d.roads[1].sections[0].lanes.erase(d.roads[1].sections[0].lanes.begin()); });
  ExpectCode("RN-E121", "two center lanes",
      [](RoadNetworkData& d) { d.roads[1].sections[0].lanes.push_back(MakeLane(0)); });
  ExpectCode("RN-E122", "gap in the right lanes",
      [](RoadNetworkData& d) { d.roads[0].sections[1].lanes[2].number = -3; });
  ExpectCode("RN-E122", "left lanes start at 2",
      [](RoadNetworkData& d) { d.roads[0].sections[0].lanes[0].number = 2; });
  ExpectCode("RN-E122", "repeated lane number",
      [](RoadNetworkData& d) { d.roads[0].sections[1].lanes[2].number = -1; });
}

void TestNumeric() {
  ExpectCode("RN-E301", "non-finite source offset",
      [](RoadNetworkData& d) { d.source.offset->z = kNaN; });
  ExpectCode("RN-E301", "non-finite road length", [](RoadNetworkData& d) { d.roads[0].length = kInf; });
  ExpectCode("RN-E301", "non-finite segment heading",
      [](RoadNetworkData& d) { d.roads[0].referenceLine.segments[0].hdg = kNaN; });
  ExpectCode("RN-E301", "non-finite curvature",
      [](RoadNetworkData& d) { d.roads[0].referenceLine.segments[1].shape = Arc{kInf}; });
  ExpectCode("RN-E301", "non-finite elevation",
      [](RoadNetworkData& d) { d.roads[0].elevation.records[0].d = kNaN; });
  ExpectCode("RN-E301", "non-finite lane offset",
      [](RoadNetworkData& d) { d.roads[0].laneOffset.records[1].c = -kInf; });
  ExpectCode("RN-E301", "non-finite speed",
      [](RoadNetworkData& d) { d.roads[0].speeds[0].maxMetresPerSecond = kNaN; });
  ExpectCode("RN-E301", "non-finite section s", [](RoadNetworkData& d) { d.roads[0].sections[1].s = kNaN; });
  ExpectCode("RN-E301", "non-finite lane width",
      [](RoadNetworkData& d) { d.roads[0].sections[0].lanes[0].width.records[0].a = kNaN; });

  ExpectCode("RN-E302", "zero road length", [](RoadNetworkData& d) { d.roads[1].length = 0.0; });
  ExpectCode("RN-E302", "negative segment length",
      [](RoadNetworkData& d) { d.roads[1].referenceLine.segments[0].length = -5.0; });

  ExpectCode("RN-E303", "no plan view", [](RoadNetworkData& d) { d.roads[1].referenceLine.segments.clear(); });
  ExpectCode("RN-E303", "plan view does not start at 0",
      [](RoadNetworkData& d) { d.roads[1].referenceLine.segments[0].s = 0.5; });
  ExpectCode("RN-E303", "gap in s between segments",
      [](RoadNetworkData& d) { d.roads[0].referenceLine.segments[1].s = 10.001; });
  ExpectCode("RN-E303", "plan view shorter than the road", [](RoadNetworkData& d) { d.roads[0].length = 21.0; });

  ExpectCode("RN-E304", "no lane section", [](RoadNetworkData& d) {
    d.roads[1].sections.clear();
    d.laneLinks.pop_back();
  });
  ExpectCode("RN-E304", "first section after 0", [](RoadNetworkData& d) { d.roads[0].sections[0].s = 1.0; });
  ExpectCode("RN-E304", "sections out of order", [](RoadNetworkData& d) { d.roads[0].sections[1].s = 0.0; });

  // ADR 0004 section 6: a 0.02 m position gap fails.
  ExpectCode("RN-E305", "0.02 m position gap",
      [](RoadNetworkData& d) { d.roads[0].referenceLine.segments[1].y += 0.02; });
  ExpectCode("RN-E306", "heading gap", [](RoadNetworkData& d) { d.roads[0].referenceLine.segments[1].hdg = 0.002; });

  ExpectCode("RN-E307", "elevation records out of order", [](RoadNetworkData& d) {
    d.roads[0].elevation.records.push_back(CubicRecord{0.0, 1.0, 0.0, 0.0, 0.0});
  });
  ExpectCode("RN-E307", "repeated lane-offset s0",
      [](RoadNetworkData& d) { d.roads[0].laneOffset.records[1].s0 = 0.0; });
  ExpectCode("RN-E307", "width records out of order",
      [](RoadNetworkData& d) { d.roads[0].sections[1].lanes[1].width.records[1].s0 = -1.0; });
}

/// Expects the network to be valid with exactly one RN-W301 that counts
/// `count` borders and names `mentions`.
void ExpectFoldWarning(const RoadNetworkData& data, std::uint32_t count, const std::string& mentions,
    const std::string& what) {
  std::vector<ValidationWarning> warnings;
  const auto failure = Validate(data, &warnings);
  Check(!failure, what + " is valid" + (failure ? " (got " + failure->message + ")" : ""));
  Check(warnings.size() == 1, what + ": " + std::to_string(warnings.size()) + " warnings");
  if (warnings.size() == 1) {
    const ValidationWarning& w = warnings[0];
    Check(w.code == "RN-W301" && w.count == count && w.message.rfind("[RN-W301] ", 0) == 0 &&
              w.message.find(mentions) != std::string::npos,
        what + ": " + w.code + " " + std::to_string(w.count) + " " + w.message);
  }
}

void TestFold() {
  // ADR 0004 section 2. Road "1" section 1 lies on the left arc of radius 10 m
  // (s = 10..20); its lane offset there is 0.001 d^2, at most 0.1 m. A left
  // lane is on the inside of the arc. With `outer`, a 1 m lane 2 lies
  // outside lane 1, so lane 1's outer border is lane 2's inner border.
  const auto addLeftLane = [](RoadNetworkData& d, CubicRecord width, bool outer = false) {
    Lane lane = MakeLane(1);
    lane.width.records = {width};
    std::vector<Lane>& lanes = d.roads[0].sections[1].lanes;
    lanes.insert(lanes.begin(), lane);
    if (outer) {
      lanes.insert(lanes.begin(), MakeLane(2, 1.0));
    }
  };
  RoadNetworkData inside = MakeValid();
  addLeftLane(inside, CubicRecord{0.0, 9.85, 0.0, 0.0, 0.0});
  std::vector<ValidationWarning> none;
  Check(!Validate(inside, &none) && none.empty(),
      "an inner border 0.05 m short of the center of curvature is valid without a warning");

  // An inner lane border at or beyond the center: the lane outside it is
  // inverted.
  ExpectCode("RN-E308", "an inner border beyond the center of curvature",
      [&](RoadNetworkData& d) { addLeftLane(d, CubicRecord{0.0, 10.5, 0.0, 0.0, 0.0}, true); });
  ExpectCode("RN-E308", "an inner border that reaches the center only at the section end",
      [&](RoadNetworkData& d) { addLeftLane(d, CubicRecord{0.0, 9.0, 0.1, 0.0, 0.0}, true); });
  // 5 + 2 d - 0.2 d^2 is 5 at both ends of the section but 10 at d = 5: the
  // exact maximum of the cubic finds it.
  ExpectCode("RN-E308", "an inner border that folds between its stations",
      [&](RoadNetworkData& d) { addLeftLane(d, CubicRecord{0.0, 5.0, 2.0, -0.2, 0.0}, true); });
  ExpectCode("RN-E308", "a lane offset at the center of curvature",
      [](RoadNetworkData& d) { d.roads[0].laneOffset.records[1].a = 10.0; });
  // A right turn folds on the right: lane -1 is inside lane -2.
  ExpectCode("RN-E308", "an inner right border inside a right turn", [](RoadNetworkData& d) {
    Road& road = d.roads[0];
    road.referenceLine.segments[1].shape = Arc{-0.1};
    road.sections[1].lanes[1].width.records = {CubicRecord{0.0, 10.5, 0.0, 0.0, 0.0}};
  });

  // Only the outer border of the outermost lane: a counted warning, once per
  // border however many intervals reach the center (CARLA Town01 and Town02).
  RoadNetworkData outermost = MakeValid();
  addLeftLane(outermost, CubicRecord{0.0, 10.5, 0.0, 0.0, 0.0});
  ExpectFoldWarning(outermost, 1, "road \"1\" lane section 1 lane 1 border", "the outermost left border");
  RoadNetworkData between = MakeValid();
  addLeftLane(between, CubicRecord{0.0, 5.0, 2.0, -0.2, 0.0});
  ExpectFoldWarning(between, 1, "lane 1 border", "an outermost border that folds between its stations");
  RoadNetworkData right = MakeValid();
  right.roads[0].referenceLine.segments[1].shape = Arc{-0.1};
  right.roads[0].sections[1].lanes[2].width.records = {CubicRecord{0.0, 7.0, 0.0, 0.0, 0.0}};
  ExpectFoldWarning(right, 1, "lane -2 border", "the outermost right border inside a right turn");
  RoadNetworkData both = right;
  both.roads[0].sections[0].lanes[2].width.records = {CubicRecord{0.0, 3.5, 0.0, 0.0, 0.0}};
  both.roads[0].sections[1].lanes.insert(both.roads[0].sections[1].lanes.begin(), MakeLane(1, 1.0));
  both.roads[0].referenceLine.segments[1].shape = Arc{-0.1};
  ExpectFoldWarning(both, 1, "lane -2 border", "the warning is unaffected by the other side");
  // Without a warnings vector the same network is simply valid.
  ExpectValid(outermost, "an outermost fold without a warnings vector");

  // The outside of an arc never folds.
  RoadNetworkData outside = MakeValid();
  outside.roads[0].sections[1].lanes[2].width.records = {CubicRecord{0.0, 50.0, 0.0, 0.0, 0.0}};
  std::vector<ValidationWarning> outsideWarnings;
  Check(!Validate(outside, &outsideWarnings) && outsideWarnings.empty(),
      "a wide border on the outside of an arc is valid without a warning");
}

void TestArcEnd() {
  // The arc of road "1" turns by 1 rad over 10 m. The next segment, placed
  // at its closed-form end (x0 + sin(1)/kappa, y0 + (1 - cos(1))/kappa), is
  // continuous; its start heading must be 1 rad.
  RoadNetworkData data = MakeValid();
  Road& road = data.roads[0];
  road.length = 25.0;
  const double kappa = 0.1;
  road.referenceLine.segments.push_back(PlanViewSegment{20.0, 10.0 + std::sin(1.0) / kappa,
      (1.0 - std::cos(1.0)) / kappa, 1.0, 5.0, Line{}});
  ExpectValid(data, "a line after an arc at its closed-form end");

  // A near-zero curvature behaves like a line (the sinc Taylor branch).
  RoadNetworkData flat = MakeValid();
  flat.roads[0].referenceLine.segments[1].shape = Arc{1e-12};
  flat.roads[0].length = 25.0;
  flat.roads[0].referenceLine.segments.push_back(PlanViewSegment{20.0, 20.0, 0.0, 0.0, 5.0, Line{}});
  ExpectValid(flat, "a line after an arc with |kappa| = 1e-12");

  // An arc that turns by more than pi: a full circle returns to its start.
  RoadNetworkData circle = MakeValid();
  Road& loop = circle.roads[1];
  const double radius = 10.0;
  const double pi = 3.141592653589793;
  loop.length = 2.0 * pi * radius + 5.0;
  loop.referenceLine.segments = {
      PlanViewSegment{0.0, 0.0, 10.0, 0.0, 2.0 * pi * radius, Arc{1.0 / radius}},
      PlanViewSegment{2.0 * pi * radius, 0.0, 10.0, 0.0, 5.0, Line{}},
  };
  ExpectValid(circle, "a line after a full-circle arc");
}

void TestOrder() {
  // Identity is checked before references, references before lane
  // structure, and lane structure before numeric values.
  ExpectCode("RN-E102", "identity before references", [](RoadNetworkData& d) {
    d.roads[1].id = "1";
    d.roads[0].successor->id = "nowhere";
  });
  ExpectCode("RN-E111", "references before lane structure", [](RoadNetworkData& d) {
    d.roads[0].successor->id = "nowhere";
    d.roads[1].sections[0].lanes.push_back(MakeLane(0));
  });
  ExpectCode("RN-E121", "lane structure before numeric", [](RoadNetworkData& d) {
    d.roads[1].sections[0].lanes.push_back(MakeLane(0));
    d.roads[0].length = kNaN;
  });
  // Within numeric values, a non-finite value comes before a range check.
  ExpectCode("RN-E301", "finite before length", [](RoadNetworkData& d) {
    d.roads[0].length = -1.0;
    d.roads[0].referenceLine.segments[0].x = kNaN;
  });
}

void TestCanonicalLaneLinks() {
  const LaneEnd a = End("1", 0, -1, ContactPoint::End);
  const LaneEnd b = End("1", 1, -1, ContactPoint::Start);
  const LaneEnd c = End("10", 0, 1, ContactPoint::Start);
  const LaneEnd d = End("2", 0, -1, ContactPoint::Start);

  Check(a < b && b < c && c < d, "lane ends order by road id bytes, then section");
  Check(End("1", 0, -2, ContactPoint::End) < End("1", 0, -1, ContactPoint::Start),
      "lane ends order by lane number before the end");
  Check(End("1", 0, -1, ContactPoint::Start) < End("1", 0, -1, ContactPoint::End),
      "start orders before end");
  Check(End("B", 0, 0, ContactPoint::Start) < End("a", 0, 0, ContactPoint::Start),
      "road ids order by byte value, not case-insensitively");

  // The same link stated from both lanes, from one lane, and twice.
  std::vector<LaneLink> links = {
      LaneLink{d, c},
      LaneLink{b, a},
      LaneLink{a, b},
      LaneLink{c, d},
      LaneLink{a, b},
  };
  CanonicalizeLaneLinks(links);
  Check(links.size() == 2, "duplicate and reversed lane links collapse to one edge each");
  if (links.size() == 2) {
    Check(links[0].a == a && links[0].b == b, "the first edge is a-b with the lower end first");
    Check(links[1].a == c && links[1].b == d, "the second edge is c-d with the lower end first");
  }

  std::vector<LaneLink> again = links;
  CanonicalizeLaneLinks(again);
  Check(again.size() == links.size() && again[0].a == links[0].a && again[1].b == links[1].b,
      "canonicalization is idempotent");
}

} // namespace

int main() {
  TestValid();
  TestIdentity();
  TestReferences();
  TestLaneStructure();
  TestNumeric();
  TestFold();
  TestArcEnd();
  TestOrder();
  TestCanonicalLaneLinks();

  if (failures == 0) {
    std::printf("roadnetworkCore validation: all checks passed\n");
  }
  return failures == 0 ? 0 : 1;
}
