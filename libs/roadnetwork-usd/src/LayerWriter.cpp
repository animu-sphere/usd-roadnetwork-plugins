// SPDX-License-Identifier: Apache-2.0
#include "openstrata/rn/usd/LayerWriter.h"

#include "openstrata/rn/Tessellation.h"
#include "openstrata/rn/usd/PrimNames.h"

#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/vt/value.h"
#include "pxr/usd/sdf/attributeSpec.h"
#include "pxr/usd/sdf/fileFormat.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/sdf/primSpec.h"
#include "pxr/usd/sdf/relationshipSpec.h"
#include "pxr/usd/sdf/types.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace openstrata::rn::usd {

namespace {

/// The one code the writer raises, bracketed as it starts a message.
constexpr std::string_view kWriterFailure = "[RN-E901]";

/// An invariant that validation guarantees did not hold, or Sdf refused a
/// spec. Either is a bug; WriteLayer() reports it as RN-E901.
struct WriteFailure {
  std::string detail;
};

[[noreturn]] void Fail(std::string detail) {
  throw WriteFailure{std::move(detail)};
}

/// ADR 0003 section 5: the visualization origin is zero while every point
/// lies within this distance of it on every axis, and a multiple of the
/// grid otherwise.
constexpr double kOriginFreeHalfExtentMetres = 8192.0;
constexpr double kOriginGridMetres = 1024.0;

TfToken Token(const char* text) {
  return TfToken(text);
}

const char* ContactToken(ContactPoint contact) {
  return contact == ContactPoint::Start ? "start" : "end";
}

SdfPrimSpecHandle DefinePrim(const SdfPrimSpecHandle& parent, const std::string& name,
    const char* typeName) {
  SdfPrimSpecHandle prim = SdfPrimSpec::New(parent, name, SdfSpecifierDef, typeName);
  if (!prim) {
    Fail("could not author the prim '" + name + "' under " + parent->GetPath().GetString());
  }
  return prim;
}

/// A property this project defines: `custom`, in the `roadnetwork:`
/// namespace, and `uniform` (ADR 0002 section 2).
template <class T>
void SetAttribute(const SdfPrimSpecHandle& prim, const char* name, const SdfValueTypeName& type,
    const T& value) {
  SdfAttributeSpecHandle attribute =
      SdfAttributeSpec::New(prim, name, type, SdfVariabilityUniform, /*custom=*/true);
  if (!attribute || !attribute->SetDefaultValue(VtValue(value))) {
    Fail(std::string("could not author ") + name + " on " + prim->GetPath().GetString());
  }
}

/// A property of a standard schema, with the variability the schema gives it.
template <class T>
void SetSchemaAttribute(const SdfPrimSpecHandle& prim, const char* name,
    const SdfValueTypeName& type, SdfVariability variability, const T& value) {
  SdfAttributeSpecHandle attribute =
      SdfAttributeSpec::New(prim, name, type, variability, /*custom=*/false);
  if (!attribute || !attribute->SetDefaultValue(VtValue(value))) {
    Fail(std::string("could not author ") + name + " on " + prim->GetPath().GetString());
  }
}

void SetRelationship(const SdfPrimSpecHandle& prim, const char* name, const SdfPathVector& targets) {
  SdfRelationshipSpecHandle relationship =
      SdfRelationshipSpec::New(prim, name, /*custom=*/true, SdfVariabilityUniform);
  if (!relationship) {
    Fail(std::string("could not author ") + name + " on " + prim->GetPath().GetString());
  }
  relationship->GetTargetPathList().ClearEditsAndMakeExplicit();
  relationship->GetTargetPathList().GetExplicitItems() = targets;
}

void SetOptionalString(const SdfPrimSpecHandle& prim, const char* name, const std::string& value) {
  if (!value.empty()) {
    SetAttribute(prim, name, SdfValueTypeNames->String, value);
  }
}

int ToInt(std::uint32_t value, const char* what) {
  if (value > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
    Fail(std::string(what) + " does not fit an int");
  }
  return static_cast<int>(value);
}

/// Authors a piecewise cubic as five parallel arrays, `<base>:<s>`, `:a`,
/// `:b`, `:c`, `:d`. An empty profile is absent in the source and is not
/// authored (ADR 0002 section 2).
void SetCubic(const SdfPrimSpecHandle& prim, const std::string& base, const char* sName,
    const PiecewiseCubic& cubic) {
  if (cubic.records.empty()) {
    return;
  }
  VtDoubleArray s, a, b, c, d;
  for (const CubicRecord& record : cubic.records) {
    s.push_back(record.s0);
    a.push_back(record.a);
    b.push_back(record.b);
    c.push_back(record.c);
    d.push_back(record.d);
  }
  const SdfValueTypeName& type = SdfValueTypeNames->DoubleArray;
  SetAttribute(prim, (base + ":" + sName).c_str(), type, s);
  SetAttribute(prim, (base + ":a").c_str(), type, a);
  SetAttribute(prim, (base + ":b").c_str(), type, b);
  SetAttribute(prim, (base + ":c").c_str(), type, c);
  SetAttribute(prim, (base + ":d").c_str(), type, d);
}

using LaneId = std::tuple<std::string, std::uint32_t, std::int32_t>;

LaneId IdOf(const LaneKey& key) {
  return {key.roadId, key.sectionIndex, key.laneNumber};
}

/// The relationship targets of one lane (ADR 0005 section 4).
struct LaneNeighbours {
  SdfPathVector predecessors; ///< Joined at this lane's start.
  SdfPathVector successors;   ///< Joined at this lane's end.
};

/// Sorted by the path text, byte order, without repeats.
void SortTargets(SdfPathVector& targets) {
  std::sort(targets.begin(), targets.end(),
      [](const SdfPath& lhs, const SdfPath& rhs) { return lhs.GetString() < rhs.GetString(); });
  targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
}

class Writer {
public:
  Writer(const RoadNetworkData& data, const ImportRecord& record, SdfLayer& layer)
      : data_(data), record_(record), layer_(layer) {
  }

