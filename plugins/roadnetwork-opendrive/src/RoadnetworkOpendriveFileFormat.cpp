// SPDX-License-Identifier: Apache-2.0
#include "RoadnetworkOpendriveFileFormat.h"

#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"
#include "pxr/usd/sdf/layer.h"

#include <fstream>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(RoadnetworkOpendriveFileFormatTokens, ROADNETWORKOPENDRIVE_FILE_FORMAT_TOKENS);

// Register the format with USD's type system so the plug system can find it.
TF_REGISTRY_FUNCTION(TfType) {
  SDF_DEFINE_FILE_FORMAT(RoadnetworkOpendriveFileFormat, SdfFileFormat);
}

RoadnetworkOpendriveFileFormat::RoadnetworkOpendriveFileFormat()
    : SdfFileFormat(
          RoadnetworkOpendriveFileFormatTokens->Id,
          RoadnetworkOpendriveFileFormatTokens->Version,
          RoadnetworkOpendriveFileFormatTokens->Target,
          RoadnetworkOpendriveFileFormatTokens->Extension) {
}

RoadnetworkOpendriveFileFormat::~RoadnetworkOpendriveFileFormat() = default;

bool RoadnetworkOpendriveFileFormat::CanRead(const std::string& file) const {
  return SdfFileFormat::GetFileExtension(file) == "xodr";
}

bool RoadnetworkOpendriveFileFormat::Read(
    SdfLayer* layer,
    const std::string& resolvedPath,
    bool metadataOnly) const {
  (void)metadataOnly;

  std::ifstream in(resolvedPath, std::ios::binary);
  if (!in) {
    TF_RUNTIME_ERROR("Could not open '%s'", resolvedPath.c_str());
    return false;
  }

  // TODO: parse `in` and author USD. As a starting point we emit an empty
  // USDA layer so a `.xodr` file opens as a valid (if empty) stage.
  SdfFileFormatConstPtr usda = SdfFileFormat::FindByExtension("usda");
  SdfLayerRefPtr generated = SdfLayer::CreateAnonymous("roadnetwork-opendrive.generated.usda", usda);
  if (!generated || !generated->ImportFromString("#usda 1.0\n")) {
    TF_RUNTIME_ERROR("Could not synthesize a USD layer for '%s'", resolvedPath.c_str());
    return false;
  }

  layer->TransferContent(generated);
  return true;
}

bool RoadnetworkOpendriveFileFormat::WriteToString(
    const SdfLayer& layer,
    std::string* str,
    const std::string& comment) const {
  SdfFileFormatConstPtr usda = SdfFileFormat::FindByExtension("usda");
  if (usda) {
    return usda->WriteToString(layer, str, comment);
  }
  return layer.ExportToString(str);
}

PXR_NAMESPACE_CLOSE_SCOPE
