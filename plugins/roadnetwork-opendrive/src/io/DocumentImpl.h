// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "io/Reader.h"

#include "pugixml.hpp"

/// The private state of a parsed Document, shared by the reader and the
/// decoder. Only the bundle's own sources include this header, so pugixml
/// stays out of Reader.h and Decoder.h.
namespace openstrata::rn::odr {

struct Document::Impl {
  pugi::xml_document xml;
  SourceInfo source;
};

/// The `<OpenDRIVE>` element of a parsed document.
inline pugi::xml_node RootOf(const Document::Impl& impl) {
  return impl.xml.document_element();
}

} // namespace openstrata::rn::odr