  void Run() {
    layer_.SetDefaultPrim(Token("Asset"));
    layer_.SetField(SdfPath::AbsoluteRootPath(), Token("upAxis"), VtValue(Token("Z")));
    layer_.SetField(SdfPath::AbsoluteRootPath(), Token("metersPerUnit"), VtValue(1.0));

    const SdfPrimSpecHandle asset = DefinePrim(layer_.GetPseudoRoot(), "Asset", "Xform");
    asset->SetKind(Token("component"));
    AuthorProvenance(asset);

    const SdfPrimSpecHandle network = DefinePrim(asset, "Network", "Scope");
    IndexPaths(network->GetPath());
    if (!data_.roads.empty()) {
      const SdfPrimSpecHandle roads = DefinePrim(network, "Roads", "Scope");
      for (const Road& road : data_.roads) {
        AuthorRoad(roads, road);
      }
    }
    if (!data_.junctions.empty()) {
      const SdfPrimSpecHandle junctions = DefinePrim(network, "Junctions", "Scope");
      for (const Junction& junction : data_.junctions) {
        AuthorJunction(junctions, junction);
      }
    }
    if (!data_.roads.empty()) {
      AuthorVisualization(asset);
    }
  }

private:
  // ---- /Asset -------------------------------------------------------------

  void AuthorProvenance(const SdfPrimSpecHandle& asset) {
    const SourceInfo& source = data_.source;
    const SdfValueTypeName& text = SdfValueTypeNames->String;
    SetAttribute(asset, "roadnetwork:source:format", text, source.format);
    SetAttribute(asset, "roadnetwork:source:formatVersion", text,
        std::to_string(source.versionMajor) + "." + std::to_string(source.versionMinor));
    SetOptionalString(asset, "roadnetwork:source:name", source.name);
    SetOptionalString(asset, "roadnetwork:source:date", source.date);
    SetOptionalString(asset, "roadnetwork:source:vendor", source.vendor);
    SetOptionalString(asset, "roadnetwork:source:version", source.version);
    if (source.geoReference && !source.geoReference->empty()) {
      SetAttribute(asset, "roadnetwork:source:geoReference", text, *source.geoReference);
    }
    if (source.offset) {
      const SourceOffset& offset = *source.offset;
      SetAttribute(asset, "roadnetwork:source:offset", SdfValueTypeNames->Double3,
          GfVec3d(offset.x, offset.y, offset.z));
      SetAttribute(asset, "roadnetwork:source:offsetHeading", SdfValueTypeNames->Double, offset.hdg);
    }

    SetAttribute(asset, "roadnetwork:import:importer", text, record_.importer);
    SetAttribute(asset, "roadnetwork:import:importerVersion", text, record_.importerVersion);
    SetAttribute(asset, "roadnetwork:import:roadCount", SdfValueTypeNames->Int,
        ToInt(record_.roadCount, "the road count"));
    SetAttribute(asset, "roadnetwork:import:junctionCount", SdfValueTypeNames->Int,
        ToInt(record_.junctionCount, "the junction count"));
    SetAttribute(asset, "roadnetwork:import:laneCount", SdfValueTypeNames->Int,
        ToInt(record_.laneCount, "the lane count"));

    // Always authored: empty arrays state that the read lost nothing.
    std::vector<WarningSummary> warnings = data_.warnings;
    std::sort(warnings.begin(), warnings.end(),
        [](const WarningSummary& lhs, const WarningSummary& rhs) { return lhs.code < rhs.code; });
    VtStringArray codes;
    VtIntArray counts;
    for (const WarningSummary& warning : warnings) {
      if (!codes.empty() && codes.back() == warning.code) {
        Fail("the warning " + warning.code + " is summarized twice");
      }
      codes.push_back(warning.code);
      counts.push_back(ToInt(warning.count, "a warning count"));
    }
    SetAttribute(asset, "roadnetwork:import:warningCodes", SdfValueTypeNames->StringArray, codes);
    SetAttribute(asset, "roadnetwork:import:warningCounts", SdfValueTypeNames->IntArray, counts);
  }

