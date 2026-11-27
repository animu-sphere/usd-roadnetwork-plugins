// SPDX-License-Identifier: Apache-2.0
// The shared writer against the stage contract (WORKSPACE.md section 5):
// prim names (ADR 0005 sections 1-2), the hierarchy and relationships
// (ADR 0005 sections 3-5), one check group per property group of ADR 0002
// section 3, the axes and origin shift of ADR 0003, and the Sdf-only,
// transactional authoring of ADR 0006 sections 3 and 5.
#include "openstrata/rn/usd/LayerWriter.h"
#include "openstrata/rn/usd/PrimNames.h"

#include "openstrata/rn/Tessellation.h"
#include "openstrata/rn/Validation.h"

#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/vt/array.h"
#include "pxr/usd/sdf/attributeSpec.h"
#include "pxr/usd/sdf/changeBlock.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/sdf/primSpec.h"
#include "pxr/usd/sdf/relationshipSpec.h"
#include "pxr/usd/sdf/types.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <optional>
#include <string>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace rn = openstrata::rn;
namespace rnusd = openstrata::rn::usd;

namespace {

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
  }
}

// ---- Model builders --------------------------------------------------------

rn::Lane MakeLane(std::int32_t number, const char* type, double width) {
  rn::Lane lane;
  lane.number = number;
  lane.type = type;
  if (number != 0) {
    lane.width.records.push_back(rn::CubicRecord{0.0, width, 0.0, 0.0, 0.0});
  }
  return lane;
}

rn::Road MakeLineRoad(const char* id, double x, double y, double length) {
  rn::Road road;
  road.id = id;
  road.length = length;
  road.referenceLine.segments.push_back(rn::PlanViewSegment{0.0, x, y, 0.0, length, rn::Line{}});
  rn::LaneSection section;
  section.lanes = {MakeLane(0, "none", 0.0), MakeLane(-1, "driving", 3.5)};
  road.sections.push_back(section);
  return road;
}

rn::LaneLink Link(const char* roadA, std::uint32_t sectionA, std::int32_t laneA, rn::ContactPoint endA,
    const char* roadB, std::uint32_t sectionB, std::int32_t laneB, rn::ContactPoint endB) {
  return rn::LaneLink{rn::LaneEnd{rn::LaneKey{roadA, sectionA, laneA}, endA},
      rn::LaneEnd{rn::LaneKey{roadB, sectionB, laneB}, endB}};
}

/// Three roads and one junction. Road `1` has two lane sections and a left
/// lane in the first; `a_b` is the junction's connecting road, an arc; road
/// `2` leaves the junction. IDs `a_b` and `J 1` need escaping.
rn::RoadNetworkData MakeNetwork() {
  using rn::ContactPoint;
  rn::RoadNetworkData data;
  data.source.format = "opendrive";
  data.source.versionMajor = 1;
  data.source.versionMinor = 8;
  data.source.name = "writer test";
  data.source.vendor = "hand-authored";
  data.source.geoReference = "+proj=utm +zone=32 +datum=WGS84";
  data.source.offset = rn::SourceOffset{100.0, 200.0, 3.0, 0.25};

  rn::Road first = MakeLineRoad("1", 0.0, 0.0, 20.0);
  first.name = "first";
  first.successor = rn::RoadLink{rn::RoadLink::Kind::Junction, "J 1", std::nullopt};
  first.sections[0].lanes.insert(first.sections[0].lanes.begin(), MakeLane(1, "sidewalk", 2.0));
  rn::LaneSection second;
  second.s = 10.0;
  second.lanes = {MakeLane(0, "none", 0.0), MakeLane(-1, "driving", 3.5)};
  first.sections.push_back(second);
  first.elevation.records.push_back(rn::CubicRecord{0.0, 1.0, 0.01, 0.0, 0.0});
  first.laneOffset.records.push_back(rn::CubicRecord{0.0, 0.25, 0.0, 0.0, 0.0});
  first.speeds.push_back(rn::SpeedRecord{0.0, 13.8889});

  rn::Road connecting;
  connecting.id = "a_b";
  connecting.length = 5.0;
  connecting.junction = "J 1";
  connecting.predecessor = rn::RoadLink{rn::RoadLink::Kind::Road, "1", ContactPoint::End};
  connecting.successor = rn::RoadLink{rn::RoadLink::Kind::Road, "2", ContactPoint::Start};
  connecting.referenceLine.segments.push_back(
      rn::PlanViewSegment{0.0, 20.0, 0.0, 0.0, 5.0, rn::Arc{0.1}});
  rn::LaneSection arcSection;
  arcSection.lanes = {MakeLane(0, "none", 0.0), MakeLane(-1, "driving", 3.5)};
  connecting.sections.push_back(arcSection);

  rn::Road last = MakeLineRoad("2", 25.0, 1.25, 10.0);
  last.predecessor = rn::RoadLink{rn::RoadLink::Kind::Junction, "J 1", std::nullopt};

  data.roads = {first, connecting, last};

  rn::Junction junction;
  junction.id = "J 1";
  junction.name = "crossing";
  rn::Connection connection;
  connection.id = "0";
  connection.incomingRoad = "1";
  connection.connectingRoad = "a_b";
  connection.contact = ContactPoint::Start;
  connection.laneLinks = {{-1, -1}};
  junction.connections.push_back(connection);
  data.junctions.push_back(junction);

  data.laneLinks = {
      Link("1", 0, -1, ContactPoint::End, "1", 1, -1, ContactPoint::Start),
      Link("1", 1, -1, ContactPoint::End, "a_b", 0, -1, ContactPoint::Start),
      Link("a_b", 0, -1, ContactPoint::End, "2", 0, -1, ContactPoint::Start),
  };
  rn::CanonicalizeLaneLinks(data.laneLinks);
  data.warnings = {{"RN-W301", 1}, {"ODR-W201", 7}};
  return data;
}

