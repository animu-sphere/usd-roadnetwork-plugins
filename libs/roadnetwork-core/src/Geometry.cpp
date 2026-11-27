// SPDX-License-Identifier: Apache-2.0
#include "openstrata/rn/Geometry.h"

#include "PlanView.h"
#include "SectionIntervals.h"
#include "openstrata/rn/Validation.h"

#include <algorithm>
#include <cmath>
#include <variant>

namespace openstrata::rn {

namespace {

// sin(z) / z, with sinc(0) = 1; the Taylor series below |z| = 1e-4, where
// its first omitted term is below 1e-18 (ADR 0004 section 2).
double Sinc(double z) noexcept {
  if (std::abs(z) < 1e-4) {
    const double z2 = z * z;
    return 1.0 - z2 / 6.0 + z2 * z2 / 120.0;
  }
  return std::sin(z) / z;
}

// The last element whose start is at or before `s`, or the first element.
template <typename Container, typename Start>
std::size_t LastAtOrBefore(const Container& items, double s, Start start) noexcept {
  const auto after = std::upper_bound(items.begin(), items.end(), s,
      [&](double value, const auto& item) { return value < start(item); });
  return after == items.begin() ? 0 : static_cast<std::size_t>(after - items.begin()) - 1;
}

const Lane* FindLane(const LaneSection& section, std::int32_t number) noexcept {
  for (const Lane& lane : section.lanes) {
    if (lane.number == number) {
      return &lane;
    }
  }
  return nullptr;
}

} // namespace

namespace {

// The cubic and its first two derivatives at `p`.
struct Cubic3 {
  double value = 0.0;
  double first = 0.0;
  double second = 0.0;
};

Cubic3 EvaluateCubic3(double a, double b, double c, double d, double p) noexcept {
  return Cubic3{a + p * (b + p * (c + p * d)), b + p * (2.0 * c + 3.0 * p * d), 2.0 * c + 6.0 * p * d};
}

// 8-point Gauss-Legendre on [-1, 1]: the positive nodes and their weights.
constexpr double kGl8Node[4] = {0.18343464249564980493947614236018398, 0.52553240991632898581773904918924635,
    0.79666647741362673959155393647583044, 0.96028985649753623168356086856947299};
constexpr double kGl8Weight[4] = {0.36268378337836198296515044927719561, 0.31370664587788728733796220198660131,
    0.22238103445337447054435599442624088, 0.10122853629037625915253135430996219};

// N-point Gauss-Legendre on [-1, 1], by Newton's method on the Legendre
// polynomial: the positive half of the nodes and their weights.
template <int N>
struct GaussLegendre {
  double node[N / 2] = {};
  double weight[N / 2] = {};

