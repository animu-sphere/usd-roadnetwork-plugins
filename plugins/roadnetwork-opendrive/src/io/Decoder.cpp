// SPDX-License-Identifier: Apache-2.0
#include "io/Decoder.h"

#include "io/DocumentImpl.h"

#include "openstrata/rn/Validation.h"

#include <algorithm>
#include <clocale>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace openstrata::rn::odr {

namespace {

using pugi::xml_attribute;
using pugi::xml_node;

constexpr std::string_view kLaneTypes[] = {
    "HOV", "bidirectional", "biking", "border", "bus", "connectingRamp", "curb", "driving",
    "entry", "exit", "median", "mwyEntry", "mwyExit", "none", "offRamp", "onRamp", "parking",
    "rail", "restricted", "roadWorks", "shared", "shoulder", "sidewalk", "slipLane", "special1",
    "special2", "special3", "stop", "taxi", "tram", "walking"};

/// The first failure, thrown inside the decoder and caught at its entry
/// points, so that a failed read reports exactly one error.
struct Failure {
  Diagnostic diagnostic;
};

[[noreturn]] void Fail(std::string_view code, std::string detail) {
  throw Failure{Diagnostic{std::string(code), std::move(detail)}};
}

bool IsXmlSpace(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string_view Trim(std::string_view s) noexcept {
  while (!s.empty() && IsXmlSpace(s.front())) {
    s.remove_prefix(1);
  }
  while (!s.empty() && IsXmlSpace(s.back())) {
    s.remove_suffix(1);
  }
  return s;
}

bool IsDigit(char c) noexcept {
  return c >= '0' && c <= '9';
}

/// An `xsd:double`: a decimal with an optional exponent, `INF`, `-INF`, or
/// `NaN`, with surrounding whitespace. Non-finite and overflowing values are
/// accepted here, so that validation reports them as non-finite.
bool ParseDouble(const char* text, double* out) {
  const std::string_view s = Trim(text);
  if (s == "INF" || s == "+INF") {
    *out = std::numeric_limits<double>::infinity();
    return true;
  }
  if (s == "-INF") {
    *out = -std::numeric_limits<double>::infinity();
    return true;
  }
  if (s == "NaN") {
    *out = std::numeric_limits<double>::quiet_NaN();
    return true;
  }
  std::size_t i = 0;
  if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
    ++i;
  }
  std::size_t digits = 0;
  while (i < s.size() && IsDigit(s[i])) {
    ++i;
    ++digits;
  }
  if (i < s.size() && s[i] == '.') {
    ++i;
    while (i < s.size() && IsDigit(s[i])) {
      ++i;
      ++digits;
    }
  }
  if (digits == 0) {
    return false;
  }
  if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
    ++i;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
      ++i;
    }
    std::size_t exponent = 0;
    while (i < s.size() && IsDigit(s[i])) {
      ++i;
      ++exponent;
    }
    if (exponent == 0) {
      return false;
    }
  }
  if (i != s.size()) {
    return false;
  }
  // strtod() reads the decimal point of the C locale, which a host
  // application may have changed; the lexical form is checked above, so
  // replacing the point is enough to make the conversion locale-independent.
  std::string copy(s);
  const char* point = std::localeconv()->decimal_point;
  if (point && point[0] != '\0' && std::string_view(point) != ".") {
    const std::size_t dot = copy.find('.');
    if (dot != std::string::npos) {
      copy.replace(dot, 1, point);
    }
  }
  char* end = nullptr;
  *out = std::strtod(copy.c_str(), &end);
  return end == copy.c_str() + copy.size();
}

/// An `xsd:int`: an optionally signed decimal integer within 32 bits.
bool ParseInt32(const char* text, std::int32_t* out) {
  const std::string_view s = Trim(text);
  std::size_t i = 0;
  bool negative = false;
  if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
    negative = s[i] == '-';
    ++i;
  }
  if (i == s.size()) {
    return false;
  }
  std::int64_t value = 0;
  for (; i < s.size(); ++i) {
    if (!IsDigit(s[i])) {
      return false;
    }
    value = value * 10 + (s[i] - '0');
    if (value > std::int64_t{std::numeric_limits<std::int32_t>::max()} + 1) {
      return false;
    }
  }
  value = negative ? -value : value;
  if (value > std::numeric_limits<std::int32_t>::max()) {
    return false;
  }
  *out = static_cast<std::int32_t>(value);
  return true;
}

std::string Quote(std::string_view id) {
  return "\"" + std::string(id) + "\"";
}

std::string Tag(const xml_node& node) {
  return "<" + std::string(node.name()) + ">";
}

bool Is(const xml_node& node, std::string_view name) {
  return std::string_view(node.name()) == name;
}

bool IsAdditionalData(const xml_node& node) {
  return Is(node, "userData") || Is(node, "include") || Is(node, "dataQuality");
}

/// The tally behind each warning code.
class Warnings {
public:
  void Add(std::string_view code, const std::string& kind, const std::string& where) {
    Tally& tally = tallies_[std::string(code)];
    ++tally.count;
    ++tally.kinds[kind];
    if (tally.where.size() < 5 &&
        std::find(tally.where.begin(), tally.where.end(), where) == tally.where.end()) {
      tally.where.push_back(where);
    }
  }