rnusd::ImportRecord MakeRecord(const rn::RoadNetworkData& data) {
  rnusd::ImportRecord record;
  record.importer = "roadnetwork-test";
  record.importerVersion = "9.8.7";
  record.roadCount = static_cast<std::uint32_t>(data.roads.size());
  record.junctionCount = static_cast<std::uint32_t>(data.junctions.size());
  for (const rn::Road& road : data.roads) {
    for (const rn::LaneSection& section : road.sections) {
      record.laneCount += static_cast<std::uint32_t>(section.lanes.size());
    }
  }
  return record;
}

// ---- Layer access ----------------------------------------------------------

template <class T>
std::optional<T> Value(const SdfLayerHandle& layer, const char* path) {
  const SdfAttributeSpecHandle attribute = layer->GetAttributeAtPath(SdfPath(path));
  if (!attribute) {
    return std::nullopt;
  }
  const VtValue value = attribute->GetDefaultValue();
  if (!value.IsHolding<T>()) {
    return std::nullopt;
  }
  return value.UncheckedGet<T>();
}

bool Has(const SdfLayerHandle& layer, const char* path) {
  return static_cast<bool>(layer->GetObjectAtPath(SdfPath(path)));
}

SdfPathVector Targets(const SdfLayerHandle& layer, const char* path) {
  const SdfRelationshipSpecHandle relationship = layer->GetRelationshipAtPath(SdfPath(path));
  if (!relationship) {
    return {};
  }
  const auto items = relationship->GetTargetPathList().GetExplicitItems();
  return SdfPathVector(items.begin(), items.end());
}

std::string TypeName(const SdfLayerHandle& layer, const char* path) {
  const SdfPrimSpecHandle prim = layer->GetPrimAtPath(SdfPath(path));
  return prim ? prim->GetTypeName().GetString() : std::string("<missing>");
}

std::vector<std::string> ChildNames(const SdfLayerHandle& layer, const char* path) {
  std::vector<std::string> names;
  const SdfPrimSpecHandle prim = layer->GetPrimAtPath(SdfPath(path));
  if (prim) {
    for (const SdfPrimSpecHandle& child : prim->GetNameChildren()) {
      names.push_back(child->GetName());
    }
  }
  return names;
}

SdfPathVector Paths(std::initializer_list<const char*> paths) {
  SdfPathVector result;
  for (const char* path : paths) {
    result.emplace_back(path);
  }
  return result;
}

bool Write(const rn::RoadNetworkData& data, const SdfLayerRefPtr& layer, rn::Diagnostic* error = nullptr) {
  return rnusd::WriteLayer(data, MakeRecord(data), *layer, error);
}

std::string Export(const SdfLayerRefPtr& layer) {
  std::string text;
  layer->ExportToString(&text);
  return text;
}

// ---- Checks ----------------------------------------------------------------

void CheckPrimNames() {
  // The ADR 0005 section 1 table.
  Check(rnusd::PrimName(rnusd::kRoadPrefix, "1") == "road_1", "E(1)");
  Check(rnusd::PrimName(rnusd::kRoadPrefix, "-1") == "road__2d1", "E(-1)");
  Check(rnusd::PrimName(rnusd::kRoadPrefix, "a_b") == "road_a_5fb", "E(a_b)");
  Check(rnusd::PrimName(rnusd::kRoadPrefix, "North 2") == "road_North_202", "E(North 2)");
  Check(rnusd::PrimName(rnusd::kRoadPrefix, "\xe9\x81\x93") == "road__e9_81_93", "E(U+9053)");
  Check(rnusd::PrimName(rnusd::kJunctionPrefix, "J.1/x") == "junction_J_2e1_2fx", "E(punctuation)");
  Check(rnusd::PrimName(rnusd::kConnectionPrefix, "0") == "connection_0", "connection prefix");
  Check(!rnusd::PrimName(rnusd::kRoadPrefix, ""), "the empty ID is rejected");
  Check(rnusd::EncodeId("_") == "_5f", "the underscore is escaped");
  Check(rnusd::EncodeId(std::string("\0", 1)) == "_00", "a NUL byte is escaped");

  const std::vector<std::string> ids = {"1", "-1", "a_b", "North 2", "\xe9\x81\x93", "_", "__",
      "_5f", "a b\tc", std::string("\0\xff", 2), "Road-007.x"};
  for (const std::string& id : ids) {
    const std::string encoded = rnusd::EncodeId(id);
    Check(rnusd::DecodeId(encoded) == id, "E^-1(E(id)) == id for '" + encoded + "'");
    Check(SdfPath::IsValidIdentifier("road_" + encoded), "road_" + encoded + " is a valid identifier");
    for (const char c : encoded) {
      Check(static_cast<unsigned char>(c) < 0x80, "E(id) is ASCII for '" + encoded + "'");
    }
  }
  // Two IDs that differ only in the escape character stay apart.
  Check(rnusd::EncodeId("a_5f") != rnusd::EncodeId("a__"), "E is injective on escapes");

  // Strings that are not encodings have no preimage.
  for (const char* bad : {"_", "_5", "_5F", "_zz", "a-b", "_41", "_30", "a b"}) {
    Check(!rnusd::DecodeId(bad), std::string("DecodeId rejects '") + bad + "'");
  }
  Check(rnusd::DecodeId("") == std::string(), "DecodeId of the empty string");

  Check(rnusd::SectionName(0) == "section_0" && rnusd::SectionName(12) == "section_12",
      "section names");
  Check(rnusd::LaneName(0) == "center", "lane 0 is center");
  Check(rnusd::LaneName(3) == "left_3", "lane 3 is left_3");
  Check(rnusd::LaneName(-2) == "right_2", "lane -2 is right_2");
  Check(rnusd::LaneName(std::numeric_limits<std::int32_t>::min()) == "right_2147483648",
      "the most negative lane number");
}

