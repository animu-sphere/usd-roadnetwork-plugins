// SPDX-License-Identifier: Apache-2.0
#include "openstrata/rn/usd/LayerWriter.h"

#include "pxr/usd/sdf/layer.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/sdf/primSpec.h"

#include <cstdio>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

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
  // The writer replaces whatever the target layer held, as SdfFileFormat::Read
  // requires on reload.
  SdfLayerRefPtr layer = SdfLayer::CreateAnonymous("target.usda");
  Check(static_cast<bool>(layer), "anonymous target layer is created");
  if (!layer) {
    return 1;
  }
  SdfPrimSpec::New(layer->GetPseudoRoot(), "Stale", SdfSpecifierDef);
  layer->SetDefaultPrim(TfToken("Stale"));

  std::string error;
  const bool ok = openstrata::rn::usd::WriteLayer(openstrata::rn::RoadNetworkData{}, *layer, &error);
  Check(ok, "WriteLayer succeeds for an empty RoadNetworkData");
  Check(error.empty(), "WriteLayer reports no error on success");
  Check(layer->GetRootPrims().empty(), "the written layer has no root prims");
  Check(!layer->HasDefaultPrim(), "the written layer has no defaultPrim");

  // Determinism: two writes of the same data export identical text.
  SdfLayerRefPtr second = SdfLayer::CreateAnonymous("second.usda");
  Check(openstrata::rn::usd::WriteLayer(openstrata::rn::RoadNetworkData{}, *second, nullptr),
      "WriteLayer accepts a null error pointer");
  std::string first_text;
  std::string second_text;
  layer->ExportToString(&first_text);
  second->ExportToString(&second_text);
  Check(first_text == second_text, "two writes of the same data are identical");

  if (failures == 0) {
    std::printf("roadnetworkUsd: all checks passed\n");
  }
  return failures == 0 ? 0 : 1;
}