  /// One warning per code, in ascending code order.
  std::vector<Warning> Finish() const {
    std::vector<Warning> out;
    for (const auto& [code, tally] : tallies_) {
      std::string detail = std::to_string(tally.count) + " " + Noun(code, tally.count) + " " +
                           Verb(code) + ": ";
      bool first = true;
      for (const auto& [kind, count] : tally.kinds) {
        detail += (first ? "" : ", ") + kind + " (" + std::to_string(count) + ")";
        first = false;
      }
      detail += "; first at ";
      for (std::size_t i = 0; i < tally.where.size(); ++i) {
        detail += (i ? ", " : "") + tally.where[i];
      }
      out.push_back(Warning{code, tally.count, std::move(detail)});
    }
    return out;
  }

private:
  struct Tally {
    std::uint32_t count = 0;
    std::map<std::string, std::uint32_t> kinds;
    std::vector<std::string> where;
  };

  static std::string Noun(const std::string& code, std::uint32_t count) {
    const char* noun = code == codes::kAttributesIgnored    ? "attribute"
                       : code == codes::kSpeedStringIgnored ? "speed record"
                                                            : "element";
    return std::string(noun) + (count == 1 ? "" : "s");
  }

  static std::string Verb(const std::string& code) {
    if (code == codes::kRoadMarksIgnored) {
      return "(road marks) not authored";
    }
    if (code == codes::kSignalsIgnored) {
      return "(signals and controllers) not authored";
    }
    if (code == codes::kObjectsIgnored) {
      return "(road objects) not authored";
    }
    if (code == codes::kUserDataIgnored) {
      return "(user data and vendor extensions) not authored";
    }
    if (code == codes::kSpeedStringIgnored) {
      return "with a maximum that is not a number not authored";
    }
    if (code == codes::kElementsIgnored) {
      return "outside the Phase 1 scope not authored";
    }
    if (code == codes::kAttributesIgnored) {
      return "not represented";
    }
    return "unknown to this decoder ignored";
  }

  std::map<std::string, Tally> tallies_;
};

/// The source statement of one lane link, resolved into an edge once every
/// road is decoded.
struct LaneLinkStatement {
  std::size_t road = 0;
  std::uint32_t section = 0;
  std::int32_t lane = 0;
  bool successor = false;
  std::int32_t target = 0;
  std::string where;
};

class Decoder {
public:
  explicit Decoder(const xml_node& root) : root_(root) {
  }

  void Header(SourceInfo& source) {
    const xml_node header = root_.find_child(
        [](const xml_node& node) { return node.type() == pugi::node_element; });
    CheckAttributes(header,
        {"revMajor", "revMinor", "name", "date", "vendor", "version",
            // The bounding box is derived from the geometry, so dropping it
            // loses nothing (OPENDRIVE_MAPPING.md section 1).
            "north", "south", "east", "west"},
        "header");
    bool seenGeoReference = false;
    bool seenOffset = false;
    for (const xml_node& child : header.children()) {
      if (child.type() != pugi::node_element) {
        continue;
      }
      if (Is(child, "geoReference")) {
        Once(child, &seenGeoReference, "header");
        source.geoReference = GeoReference(child);
      } else if (Is(child, "offset")) {
        Once(child, &seenOffset, "header");
        CheckAttributes(child, {"x", "y", "z", "hdg"}, "header");
        SourceOffset offset;
        offset.x = Number(child, "x", "header");
        offset.y = Number(child, "y", "header");
        offset.z = Number(child, "z", "header");
        offset.hdg = Number(child, "hdg", "header");
        source.offset = offset;
      } else if (Is(child, "license") || Is(child, "defaultRegulations")) {
        warnings_.Add(codes::kElementsIgnored, child.name(), "header");
      } else {
        Ignore(child, "header");
      }
    }
  }

  void Network(RoadNetworkData& data) {
    Header(data.source);
    bool first = true;
    for (const xml_node& child : root_.children()) {
      if (child.type() != pugi::node_element) {
        continue;
      }
      if (first) { // The header, checked by the reader and decoded above.
        first = false;
        continue;
      }
      if (Is(child, "header")) {
        Fail(codes::kMisplacedElement, "<OpenDRIVE> has a second <header>");
      } else if (Is(child, "road")) {
        data.roads.push_back(RoadOf(child, data.roads.size()));
      } else if (Is(child, "junction")) {
        data.junctions.push_back(JunctionOf(child, data.junctions.size()));
      } else if (Is(child, "controller")) {
        warnings_.Add(codes::kSignalsIgnored, "controller",
            "controller " + Quote(child.attribute("id").value()));
      } else if (Is(child, "junctionGroup") || Is(child, "station")) {
        warnings_.Add(codes::kElementsIgnored, child.name(),
            std::string(child.name()) + " " + Quote(child.attribute("id").value()));
      } else {
        Ignore(child, "<OpenDRIVE>");
      }
    }
    BuildLaneGraph(data);
  }

  Warnings& Collected() {
    return warnings_;
  }

private:
  // Attributes and values.

