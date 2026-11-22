// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pxr/pxr.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/usd/sdf/fileFormat.h"

PXR_NAMESPACE_OPEN_SCOPE

// The tokens that identify this file format to USD's Sdf layer registry.
// clang-format off: TfStaticTokens uses a deliberately column-aligned macro body.
#define ROADNETWORKOPENDRIVE_FILE_FORMAT_TOKENS \
    ((Id, "xodr"))         \
    ((Version, "1.0"))              \
    ((Target, "usd"))              \
    ((Extension, "xodr"))
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(RoadnetworkOpendriveFileFormatTokens, ROADNETWORKOPENDRIVE_FILE_FORMAT_TOKENS);

/// The read-only SdfFileFormat for ASAM OpenDRIVE `.xodr` files (ADR 0006).
/// The OpenDRIVE reader lives in `src/io/` without OpenUSD; this class adds
/// the asset access, the diagnostics, and the shared writer.
class RoadnetworkOpendriveFileFormat : public SdfFileFormat {
public:
  bool CanRead(const std::string& file) const override;
  bool Read(SdfLayer* layer, const std::string& resolvedPath, bool metadataOnly) const override;
  bool WriteToString(
      const SdfLayer& layer,
      std::string* str,
      const std::string& comment = std::string()) const override;

protected:
  SDF_FILE_FORMAT_FACTORY_ACCESS;

  RoadnetworkOpendriveFileFormat();
  ~RoadnetworkOpendriveFileFormat() override;
};

PXR_NAMESPACE_CLOSE_SCOPE