void CheckHeaderOnly() {
  // A metadata-only read decodes the header only (ADR 0006 section 2).
  rn::RoadNetworkData data;
  data.source.format = "opendrive";
  data.source.versionMajor = 1;
  data.source.versionMinor = 4;
  data.warnings = {{"ODR-W204", 2}};
  rnusd::ImportRecord record;
  record.importer = "roadnetwork-test";
  record.importerVersion = "1.2.3";
  record.roadCount = 98;
  record.junctionCount = 12;
  record.laneCount = 482;

  SdfLayerRefPtr layer = SdfLayer::CreateAnonymous("header.usda");
  SdfPrimSpec::New(layer->GetPseudoRoot(), "Stale", SdfSpecifierDef);
  rn::Diagnostic error;
  Check(rnusd::WriteLayer(data, record, *layer, &error), "a header-only write succeeds: " + error.message);

  Check(layer->GetDefaultPrim() == TfToken("Asset"), "defaultPrim is Asset");
  Check(layer->GetField(SdfPath::AbsoluteRootPath(), TfToken("upAxis")) == VtValue(TfToken("Z")),
      "upAxis is Z");
  Check(layer->GetField(SdfPath::AbsoluteRootPath(), TfToken("metersPerUnit")) == VtValue(1.0),
      "metersPerUnit is 1");
  Check(ChildNames(layer, "/") == std::vector<std::string>{"Asset"}, "the only root prim is Asset");
  Check(TypeName(layer, "/Asset") == "Xform", "/Asset is an Xform");
  Check(layer->GetPrimAtPath(SdfPath("/Asset"))->GetKind() == TfToken("component"),
      "/Asset is a component");
  Check(TypeName(layer, "/Asset/Network") == "Scope", "/Asset/Network is a Scope");
  Check(ChildNames(layer, "/Asset") == std::vector<std::string>{"Network"},
      "a header-only write authors no /Asset/Visualization");
  Check(ChildNames(layer, "/Asset/Network").empty(), "/Asset/Network is empty");

  Check(Value<std::string>(layer, "/Asset.roadnetwork:source:format") == "opendrive", "source format");
  Check(Value<std::string>(layer, "/Asset.roadnetwork:source:formatVersion") == "1.4", "format version");
  Check(Value<std::string>(layer, "/Asset.roadnetwork:import:importer") == "roadnetwork-test", "importer");
  Check(Value<std::string>(layer, "/Asset.roadnetwork:import:importerVersion") == "1.2.3",
      "importer version");
  Check(Value<int>(layer, "/Asset.roadnetwork:import:roadCount") == 98, "road count from the record");
  Check(Value<int>(layer, "/Asset.roadnetwork:import:junctionCount") == 12, "junction count");
  Check(Value<int>(layer, "/Asset.roadnetwork:import:laneCount") == 482, "lane count");
  Check(Value<VtStringArray>(layer, "/Asset.roadnetwork:import:warningCodes") == VtStringArray{"ODR-W204"},
      "warning codes");
  Check(Value<VtIntArray>(layer, "/Asset.roadnetwork:import:warningCounts") == VtIntArray{2},
      "warning counts");
  // Absent source values are not authored (ADR 0002 section 2).
  for (const char* absent : {"/Asset.roadnetwork:source:name", "/Asset.roadnetwork:source:date",
           "/Asset.roadnetwork:source:vendor", "/Asset.roadnetwork:source:version",
           "/Asset.roadnetwork:source:geoReference", "/Asset.roadnetwork:source:offset",
           "/Asset.roadnetwork:source:offsetHeading"}) {
    Check(!Has(layer, absent), std::string(absent) + " is not authored when absent");
  }

  // No warnings: the arrays are authored, and empty.
  data.warnings.clear();
  Check(rnusd::WriteLayer(data, record, *layer, nullptr), "a write without warnings succeeds");
  Check(Value<VtStringArray>(layer, "/Asset.roadnetwork:import:warningCodes") == VtStringArray{},
      "no warning codes is an empty array");
  Check(Value<VtIntArray>(layer, "/Asset.roadnetwork:import:warningCounts") == VtIntArray{},
      "no warning counts is an empty array");
}

void CheckAssetProvenance(const SdfLayerRefPtr& layer) {
  Check(Value<std::string>(layer, "/Asset.roadnetwork:source:formatVersion") == "1.8", "1.8");
  Check(Value<std::string>(layer, "/Asset.roadnetwork:source:name") == "writer test", "header name");
  Check(Value<std::string>(layer, "/Asset.roadnetwork:source:vendor") == "hand-authored", "header vendor");
  Check(Value<std::string>(layer, "/Asset.roadnetwork:source:geoReference") ==
            "+proj=utm +zone=32 +datum=WGS84",
      "the georeference is verbatim");
  Check(Value<GfVec3d>(layer, "/Asset.roadnetwork:source:offset") == GfVec3d(100.0, 200.0, 3.0),
      "the offset is a double3");
  Check(Value<double>(layer, "/Asset.roadnetwork:source:offsetHeading") == 0.25, "the offset heading");
  Check(Value<int>(layer, "/Asset.roadnetwork:import:roadCount") == 3, "three roads");
  Check(Value<int>(layer, "/Asset.roadnetwork:import:laneCount") == 9, "nine lane prims");
  // Ascending code order, whatever the order of RoadNetworkData::warnings.
  Check(Value<VtStringArray>(layer, "/Asset.roadnetwork:import:warningCodes") ==
            (VtStringArray{"ODR-W201", "RN-W301"}),
      "warning codes ascend");
  Check(Value<VtIntArray>(layer, "/Asset.roadnetwork:import:warningCounts") == (VtIntArray{7, 1}),
      "warning counts follow their codes");
}