  void CheckAttributes(
      const xml_node& node, std::initializer_list<std::string_view> known, const std::string& where) {
    for (const xml_attribute& attribute : node.attributes()) {
      const std::string_view name = attribute.name();
      if (std::find(known.begin(), known.end(), name) == known.end()) {
        warnings_.Add(codes::kAttributesIgnored, std::string(node.name()) + "@" + std::string(name),
            where);
      }
    }
  }

  static const char* Required(const xml_node& node, const char* name, const std::string& where) {
    const xml_attribute attribute = node.attribute(name);
    if (!attribute) {
      Fail(codes::kMissingRequired, where + ": " + Tag(node) + " has no " + name + " attribute");
    }
    return attribute.value();
  }

  [[noreturn]] static void Invalid(const xml_node& node, const char* name, const char* value,
      const std::string& where, const char* expected) {
    Fail(codes::kInvalidValue, where + ": " + Tag(node) + " " + name + "=" + Quote(value) +
                                   " is not " + expected);
  }

  static double Number(const xml_node& node, const char* name, const std::string& where) {
    const char* value = Required(node, name, where);
    double number = 0.0;
    if (!ParseDouble(value, &number)) {
      Invalid(node, name, value, where, "a number");
    }
    return number;
  }

  static std::int32_t Integer(const xml_node& node, const char* name, const std::string& where) {
    const char* value = Required(node, name, where);
    std::int32_t number = 0;
    if (!ParseInt32(value, &number)) {
      Invalid(node, name, value, where, "a 32-bit integer");
    }
    return number;
  }

  static ContactPoint Contact(const xml_node& node, const char* name, const std::string& where) {
    const char* value = Required(node, name, where);
    const std::string_view text = value;
    if (text == "start") {
      return ContactPoint::Start;
    }
    if (text == "end") {
      return ContactPoint::End;
    }
    Invalid(node, name, value, where, "start or end");
  }

  static CubicRecord Cubic(const xml_node& node, const char* s, const std::string& where) {
    CubicRecord record;
    record.s0 = Number(node, s, where);
    record.a = Number(node, "a", where);
    record.b = Number(node, "b", where);
    record.c = Number(node, "c", where);
    record.d = Number(node, "d", where);
    return record;
  }

  // Elements.

  static void Once(const xml_node& node, bool* seen, const std::string& where) {
    if (*seen) {
      Fail(codes::kMisplacedElement, where + " has more than one " + Tag(node));
    }
    *seen = true;
  }

  /// An element that the decoder does not represent and that cannot change
  /// geometry or connectivity where it appears: user data and vendor
  /// extensions, or an element unknown to this decoder.
  void Ignore(const xml_node& node, const std::string& where) {
    warnings_.Add(IsAdditionalData(node) ? codes::kUserDataIgnored : codes::kUnknownElementsIgnored,
        node.name(), where);
  }

  /// An element inside the plan view, the profiles, or the lane structure,
  /// where anything this decoder does not know could move a border or change
  /// the lanes: an error, except for user data.
  void Structural(const xml_node& node, const std::string& where) {
    if (IsAdditionalData(node)) {
      warnings_.Add(codes::kUserDataIgnored, node.name(), where);
      return;
    }
    Fail(codes::kUnknownStructuralElement,
        where + " has " + Tag(node) + ", which this decoder does not know; it could change the "
                                      "geometry or the lanes, so it is not ignored");
  }