  // ---- Paths --------------------------------------------------------------

  /// Every path is a pure function of the data (ADR 0005 section 7), so all
  /// of them are known before any relationship is authored.
  void IndexPaths(const SdfPath& network) {
    const SdfPath roads = network.AppendChild(Token("Roads"));
    for (const Road& road : data_.roads) {
      const SdfPath roadPath = roads.AppendChild(TfToken(NameOf(kRoadPrefix, road.id)));
      if (!roadPaths_.emplace(road.id, roadPath).second) {
        Fail("the road ID '" + road.id + "' repeats");
      }
      for (std::uint32_t index = 0; index < road.sections.size(); ++index) {
        const SdfPath sectionPath = roadPath.AppendChild(TfToken(SectionName(index)));
        for (const Lane& lane : road.sections[index].lanes) {
          if (!lanes_.emplace(LaneId{road.id, index, lane.number},
                         LaneEntry{sectionPath.AppendChild(TfToken(LaneName(lane.number))), {}})
                  .second) {
            Fail("lane " + std::to_string(lane.number) + " repeats in section " +
                 std::to_string(index) + " of road '" + road.id + "'");
          }
        }
      }
    }
    const SdfPath junctions = network.AppendChild(Token("Junctions"));
    for (const Junction& junction : data_.junctions) {
      const SdfPath path = junctions.AppendChild(TfToken(NameOf(kJunctionPrefix, junction.id)));
      if (!junctionPaths_.emplace(junction.id, path).second) {
        Fail("the junction ID '" + junction.id + "' repeats");
      }
    }

    // Each lane-graph edge appears on both of its lanes (ADR 0005 section 4).
    for (const LaneLink& link : data_.laneLinks) {
      AddNeighbour(link.a, LanePath(link.b.lane));
      AddNeighbour(link.b, LanePath(link.a.lane));
    }
    for (auto& [id, entry] : lanes_) {
      SortTargets(entry.neighbours.predecessors);
      SortTargets(entry.neighbours.successors);
    }
  }