void CheckHierarchy(const SdfLayerRefPtr& layer) {
  Check(ChildNames(layer, "/Asset") == (std::vector<std::string>{"Network", "Visualization"}),
      "/Asset holds Network then Visualization");
  Check(ChildNames(layer, "/Asset/Network") == (std::vector<std::string>{"Roads", "Junctions"}),
      "/Asset/Network holds Roads then Junctions");
  Check(ChildNames(layer, "/Asset/Network/Roads") ==
            (std::vector<std::string>{"road_1", "road_a_5fb", "road_2"}),
      "roads in source order");
  Check(ChildNames(layer, "/Asset/Network/Roads/road_1") ==
            (std::vector<std::string>{"section_0", "section_1"}),
      "sections in index order");
  Check(ChildNames(layer, "/Asset/Network/Roads/road_1/section_0") ==
            (std::vector<std::string>{"left_1", "center", "right_1"}),
      "lanes in descending lane number");
  Check(ChildNames(layer, "/Asset/Network/Junctions") == std::vector<std::string>{"junction_J_201"},
      "the junction name is escaped");
  Check(ChildNames(layer, "/Asset/Network/Junctions/junction_J_201") ==
            std::vector<std::string>{"connection_0"},
      "the connection");
  for (const char* scope : {"/Asset/Network/Roads", "/Asset/Network/Roads/road_1",
           "/Asset/Network/Roads/road_1/section_1", "/Asset/Network/Roads/road_1/section_1/right_1",
           "/Asset/Network/Junctions", "/Asset/Network/Junctions/junction_J_201/connection_0",
           "/Asset/Visualization/Centerlines", "/Asset/Visualization/Boundaries"}) {
    Check(TypeName(layer, scope) == "Scope", std::string(scope) + " is a Scope");
  }
  Check(TypeName(layer, "/Asset/Visualization") == "Xform", "/Asset/Visualization is an Xform");
}

void CheckRoad(const SdfLayerRefPtr& layer) {
  const char* road = "/Asset/Network/Roads/road_1";
  Check(Value<std::string>(layer, "/Asset/Network/Roads/road_1.roadnetwork:id") == "1", "road id");
  Check(Value<std::string>(layer, "/Asset/Network/Roads/road_a_5fb.roadnetwork:id") == "a_b",
      "the verbatim ID beside an escaped name");
  Check(Value<std::string>(layer, "/Asset/Network/Roads/road_1.roadnetwork:name") == "first", "road name");
  Check(!Has(layer, "/Asset/Network/Roads/road_2.roadnetwork:name"), "an empty name is not authored");
  Check(Value<double>(layer, "/Asset/Network/Roads/road_1.roadnetwork:length") == 20.0, "road length");
  Check(Targets(layer, "/Asset/Network/Roads/road_1.roadnetwork:successor") ==
            Paths({"/Asset/Network/Junctions/junction_J_201"}),
      "road successor targets the junction");
  Check(!Has(layer, "/Asset/Network/Roads/road_1.roadnetwork:successorContact"),
      "no contact for a junction link");
  Check(!Has(layer, "/Asset/Network/Roads/road_1.roadnetwork:predecessor"), "no predecessor when absent");
  Check(!Has(layer, "/Asset/Network/Roads/road_1.roadnetwork:junction"), "road 1 is in no junction");
  Check(Targets(layer, "/Asset/Network/Roads/road_a_5fb.roadnetwork:junction") ==
            Paths({"/Asset/Network/Junctions/junction_J_201"}),
      "the connecting road's junction");
  Check(Targets(layer, "/Asset/Network/Roads/road_a_5fb.roadnetwork:predecessor") ==
            Paths({"/Asset/Network/Roads/road_1"}),
      "road predecessor targets the road");
  Check(Value<TfToken>(layer, "/Asset/Network/Roads/road_a_5fb.roadnetwork:predecessorContact") ==
            TfToken("end"),
      "predecessor contact");
  Check(Value<TfToken>(layer, "/Asset/Network/Roads/road_a_5fb.roadnetwork:successorContact") ==
            TfToken("start"),
      "successor contact");

  // Reference line: one entry per segment.
  Check(Value<VtTokenArray>(layer, "/Asset/Network/Roads/road_1.roadnetwork:referenceLine:kind") ==
            VtTokenArray{TfToken("line")},
      "line kind");
  Check(Value<VtTokenArray>(layer, "/Asset/Network/Roads/road_a_5fb.roadnetwork:referenceLine:kind") ==
            VtTokenArray{TfToken("arc")},
      "arc kind");
  Check(Value<VtDoubleArray>(layer, "/Asset/Network/Roads/road_a_5fb.roadnetwork:referenceLine:curvature") ==
            VtDoubleArray{0.1},
      "arc curvature");
  Check(Value<VtDoubleArray>(layer, "/Asset/Network/Roads/road_1.roadnetwork:referenceLine:curvature") ==
            VtDoubleArray{0.0},
      "a line has curvature 0");
  Check(Value<VtDoubleArray>(layer, "/Asset/Network/Roads/road_2.roadnetwork:referenceLine:x") ==
            VtDoubleArray{25.0},
      "segment x");
  Check(Value<VtDoubleArray>(layer, "/Asset/Network/Roads/road_2.roadnetwork:referenceLine:y") ==
            VtDoubleArray{1.25},
      "segment y");
  for (const char* suffix : {"s", "hdg", "length"}) {
    const std::string path = std::string(road) + ".roadnetwork:referenceLine:" + suffix;
    Check(Value<VtDoubleArray>(layer, path.c_str()).has_value(), path + " is authored");
  }
  for (const char* suffix : {"curvatureEnd", "aU", "dV", "pRange"}) {
    const std::string path = std::string(road) + ".roadnetwork:referenceLine:" + suffix;
    Check(!Has(layer, path.c_str()), path + " is not authored for lines and arcs");
  }

  // Profiles: five parallel arrays; absent ones are not authored.
  Check(Value<VtDoubleArray>(layer, "/Asset/Network/Roads/road_1.roadnetwork:elevation:a") ==
            VtDoubleArray{1.0},
      "elevation a");
  Check(Value<VtDoubleArray>(layer, "/Asset/Network/Roads/road_1.roadnetwork:elevation:b") ==
            VtDoubleArray{0.01},
      "elevation b");
  Check(Value<VtDoubleArray>(layer, "/Asset/Network/Roads/road_1.roadnetwork:laneOffset:a") ==
            VtDoubleArray{0.25},
      "lane offset a");
  for (const char* suffix : {"s", "c", "d"}) {
    const std::string path = std::string(road) + ".roadnetwork:laneOffset:" + suffix;
    Check(Value<VtDoubleArray>(layer, path.c_str()) == VtDoubleArray{0.0}, path);
  }
  Check(!Has(layer, "/Asset/Network/Roads/road_2.roadnetwork:elevation:s"), "no elevation when absent");
  Check(!Has(layer, "/Asset/Network/Roads/road_2.roadnetwork:laneOffset:s"), "no lane offset when absent");
  Check(Value<VtDoubleArray>(layer, "/Asset/Network/Roads/road_1.roadnetwork:speed:max") ==
            VtDoubleArray{13.8889},
      "speed in metres per second");
  Check(!Has(layer, "/Asset/Network/Roads/road_2.roadnetwork:speed:s"), "no speed when absent");
}