  static std::optional<std::string> GeoReference(const xml_node& node) {
    std::string text;
    for (const xml_node& child : node.children()) {
      if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata) {
        text += child.value();
      }
    }
    const std::string_view trimmed = Trim(text);
    if (trimmed.empty()) {
      return std::nullopt;
    }
    return std::string(trimmed);
  }

  rn::Road RoadOf(const xml_node& node, std::size_t index) {
    rn::Road road;
    road.id = Required(node, "id", "road at position " + std::to_string(index));
    const std::string where = "road " + Quote(road.id);
    CheckAttributes(node, {"id", "name", "length", "junction"}, where);
    road.name = node.attribute("name").value();
    road.length = Number(node, "length", where);
    const std::string junction = Required(node, "junction", where);
    if (junction != "-1") {
      road.junction = junction;
    }

    bool seenLink = false;
    bool seenPlanView = false;
    bool seenElevation = false;
    bool seenLateral = false;
    bool seenLanes = false;
    for (const xml_node& child : node.children()) {
      if (child.type() != pugi::node_element) {
        continue;
      }
      if (Is(child, "link")) {
        Once(child, &seenLink, where);
        RoadLinks(child, road, where);
      } else if (Is(child, "type")) {
        RoadType(child, road, where);
      } else if (Is(child, "planView")) {
        Once(child, &seenPlanView, where);
        PlanView(child, road, where);
      } else if (Is(child, "elevationProfile")) {
        Once(child, &seenElevation, where);
        ElevationProfile(child, road, where);
      } else if (Is(child, "lateralProfile")) {
        Once(child, &seenLateral, where);
        LateralProfile(child, where);
      } else if (Is(child, "lanes")) {
        CheckAttributes(child, {"layer"}, where);
        if (const xml_attribute layer = child.attribute("layer")) {
          const std::string_view value = layer.value();
          if (value == "temporary") {
            Fail(codes::kUnsupportedLaneLayer,
                where + " has a temporary lane layer, <lanes layer=\"temporary\">, which Phase 1 "
                        "does not support");
          }
          if (value != "permanent") {
            Invalid(child, "layer", layer.value(), where, "permanent or temporary");
          }
        }
        Once(child, &seenLanes, where);
        Lanes(child, road, index, where);
      } else if (Is(child, "objects")) {
        Children(child, codes::kObjectsIgnored, where);
      } else if (Is(child, "signals")) {
        Children(child, codes::kSignalsIgnored, where);
      } else if (Is(child, "surface") || Is(child, "railroad")) {
        warnings_.Add(codes::kElementsIgnored, child.name(), where);
      } else {
        Ignore(child, where);
      }
    }
    return road;
  }

  /// Counts each child element of `<objects>` or `<signals>` under `code`.
  void Children(const xml_node& node, std::string_view code, const std::string& where) {
    for (const xml_node& child : node.children()) {
      if (child.type() != pugi::node_element) {
        continue;
      }
      if (IsAdditionalData(child)) {
        warnings_.Add(codes::kUserDataIgnored, child.name(), where);
      } else {
        warnings_.Add(code, child.name(), where);
      }
    }
  }

  void RoadLinks(const xml_node& node, rn::Road& road, const std::string& where) {
    CheckAttributes(node, {}, where);
    bool seenPredecessor = false;
    bool seenSuccessor = false;
    for (const xml_node& child : node.children()) {
      if (child.type() != pugi::node_element) {
        continue;
      }
      if (Is(child, "predecessor")) {
        Once(child, &seenPredecessor, where + " <link>");
        road.predecessor = RoadLinkOf(child, where + " predecessor");
      } else if (Is(child, "successor")) {
        Once(child, &seenSuccessor, where + " <link>");
        road.successor = RoadLinkOf(child, where + " successor");
      } else if (Is(child, "neighbor")) {
        warnings_.Add(codes::kElementsIgnored, "neighbor", where);
      } else {
        Ignore(child, where);
      }
    }
  }

  rn::RoadLink RoadLinkOf(const xml_node& node, const std::string& where) {
    if (node.attribute("elementS") || node.attribute("elementDir")) {
      Fail(codes::kUnsupportedJunction,
          where + " links into the middle of a road (elementS, elementDir), as a virtual junction "
                  "does; Phase 1 does not support it");
    }
    CheckAttributes(node, {"elementType", "elementId", "contactPoint"}, where);
    rn::RoadLink link;
    const char* type = Required(node, "elementType", where);
    if (std::string_view(type) == "road") {
      link.kind = rn::RoadLink::Kind::Road;
    } else if (std::string_view(type) == "junction") {
      link.kind = rn::RoadLink::Kind::Junction;
    } else {
      Invalid(node, "elementType", type, where, "road or junction");
    }
    link.id = Required(node, "elementId", where);
    if (link.kind == rn::RoadLink::Kind::Road) {
      link.contact = Contact(node, "contactPoint", where);
    }
    return link;
  }

  void RoadType(const xml_node& node, rn::Road& road, const std::string& where) {
    // The road type and country are not in the Phase 1 model (ADR 0002
    // section 3); the speed is.
    CheckAttributes(node, {"s"}, where);
    const double s = Number(node, "s", where);
    bool seenSpeed = false;
    for (const xml_node& child : node.children()) {
      if (child.type() != pugi::node_element) {
        continue;
      }
      if (!Is(child, "speed")) {
        Ignore(child, where);
        continue;
      }
      Once(child, &seenSpeed, where + " <type>");
      CheckAttributes(child, {"max", "unit"}, where);
      const char* max = Required(child, "max", where);
      const std::string_view text = Trim(max);
      if (text == "no limit" || text == "undefined") {
        warnings_.Add(codes::kSpeedStringIgnored, "max=" + Quote(text), where);
        continue;
      }
      double value = 0.0;
      if (!ParseDouble(max, &value)) {
        Invalid(child, "max", max, where, "a number, \"no limit\", or \"undefined\"");
      }
      double factor = 1.0; // m/s, also when the unit is absent.
      if (const xml_attribute unit = child.attribute("unit")) {
        const std::string_view u = unit.value();
        if (u == "km/h") {
          factor = 1.0 / 3.6;
        } else if (u == "mph") {
          factor = 0.44704; // Exactly 1609.344 m per 3600 s.
        } else if (u != "m/s") {
          Invalid(child, "unit", unit.value(), where, "m/s, km/h, or mph");
        }
      }
      road.speeds.push_back(SpeedRecord{s, value * factor});
    }
  }

  void PlanView(const xml_node& node, rn::Road& road, const std::string& where) {
    CheckAttributes(node, {}, where);
    for (const xml_node& child : node.children()) {
      if (child.type() != pugi::node_element) {
        continue;
      }
      if (!Is(child, "geometry")) {
        Structural(child, where + " <planView>");
        continue;
      }
      const std::string at =
          where + " plan-view geometry " + std::to_string(road.referenceLine.segments.size());
      CheckAttributes(child, {"s", "x", "y", "hdg", "length"}, at);
      PlanViewSegment segment;
      segment.s = Number(child, "s", at);
      segment.x = Number(child, "x", at);
      segment.y = Number(child, "y", at);
      segment.hdg = Number(child, "hdg", at);
      segment.length = Number(child, "length", at);
      bool seenShape = false;
      for (const xml_node& shape : child.children()) {
        if (shape.type() != pugi::node_element) {
          continue;
        }
        if (Is(shape, "line")) {
          Once(shape, &seenShape, at);
          CheckAttributes(shape, {}, at);
          segment.shape = Line{};
        } else if (Is(shape, "arc")) {
          Once(shape, &seenShape, at);
          CheckAttributes(shape, {"curvature"}, at);
          segment.shape = Arc{Number(shape, "curvature", at)};
        } else if (Is(shape, "spiral") || Is(shape, "poly3") || Is(shape, "paramPoly3")) {
          Fail(codes::kUnsupportedGeometry,
              at + " is a " + Tag(shape) + ", which Phase 1 does not support; only <line> and "
                                           "<arc> are");
        } else {
          Structural(shape, at);
        }
      }
      if (!seenShape) {
        Fail(codes::kMissingRequired, at + ": <geometry> has no <line> or <arc>");
      }
      road.referenceLine.segments.push_back(segment);
    }
  }

  void ElevationProfile(const xml_node& node, rn::Road& road, const std::string& where) {
    CheckAttributes(node, {}, where);
    for (const xml_node& child : node.children()) {
      if (child.type() != pugi::node_element) {
        continue;
      }
      if (!Is(child, "elevation")) {
        Structural(child, where + " <elevationProfile>");
        continue;
      }
      const std::string at = where + " elevation " + std::to_string(road.elevation.records.size());
      CheckAttributes(child, {"s", "a", "b", "c", "d"}, at);
      road.elevation.records.push_back(Cubic(child, "s", at));
    }
  }

  /// Phase 1 accepts lateral-profile records only when every coefficient is
  /// zero, so that they change nothing (ADR 0004 section 4).
  void LateralProfile(const xml_node& node, const std::string& where) {
    for (const xml_node& child : node.children()) {
      if (child.type() != pugi::node_element) {
        continue;
      }
      const bool known = Is(child, "superelevation") || Is(child, "crossfall") || Is(child, "shape");
      if (!known && !Is(child, "crossSectionSurface")) {
        Structural(child, where + " <lateralProfile>");
        continue;
      }
      if (known) {
        const CubicRecord record = Cubic(child, "s", where);
        if (record.a == 0.0 && record.b == 0.0 && record.c == 0.0 && record.d == 0.0) {
          continue;
        }
      }
      Fail(codes::kUnsupportedLateralProfile,
          where + " has a " + Tag(child) + (known ? " with a non-zero coefficient" : "") +
              "; Phase 1 does not support superelevation, crossfall, or lateral shape, which "
              "would move the borders out of the road plane");
    }
  }

  void Lanes(const xml_node& node, rn::Road& road, std::size_t roadIndex, const std::string& where) {
    for (const xml_node& child : node.children()) {
      if (child.type() != pugi::node_element) {
        continue;
      }
      if (Is(child, "laneOffset")) {
        const std::string at =
            where + " lane offset " + std::to_string(road.laneOffset.records.size());
        CheckAttributes(child, {"s", "a", "b", "c", "d"}, at);
        road.laneOffset.records.push_back(Cubic(child, "s", at));
      } else if (Is(child, "laneSection")) {
        road.sections.push_back(Section(child, road, roadIndex, where));
      } else {
        Structural(child, where + " <lanes>");
      }
    }
  }

  rn::LaneSection Section(
      const xml_node& node, const rn::Road& road, std::size_t roadIndex, const std::string& roadWhere) {
    const auto index = static_cast<std::uint32_t>(road.sections.size());
    const std::string where = roadWhere + " lane section " + std::to_string(index);
    // singleSide only says that one side is empty, which the lanes show.
    CheckAttributes(node, {"s", "singleSide"}, where);
    rn::LaneSection section;
    section.s = Number(node, "s", where);
    bool seen[3] = {false, false, false};
    std::vector<rn::Lane> groups[3];
    const char* const names[3] = {"left", "center", "right"};
    for (const xml_node& child : node.children()) {
      if (child.type() != pugi::node_element) {
        continue;
      }
      int group = -1;
      for (int g = 0; g < 3; ++g) {
        if (Is(child, names[g])) {
          group = g;
        }
      }
      if (group < 0) {
        Structural(child, where);
        continue;
      }
      Once(child, &seen[group], where);
      CheckAttributes(child, {}, where);
      for (const xml_node& lane : child.children()) {
        if (lane.type() != pugi::node_element) {
          continue;
        }
        if (!Is(lane, "lane")) {
          Structural(lane, where + " " + Tag(child));
          continue;
        }
        groups[group].push_back(LaneOf(lane, group, roadIndex, index, where));
      }
    }
    // Descending lane number, as ADR 0001 orders them; a repeated number
    // stays for validation to report.
    for (std::vector<rn::Lane>& group : groups) {
      std::stable_sort(group.begin(), group.end(),
          [](const rn::Lane& a, const rn::Lane& b) { return a.number > b.number; });
      section.lanes.insert(section.lanes.end(), group.begin(), group.end());
    }
    return section;
  }

  rn::Lane LaneOf(const xml_node& node, int group, std::size_t roadIndex, std::uint32_t section,
      const std::string& sectionWhere) {
    rn::Lane lane;
    lane.number = Integer(node, "id", sectionWhere + " lane");
    const std::string where = sectionWhere + " lane " + std::to_string(lane.number);
    CheckAttributes(node, {"id", "type", "level"}, where);
    const bool matches = group == 0 ? lane.number > 0 : group == 1 ? lane.number == 0
                                                                   : lane.number < 0;
    if (!matches) {
      Fail(codes::kLaneGroupMismatch,
          where + " is in <" + (group == 0 ? "left" : group == 1 ? "center"
                                                                 : "right") +
              ">, whose lanes are numbered " +
              (group == 0 ? "1, 2, ..." : group == 1 ? "0"
                                                     : "-1, -2, ..."));
    }
    const char* type = Required(node, "type", where);
    if (!IsKnownLaneType(type)) {
      Invalid(node, "type", type, where, "an OpenDRIVE 1.4 to 1.9 lane type");
    }
    lane.type = type;
    if (const xml_attribute level = node.attribute("level")) {
      const std::string_view value = Trim(level.value());
      if (value == "true" || value == "1") {
        lane.level = true;
      } else if (value == "false" || value == "0") {
        lane.level = false;
      } else {
        Invalid(node, "level", level.value(), where, "true or false");
      }
    }

    bool seenLink = false;
    for (const xml_node& child : node.children()) {
      if (child.type() != pugi::node_element) {
        continue;
      }
      if (Is(child, "link")) {
        Once(child, &seenLink, where);
        LaneLinks(child, roadIndex, section, lane.number, where);
      } else if (Is(child, "width")) {
        if (lane.number == 0) {
          Fail(codes::kMisplacedElement,
              where + " is the center lane, which has no width, but it has a <width>");
        }
        const std::string at = where + " width " + std::to_string(lane.width.records.size());
        CheckAttributes(child, {"sOffset", "a", "b", "c", "d"}, at);
        lane.width.records.push_back(Cubic(child, "sOffset", at));
      } else if (Is(child, "border")) {
        Fail(codes::kUnsupportedLaneBorder,
            where + " has a <border>; Phase 1 supports lane <width> only");
      } else if (Is(child, "roadMark")) {
        warnings_.Add(codes::kRoadMarksIgnored, "roadMark", where);
      } else if (Is(child, "speed") || Is(child, "access") || Is(child, "height") ||
                 Is(child, "material") || Is(child, "rule") || Is(child, "visibility")) {
        warnings_.Add(codes::kElementsIgnored, "lane " + Tag(child), where);
      } else {
        Ignore(child, where);
      }
    }
    if (lane.number != 0 && lane.width.records.empty()) {
      Fail(codes::kMissingRequired, where + " has no <width>");
    }
    return lane;
  }

  void LaneLinks(const xml_node& node, std::size_t roadIndex, std::uint32_t section,
      std::int32_t laneNumber, const std::string& where) {
    CheckAttributes(node, {}, where);
    for (const xml_node& child : node.children()) {
      if (child.type() != pugi::node_element) {
        continue;
      }
      const bool successor = Is(child, "successor");
      if (!successor && !Is(child, "predecessor")) {
        Ignore(child, where);
        continue;
      }
      const std::string at = where + " " + child.name();
      CheckAttributes(child, {"id", "layer"}, at);
      if (const xml_attribute layer = child.attribute("layer")) {
        const std::string_view value = layer.value();
        if (value == "temporary") {
          Fail(codes::kUnsupportedLaneLayer,
              at + " links to the temporary lane layer, which Phase 1 does not support");
        }
        if (value != "permanent") {
          Invalid(child, "layer", layer.value(), at, "permanent or temporary");
        }
      }
      statements_.push_back(
          LaneLinkStatement{roadIndex, section, laneNumber, successor, Integer(child, "id", at), at});
    }
  }

  rn::Junction JunctionOf(const xml_node& node, std::size_t index) {
    rn::Junction junction;
    junction.id = Required(node, "id", "junction at position " + std::to_string(index));
    const std::string where = "junction " + Quote(junction.id);
    if (const xml_attribute type = node.attribute("type")) {
      const std::string_view value = type.value();
      if (value == "direct" || value == "virtual" || value == "crossing") {
        Fail(codes::kUnsupportedJunction,
            where + " is a " + std::string(value) + " junction; Phase 1 supports default "
                                                    "junctions only");
      }
      if (value != "default") {
        Invalid(node, "type", type.value(), where, "default, direct, virtual, or crossing");
      }
    }
    CheckAttributes(node, {"id", "name", "type"}, where);
    junction.name = node.attribute("name").value();
    for (const xml_node& child : node.children()) {
      if (child.type() != pugi::node_element) {
        continue;
      }
      if (Is(child, "connection")) {
        junction.connections.push_back(ConnectionOf(child, junction, where));
      } else if (Is(child, "priority") || Is(child, "controller") || Is(child, "surface") ||
                 Is(child, "boundary") || Is(child, "elevationGrid") || Is(child, "crossPath")) {
        warnings_.Add(codes::kElementsIgnored, "junction " + Tag(child), where);
      } else {
        Ignore(child, where);
      }
    }
    return junction;
  }

  rn::Connection ConnectionOf(
      const xml_node& node, const rn::Junction& junction, const std::string& junctionWhere) {
    rn::Connection connection;
    connection.id = Required(node, "id",
        junctionWhere + " connection at position " + std::to_string(junction.connections.size()));
    const std::string where = junctionWhere + " connection " + Quote(connection.id);
    CheckAttributes(node, {"id", "incomingRoad", "connectingRoad", "contactPoint"}, where);
    connection.incomingRoad = Required(node, "incomingRoad", where);
    connection.connectingRoad = Required(node, "connectingRoad", where);
    connection.contact = Contact(node, "contactPoint", where);
    for (const xml_node& child : node.children()) {
      if (child.type() != pugi::node_element) {
        continue;
      }
      if (Is(child, "laneLink")) {
        CheckAttributes(child, {"from", "to"}, where);
        connection.laneLinks.emplace_back(Integer(child, "from", where), Integer(child, "to", where));
      } else if (Is(child, "predecessor") || Is(child, "successor")) {
        warnings_.Add(codes::kElementsIgnored, "connection " + Tag(child), where);
      } else {
        Ignore(child, where);
      }
    }
    return connection;
  }

  // The lane graph (ADR 0001 section 3).

  static std::uint32_t SectionAt(const rn::Road& road, ContactPoint end) {
    return end == ContactPoint::Start ? 0 : static_cast<std::uint32_t>(road.sections.size() - 1);
  }

  void BuildLaneGraph(RoadNetworkData& data) {
    // A repeated road ID resolves to its first road; validation rejects it.
    std::unordered_map<std::string, std::size_t> roads;
    for (std::size_t i = 0; i < data.roads.size(); ++i) {
      roads.emplace(data.roads[i].id, i);
    }
    const auto find = [&](const std::string& id) -> const rn::Road* {
      const auto it = roads.find(id);
      return it == roads.end() ? nullptr : &data.roads[it->second];
    };
    const auto edge = [&](const rn::Road& a, std::uint32_t sa, std::int32_t la, ContactPoint ea,
                          const rn::Road& b, std::uint32_t sb, std::int32_t lb, ContactPoint eb) {
      data.laneLinks.push_back(LaneLink{LaneEnd{LaneKey{a.id, sa, la}, ea},
          LaneEnd{LaneKey{b.id, sb, lb}, eb}});
    };

    // Lane <link> elements: to the neighbouring lane section, or across the
    // road link at the end of the road.
    for (const LaneLinkStatement& st : statements_) {
      const rn::Road& road = data.roads[st.road];
      const auto last = static_cast<std::uint32_t>(road.sections.size() - 1);
      if (!st.successor && st.section > 0) {
        edge(road, st.section, st.lane, ContactPoint::Start, road, st.section - 1, st.target,
            ContactPoint::End);
        continue;
      }
      if (st.successor && st.section < last) {
        edge(road, st.section, st.lane, ContactPoint::End, road, st.section + 1, st.target,
            ContactPoint::Start);
        continue;
      }
      const std::optional<rn::RoadLink>& link = st.successor ? road.successor : road.predecessor;
      const char* which = st.successor ? "successor" : "predecessor";
      if (!link) {
        Fail(codes::kLaneLinkWithoutRoadLink,
            st.where + " is at the " + (st.successor ? "end" : "start") + " of the road, which has no " +
                which + " to follow");
      }
      if (link->kind == rn::RoadLink::Kind::Junction) {
        Fail(codes::kLaneLinkIntoJunction,
            st.where + " leads into junction " + Quote(link->id) +
                ", where it does not say which connecting road it means; the junction's "
                "<laneLink> elements state these links");
      }
      const rn::Road* target = find(link->id);
      if (!target || target->sections.empty()) {
        continue; // An unresolved road link, or a road without lanes: validation reports it.
      }
      edge(road, st.section, st.lane, st.successor ? ContactPoint::End : ContactPoint::Start,
          *target, SectionAt(*target, *link->contact), st.target, *link->contact);
    }

    // Junction <laneLink> elements: from the incoming road's end at the
    // junction to the connecting road's contact end.
    for (const rn::Junction& junction : data.junctions) {
      for (const rn::Connection& connection : junction.connections) {
        const rn::Road* incoming = find(connection.incomingRoad);
        const rn::Road* connecting = find(connection.connectingRoad);
        if (!incoming || !connecting || connecting->junction != junction.id ||
            connection.laneLinks.empty()) {
          continue; // Validation reports the unresolved or foreign road.
        }
        // The connecting road's link at its contact end names the incoming
        // road and the end it meets; failing that, the incoming road links
        // to the junction at exactly one of its ends.
        std::optional<ContactPoint> incomingEnd;
        const std::optional<rn::RoadLink>& back =
            connection.contact == ContactPoint::Start ? connecting->predecessor : connecting->successor;
        if (back && back->kind == rn::RoadLink::Kind::Road && back->id == incoming->id) {
          incomingEnd = back->contact;
        } else {
          const auto names = [&](const std::optional<rn::RoadLink>& l) {
            return l && l->kind == rn::RoadLink::Kind::Junction && l->id == junction.id;
          };
          const bool atStart = names(incoming->predecessor);
          const bool atEnd = names(incoming->successor);
          if (atStart != atEnd) {
            incomingEnd = atStart ? ContactPoint::Start : ContactPoint::End;
          }
        }
        if (!incomingEnd) {
          Fail(codes::kConnectionUnresolved,
              "junction " + Quote(junction.id) + " connection " + Quote(connection.id) +
                  ": the end of incoming road " + Quote(incoming->id) +
                  " at the junction cannot be determined; neither does connecting road " +
                  Quote(connecting->id) + " link back to it, nor does it link to the junction at "
                                          "exactly one end");
        }
        if (incoming->sections.empty() || connecting->sections.empty()) {
          continue;
        }
        for (const auto& [from, to] : connection.laneLinks) {
          edge(*incoming, SectionAt(*incoming, *incomingEnd), from, *incomingEnd, *connecting,
              SectionAt(*connecting, connection.contact), to, connection.contact);
        }
      }
    }
    CanonicalizeLaneLinks(data.laneLinks);
  }

  xml_node root_;
  Warnings warnings_;
  std::vector<LaneLinkStatement> statements_;
};

