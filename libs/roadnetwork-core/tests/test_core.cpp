// SPDX-License-Identifier: Apache-2.0
#include "openstrata/rn/RoadNetworkData.h"
#include "openstrata/rn/Version.h"

#include <cstdio>
#include <cstring>
#include <type_traits>

namespace {

int failures = 0;

void Check(bool condition, const char* what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
  }
}

} // namespace

int main() {
  Check(std::strcmp(openstrata::rn::CoreVersion(), EXPECTED_CORE_VERSION) == 0,
      "CoreVersion() matches the CMake project version");

  // Decoders and the writer pass RoadNetworkData by value and by reference;
  // keep it a regular, default-constructible value type.
  Check(std::is_default_constructible_v<openstrata::rn::RoadNetworkData>,
      "RoadNetworkData is default-constructible");
  Check(std::is_copy_constructible_v<openstrata::rn::RoadNetworkData>,
      "RoadNetworkData is copy-constructible");

  if (failures == 0) {
    std::printf("roadnetworkCore: all checks passed\n");
  }
  return failures == 0 ? 0 : 1;
}