void CheckSectionsAndLanes(const SdfLayerRefPtr& layer) {
  Check(Value<double>(layer, "/Asset/Network/Roads/road_1/section_1.roadnetwork:s") == 10.0, "section s");
  Check(Value<int>(layer, "/Asset/Network/Roads/road_1/section_0/left_1.roadnetwork:laneNumber") == 1,
      "left lane number");
  Check(Value<int>(layer, "/Asset/Network/Roads/road_1/section_0/right_1.roadnetwork:laneNumber") == -1,
      "right lane number");
  Check(Value<TfToken>(layer, "/Asset/Network/Roads/road_1/section_0/left_1.roadnetwork:laneType") ==
            TfToken("sidewalk"),
      "lane type");
  Check(Value<bool>(layer, "/Asset/Network/Roads/road_1/section_0/center.roadnetwork:level") == false,
      "lane level");
  Check(Value<VtDoubleArray>(layer, "/Asset/Network/Roads/road_1/section_0/right_1.roadnetwork:width:a") ==
            VtDoubleArray{3.5},
      "lane width");
  Check(Value<VtDoubleArray>(layer,
            "/Asset/Network/Roads/road_1/section_0/right_1.roadnetwork:width:sOffset") ==
            VtDoubleArray{0.0},
      "lane width sOffset");
  Check(!Has(layer, "/Asset/Network/Roads/road_1/section_0/center.roadnetwork:width:a"),
      "the center lane has no width");

  // Lane graph: each edge on both lanes (ADR 0005 section 4).
  Check(Targets(layer, "/Asset/Network/Roads/road_1/section_0/right_1.roadnetwork:endLinks") ==
            Paths({"/Asset/Network/Roads/road_1/section_1/right_1"}),
      "lane end links across sections");
  Check(Targets(layer, "/Asset/Network/Roads/road_1/section_1/right_1.roadnetwork:startLinks") ==
            Paths({"/Asset/Network/Roads/road_1/section_0/right_1"}),
      "lane start links across sections");
  Check(Targets(layer, "/Asset/Network/Roads/road_a_5fb/section_0/right_1.roadnetwork:startLinks") ==
            Paths({"/Asset/Network/Roads/road_1/section_1/right_1"}),
      "into the connecting road");
  Check(Targets(layer, "/Asset/Network/Roads/road_2/section_0/right_1.roadnetwork:startLinks") ==
            Paths({"/Asset/Network/Roads/road_a_5fb/section_0/right_1"}),
      "out of the connecting road");
  Check(!Has(layer, "/Asset/Network/Roads/road_1/section_0/right_1.roadnetwork:startLinks"),
      "no start-links relationship without an edge");
  Check(!Has(layer, "/Asset/Network/Roads/road_1/section_0/left_1.roadnetwork:endLinks"),
      "the left lane has no edges");
}

void CheckSortedLaneTargets() {
  // A lane joined to several lanes at one end lists them sorted by path.
  rn::RoadNetworkData data = MakeNetwork();
  data.roads[2].sections[0].lanes.push_back(MakeLane(-2, "driving", 3.0));
  data.laneLinks.push_back(Link("1", 1, -1, rn::ContactPoint::End, "1", 0, 1, rn::ContactPoint::End));
  data.laneLinks.push_back(Link("a_b", 0, -1, rn::ContactPoint::End, "2", 0, -2, rn::ContactPoint::Start));
  rn::CanonicalizeLaneLinks(data.laneLinks);
  Check(!rn::Validate(data), "the branching network validates");
  SdfLayerRefPtr layer = SdfLayer::CreateAnonymous("branching.usda");
  Check(Write(data, layer), "the branching network is written");
  Check(Targets(layer, "/Asset/Network/Roads/road_a_5fb/section_0/right_1.roadnetwork:endLinks") ==
            Paths({"/Asset/Network/Roads/road_2/section_0/right_1",
                "/Asset/Network/Roads/road_2/section_0/right_2"}),
      "end links sorted by path");
  Check(Targets(layer, "/Asset/Network/Roads/road_1/section_1/right_1.roadnetwork:endLinks") ==
            Paths({"/Asset/Network/Roads/road_1/section_0/left_1",
                "/Asset/Network/Roads/road_a_5fb/section_0/right_1"}),
      "an end joined to two lanes lists both, sorted");
}

void CheckJunction(const SdfLayerRefPtr& layer) {
  Check(Value<std::string>(layer, "/Asset/Network/Junctions/junction_J_201.roadnetwork:id") == "J 1",
      "junction id");
  Check(Value<std::string>(layer, "/Asset/Network/Junctions/junction_J_201.roadnetwork:name") ==
            "crossing",
      "junction name");
  Check(Value<std::string>(layer, "/Asset/Network/Junctions/junction_J_201/connection_0.roadnetwork:id") ==
            "0",
      "connection id");
  Check(Targets(layer, "/Asset/Network/Junctions/junction_J_201/connection_0.roadnetwork:incomingRoad") ==
            Paths({"/Asset/Network/Roads/road_1"}),
      "incoming road");
  Check(Targets(layer,
            "/Asset/Network/Junctions/junction_J_201/connection_0.roadnetwork:connectingRoad") ==
            Paths({"/Asset/Network/Roads/road_a_5fb"}),
      "connecting road");
  Check(Value<TfToken>(layer, "/Asset/Network/Junctions/junction_J_201/connection_0.roadnetwork:contactPoint") ==
            TfToken("start"),
      "contact point");
  Check(Value<VtIntArray>(layer,
            "/Asset/Network/Junctions/junction_J_201/connection_0.roadnetwork:laneLink:from") ==
            VtIntArray{-1},
      "lane link from");
  Check(Value<VtIntArray>(layer,
            "/Asset/Network/Junctions/junction_J_201/connection_0.roadnetwork:laneLink:to") ==
            VtIntArray{-1},
      "lane link to");
}