  static std::string NameOf(std::string_view prefix, const std::string& id) {
    std::optional<std::string> name = PrimName(prefix, id);
    if (!name) {
      Fail("a " + std::string(prefix) + " has an empty ID");
    }
    return *name;
  }

  const SdfPath& RoadPath(const std::string& id) const {
    const auto it = roadPaths_.find(id);
    if (it == roadPaths_.end()) {
      Fail("the road '" + id + "' does not exist");
    }
    return it->second;
  }

  const SdfPath& JunctionPath(const std::string& id) const {
    const auto it = junctionPaths_.find(id);
    if (it == junctionPaths_.end()) {
      Fail("the junction '" + id + "' does not exist");
    }
    return it->second;
  }

  const SdfPath& LanePath(const LaneKey& key) const {
    const auto it = lanes_.find(IdOf(key));
    if (it == lanes_.end()) {
      Fail("lane " + std::to_string(key.laneNumber) + " of section " +
           std::to_string(key.sectionIndex) + " of road '" + key.roadId + "' does not exist");
    }
    return it->second.path;
  }

  void AddNeighbour(const LaneEnd& end, const SdfPath& other) {
    LanePath(end.lane); // Fails when the lane does not exist.
    LaneNeighbours& neighbours = lanes_.at(IdOf(end.lane)).neighbours;
    (end.end == ContactPoint::Start ? neighbours.predecessors : neighbours.successors)
        .push_back(other);
  }

  // ---- /Asset/Network -----------------------------------------------------

  void AuthorRoad(const SdfPrimSpecHandle& roads, const Road& road) {
    const SdfPath& path = RoadPath(road.id);
    const SdfPrimSpecHandle prim = DefinePrim(roads, path.GetName(), "Scope");
    SetAttribute(prim, "roadnetwork:id", SdfValueTypeNames->String, road.id);
    SetOptionalString(prim, "roadnetwork:name", road.name);
    SetAttribute(prim, "roadnetwork:length", SdfValueTypeNames->Double, road.length);
    if (road.junction) {
      SetRelationship(prim, "roadnetwork:junction", {JunctionPath(*road.junction)});
    }
    AuthorRoadLink(prim, "roadnetwork:predecessor", "roadnetwork:predecessorContact",
        road.predecessor);
    AuthorRoadLink(prim, "roadnetwork:successor", "roadnetwork:successorContact", road.successor);

    VtTokenArray kind;
    VtDoubleArray s, x, y, hdg, length, curvature;
    for (const PlanViewSegment& segment : road.referenceLine.segments) {
      const Arc* arc = std::get_if<Arc>(&segment.shape);
      kind.push_back(Token(arc ? "arc" : "line"));
      s.push_back(segment.s);
      x.push_back(segment.x);
      y.push_back(segment.y);
      hdg.push_back(segment.hdg);
      length.push_back(segment.length);
      curvature.push_back(arc ? arc->curvature : 0.0);
    }
    const SdfValueTypeName& doubles = SdfValueTypeNames->DoubleArray;
    SetAttribute(prim, "roadnetwork:referenceLine:kind", SdfValueTypeNames->TokenArray, kind);
    SetAttribute(prim, "roadnetwork:referenceLine:s", doubles, s);
    SetAttribute(prim, "roadnetwork:referenceLine:x", doubles, x);
    SetAttribute(prim, "roadnetwork:referenceLine:y", doubles, y);
    SetAttribute(prim, "roadnetwork:referenceLine:hdg", doubles, hdg);
    SetAttribute(prim, "roadnetwork:referenceLine:length", doubles, length);
    SetAttribute(prim, "roadnetwork:referenceLine:curvature", doubles, curvature);

    SetCubic(prim, "roadnetwork:elevation", "s", road.elevation);
    SetCubic(prim, "roadnetwork:laneOffset", "s", road.laneOffset);
    if (!road.speeds.empty()) {
      VtDoubleArray speedS, speedMax;
      for (const SpeedRecord& speed : road.speeds) {
        speedS.push_back(speed.s);
        speedMax.push_back(speed.maxMetresPerSecond);
      }
      SetAttribute(prim, "roadnetwork:speed:s", doubles, speedS);
      SetAttribute(prim, "roadnetwork:speed:max", doubles, speedMax);
    }

    for (std::uint32_t index = 0; index < road.sections.size(); ++index) {
      AuthorSection(prim, road, index);
    }
  }

