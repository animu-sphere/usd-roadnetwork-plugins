// SPDX-License-Identifier: Apache-2.0
#include "openstrata/rn/usd/LayerWriter.h"

#include "pxr/usd/sdf/fileFormat.h"
#include "pxr/usd/sdf/layer.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace openstrata::rn::usd {

bool WriteLayer(const RoadNetworkData& data, SdfLayer& layer, std::string* error) {
  (void)data;

  // Author into a scratch layer first so a failure leaves `layer` untouched.
  SdfFileFormatConstPtr usda = SdfFileFormat::FindByExtension("usda");
  SdfLayerRefPtr generated =
      usda ? SdfLayer::CreateAnonymous("roadnetwork.generated.usda", usda) : SdfLayerRefPtr();
  if (!generated || !generated->ImportFromString("#usda 1.0\n")) {
    if (error) {
      *error = "could not create the scratch USDA layer";
    }
    return false;
  }

  layer.TransferContent(generated);
  return true;
}

} // namespace openstrata::rn::usd