  GaussLegendre() noexcept {
    constexpr double pi = 3.141592653589793238462643383279;
    for (int i = 0; i < N / 2; ++i) {
      double x = std::cos(pi * (i + 0.75) / (N + 0.5));
      double derivative = 1.0;
      for (int iteration = 0; iteration < 100; ++iteration) {
        double p0 = 1.0;
        double p1 = x;
        for (int k = 2; k <= N; ++k) {
          const double p2 = ((2.0 * k - 1.0) * x * p1 - (k - 1.0) * p0) / k;
          p0 = p1;
          p1 = p2;
        }
        derivative = N * (x * p1 - p0) / (x * x - 1.0);
        const double step = p1 / derivative;
        x -= step;
        if (std::abs(step) < 1e-16) {
          break;
        }
      }
      node[i] = x;
      weight[i] = 2.0 / ((1.0 - x * x) * derivative * derivative);
    }
  }
};

constexpr std::size_t kMaxSpiralPieces = std::size_t{1} << 22;

Pose2 EvaluateLineOrArc(const PlanViewSegment& segment, double kappa, double s) noexcept {
  const double u = s - segment.s;
  const double theta = segment.hdg + kappa * u / 2.0;
  const double chord = u * Sinc(kappa * u / 2.0);
  return Pose2{segment.x + chord * std::cos(theta), segment.y + chord * std::sin(theta),
      segment.hdg + kappa * u};
}

Pose2 Place(const PlanViewSegment& segment, const detail::Vec2& local, double hdg) noexcept {
  const double c = std::cos(segment.hdg);
  const double sn = std::sin(segment.hdg);
  return Pose2{segment.x + (c * local.x - sn * local.y), segment.y + (sn * local.x + c * local.y), hdg};
}

Pose2 EvaluateSpiral(const PlanViewSegment& segment, const Spiral& spiral, const detail::Vec2* prefix,
    double s) noexcept {
  const detail::SpiralFrame frame = detail::FrameOf(segment, spiral);
  const double u = s - segment.s;
  const detail::Vec2 local = detail::SpiralOffset(frame, prefix, u);
  return Place(segment, local, segment.hdg + u * (frame.kappa0 + frame.rate * u / 2.0));
}

Pose2 EvaluateParamPoly3(const PlanViewSegment& segment, const ParamPoly3& curve, double s) noexcept {
  const double p = ParameterAt(curve, segment.length, s - segment.s);
  const Cubic3 u = EvaluateCubic3(curve.aU, curve.bU, curve.cU, curve.dU, p);
  const Cubic3 v = EvaluateCubic3(curve.aV, curve.bV, curve.cV, curve.dV, p);
  return Place(segment, detail::Vec2{u.value, v.value}, segment.hdg + std::atan2(v.first, u.first));
}

} // namespace

namespace detail {

SpiralFrame FrameOf(const PlanViewSegment& segment, const Spiral& spiral) noexcept {
  SpiralFrame frame;
  frame.kappa0 = spiral.curvatureStart;
  frame.rate = (spiral.curvatureEnd - spiral.curvatureStart) / segment.length;
  const double k = std::max(std::abs(spiral.curvatureStart), std::abs(spiral.curvatureEnd));
  const double m = std::ceil(segment.length * (k + std::sqrt(std::abs(frame.rate))));
  // Not finite or below 1 for degenerate input; capped so that a spiral that
  // turns by millions of radians stays bounded in time and memory.
  frame.pieces = m >= 1.0 ? static_cast<std::size_t>(std::min(m, static_cast<double>(kMaxSpiralPieces))) : 1;
  frame.piece = segment.length / static_cast<double>(frame.pieces);
  return frame;
}

Vec2 SpiralPiece(const SpiralFrame& frame, double a, double b) noexcept {
  const double r = (b - a) / 2.0;
  const double c = a + r;
  const double phase = c * (frame.kappa0 + frame.rate * c / 2.0);
  const double kappa = frame.kappa0 + frame.rate * c;
  double sx = 0.0;
  double sy = 0.0;
  for (int i = 0; i < 4; ++i) {
    const double w = r * kGl8Node[i];
    const double plus = w * (kappa + frame.rate * w / 2.0);
    const double minus = -w * (kappa - frame.rate * w / 2.0);
    sx += kGl8Weight[i] * (std::cos(plus) + std::cos(minus));
    sy += kGl8Weight[i] * (std::sin(plus) + std::sin(minus));
  }
  sx *= r;
  sy *= r;
  const double cp = std::cos(phase);
  const double sp = std::sin(phase);
  return Vec2{cp * sx - sp * sy, sp * sx + cp * sy};
}

Vec2 SpiralOffset(const SpiralFrame& frame, const Vec2* prefix, double u) noexcept {
  const double index = std::floor(u / frame.piece);
  const std::size_t k = index <= 0.0 ? 0
                        : index >= static_cast<double>(frame.pieces - 1)
                            ? frame.pieces - 1
                            : static_cast<std::size_t>(index);
  Vec2 sum;
  if (prefix) {
    sum = prefix[k];
  } else {
    for (std::size_t j = 0; j < k; ++j) {
      const Vec2 piece = SpiralPiece(frame, static_cast<double>(j) * frame.piece,
          static_cast<double>(j + 1) * frame.piece);
      sum.x += piece.x;
      sum.y += piece.y;
    }
  }
  // The rest of the way from the start of piece k. Inside the segment it is
  // at most one piece; past either end it is cut into pieces of the same
  // length, so that extrapolation keeps the bound.
  const double a = static_cast<double>(k) * frame.piece;
  const double rest = u - a;
  const double parts = std::abs(rest) <= frame.piece * (1.0 + 1e-12) ? 1.0 : std::ceil(std::abs(rest) / frame.piece);
  const auto q = static_cast<std::size_t>(std::min(parts, static_cast<double>(kMaxSpiralPieces)));
  for (std::size_t i = 0; i < q; ++i) {
    const Vec2 piece = SpiralPiece(frame, a + rest * static_cast<double>(i) / static_cast<double>(q),
        a + rest * static_cast<double>(i + 1) / static_cast<double>(q));
    sum.x += piece.x;
    sum.y += piece.y;
  }
  return sum;
}

std::vector<Vec2> SpiralPrefix(const SpiralFrame& frame) {
  std::vector<Vec2> prefix(frame.pieces);
  Vec2 sum;
  for (std::size_t j = 0; j < frame.pieces; ++j) {
    prefix[j] = sum;
    const Vec2 piece = SpiralPiece(frame, static_cast<double>(j) * frame.piece,
        static_cast<double>(j + 1) * frame.piece);
    sum.x += piece.x;
    sum.y += piece.y;
  }
  return prefix;
}

ReferenceLineEvaluator::ReferenceLineEvaluator(const ReferenceLine& line) : line_(line) {
  prefix_.resize(line.segments.size());
  for (std::size_t i = 0; i < line.segments.size(); ++i) {
    if (const Spiral* spiral = std::get_if<Spiral>(&line.segments[i].shape)) {
      prefix_[i] = SpiralPrefix(FrameOf(line.segments[i], *spiral));
    }
  }
}

Pose2 ReferenceLineEvaluator::operator()(double s) const noexcept {
  const std::size_t index = SegmentIndexAt(line_, s);
  const PlanViewSegment& segment = line_.segments[index];
  if (const Spiral* spiral = std::get_if<Spiral>(&segment.shape)) {
    return EvaluateSpiral(segment, *spiral, prefix_[index].data(), s);
  }
  return EvaluateSegment(segment, s);
}

} // namespace detail

double CurvatureAt(const PlanViewSegment& segment, double s) noexcept {
  if (const Arc* arc = std::get_if<Arc>(&segment.shape)) {
    return arc->curvature;
  }
  if (const Spiral* spiral = std::get_if<Spiral>(&segment.shape)) {
    const double rate = (spiral->curvatureEnd - spiral->curvatureStart) / segment.length;
    return spiral->curvatureStart + rate * (s - segment.s);
  }
  if (const ParamPoly3* curve = std::get_if<ParamPoly3>(&segment.shape)) {
    const double p = ParameterAt(*curve, segment.length, s - segment.s);
    const Cubic3 u = EvaluateCubic3(curve->aU, curve->bU, curve->cU, curve->dU, p);
    const Cubic3 v = EvaluateCubic3(curve->aV, curve->bV, curve->cV, curve->dV, p);
    const double speed2 = u.first * u.first + v.first * v.first;
    if (!(speed2 > 0.0)) {
      return 0.0;
    }
    return (u.first * v.second - v.first * u.second) / (speed2 * std::sqrt(speed2));
  }
  return 0.0;
}

double ParameterRange(const ParamPoly3& curve, double length) noexcept {
  return curve.pRange == ParamRange::ArcLength ? length : 1.0;
}

double ParameterAt(const ParamPoly3& curve, double length, double u) noexcept {
  return u * ParameterRange(curve, length) / length;
}

double CurveLength(const PlanViewSegment& segment) noexcept {
  const ParamPoly3* curve = std::get_if<ParamPoly3>(&segment.shape);
  if (!curve) {
    return segment.length;
  }
  static const GaussLegendre<32> rule;
  constexpr int kPieces = 8;
  const double range = ParameterRange(*curve, segment.length);
  const double r = range / kPieces / 2.0;
  double total = 0.0;
  for (int piece = 0; piece < kPieces; ++piece) {
    const double c = range * (piece + 0.5) / kPieces;
    double sum = 0.0;
    for (int i = 0; i < 16; ++i) {
      for (const double p : {c - r * rule.node[i], c + r * rule.node[i]}) {
        const double du = curve->bU + p * (2.0 * curve->cU + 3.0 * p * curve->dU);
        const double dv = curve->bV + p * (2.0 * curve->cV + 3.0 * p * curve->dV);
        sum += rule.weight[i] * std::hypot(du, dv);
      }
    }
    total += r * sum;
  }
  return total;
}

Pose2 EvaluateSegment(const PlanViewSegment& segment, double s) noexcept {
  if (const Spiral* spiral = std::get_if<Spiral>(&segment.shape)) {
    return EvaluateSpiral(segment, *spiral, nullptr, s);
  }
  if (const ParamPoly3* curve = std::get_if<ParamPoly3>(&segment.shape)) {
    return EvaluateParamPoly3(segment, *curve, s);
  }
  const Arc* arc = std::get_if<Arc>(&segment.shape);
  return EvaluateLineOrArc(segment, arc ? arc->curvature : 0.0, s);
}

std::size_t SegmentIndexAt(const ReferenceLine& line, double s) noexcept {
  return LastAtOrBefore(line.segments, s, [](const PlanViewSegment& g) { return g.s; });
}

Pose2 EvaluateReferenceLine(const ReferenceLine& line, double s) noexcept {
  return EvaluateSegment(line.segments[SegmentIndexAt(line, s)], s);
}

std::size_t RecordIndexAt(const PiecewiseCubic& cubic, double s) noexcept {
  return LastAtOrBefore(cubic.records, s, [](const CubicRecord& r) { return r.s0; });
}

double EvaluateCubic(const PiecewiseCubic& cubic, double s) noexcept {
  if (cubic.records.empty()) {
    return 0.0;
  }
  const CubicRecord& r = cubic.records[RecordIndexAt(cubic, s)];
  const double d = s - r.s0;
  return r.a + d * (r.b + d * (r.c + d * r.d));
}

std::size_t SectionIndexAt(const Road& road, double s) noexcept {
  return LastAtOrBefore(road.sections, s, [](const LaneSection& section) { return section.s; });
}

double BorderOffset(const Road& road, std::size_t sectionIndex, std::int32_t laneNumber,
    double s) noexcept {
  const LaneSection& section = road.sections[sectionIndex];
  const double ds = s - section.s;
  const std::int32_t step = laneNumber > 0 ? 1 : -1;
  double t = EvaluateCubic(road.laneOffset, s);
  for (std::int32_t number = step; laneNumber != 0 && number != laneNumber + step; number += step) {
    if (const Lane* lane = FindLane(section, number)) {
      t += step * EvaluateCubic(lane->width, ds);
    }
  }
  return t;
}

Point3 BorderPoint(const Road& road, std::size_t sectionIndex, std::int32_t laneNumber,
    double s) noexcept {
  return detail::BorderPointAt(road, sectionIndex, laneNumber, s,
      EvaluateReferenceLine(road.referenceLine, s));
}

Point3 detail::BorderPointAt(const Road& road, std::size_t sectionIndex, std::int32_t laneNumber,
    double s, const Pose2& pose) noexcept {
  const double t = BorderOffset(road, sectionIndex, laneNumber, s);
  return Point3{pose.x - t * std::sin(pose.hdg), pose.y + t * std::cos(pose.hdg),
      EvaluateCubic(road.elevation, s)};
}

Point3 LaneCenterPoint(const Road& road, std::size_t sectionIndex, std::int32_t laneNumber,
    double s) noexcept {
  const Point3 outer = BorderPoint(road, sectionIndex, laneNumber, s);
  const Point3 inner = BorderPoint(road, sectionIndex, laneNumber > 0 ? laneNumber - 1 : laneNumber + 1, s);
  return Point3{(outer.x + inner.x) / 2.0, (outer.y + inner.y) / 2.0, (outer.z + inner.z) / 2.0};
}

// ------------------------------------------------------------- intervals

namespace detail {

double Poly3::MaxAbsSecond(double length) const noexcept {
  return std::max(std::abs(Second(0.0)), std::abs(Second(length)));
}

Poly3& Poly3::operator+=(const Poly3& other) noexcept {
  c0 += other.c0;
  c1 += other.c1;
  c2 += other.c2;
  c3 += other.c3;
  return *this;
}

Poly3& Poly3::operator-=(const Poly3& other) noexcept {
  c0 -= other.c0;
  c1 -= other.c1;
  c2 -= other.c2;
  c3 -= other.c3;
  return *this;
}

Extremum Maximum(const Poly3& p, double length) noexcept {
  Extremum best{0.0, p.Value(0.0)};
  const auto consider = [&](double x) {
    if (x > 0.0 && x <= length) {
      const double value = p.Value(x);
      if (value > best.value) {
        best = Extremum{x, value};
      }
    }
  };
  consider(length);
  // f'(x) = c1 + 2 c2 x + 3 c3 x^2.
  const double qa = 3.0 * p.c3;
  const double qb = 2.0 * p.c2;
  const double qc = p.c1;
  if (qa == 0.0) {
    if (qb != 0.0) {
      consider(-qc / qb);
    }
  } else {
    const double disc = qb * qb - 4.0 * qa * qc;
    if (disc >= 0.0) {
      // The stable form of the two roots.
      const double q = -0.5 * (qb + std::copysign(std::sqrt(disc), qb));
      consider(q / qa);
      if (q != 0.0) {
        consider(qc / q);
      }
    }
  }
  return best;
}

double Poly4::Value(double x) const noexcept {
  return c[0] + x * (c[1] + x * (c[2] + x * (c[3] + x * c[4])));
}

Poly4 Poly4::Derivative() const noexcept {
  return Poly4{{c[1], 2.0 * c[2], 3.0 * c[3], 4.0 * c[4], 0.0}};
}

Poly4 Product(const Poly3& p, double a, double b) noexcept {
  return Poly4{{p.c0 * a, p.c1 * a + p.c0 * b, p.c2 * a + p.c1 * b, p.c3 * a + p.c2 * b, p.c3 * b}};
}

namespace {

int DegreeOf(const Poly4& p) noexcept {
  for (int d = 4; d > 0; --d) {
    if (p.c[d] != 0.0) {
      return d;
    }
  }
  return 0;
}

// The roots of `p` in (lo, hi), ascending: each between two consecutive
// extrema, found by bisection, so no root of odd multiplicity is missed.
// A root of even multiplicity is an extremum of `p`, which the callers
// consider anyway.
void RootsIn(const Poly4& p, double lo, double hi, std::vector<double>& roots) {
  const int degree = DegreeOf(p);
  if (degree == 0) {
    return;
  }
  if (degree == 1) {
    const double x = -p.c[0] / p.c[1];
    if (x > lo && x < hi) {
      roots.push_back(x);
    }
    return;
  }
  std::vector<double> marks{lo};
  RootsIn(p.Derivative(), lo, hi, marks);
  marks.push_back(hi);
  for (std::size_t i = 0; i + 1 < marks.size(); ++i) {
    double a = marks[i];
    double b = marks[i + 1];
    double fa = p.Value(a);
    const double fb = p.Value(b);
    if (fa == 0.0 || fb == 0.0 || (fa < 0.0) == (fb < 0.0)) {
      if (fb == 0.0 && b < hi) {
        roots.push_back(b);
      }
      continue;
    }
    for (int iteration = 0; iteration < 200; ++iteration) {
      const double m = a + (b - a) / 2.0;
      if (!(m > a && m < b)) {
        break;
      }
      const double fm = p.Value(m);
      if ((fm < 0.0) == (fa < 0.0)) {
        a = m;
        fa = fm;
      } else {
        b = m;
      }
    }
    roots.push_back(a + (b - a) / 2.0);
  }
}

} // namespace

Extremum Maximum(const Poly4& p, double length) noexcept {
  Extremum best{0.0, p.Value(0.0)};
  const auto consider = [&](double x) {
    const double value = p.Value(x);
    if (value > best.value) {
      best = Extremum{x, value};
    }
  };
  consider(length);
  std::vector<double> critical;
  RootsIn(p.Derivative(), 0.0, length, critical);
  for (double x : critical) {
    consider(x);
  }
  return best;
}

std::vector<double> CurvatureSignChanges(const PlanViewSegment& segment) {
  std::vector<double> result;
  if (const Spiral* spiral = std::get_if<Spiral>(&segment.shape)) {
    // kappa0 + rate u = 0, where the end curvatures have opposite signs.
    if ((spiral->curvatureStart < 0.0 && spiral->curvatureEnd > 0.0) ||
        (spiral->curvatureStart > 0.0 && spiral->curvatureEnd < 0.0)) {
      const double u = segment.length * spiral->curvatureStart /
                       (spiral->curvatureStart - spiral->curvatureEnd);
      if (u > 0.0 && u < segment.length) {
        result.push_back(u);
      }
    }
  } else if (const ParamPoly3* c = std::get_if<ParamPoly3>(&segment.shape)) {
    // u'v'' - v'u'' is a quadratic in p: its p^3 terms cancel.
    const Poly4 cross{{2.0 * (c->bU * c->cV - c->bV * c->cU), 6.0 * (c->bU * c->dV - c->bV * c->dU),
        6.0 * (c->cU * c->dV - c->cV * c->dU), 0.0, 0.0}};
    const double range = ParameterRange(*c, segment.length);
    std::vector<double> roots;
    RootsIn(cross, 0.0, range, roots);
    for (double p : roots) {
      // A root where the sign does not change, such as a double root, is
      // not an inflection.
      const double step = range * 1e-9;
      if ((cross.Value(p - step) < 0.0) != (cross.Value(p + step) < 0.0)) {
        result.push_back(p * segment.length / range);
      }
    }
  }
  return result;
}

Poly3 Shift(const CubicRecord& r, double delta) noexcept {
  const double d2 = delta * delta;
  return Poly3{r.a + r.b * delta + r.c * d2 + r.d * d2 * delta, r.b + 2.0 * r.c * delta + 3.0 * r.d * d2,
      r.c + 3.0 * r.d * delta, r.d};
}

double SectionEnd(const Road& road, std::size_t sectionIndex) noexcept {
  return sectionIndex + 1 < road.sections.size() ? road.sections[sectionIndex + 1].s : road.length;
}

std::vector<double> SectionStations(const Road& road, std::size_t sectionIndex) {
  constexpr double tolerance = ValidationTolerances::kStationMetres;
  const LaneSection& section = road.sections[sectionIndex];
  const double start = section.s;
  const double end = SectionEnd(road, sectionIndex);

  std::vector<double> inner;
  const auto add = [&](double s) {
    if (s > start && s < end) {
      inner.push_back(s);
    }
  };
  for (const PlanViewSegment& segment : road.referenceLine.segments) {
    add(segment.s);
    for (double u : CurvatureSignChanges(segment)) {
      add(segment.s + u);
    }
  }
  for (const CubicRecord& r : road.elevation.records) {
    add(r.s0);
  }
  for (const CubicRecord& r : road.laneOffset.records) {
    add(r.s0);
  }
  for (const Lane& lane : section.lanes) {
    for (const CubicRecord& r : lane.width.records) {
      add(start + r.s0);
    }
  }
  std::sort(inner.begin(), inner.end());

  std::vector<double> stations{start};
  for (double s : inner) {
    if (s - stations.back() > tolerance && end - s > tolerance) {
      stations.push_back(s);
    }
  }
  stations.push_back(end);
  return stations;
}

namespace {

Poly3 InForce(const PiecewiseCubic& cubic, double frameOrigin, double s0, double mid) {
  if (cubic.records.empty()) {
    return Poly3{};
  }
  const CubicRecord& r = cubic.records[RecordIndexAt(cubic, mid - frameOrigin)];
  return Shift(r, s0 - frameOrigin - r.s0);
}

} // namespace

Interval MakeInterval(const Road& road, std::size_t sectionIndex, double s0, double s1) {
  const LaneSection& section = road.sections[sectionIndex];
  const double mid = 0.5 * (s0 + s1);

  Interval interval;
  interval.s0 = s0;
  interval.length = s1 - s0;
  interval.segment = SegmentIndexAt(road.referenceLine, mid);
  interval.offset = InForce(road.laneOffset, 0.0, s0, mid);
  interval.elevation = InForce(road.elevation, 0.0, s0, mid);

  const std::size_t count = section.lanes.size();
  interval.widths.resize(count);
  interval.borders.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    if (section.lanes[i].number != 0) {
      interval.widths[i] = InForce(section.lanes[i].width, section.s, s0, mid);
    }
  }
  for (std::size_t i = 0; i < count; ++i) {
    const std::int32_t number = section.lanes[i].number;
    Poly3 border = interval.offset;
    for (std::size_t j = 0; j < count; ++j) {
      const std::int32_t other = section.lanes[j].number;
      if (number > 0 && other > 0 && other <= number) {
        border += interval.widths[j];
      } else if (number < 0 && other < 0 && other >= number) {
        border -= interval.widths[j];
      }
    }
    interval.borders[i] = border;
  }
  return interval;
}

} // namespace detail

} // namespace openstrata::rn
