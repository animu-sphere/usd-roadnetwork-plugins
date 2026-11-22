// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

/// The format-independent road network that every decoder produces and the
/// shared USD writer consumes (ADR 0001, docs/adr/0001-roadnetwork-data-model.md).
///
/// Elements are plain values that refer to each other by ID; Validate()
/// (Validation.h) resolves the references. Lengths are in metres, angles in
/// radians, and speeds in metres per second (ADR 0003). Containers keep the
/// source document order, except RoadNetworkData::laneLinks, which is in the
/// canonical order that CanonicalizeLaneLinks() produces.
namespace openstrata::rn {

/// An end of a road or lane: `Start` is its lowest `s`.
enum class ContactPoint { Start, End };

/// `a + b*d + c*d^2 + d*d^3` with `d = s - s0`, valid from `s0` up to the next
/// record's `s0`.
struct CubicRecord {
  double s0 = 0.0;
  double a = 0.0;
  double b = 0.0;
  double c = 0.0;
  double d = 0.0;
};

/// Elevation, lane offset, and lane width. `s0` is measured along the road
/// for elevation and lane offset, and from the lane-section start for a lane
/// width, as in the source. Records ascend strictly in `s0`.
struct PiecewiseCubic {
  std::vector<CubicRecord> records;
};

/// A straight plan-view segment.
struct Line {};

/// A plan-view segment of constant curvature; positive turns left.
struct Arc {
  double curvature = 0.0;
};

struct PlanViewSegment {
  double s = 0.0;      ///< Start along the reference line.
  double x = 0.0;      ///< Start position.
  double y = 0.0;
  double hdg = 0.0;    ///< Start heading, counter-clockwise from +x.
  double length = 0.0; ///< Positive.
  std::variant<Line, Arc> shape;
};

/// The analytic reference-line representation (ADR 0001 section 4, ADR 0004).
struct ReferenceLine {
  std::vector<PlanViewSegment> segments;
};

struct Lane {
  std::int32_t number = 0; ///< > 0 left, 0 center, < 0 right.
  std::string type;        ///< Source vocabulary, e.g. "driving".
  bool level = false;
  PiecewiseCubic width; ///< Empty for the center lane.
};

struct LaneSection {
  double s = 0.0;
  std::vector<Lane> lanes; ///< Source order: descending lane number.
};

/// A road's predecessor or successor, in the reference-line direction.
struct RoadLink {
  enum class Kind { Road, Junction };
  Kind kind = Kind::Road;
  std::string id;
  std::optional<ContactPoint> contact; ///< Set when kind == Road.
};

struct SpeedRecord {
  double s = 0.0;
  double maxMetresPerSecond = 0.0;
};

struct Road {
  std::string id;
  std::string name;
  double length = 0.0;
  std::optional<std::string> junction; ///< Set for a road inside a junction.
  std::optional<RoadLink> predecessor;
  std::optional<RoadLink> successor;
  ReferenceLine referenceLine;
  PiecewiseCubic elevation;
  PiecewiseCubic laneOffset;
  std::vector<SpeedRecord> speeds;
  std::vector<LaneSection> sections;
};

/// A lane's identity: its road, the 0-based position of its lane section in
/// source order, and its signed source lane number (ADR 0001 section 2).
struct LaneKey {
  std::string roadId;
  std::uint32_t sectionIndex = 0;
  std::int32_t laneNumber = 0;
};

struct LaneEnd {
  LaneKey lane;
  ContactPoint end = ContactPoint::Start;
};

/// An undirected edge between two lane ends. It implies no direction of
/// travel. In canonical form `a < b`.
struct LaneLink {
  LaneEnd a;
  LaneEnd b;
};

struct Connection {
  std::string id;
  std::string incomingRoad;
  std::string connectingRoad;
  ContactPoint contact = ContactPoint::Start;
  /// `{ from, to }` lane numbers as the source states them.
  std::vector<std::pair<std::int32_t, std::int32_t>> laneLinks;
};

struct Junction {
  std::string id;
  std::string name;
  std::vector<Connection> connections;
};

/// The source header offset (ADR 0003).
struct SourceOffset {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  double hdg = 0.0;
};

/// Import provenance (ADR 0001 section 5). The header strings are opaque.
struct SourceInfo {
  std::string format; ///< e.g. "opendrive".
  int versionMajor = 0;
  int versionMinor = 0;
  std::string name;
  std::string date;
  std::string vendor;
  std::string version;                     ///< The network's own version string.
  std::optional<std::string> geoReference; ///< Verbatim.
  std::optional<SourceOffset> offset;
};

/// A warning posted during the read and the number of affected elements.
struct WarningSummary {
  std::string code; ///< e.g. "RN-W201".
  std::uint32_t count = 0;
};

struct RoadNetworkData {
  SourceInfo source;
  std::vector<Road> roads;
  std::vector<Junction> junctions;
  std::vector<LaneLink> laneLinks; ///< Canonical order.
  std::vector<WarningSummary> warnings;
};

/// The canonical order of lane ends: road ID (byte order), section index,
/// lane number, then `Start < End` (ADR 0001 section 3).
bool operator<(const LaneEnd& lhs, const LaneEnd& rhs) noexcept;
bool operator==(const LaneEnd& lhs, const LaneEnd& rhs) noexcept;
bool operator!=(const LaneEnd& lhs, const LaneEnd& rhs) noexcept;

/// Puts every edge's lower end first, sorts the edges, and removes
/// duplicates, so that a link stated on one lane or on both lanes yields one
/// edge.
void CanonicalizeLaneLinks(std::vector<LaneLink>& links);

} // namespace openstrata::rn