/// Every property under /Asset/Network is a uniform, custom roadnetwork:
/// property with no single-precision value (ADR 0002 section 2), and every
/// relationship target is inside /Asset (ADR 0005 section 4).
void CheckNetworkProperties(const SdfPrimSpecHandle& prim) {
  for (const SdfAttributeSpecHandle& attribute : prim->GetAttributes()) {
    const std::string name = attribute->GetName();
    const std::string type = attribute->GetTypeName().GetAsToken().GetString();
    Check(name.rfind("roadnetwork:", 0) == 0, name + " is in the roadnetwork: namespace");
    Check(attribute->GetVariability() == SdfVariabilityUniform, name + " is uniform");
    Check(attribute->IsCustom(), name + " is custom");
    Check(type.find("float") == std::string::npos && type.find("half") == std::string::npos &&
              type.find("3f") == std::string::npos,
        name + " is not single precision (" + type + ")");
    Check(!attribute->HasInfo(TfToken("timeSamples")), name + " has no time samples");
  }
  for (const SdfRelationshipSpecHandle& relationship : prim->GetRelationships()) {
    for (const SdfPath& target : relationship->GetTargetPathList().GetExplicitItems()) {
      Check(target.HasPrefix(SdfPath("/Asset")), relationship->GetPath().GetString() + " leaves /Asset");
    }
  }
  for (const SdfPrimSpecHandle& child : prim->GetNameChildren()) {
    CheckNetworkProperties(child);
  }
}

void CheckVisualization(const SdfLayerRefPtr& layer, const rn::RoadNetworkData& data) {
  Check(!Has(layer, "/Asset/Visualization.xformOp:translate"),
      "no origin shift within 8,192 m of the origin");
  Check(!Has(layer, "/Asset/Visualization.xformOpOrder"), "no xformOpOrder without a shift");
  Check(ChildNames(layer, "/Asset/Visualization") ==
            (std::vector<std::string>{"Centerlines", "Boundaries"}),
      "Centerlines then Boundaries");
  Check(ChildNames(layer, "/Asset/Visualization/Centerlines") ==
            (std::vector<std::string>{"road_1", "road_a_5fb", "road_2"}),
      "one centerline prim per road");
  Check(TypeName(layer, "/Asset/Visualization/Centerlines/road_1") == "BasisCurves",
      "curves are BasisCurves");
  Check(Value<TfToken>(layer, "/Asset/Visualization/Centerlines/road_1.type") == TfToken("linear"),
      "linear curves");

  // Curve order: section, then lane order; the center border first among
  // the boundaries (ADR 0005 section 5).
  Check(Targets(layer, "/Asset/Visualization/Centerlines/road_1.roadnetwork:represents") ==
            Paths({"/Asset/Network/Roads/road_1/section_0/left_1",
                "/Asset/Network/Roads/road_1/section_0/right_1",
                "/Asset/Network/Roads/road_1/section_1/right_1"}),
      "centerline curves represent their lanes");
  Check(Targets(layer, "/Asset/Visualization/Boundaries/road_1.roadnetwork:represents") ==
            Paths({"/Asset/Network/Roads/road_1/section_0/center",
                "/Asset/Network/Roads/road_1/section_0/left_1",
                "/Asset/Network/Roads/road_1/section_0/right_1",
                "/Asset/Network/Roads/road_1/section_1/center",
                "/Asset/Network/Roads/road_1/section_1/right_1"}),
      "boundary curves represent their lanes, center border first");

  // The points are the tessellator's, unshifted, in curve order.
  const rn::SampledRoad sampled = rn::TessellateRoad(data.roads[0]);
  VtIntArray counts;
  std::vector<rn::Point3> expected;
  for (const rn::SampledSection& section : sampled.sections) {
    for (const rn::SampledCurve& curve : section.borders) {
      counts.push_back(static_cast<int>(curve.points.size()));
      expected.insert(expected.end(), curve.points.begin(), curve.points.end());
    }
  }
  Check(Value<VtIntArray>(layer, "/Asset/Visualization/Boundaries/road_1.curveVertexCounts") == counts,
      "curve vertex counts follow the tessellation");
  const std::optional<VtVec3fArray> points =
      Value<VtVec3fArray>(layer, "/Asset/Visualization/Boundaries/road_1.points");
  Check(points && points->size() == expected.size(), "one point per sample");
  if (points && points->size() == expected.size()) {
    for (std::size_t i = 0; i < expected.size(); ++i) {
      const GfVec3f& p = (*points)[i];
      Check(p == GfVec3f(static_cast<float>(expected[i].x), static_cast<float>(expected[i].y),
                     static_cast<float>(expected[i].z)),
          "point " + std::to_string(i) + " is the sample in float");
    }
    const std::optional<VtVec3fArray> extent =
        Value<VtVec3fArray>(layer, "/Asset/Visualization/Boundaries/road_1.extent");
    Check(extent && extent->size() == 2, "an extent is authored");
    if (extent && extent->size() == 2) {
      for (const GfVec3f& p : *points) {
        for (int axis = 0; axis < 3; ++axis) {
          Check((*extent)[0][axis] <= p[axis] && p[axis] <= (*extent)[1][axis], "the extent bounds the points");
        }
      }
    }
  }
  const SdfAttributeSpecHandle pointsSpec =
      layer->GetAttributeAtPath(SdfPath("/Asset/Visualization/Boundaries/road_1.points"));
  Check(pointsSpec && pointsSpec->GetTypeName() == SdfValueTypeNames->Point3fArray, "points are point3f[]");
}

void CheckNetwork() {
  const rn::RoadNetworkData data = MakeNetwork();
  Check(!rn::Validate(data), "the hand-built network validates");
  SdfLayerRefPtr layer = SdfLayer::CreateAnonymous("network.usda");
  rn::Diagnostic error;
  Check(Write(data, layer, &error), "the network is written: " + error.message);
  CheckAssetProvenance(layer);
  CheckHierarchy(layer);
  CheckRoad(layer);
  CheckSectionsAndLanes(layer);
  CheckJunction(layer);
  if (const SdfPrimSpecHandle network = layer->GetPrimAtPath(SdfPath("/Asset/Network"))) {
    CheckNetworkProperties(network);
  }
  CheckVisualization(layer, data);
}