void Summarize(const std::vector<Warning>& warnings, RoadNetworkData* data) {
  data->warnings.clear();
  for (const Warning& warning : warnings) {
    data->warnings.push_back(
        WarningSummary{warning.code.substr(1, warning.code.size() - 2), warning.count});
  }
}

} // namespace

bool IsKnownLaneType(std::string_view type) noexcept {
  return std::find(std::begin(kLaneTypes), std::end(kLaneTypes), type) != std::end(kLaneTypes);
}

bool DecodeHeader(const Document& document, SourceInfo* source, std::vector<Warning>* warnings,
    Diagnostic* error) {
  Decoder decoder(RootOf(document.Internals()));
  try {
    decoder.Header(*source);
  } catch (const Failure& failure) {
    if (error) {
      *error = failure.diagnostic;
    }
    return false;
  }
  *warnings = decoder.Collected().Finish();
  return true;
}

bool Decode(const Document& document, RoadNetworkData* data, std::vector<Warning>* warnings,
    Diagnostic* error) {
  Decoder decoder(RootOf(document.Internals()));
  RoadNetworkData result;
  result.source = document.Source();
  try {
    decoder.Network(result);
  } catch (const Failure& failure) {
    if (error) {
      *error = failure.diagnostic;
    }
    return false;
  }
  *warnings = decoder.Collected().Finish();
  Summarize(*warnings, &result);
  *data = std::move(result);
  return true;
}