  void AuthorRoadLink(const SdfPrimSpecHandle& prim, const char* name, const char* contactName,
      const std::optional<RoadLink>& link) {
    if (!link) {
      return;
    }
    if (link->kind == RoadLink::Kind::Junction) {
      SetRelationship(prim, name, {JunctionPath(link->id)});
      return;
    }
    SetRelationship(prim, name, {RoadPath(link->id)});
    if (link->contact) {
      SetAttribute(prim, contactName, SdfValueTypeNames->Token, Token(ContactToken(*link->contact)));
    }
  }

  void AuthorSection(const SdfPrimSpecHandle& road, const Road& source, std::uint32_t index) {
    const LaneSection& section = source.sections[index];
    const SdfPrimSpecHandle prim = DefinePrim(road, SectionName(index), "Scope");
    SetAttribute(prim, "roadnetwork:s", SdfValueTypeNames->Double, section.s);

    // Descending lane number: left_<n> through center to right_<m>
    // (ADR 0005 section 3).
    std::vector<const Lane*> lanes;
    for (const Lane& lane : section.lanes) {
      lanes.push_back(&lane);
    }
    std::stable_sort(lanes.begin(), lanes.end(),
        [](const Lane* lhs, const Lane* rhs) { return lhs->number > rhs->number; });
    for (const Lane* lane : lanes) {
      AuthorLane(prim, LaneKey{source.id, index, lane->number}, *lane);
    }
  }

  void AuthorLane(const SdfPrimSpecHandle& section, const LaneKey& key, const Lane& lane) {
    const SdfPrimSpecHandle prim = DefinePrim(section, LaneName(lane.number), "Scope");
    SetAttribute(prim, "roadnetwork:laneNumber", SdfValueTypeNames->Int, static_cast<int>(lane.number));
    SetAttribute(prim, "roadnetwork:laneType", SdfValueTypeNames->Token, TfToken(lane.type));
    SetAttribute(prim, "roadnetwork:level", SdfValueTypeNames->Bool, lane.level);
    SetCubic(prim, "roadnetwork:width", "sOffset", lane.width);
    const LaneNeighbours& neighbours = lanes_.at(IdOf(key)).neighbours;
    if (!neighbours.predecessors.empty()) {
      SetRelationship(prim, "roadnetwork:predecessor", neighbours.predecessors);
    }
    if (!neighbours.successors.empty()) {
      SetRelationship(prim, "roadnetwork:successor", neighbours.successors);
    }
  }

  void AuthorJunction(const SdfPrimSpecHandle& junctions, const Junction& junction) {
    const SdfPrimSpecHandle prim =
        DefinePrim(junctions, JunctionPath(junction.id).GetName(), "Scope");
    SetAttribute(prim, "roadnetwork:id", SdfValueTypeNames->String, junction.id);
    SetOptionalString(prim, "roadnetwork:name", junction.name);
    for (const Connection& connection : junction.connections) {
      const SdfPrimSpecHandle child =
          DefinePrim(prim, NameOf(kConnectionPrefix, connection.id), "Scope");
      SetAttribute(child, "roadnetwork:id", SdfValueTypeNames->String, connection.id);
      SetRelationship(child, "roadnetwork:incomingRoad", {RoadPath(connection.incomingRoad)});
      SetRelationship(child, "roadnetwork:connectingRoad", {RoadPath(connection.connectingRoad)});
      SetAttribute(child, "roadnetwork:contactPoint", SdfValueTypeNames->Token,
          Token(ContactToken(connection.contact)));
      if (!connection.laneLinks.empty()) {
        VtIntArray from, to;
        for (const auto& [fromLane, toLane] : connection.laneLinks) {
          from.push_back(fromLane);
          to.push_back(toLane);
        }
        SetAttribute(child, "roadnetwork:laneLink:from", SdfValueTypeNames->IntArray, from);
        SetAttribute(child, "roadnetwork:laneLink:to", SdfValueTypeNames->IntArray, to);
      }
    }
  }

