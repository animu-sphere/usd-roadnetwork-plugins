// SPDX-License-Identifier: Apache-2.0
// The OpenDRIVE decoder without OpenUSD (ODR-02): the minimal network decodes
// to the expected model and lane graph, every negative fixture fails with its
// code in a full read and opens in a metadata-only read, CARLA Town01 and
// Town02 decode and validate, and number parsing does not depend on the C
// locale.
#include "io/Decoder.h"
#include "io/Diagnostics.h"
#include "openstrata/rn/Geometry.h"

#include <clocale>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#ifndef ROADNETWORK_FIXTURES
#error "ROADNETWORK_FIXTURES must name the fixture directory"
#endif

namespace {

using namespace openstrata::rn;
using namespace openstrata::rn::odr;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
  }
}

bool Near(double a, double b, double tolerance = 1e-12) {
  return std::fabs(a - b) <= tolerance;
}

std::string Fixture(const std::string& relative) {
  std::ifstream in(std::string(ROADNETWORK_FIXTURES) + "/" + relative, std::ios::binary);
  if (!in) {
    Check(false, "cannot open fixture " + relative);
    return {};
  }
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

std::string Replace(std::string text, const std::string& from, const std::string& to) {
  const std::size_t at = text.find(from);
  Check(at != std::string::npos, "replace: \"" + from + "\" not found");
  if (at != std::string::npos) {
    text.replace(at, from.size(), to);
  }
  return text;
}

bool Import(const std::string& bytes, bool metadataOnly, ImportResult* result, const std::string& what) {
  Diagnostic error;
  const bool ok = ImportNetwork(bytes, metadataOnly, result, &error);
  Check(ok, what + ": " + error.code + " " + error.detail);
  return ok;
}

void ExpectFailure(const std::string& bytes, const std::string& code, const std::string& what) {
  ImportResult result;
  Diagnostic error;
  Check(!ImportNetwork(bytes, false, &result, &error), what + ": imported, expected " + code);
  Check(error.code == "[" + code + "]", what + ": got " + error.code + " (" + error.detail +
                                            "), expected " + code);
  Check(!error.detail.empty() && error.detail.front() != '[', what + ": bad detail " + error.detail);
}

const Warning* FindWarning(const ImportResult& result, std::string_view code) {
  for (const Warning& warning : result.warnings) {
    if (warning.code == code) {
      return &warning;
    }
  }
  return nullptr;
}

std::string Describe(const LaneEnd& end) {
  return end.lane.roadId + ":" + std::to_string(end.lane.sectionIndex) + ":" +
         std::to_string(end.lane.laneNumber) + (end.end == ContactPoint::Start ? ":start" : ":end");
}

/// The six edges of network.xodr, in canonical order. Road IDs sort by
/// bytes, so "10" comes between "1" and "2".
void CheckNetworkLaneGraph(const RoadNetworkData& data, const std::string& what) {
  const char* const expected[][2] = {
      {"1:0:-1:end", "1:1:-1:start"},
      {"1:0:1:end", "1:1:1:start"},
      {"1:1:-1:end", "10:0:-1:start"},
      {"1:1:1:end", "10:0:1:start"},
      {"10:0:-1:end", "2:0:-1:start"},
      {"10:0:1:end", "2:0:1:start"},
  };
  Check(data.laneLinks.size() == 6, what + ": " + std::to_string(data.laneLinks.size()) + " lane links");
  for (std::size_t i = 0; i < 6 && i < data.laneLinks.size(); ++i) {
    const std::string a = Describe(data.laneLinks[i].a);
    const std::string b = Describe(data.laneLinks[i].b);
    Check(a == expected[i][0] && b == expected[i][1],
        what + ": lane link " + std::to_string(i) + " is " + a + " - " + b);
  }
}

void TestNetwork() {
  const std::string text = Fixture("decoder/network.xodr");
  ImportResult result;
  if (!Import(text, false, &result, "network.xodr")) {
    return;
  }
  const RoadNetworkData& data = result.data;

  const SourceInfo& source = data.source;
  Check(source.format == "opendrive" && source.versionMajor == 1 && source.versionMinor == 7,
      "network: version");
  Check(source.vendor == "usd-roadnetwork-plugins", "network: vendor");
  Check(source.geoReference ==
            std::optional<std::string>(
                "+proj=tmerc +lat_0=35 +lon_0=139 +k=1 +x_0=0 +y_0=0 +ellps=GRS80 +units=m"),
      "network: geoReference is not the trimmed CDATA");
  Check(source.offset && source.offset->x == 512000.0 && source.offset->y == 4100000.0 &&
            source.offset->z == 12.5 && source.offset->hdg == 0.0,
      "network: offset");

  Check(data.roads.size() == 3 && data.junctions.size() == 1, "network: road and junction counts");
  if (data.roads.size() != 3 || data.junctions.size() != 1) {
    return;
  }

  const Road& main = data.roads[0];
  Check(main.id == "1" && main.name == "Main" && main.length == 50.0 && !main.junction,
      "road 1: identity");
  Check(!main.predecessor && main.successor && main.successor->kind == RoadLink::Kind::Junction &&
            main.successor->id == "100" && !main.successor->contact,
      "road 1: links");
  Check(main.speeds.size() == 1 && main.speeds[0].s == 0.0 &&
            Near(main.speeds[0].maxMetresPerSecond, 50.0 / 3.6),
      "road 1: 50 km/h");
  Check(main.referenceLine.segments.size() == 1 &&
            std::holds_alternative<Line>(main.referenceLine.segments[0].shape),
      "road 1: one line");
  Check(main.elevation.records.size() == 1 && main.elevation.records[0].a == 1.0 &&
            main.elevation.records[0].b == 0.01,
      "road 1: elevation");
  Check(main.laneOffset.records.size() == 1, "road 1: lane offset");
  Check(main.sections.size() == 2, "road 1: two lane sections");
  if (main.sections.size() == 2) {
    const LaneSection& second = main.sections[1];
    Check(second.s == 25.0, "road 1 section 1: s");
    // The source lists -2 before -1; the model orders lanes by descending number.
    std::vector<std::int32_t> numbers;
    for (const Lane& lane : second.lanes) {
      numbers.push_back(lane.number);
    }
    Check((numbers == std::vector<std::int32_t>{1, 0, -1, -2}), "road 1 section 1: lane order");
    if (second.lanes.size() == 4) {
      const Lane& shoulder = second.lanes[3];
      Check(shoulder.type == "shoulder" && shoulder.level && shoulder.width.records.size() == 1 &&
                shoulder.width.records[0].s0 == 0.0 && shoulder.width.records[0].b == 0.04,
          "road 1 section 1 lane -2");
      Check(second.lanes[1].type == "none" && second.lanes[1].width.records.empty(),
          "road 1 section 1: center lane");
    }
  }

  const Road& connector = data.roads[1];
  Check(connector.id == "10" && connector.junction == std::optional<std::string>("100"),
      "road 10: junction");
  Check(connector.predecessor && connector.predecessor->kind == RoadLink::Kind::Road &&
            connector.predecessor->id == "1" && connector.predecessor->contact == ContactPoint::End,
      "road 10: predecessor");
  Check(connector.successor && connector.successor->id == "2" &&
            connector.successor->contact == ContactPoint::Start,
      "road 10: successor");

  const Road& bend = data.roads[2];
  Check(bend.referenceLine.segments.size() == 2, "road 2: two segments");
  if (bend.referenceLine.segments.size() == 2) {
    const auto* arc = std::get_if<Arc>(&bend.referenceLine.segments[0].shape);
    Check(arc && arc->curvature == 0.02, "road 2: arc curvature");
    Check(bend.referenceLine.segments[1].s == 30.0 && bend.referenceLine.segments[1].hdg == 0.6,
        "road 2: line start");
  }
  Check(bend.speeds.empty(), "road 2: a \"no limit\" speed is not kept");
  Check(bend.laneOffset.records.size() == 1 && bend.laneOffset.records[0].a == 0.25,
      "road 2: lane offset");

  const Junction& junction = data.junctions[0];
  Check(junction.id == "100" && junction.name == "Crossing" && junction.connections.size() == 2,
      "junction 100");
  if (junction.connections.size() == 2) {
    const Connection& c = junction.connections[1];
    Check(c.id == "1" && c.incomingRoad == "2" && c.connectingRoad == "10" &&
              c.contact == ContactPoint::End &&
              (c.laneLinks == std::vector<std::pair<std::int32_t, std::int32_t>>{{1, 1}, {-1, -1}}),
        "junction 100 connection 1");
  }

  CheckNetworkLaneGraph(data, "network");

  // One warning per kind, in ascending code order, mirrored in the model.
  const struct {
    const char* code;
    std::uint32_t count;
    const char* mentions;
  } warnings[] = {
      {"[ODR-W201]", 1, "road \"1\" lane section 0 lane 0"},
      {"[ODR-W202]", 1, "signal (1)"},
      {"[ODR-W203]", 1, "object (1)"},
      {"[ODR-W204]", 1, "userData (1)"},
      {"[ODR-W205]", 1, "max=\"no limit\""},
      {"[ODR-W206]", 1, "lane <speed> (1)"},
      {"[ODR-W207]", 3, "road@rule (1), type@type (2)"},
  };
  Check(result.warnings.size() == std::size(warnings),
      "network: " + std::to_string(result.warnings.size()) + " warnings");
  Check(data.warnings.size() == result.warnings.size(), "network: warnings are not in the model");
  for (std::size_t i = 0; i < std::size(warnings) && i < result.warnings.size(); ++i) {
    const Warning& w = result.warnings[i];
    Check(w.code == warnings[i].code && w.count == warnings[i].count &&
              w.detail.find(warnings[i].mentions) != std::string::npos,
        "network warning " + std::to_string(i) + ": " + w.code + " " + std::to_string(w.count) +
            " " + w.detail);
    if (i < data.warnings.size()) {
      Check("[" + data.warnings[i].code + "]" == w.code && data.warnings[i].count == w.count,
          "network: model warning " + std::to_string(i));
    }
  }

  // The same lane graph from the junction alone: the connecting road states
  // no links, so each connection falls back to the incoming road's link to
  // the junction.
  std::string junctionOnly = Replace(text,
      "      <predecessor elementType=\"road\" elementId=\"1\" contactPoint=\"end\"/>\n"
      "      <successor elementType=\"road\" elementId=\"2\" contactPoint=\"start\"/>\n",
      "");
  junctionOnly = Replace(junctionOnly,
      "            <link>\n"
      "              <predecessor id=\"1\"/>\n"
      "              <successor id=\"1\"/>\n"
      "            </link>\n",
      "");
  ImportResult fallback;
  if (Import(junctionOnly, false, &fallback, "network without connecting-road links")) {
    CheckNetworkLaneGraph(fallback.data, "network without connecting-road links");
  }
}

void TestMetadataOnly() {
  ImportResult result;
  if (!Import(Fixture("decoder/network.xodr"), true, &result, "metadata-only network.xodr")) {
    return;
  }
  Check(result.data.roads.empty() && result.data.junctions.empty() && result.data.laneLinks.empty(),
      "metadata-only: decoded content");
  Check(result.data.source.geoReference && result.data.source.offset,
      "metadata-only: geoReference and offset are header provenance");
  Check(result.counts.roads == 3 && result.counts.junctions == 1 && result.counts.lanes == 13,
      "metadata-only: element counts");
  Check(result.warnings.size() == 1 && result.warnings[0].code == "[ODR-W204]",
      "metadata-only: only the header's own warnings");
}

void TestNegativeFixtures() {
  const struct {
    const char* file;
    const char* code;
  } cases[] = {
      {"missing-attribute", "ODR-E008"},
      {"invalid-number", "ODR-E009"},
      {"unknown-lane-type", "ODR-E009"},
      {"duplicate-plan-view", "ODR-E010"},
      {"lane-group-mismatch", "ODR-E101"},
      {"lane-link-without-road-link", "ODR-E102"},
      {"lane-link-into-junction", "ODR-E103"},
      {"connection-unresolved", "ODR-E104"},
      {"poly3", "ODR-E201"},
      {"superelevation", "ODR-E202"},
      {"lane-border", "ODR-E203"},
      {"direct-junction", "ODR-E204"},
      {"temporary-lane-layer", "ODR-E205"},
      {"unknown-lane-element", "ODR-E206"},
      {"duplicate-road", "RN-E102"},
      {"unresolved-link", "RN-E111"},
      {"unresolved-lane-link", "RN-E115"},
      {"non-finite", "RN-E301"},
      {"plan-view-gap", "RN-E305"},
      {"inner-border", "RN-E308"},
  };
  for (const auto& c : cases) {
    const std::string path = std::string("decoder/") + c.file + ".xodr";
    const std::string text = Fixture(path);
    ExpectFailure(text, c.code, path);
    // A metadata-only read does not decode the content (ADR 0006 section 2).
    ImportResult result;
    Import(text, true, &result, "metadata-only " + path);
  }
}

void TestInline() {
  const std::string text = Fixture("basic.xodr");
  ImportResult result;
  Import(text, false, &result, "basic.xodr");
  Check(result.warnings.empty(), "basic.xodr: warnings");

  // Number syntax: xsd:double, with surrounding whitespace.
  for (const char* good : {" 10 ", "10.", "1e1", "+10.0E+0", "100e-1"}) {
    Import(Replace(text, "length=\"10.0\" id", std::string("length=\"") + good + "\" id"), false,
        &result, std::string("road length ") + good);
  }
  for (const char* bad : {"", "1e", "0x10", "10 m", "1,0", ".", "inf", "1.0.0"}) {
    ExpectFailure(Replace(text, "length=\"10.0\" id", std::string("length=\"") + bad + "\" id"),
        "ODR-E009", std::string("road length \"") + bad + "\"");
  }
  ExpectFailure(Replace(text, "length=\"10.0\" id", "length=\"INF\" id"), "RN-E301", "length INF");
  ExpectFailure(Replace(text, "length=\"10.0\" id", "length=\"1e999\" id"), "RN-E301", "length 1e999");
  ExpectFailure(Replace(text, "id=\"-1\"", "id=\"-2147483649\""), "ODR-E009", "lane id beyond 32 bits");
  ExpectFailure(Replace(text, "level=\"false\"/>", "level=\"no\"/>"), "ODR-E009", "level=\"no\"");
  ExpectFailure(Replace(text, " junction=\"-1\"", ""), "ODR-E008", "road without junction");
  ExpectFailure(Replace(text, "<width sOffset=\"0.0\" a=\"3.5\" b=\"0.0\" c=\"0.0\" d=\"0.0\"/>", ""),
      "ODR-E008", "a right lane without a width");
  ExpectFailure(Replace(text, "<lane id=\"0\" type=\"none\" level=\"false\"/>",
                    "<lane id=\"0\" type=\"none\"><width sOffset=\"0\" a=\"1\" b=\"0\" c=\"0\" "
                    "d=\"0\"/></lane>"),
      "ODR-E010", "a center lane with a width");
  ExpectFailure(Replace(text, "<link/>",
                    "<link><predecessor elementType=\"road\" elementId=\"1\"/></link>"),
      "ODR-E008", "a road link without contactPoint");
  ExpectFailure(Replace(text, "<link/>",
                    "<link><predecessor elementType=\"road\" elementId=\"1\" contactPoint=\"middle\"/></link>"),
      "ODR-E009", "contactPoint=\"middle\"");
  ExpectFailure(Replace(text, "<link/>",
                    "<link><predecessor elementType=\"junction\" elementId=\"9\" elementS=\"1\"/></link>"),
      "ODR-E204", "a link with elementS");
  ExpectFailure(Replace(text, "<line/>", "<line/><arc curvature=\"0.1\"/>"), "ODR-E010", "two shapes");
  ExpectFailure(Replace(text, "<line/>", ""), "ODR-E008", "no shape");
  ExpectFailure(Replace(text, "<line/>", "<poly3 a=\"0\" b=\"0\" c=\"0\" d=\"0\"/>"), "ODR-E201", "poly3");
  ExpectFailure(Replace(text, "</planView>",
                    "</planView><lateralProfile><crossSectionSurface/></lateralProfile>"),
      "ODR-E202", "crossSectionSurface");

  // A speed without a unit is in m/s; a lane without a link and an unknown
  // element under a road are warnings, not errors.
  const std::string speed = Replace(text, "<link/>",
      "<link/><type s=\"0\" type=\"town\"><speed max=\"12.5\"/></type><foo/>");
  if (Import(speed, false, &result, "speed without unit")) {
    Check(result.data.roads[0].speeds.size() == 1 &&
              result.data.roads[0].speeds[0].maxMetresPerSecond == 12.5,
        "a speed without a unit is m/s");
    Check(FindWarning(result, "[ODR-W208]") && FindWarning(result, "[ODR-W208]")->count == 1,
        "an unknown road child is ODR-W208");
  }
}

void TestPlanViewCurves() {
  // ADR 0010: <spiral> and <paramPoly3> decode, and their end poses are the
  // golden values of ADR 0010 section 6.
  const std::string text = Fixture("decoder/plan-view-curves.xodr");
  ImportResult result;
  if (!Import(text, false, &result, "plan-view-curves.xodr")) {
    return;
  }
  Check(result.warnings.empty(), "plan-view-curves.xodr: no warnings");
  const std::vector<Road>& roads = result.data.roads;
  Check(roads.size() == 3, "plan-view-curves.xodr: three roads");
  if (roads.size() != 3) {
    return;
  }
  const auto* spiral = std::get_if<Spiral>(&roads[0].referenceLine.segments[0].shape);
  Check(spiral && spiral->curvatureStart == 0.0 && spiral->curvatureEnd == 0.02, "S1: spiral curvatures");
  const auto* p1 = std::get_if<ParamPoly3>(&roads[1].referenceLine.segments[0].shape);
  Check(p1 && p1->aU == 0.0 && p1->bU == 28.8 && p1->cU == -14.4 && p1->dU == 0.0 && p1->aV == 0.0 &&
            p1->bV == 0.0 && p1->cV == -14.4 && p1->dV == 0.0 && !p1->pRange,
      "P1: coefficients, and an absent pRange stays absent");
  const auto* p2 = std::get_if<ParamPoly3>(&roads[2].referenceLine.segments[0].shape);
  Check(p2 && p2->dU == -0.0001 && p2->dV == 0.0002 && p2->pRange == ParamRange::ArcLength,
      "P2: coefficients and pRange");

  const struct {
    std::size_t road;
    double x;
    double y;
    double hdg;
  } ends[] = {
      {0, 90.452423790027208, 31.026830172338111, 1.0},
      {1, 135.59999995384651, -59.999999953846512, -3.1415926567948967},
      {2, 9.9, 1.2, 0.26188526468051904},
  };
  for (const auto& end : ends) {
    const Road& road = roads[end.road];
    const Pose2 pose = EvaluateReferenceLine(road.referenceLine, road.length);
    Check(Near(pose.x, end.x, 1e-9) && Near(pose.y, end.y, 1e-9) && Near(pose.hdg, end.hdg, 1e-12),
        "road " + road.id + ": end pose is the ADR 0010 golden value");
  }

  ExpectFailure(Replace(text, "dV=\"0.0002\" pRange=\"arcLength\"", "dV=\"0.0002\" pRange=\"length\""), "ODR-E009",
      "pRange=\"length\"");
  ExpectFailure(Replace(text, " dV=\"0.0002\"", ""), "ODR-E008", "a paramPoly3 without dV");
  ExpectFailure(Replace(text, " curvEnd=\"0.02\"", ""), "ODR-E008", "a spiral without curvEnd");
  const std::string extra = Replace(text, "<spiral curvStart=\"0.0\" curvEnd=\"0.02\"/>",
      "<spiral curvStart=\"0.0\" curvEnd=\"0.02\" extra=\"1\"/>");
  if (Import(extra, false, &result, "a spiral with an unknown attribute")) {
    Check(FindWarning(result, "[ODR-W207]") != nullptr, "an unknown spiral attribute is ODR-W207");
  }
  ExpectFailure(Replace(text, "bU=\"28.8\" cU=\"-14.4\"", "bU=\"0.0\" cU=\"-14.4\""), "RN-E310",
      "a paramPoly3 whose tangent vanishes at its start");

  // ADR 0010 section 6: P2 with dU -0.002 and dV 0.0005 is 17% short of its
  // length, a counted RN-W302 that does not fail the read.
  const std::string shorter =
      Replace(Replace(text, "dU=\"-0.0001\"", "dU=\"-0.002\""), "dV=\"0.0002\"", "dV=\"0.0005\"");
  if (Import(shorter, false, &result, "a short paramPoly3")) {
    const Warning* warning = FindWarning(result, "[RN-W302]");
    Check(warning && warning->count == 1, "a paramPoly3 17% short of its length raises one RN-W302");
  }
}

void TestCarla() {
  const struct {
    const char* file;
    std::size_t roads;
    std::size_t junctions;
  } towns[] = {{"carla/Town01.xodr", 98, 12}, {"carla/Town02.xodr", 68, 8}};
  for (const auto& town : towns) {
    ImportResult result;
    if (!Import(Fixture(town.file), false, &result, town.file)) {
      continue;
    }
    const RoadNetworkData& data = result.data;
    Check(data.roads.size() == town.roads && data.junctions.size() == town.junctions,
        std::string(town.file) + ": counts");
    std::size_t sections = 0;
    std::size_t lanes = 0;
    for (const Road& road : data.roads) {
      sections += road.sections.size();
      for (const LaneSection& section : road.sections) {
        lanes += section.lanes.size();
      }
      for (const SpeedRecord& speed : road.speeds) {
        Check(Near(speed.maxMetresPerSecond, 25 * 0.44704), std::string(town.file) + ": 25 mph");
      }
    }
    Check(lanes == result.counts.lanes, std::string(town.file) + ": lanes");
    Check(!FindWarning(result, "[ODR-W208]"), std::string(town.file) + ": unknown elements");
    std::printf("%s: %zu roads, %zu lane sections, %zu lanes, %zu junctions, %zu lane links\n",
        town.file, data.roads.size(), sections, lanes, data.junctions.size(), data.laneLinks.size());
    for (const Warning& warning : result.warnings) {
      std::printf("  %s %s\n", warning.code.c_str(), warning.detail.c_str());
    }
  }
}

/// A host application may set a C locale whose decimal separator is a
/// comma; numbers must still parse.
void TestLocale() {
  const char* names[] = {"de_DE.UTF-8", "de_DE", "de-DE", "German_Germany.1252"};
  const char* set = nullptr;
  for (const char* name : names) {
    if (std::setlocale(LC_NUMERIC, name)) {
      set = name;
      break;
    }
  }
  if (!set || std::localeconv()->decimal_point[0] != ',') {
    std::printf("locale: no comma-decimal C locale available; skipped\n");
    std::setlocale(LC_NUMERIC, "C");
    return;
  }
  ImportResult result;
  if (Import(Fixture("decoder/network.xodr"), false, &result, std::string("network.xodr in ") + set)) {
    Check(result.data.roads[0].elevation.records[0].b == 0.01 &&
              result.data.roads[2].laneOffset.records[0].a == 0.25,
        std::string("numbers under the ") + set + " locale");
  }
  std::setlocale(LC_NUMERIC, "C");
  std::printf("locale: checked under %s\n", set);
}

} // namespace

int main() {
  TestNetwork();
  TestMetadataOnly();
  TestNegativeFixtures();
  TestInline();
  TestPlanViewCurves();
  TestCarla();
  TestLocale();
  if (failures) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  std::printf("roadnetworkOpendrive_decoder: all checks passed\n");
  return 0;
}