bool ImportNetwork(std::string_view bytes, bool metadataOnly, ImportResult* result,
    Diagnostic* error) {
  Diagnostic ignored;
  Diagnostic& out = error ? *error : ignored;
  const std::unique_ptr<Document> document = Document::Parse(bytes, &out);
  if (!document) {
    return false;
  }
  ImportResult import;
  import.counts = document->CountElements();
  if (metadataOnly) {
    import.data.source = document->Source();
    if (!DecodeHeader(*document, &import.data.source, &import.warnings, &out)) {
      return false;
    }
    Summarize(import.warnings, &import.data);
  } else {
    if (!Decode(*document, &import.data, &import.warnings, &out)) {
      return false;
    }
    std::vector<ValidationWarning> shared;
    if (const std::optional<rn::Diagnostic> invalid = Validate(import.data, &shared)) {
      // Relayed unchanged (ADR 0006 section 6): the message starts with the
      // bracketed shared code, which is split off as the code.
      out.code = "[" + invalid->code + "]";
      out.detail = invalid->message;
      if (out.detail.compare(0, out.code.size(), out.code) == 0) {
        out.detail.erase(0, out.code.size());
        if (!out.detail.empty() && out.detail.front() == ' ') {
          out.detail.erase(0, 1);
        }
      }
      return false;
    }
    // Shared warnings are relayed unchanged too, after the ODR- ones, which
    // keeps the codes in ascending order.
    for (const ValidationWarning& warning : shared) {
      std::string code = "[" + warning.code + "]";
      std::string detail = warning.message;
      if (detail.compare(0, code.size(), code) == 0) {
        detail.erase(0, std::min(detail.size(), code.size() + 1));
      }
      import.warnings.push_back(Warning{std::move(code), warning.count, std::move(detail)});
    }
    Summarize(import.warnings, &import.data);
  }
  *result = std::move(import);
  return true;
}

} // namespace openstrata::rn::odr