  // ---- /Asset/Visualization ----------------------------------------------

  /// One road's curves for one container, with the semantic prim of each.
  struct CurveSet {
    std::vector<const SampledCurve*> curves;
    SdfPathVector represents;
  };

  void AuthorVisualization(const SdfPrimSpecHandle& asset) {
    // The writer authors the tessellator's samples as they are; the only
    // arithmetic here is the origin shift of ADR 0003 section 5.
    std::vector<SampledRoad> sampled;
    sampled.reserve(data_.roads.size());
    for (const Road& road : data_.roads) {
      sampled.push_back(TessellateRoad(road));
    }

    std::vector<CurveSet> centerlines(sampled.size());
    std::vector<CurveSet> boundaries(sampled.size());
    for (std::size_t r = 0; r < sampled.size(); ++r) {
      for (const SampledSection& section : sampled[r].sections) {
        for (const SampledCurve& curve : section.centerlines) {
          centerlines[r].curves.push_back(&curve);
          centerlines[r].represents.push_back(
              LanePath(LaneKey{sampled[r].roadId, section.sectionIndex, curve.laneNumber}));
        }
        for (const SampledCurve& curve : section.borders) {
          boundaries[r].curves.push_back(&curve);
          boundaries[r].represents.push_back(
              LanePath(LaneKey{sampled[r].roadId, section.sectionIndex, curve.laneNumber}));
        }
      }
    }

    const GfVec3d origin = Origin(sampled);
    const SdfPrimSpecHandle visualization = DefinePrim(asset, "Visualization", "Xform");
    if (origin != GfVec3d(0.0)) {
      SetSchemaAttribute(visualization, "xformOp:translate", SdfValueTypeNames->Double3,
          SdfVariabilityVarying, origin);
      SetSchemaAttribute(visualization, "xformOpOrder", SdfValueTypeNames->TokenArray,
          SdfVariabilityUniform, VtTokenArray{Token("xformOp:translate")});
    }
    AuthorCurveContainer(visualization, "Centerlines", centerlines, origin);
    AuthorCurveContainer(visualization, "Boundaries", boundaries, origin);
  }

  static GfVec3d Origin(const std::vector<SampledRoad>& sampled) {
    double lo[3] = {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()};
    double hi[3] = {-lo[0], -lo[1], -lo[2]};
    bool any = false;
    const auto include = [&](const Point3& p) {
      const double v[3] = {p.x, p.y, p.z};
      for (int axis = 0; axis < 3; ++axis) {
        lo[axis] = std::min(lo[axis], v[axis]);
        hi[axis] = std::max(hi[axis], v[axis]);
      }
      any = true;
    };
    for (const SampledRoad& road : sampled) {
      for (const SampledSection& section : road.sections) {
        for (const SampledCurve& curve : section.borders) {
          std::for_each(curve.points.begin(), curve.points.end(), include);
        }
        for (const SampledCurve& curve : section.centerlines) {
          std::for_each(curve.points.begin(), curve.points.end(), include);
        }
      }
    }
    if (!any) {
      return GfVec3d(0.0);
    }
    bool inside = true;
    for (int axis = 0; axis < 3; ++axis) {
      inside = inside && lo[axis] >= -kOriginFreeHalfExtentMetres &&
               hi[axis] <= kOriginFreeHalfExtentMetres;
    }
    if (inside) {
      return GfVec3d(0.0);
    }
    GfVec3d origin;
    for (int axis = 0; axis < 3; ++axis) {
      const double center = 0.5 * (lo[axis] + hi[axis]);
      // + 0.0 turns a rounded -0 into +0, so the authored text is stable.
      origin[axis] = std::round(center / kOriginGridMetres) * kOriginGridMetres + 0.0;
    }
    return origin;
  }

