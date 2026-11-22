// SPDX-License-Identifier: Apache-2.0
#include "RoadnetworkOpendriveFileFormat.h"

#include "io/Decoder.h"
#include "io/Diagnostics.h"
#include "io/Reader.h"

#include "openstrata/rn/RoadNetworkData.h"
#include "openstrata/rn/usd/LayerWriter.h"

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/errorMark.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"
#include "pxr/usd/ar/asset.h"
#include "pxr/usd/ar/resolvedPath.h"
#include "pxr/usd/ar/resolver.h"
#include "pxr/usd/sdf/layer.h"

#include <algorithm>
#include <exception>
#include <memory>
#include <string>
#include <string_view>

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

namespace {

namespace odr = openstrata::rn::odr;

/// Posts the one error a failed Read() reports (ADR 0006 section 3): the
/// bracketed code first, then the asset, then what was found.
void PostReadError(std::string_view code, const std::string& asset, const std::string& detail) {
  TF_RUNTIME_ERROR("%.*s %s: %s", static_cast<int>(code.size()), code.data(), asset.c_str(),
      detail.c_str());
}

/// Reads up to `limit` bytes of the asset at the start, through the resolver
/// so that package and resolver-backed assets behave like files.
bool ReadAssetBytes(const std::string& resolvedPath, std::size_t limit, std::string* bytes) {
  const std::shared_ptr<ArAsset> asset = ArGetResolver().OpenAsset(ArResolvedPath(resolvedPath));
  if (!asset) {
    return false;
  }
  const std::size_t size = std::min(asset->GetSize(), limit);
  bytes->assign(size, '\0');
  return size == 0 || asset->Read(bytes->data(), size, 0) == size;
}

} // namespace

bool RoadnetworkOpendriveFileFormat::CanRead(const std::string& file) const {
  // CanRead() posts no diagnostics (ADR 0006 section 1), including any the
  // resolver posts while opening the asset.
  TfErrorMark mark;
  bool result = false;
  try {
    std::string prefix;
    result = SdfFileFormat::GetFileExtension(file) == RoadnetworkOpendriveFileFormatTokens->Extension &&
             ReadAssetBytes(file, odr::kSniffBytes, &prefix) && odr::LooksLikeOpenDrive(prefix);
  } catch (...) {
    result = false;
  }
  mark.Clear();
  return result;
}

bool RoadnetworkOpendriveFileFormat::Read(
    SdfLayer* layer,
    const std::string& resolvedPath,
    bool metadataOnly) const {
  // A metadata-only read checks the container, root, header, and version,
  // and decodes the header only (ADR 0006 section 2); a full read decodes
  // and validates the whole network.
  try {
    std::string bytes;
    if (!ReadAssetBytes(resolvedPath, std::string::npos, &bytes)) {
      PostReadError(odr::codes::kAssetUnreadable, resolvedPath, "the asset could not be opened or read");
      return false;
    }

    odr::Diagnostic diagnostic;
    odr::ImportResult import;
    if (!odr::ImportNetwork(bytes, metadataOnly, &import, &diagnostic)) {
      PostReadError(diagnostic.code, resolvedPath, diagnostic.detail);
      return false;
    }

    // The shared writer is the only author of the layer. Until the stage
    // contract (USD-01) it authors an empty layer, so a failure there leaves
    // the warnings unposted.
    std::string error;
    if (!openstrata::rn::usd::WriteLayer(import.data, *layer, &error)) {
      TF_RUNTIME_ERROR("Could not author a USD layer for '%s': %s", resolvedPath.c_str(), error.c_str());
      return false;
    }
    // Each warning code once per read (ADR 0006 section 4).
    for (const odr::Warning& warning : import.warnings) {
      TF_WARN("%s %s: %s", warning.code.c_str(), resolvedPath.c_str(), warning.detail.c_str());
    }
    return true;
  } catch (const std::exception& e) {
    PostReadError(odr::codes::kInternal, resolvedPath, std::string("internal failure: ") + e.what());
  } catch (...) {
    PostReadError(odr::codes::kInternal, resolvedPath, "internal failure");
  }
  return false;
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
