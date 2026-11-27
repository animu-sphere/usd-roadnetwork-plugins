// SPDX-License-Identifier: Apache-2.0
#pragma once

// Private to roadnetwork-core: a lane section cut at its ADR 0004 section 3
// stations. Between two stations one plan-view segment and one record of each
// profile are in force, so the lateral position of every border is a single
// cubic in `x = s - s_start`. Validation uses it for the fold check of
// ADR 0004 section 2, and the tessellator for the error bound of section 3.

#include "openstrata/rn/RoadNetworkData.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace openstrata::rn::detail {

/// `c0 + c1 x + c2 x^2 + c3 x^3`.
struct Poly3 {
  double c0 = 0.0;
  double c1 = 0.0;
  double c2 = 0.0;
  double c3 = 0.0;

  double Value(double x) const noexcept {
    return c0 + x * (c1 + x * (c2 + x * c3));
  }
  double Second(double x) const noexcept {
    return 2.0 * c2 + 6.0 * c3 * x;
  }
  /// The largest |f''| on [0, length]. f'' is linear, so it is at an end.
  double MaxAbsSecond(double length) const noexcept;

  Poly3& operator+=(const Poly3& other) noexcept;
  Poly3& operator-=(const Poly3& other) noexcept;
};

struct Extremum {
  double x = 0.0;
  double value = 0.0;
};

/// The largest value of `p` on [0, length], and where it is reached.
Extremum Maximum(const Poly3& p, double length) noexcept;

/// `c[0] + c[1] x + ... + c[4] x^4`.
struct Poly4 {
  double c[5] = {};

  double Value(double x) const noexcept;
  Poly4 Derivative() const noexcept;
};

/// `p(x) * (a + b x)`.
Poly4 Product(const Poly3& p, double a, double b) noexcept;

/// The largest value of `p` on [0, length], and where it is reached; the
/// critical points are found by bisection between those of the derivative.
Extremum Maximum(const Poly4& p, double length) noexcept;

/// The distances from the segment start, ascending and inside the segment,
/// where its curvature changes sign: the zero of a spiral's curvature, and
/// the inflections of a ParamPoly3, where `u'v'' - v'u''` changes sign
/// (ADR 0010 section 4). Empty for lines and arcs.
std::vector<double> CurvatureSignChanges(const PlanViewSegment& segment);

/// The record `r` as a polynomial in `x`, where the record's own variable is
/// `d = x + delta`.
Poly3 Shift(const CubicRecord& r, double delta) noexcept;

/// The end of section `sectionIndex`: the start of the next section, or the
/// road length.
double SectionEnd(const Road& road, std::size_t sectionIndex) noexcept;

/// The stations of a section before subdivision, ascending, from its start to
/// its end. Stations closer than the 1e-6 m station tolerance to the previous
/// one, or to the section end, are merged.
std::vector<double> SectionStations(const Road& road, std::size_t sectionIndex);

struct Interval {
  double s0 = 0.0; ///< Start along the road.
  double length = 0.0;
  std::size_t segment = 0; ///< The plan-view segment in force.
  Poly3 offset;            ///< The lane offset, which is the center border.
  Poly3 elevation;
  std::vector<Poly3> widths;  ///< One per lane, in section lane order; 0 for the center.
  std::vector<Poly3> borders; ///< The outer border of each lane, in section lane order.
};

/// The polynomials in force on [s0, s1] of section `sectionIndex`. The
/// records are chosen at the interval midpoint, so rounding at a station
/// cannot select the neighbouring record.
Interval MakeInterval(const Road& road, std::size_t sectionIndex, double s0, double s1);

/// The fewest equal parts of `interval` for which every part meets the
/// error bound of ADR 0004 section 3 and ADR 0010 section 4 within
/// `tolerance`. The tessellator samples at the part ends, and validation
/// checks a ParamPoly3 fold at the part ends and midpoints.
std::size_t PartsFor(const Road& road, const Interval& interval, double tolerance);

} // namespace openstrata::rn::detail