void CheckCurveShapes() {
  // ADR 0010 section 5: a road with a spiral and paramPoly3 segments gains
  // :curvatureEnd, the eight coefficients, and :pRange; a road of lines and
  // arcs (CheckRoad) authors none of them.
  rn::RoadNetworkData data;
  data.source.format = "opendrive";
  data.source.versionMajor = 1;
  data.source.versionMinor = 8;
  rn::Road road = MakeLineRoad("c", 0.0, 0.0, 10.0);
  rn::ParamPoly3 unstated;
  unstated.bU = 28.8;
  unstated.cU = -14.4;
  unstated.cV = -14.4;
  rn::ParamPoly3 arcLength;
  arcLength.aU = 0.5;
  arcLength.bU = 1.0;
  arcLength.dU = -0.0001;
  arcLength.aV = -0.25;
  arcLength.bV = 0.125;
  arcLength.cV = 0.01;
  arcLength.dV = 0.0002;
  arcLength.pRange = rn::ParamRange::ArcLength;
  road.referenceLine.segments = {
      rn::PlanViewSegment{0.0, 0.0, 0.0, 0.0, 10.0, rn::Line{}},
      rn::PlanViewSegment{10.0, 10.0, 0.0, 0.0, 20.0, rn::Spiral{0.0, 0.02}},
      rn::PlanViewSegment{30.0, 1.0, 2.0, 0.3, 5.0, rn::Arc{0.02}},
      rn::PlanViewSegment{35.0, 3.0, 4.0, 0.4, 23.35459442, unstated},
      rn::PlanViewSegment{58.35459442, 5.0, 6.0, 0.5, 10.0, arcLength},
  };
  road.length = 68.35459442;
  data.roads.push_back(road);
  SdfLayerRefPtr layer = SdfLayer::CreateAnonymous("curves.usda");
  Check(Write(data, layer), "the curve road is written");

  const auto doubles = [&](const char* suffix) {
    const std::string path = std::string("/Asset/Network/Roads/road_c.roadnetwork:referenceLine:") + suffix;
    return Value<VtDoubleArray>(layer, path.c_str());
  };
  Check(Value<VtTokenArray>(layer, "/Asset/Network/Roads/road_c.roadnetwork:referenceLine:kind") ==
            (VtTokenArray{TfToken("line"), TfToken("spiral"), TfToken("arc"), TfToken("paramPoly3"),
                TfToken("paramPoly3")}),
      "curve kinds");
  Check(doubles("curvature") == (VtDoubleArray{0.0, 0.0, 0.02, 0.0, 0.0}),
      "curvature: curvStart for a spiral, 0 for a paramPoly3");
  Check(doubles("curvatureEnd") == (VtDoubleArray{0.0, 0.02, 0.02, 0.0, 0.0}),
      "curvatureEnd: curvEnd for a spiral, the curvature for a line or arc, 0 for a paramPoly3");
  Check(doubles("aU") == (VtDoubleArray{0.0, 0.0, 0.0, 0.0, 0.5}), "aU");
  Check(doubles("bU") == (VtDoubleArray{0.0, 0.0, 0.0, 28.8, 1.0}), "bU");
  Check(doubles("cU") == (VtDoubleArray{0.0, 0.0, 0.0, -14.4, 0.0}), "cU");
  Check(doubles("dU") == (VtDoubleArray{0.0, 0.0, 0.0, 0.0, -0.0001}), "dU");
  Check(doubles("aV") == (VtDoubleArray{0.0, 0.0, 0.0, 0.0, -0.25}), "aV");
  Check(doubles("bV") == (VtDoubleArray{0.0, 0.0, 0.0, 0.0, 0.125}), "bV");
  Check(doubles("cV") == (VtDoubleArray{0.0, 0.0, 0.0, -14.4, 0.01}), "cV");
  Check(doubles("dV") == (VtDoubleArray{0.0, 0.0, 0.0, 0.0, 0.0002}), "dV");
  Check(Value<VtTokenArray>(layer, "/Asset/Network/Roads/road_c.roadnetwork:referenceLine:pRange") ==
            (VtTokenArray{TfToken(""), TfToken(""), TfToken(""), TfToken("unstated"), TfToken("arcLength")}),
      "pRange: unstated keeps the omission visible");

  // A spiral without a paramPoly3 authors :curvatureEnd only.
  rn::RoadNetworkData spiralOnly = data;
  spiralOnly.roads[0].referenceLine.segments.resize(3);
  spiralOnly.roads[0].length = 35.0;
  SdfLayerRefPtr spiralLayer = SdfLayer::CreateAnonymous("spiral.usda");
  Check(Write(spiralOnly, spiralLayer), "the spiral road is written");
  Check(Has(spiralLayer, "/Asset/Network/Roads/road_c.roadnetwork:referenceLine:curvatureEnd"),
      "a spiral road authors curvatureEnd");
  Check(!Has(spiralLayer, "/Asset/Network/Roads/road_c.roadnetwork:referenceLine:bU") &&
            !Has(spiralLayer, "/Asset/Network/Roads/road_c.roadnetwork:referenceLine:pRange"),
      "a road without a paramPoly3 authors no coefficients");
}