  void AuthorCurveContainer(const SdfPrimSpecHandle& visualization, const char* name,
      const std::vector<CurveSet>& sets, const GfVec3d& origin) {
    SdfPrimSpecHandle container;
    for (std::size_t r = 0; r < sets.size(); ++r) {
      if (sets[r].curves.empty()) {
        continue; // A road with a center lane only has no centerline.
      }
      if (!container) {
        container = DefinePrim(visualization, name, "Scope");
      }
      AuthorCurves(container, RoadPath(data_.roads[r].id).GetName(), sets[r], origin);
    }
  }

  static void AuthorCurves(const SdfPrimSpecHandle& container, const std::string& name,
      const CurveSet& set, const GfVec3d& origin) {
    const SdfPrimSpecHandle prim = DefinePrim(container, name, "BasisCurves");
    VtIntArray counts;
    VtVec3fArray points;
    GfVec3f lo(std::numeric_limits<float>::infinity());
    GfVec3f hi(-std::numeric_limits<float>::infinity());
    for (const SampledCurve* curve : set.curves) {
      counts.push_back(ToInt(static_cast<std::uint32_t>(curve->points.size()), "a curve size"));
      for (const Point3& p : curve->points) {
        const GfVec3f local(static_cast<float>(p.x - origin[0]), static_cast<float>(p.y - origin[1]),
            static_cast<float>(p.z - origin[2]));
        points.push_back(local);
        for (int axis = 0; axis < 3; ++axis) {
          lo[axis] = std::min(lo[axis], local[axis]);
          hi[axis] = std::max(hi[axis], local[axis]);
        }
      }
    }
    SetSchemaAttribute(prim, "type", SdfValueTypeNames->Token, SdfVariabilityUniform, Token("linear"));
    SetSchemaAttribute(prim, "curveVertexCounts", SdfValueTypeNames->IntArray, SdfVariabilityVarying,
        counts);
    SetSchemaAttribute(prim, "points", SdfValueTypeNames->Point3fArray, SdfVariabilityVarying, points);
    SetSchemaAttribute(prim, "extent", SdfValueTypeNames->Float3Array, SdfVariabilityVarying,
        VtVec3fArray{lo, hi});
    SetRelationship(prim, "roadnetwork:represents", set.represents);
  }

  struct LaneEntry {
    SdfPath path;
    LaneNeighbours neighbours;
  };

  const RoadNetworkData& data_;
  const ImportRecord& record_;
  SdfLayer& layer_;
  std::map<std::string, SdfPath> roadPaths_;
  std::map<std::string, SdfPath> junctionPaths_;
  std::map<LaneId, LaneEntry> lanes_;
};

} // namespace

bool WriteLayer(const RoadNetworkData& data, const ImportRecord& record, SdfLayer& layer,
    Diagnostic* error) {
  const auto report = [error](const std::string& detail) {
    if (error) {
      error->code = std::string(kWriterFailure.substr(1, kWriterFailure.size() - 2));
      error->message = std::string(kWriterFailure) + " " + detail;
    }
    return false;
  };

  // Author into a scratch layer first so a failure leaves `layer` untouched.
  SdfFileFormatConstPtr usda = SdfFileFormat::FindByExtension("usda");
  SdfLayerRefPtr generated =
      usda ? SdfLayer::CreateAnonymous("roadnetwork.generated.usda", usda) : SdfLayerRefPtr();
  if (!generated) {
    return report("could not create the scratch USDA layer");
  }
  try {
    Writer(data, record, *generated).Run();
  } catch (const WriteFailure& failure) {
    return report("could not author the network: " + failure.detail);
  }
  layer.TransferContent(generated);
  return true;
}

} // namespace openstrata::rn::usd