void CheckOriginShift() {
  // ADR 0003 section 5 at UTM scale: a 1,024 m multiple near the center of
  // the bounds, authored in double, and float points that reconstruct the
  // samples within 0.5 mm.
  rn::RoadNetworkData data;
  data.source.format = "opendrive";
  data.source.versionMajor = 1;
  data.source.versionMinor = 6;
  rn::Road road = MakeLineRoad("7", 500737.123456, 5427713.654321, 120.0);
  road.referenceLine.segments[0].length = 60.0;
  road.referenceLine.segments.push_back(
      rn::PlanViewSegment{60.0, 500797.123456, 5427713.654321, 0.0, 60.0, rn::Arc{0.01}});
  road.elevation.records.push_back(rn::CubicRecord{0.0, 12.5, 0.0, 0.0, 0.0});
  data.roads.push_back(road);
  Check(!rn::Validate(data), "the UTM-scale road validates");

  SdfLayerRefPtr layer = SdfLayer::CreateAnonymous("utm.usda");
  Check(Write(data, layer), "the UTM-scale road is written");
  const std::optional<GfVec3d> origin = Value<GfVec3d>(layer, "/Asset/Visualization.xformOp:translate");
  Check(origin.has_value(), "the origin shift is authored as a double3");
  const SdfAttributeSpecHandle translate =
      layer->GetAttributeAtPath(SdfPath("/Asset/Visualization.xformOp:translate"));
  Check(translate && translate->GetTypeName() == SdfValueTypeNames->Double3, "the shift is double precision");
  Check(Value<VtTokenArray>(layer, "/Asset/Visualization.xformOpOrder") ==
            VtTokenArray{TfToken("xformOp:translate")},
      "xformOpOrder names the translate");
  if (!origin) {
    return;
  }
  for (int axis = 0; axis < 3; ++axis) {
    Check(std::fmod((*origin)[axis], 1024.0) == 0.0, "the origin is a multiple of 1,024 m");
  }
  // The bounds span about x 500,737-500,855 and y 5,427,710-5,427,731, so
  // their center rounds to 489 and 5,301 multiples of 1,024 m; z rounds to 0.
  Check(*origin == GfVec3d(500736.0, 5428224.0, 0.0),
      "the origin is the rounded center of the bounds, got (" + std::to_string((*origin)[0]) + ", " +
          std::to_string((*origin)[1]) + ", " + std::to_string((*origin)[2]) + ")");

  const rn::SampledRoad sampled = rn::TessellateRoad(data.roads[0]);
  double worst = 0.0;
  for (const char* container : {"Boundaries", "Centerlines"}) {
    std::vector<rn::Point3> expected;
    for (const rn::SampledSection& section : sampled.sections) {
      for (const rn::SampledCurve& curve :
          std::string(container) == "Boundaries" ? section.borders : section.centerlines) {
        expected.insert(expected.end(), curve.points.begin(), curve.points.end());
      }
    }
    const std::string path = std::string("/Asset/Visualization/") + container + "/road_7.points";
    const std::optional<VtVec3fArray> points = Value<VtVec3fArray>(layer, path.c_str());
    Check(points && points->size() == expected.size(), path + " has one point per sample");
    if (!points || points->size() != expected.size()) {
      continue;
    }
    for (std::size_t i = 0; i < expected.size(); ++i) {
      const GfVec3f& p = (*points)[i];
      worst = std::max({worst, std::abs(p[0] + (*origin)[0] - expected[i].x),
          std::abs(p[1] + (*origin)[1] - expected[i].y), std::abs(p[2] + (*origin)[2] - expected[i].z)});
    }
  }
  Check(worst <= 0.0005, "points reconstruct their samples within 0.5 mm (worst " +
                             std::to_string(worst) + " m)");
}

void CheckFailureLeavesLayerUnchanged() {
  rn::RoadNetworkData data = MakeNetwork();
  // An edge to a lane that does not exist, which validation would reject
  // (RN-E115): the writer must fail with its own code, not author a dangling
  // relationship.
  data.laneLinks.push_back(Link("2", 0, -1, rn::ContactPoint::End, "2", 0, -5, rn::ContactPoint::Start));
  SdfLayerRefPtr layer = SdfLayer::CreateAnonymous("unchanged.usda");
  SdfPrimSpec::New(layer->GetPseudoRoot(), "Stale", SdfSpecifierDef);
  const std::string before = Export(layer);
  rn::Diagnostic error;
  Check(!Write(data, layer, &error), "a dangling lane edge fails the write");
  Check(error.code == "RN-E901", "the failure is RN-E901, got '" + error.code + "'");
  Check(error.message.rfind("[RN-E901] ", 0) == 0, "the message starts with the bracketed code");
  Check(error.message.find("-5") != std::string::npos, "the message names the lane");
  Check(Export(layer) == before, "a failed write leaves the layer unchanged");

  rn::RoadNetworkData unnamed = MakeNetwork();
  unnamed.junctions[0].connections[0].id.clear();
  Check(!Write(unnamed, layer, &error) && error.code == "RN-E901", "an empty ID fails the write");
  Check(Export(layer) == before, "the layer is still unchanged");
}

void CheckDeterminismAndChangeBlock() {
  const rn::RoadNetworkData data = MakeNetwork();
  SdfLayerRefPtr first = SdfLayer::CreateAnonymous("first.usda");
  SdfLayerRefPtr second = SdfLayer::CreateAnonymous("second.usda");
  Check(Write(data, first) && Write(data, second), "two writes succeed");
  const std::string text = Export(first);
  Check(text == Export(second), "two writes of the same data are identical");
  Check(Write(data, first) && Export(first) == text, "rewriting a layer replaces its content");

  // Reload runs SdfFileFormat::Read inside an SdfChangeBlock (ADR 0006). The
  // writer authors with Sdf specs only, so it behaves identically there.
  SdfLayerRefPtr blocked = SdfLayer::CreateAnonymous("blocked.usda");
  SdfPrimSpec::New(blocked->GetPseudoRoot(), "Stale", SdfSpecifierDef);
  {
    SdfChangeBlock block;
    Check(Write(data, blocked), "a write inside an SdfChangeBlock succeeds");
    Check(!blocked->GetPrimAtPath(SdfPath("/Stale")), "content is replaced inside an SdfChangeBlock");
  }
  Check(Export(blocked) == text, "a write inside an SdfChangeBlock matches one outside");
}

} // namespace

int main() {
  CheckPrimNames();
  CheckHeaderOnly();
  CheckNetwork();
  CheckSortedLaneTargets();
  CheckOriginShift();
  CheckCurveShapes();
  CheckFailureLeavesLayerUnchanged();
  CheckDeterminismAndChangeBlock();
  if (failures == 0) {
    std::printf("roadnetworkUsd: all checks passed\n");
  }
  return failures == 0 ? 0 : 1;
}
