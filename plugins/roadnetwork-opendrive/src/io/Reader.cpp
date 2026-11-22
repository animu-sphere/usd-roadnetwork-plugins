// SPDX-License-Identifier: Apache-2.0
#include "io/Reader.h"

#include "pugixml.hpp"

#include <cstring>
#include <optional>
#include <string>
#include <utility>

namespace openstrata::rn::odr {

namespace {

constexpr std::string_view kUtf8Bom = "\xEF\xBB\xBF";
constexpr std::string_view kRootName = "OpenDRIVE";

bool IsXmlSpace(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool EqualsIgnoreAsciiCase(std::string_view a, std::string_view b) noexcept {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    char x = a[i];
    char y = b[i];
    if (x >= 'A' && x <= 'Z') {
      x = static_cast<char>(x - 'A' + 'a');
    }
    if (y >= 'A' && y <= 'Z') {
      y = static_cast<char>(y - 'A' + 'a');
    }
    if (x != y) {
      return false;
    }
  }
  return true;
}

/// A forward-only scanner over the document prologue. It reads only what
/// CanRead() and the encoding check need, and treats anything it does not
/// understand, or a construct that does not end within the bytes, as a
/// failure.
class Cursor {
public:
  explicit Cursor(std::string_view text) noexcept : text_(text) {}

  bool AtEnd() const noexcept { return pos_ >= text_.size(); }
  std::size_t Pos() const noexcept { return pos_; }
  char Peek() const noexcept { return AtEnd() ? '\0' : text_[pos_]; }

  bool StartsWith(std::string_view s) const noexcept {
    return text_.substr(pos_, s.size()) == s;
  }

  void Advance(std::size_t n) noexcept { pos_ = pos_ + n > text_.size() ? text_.size() : pos_ + n; }

  void SkipSpace() noexcept {
    while (!AtEnd() && IsXmlSpace(text_[pos_])) {
      ++pos_;
    }
  }

  /// Moves past the next `terminator`; false if it does not occur.
  bool SkipPast(std::string_view terminator) noexcept {
    const std::size_t found = text_.find(terminator, pos_);
    if (found == std::string_view::npos) {
      return false;
    }
    pos_ = found + terminator.size();
    return true;
  }

  /// A name: everything up to whitespace, `=`, `/`, `>`, or `?`.
  std::string_view Name() noexcept {
    const std::size_t start = pos_;
    while (!AtEnd()) {
      const char c = text_[pos_];
      if (IsXmlSpace(c) || c == '=' || c == '/' || c == '>' || c == '?') {
        break;
      }
      ++pos_;
    }
    return text_.substr(start, pos_ - start);
  }

  /// `name = "value"` or `name = 'value'`, after optional whitespace.
  bool Attribute(std::string_view* name, std::string_view* value) noexcept {
    *name = Name();
    if (name->empty()) {
      return false;
    }
    SkipSpace();
    if (Peek() != '=') {
      return false;
    }
    Advance(1);
    SkipSpace();
    const char quote = Peek();
    if (quote != '"' && quote != '\'') {
      return false;
    }
    Advance(1);
    const std::size_t start = pos_;
    const std::size_t end = text_.find(quote, pos_);
    if (end == std::string_view::npos) {
      return false;
    }
    *value = text_.substr(start, end - start);
    pos_ = end + 1;
    return true;
  }

  /// Skips a document type declaration from `<!DOCTYPE`, including an
  /// internal subset whose declarations may hold quoted strings, comments,
  /// and processing instructions.
  bool SkipDoctype() noexcept {
    Advance(std::strlen("<!DOCTYPE"));
    int depth = 0;
    while (!AtEnd()) {
      if (depth > 0 && StartsWith("<!--")) {
        Advance(4);
        if (!SkipPast("-->")) {
          return false;
        }
        continue;
      }
      if (depth > 0 && StartsWith("<?")) {
        Advance(2);
        if (!SkipPast("?>")) {
          return false;
        }
        continue;
      }
      const char c = Peek();
      Advance(1);
      if (c == '"' || c == '\'') {
        const std::size_t end = text_.find(c, pos_);
        if (end == std::string_view::npos) {
          return false;
        }
        pos_ = end + 1;
      } else if (c == '[') {
        ++depth;
      } else if (c == ']') {
        if (--depth < 0) {
          return false;
        }
      } else if (c == '>' && depth == 0) {
        return true;
      }
    }
    return false;
  }

private:
  std::string_view text_;
  std::size_t pos_ = 0;
};

enum class EncodingCheck { Utf8, NotUtf8, Unknown };

/// Reads the encoding from the byte-order mark, the first bytes, and the XML
/// declaration. On Utf8, `*body` is positioned after the BOM and the
/// declaration. Unknown means the declaration does not end within the bytes
/// or cannot be read.
EncodingCheck CheckEncoding(std::string_view bytes, std::string* declared, Cursor* body) {
  // UTF-32 and UTF-16 byte-order marks, then a UTF-16 '<' without one.
  if (bytes.substr(0, 4) == std::string_view("\x00\x00\xFE\xFF", 4) ||
      bytes.substr(0, 4) == std::string_view("\xFF\xFE\x00\x00", 4) ||
      bytes.substr(0, 2) == "\xFE\xFF" || bytes.substr(0, 2) == "\xFF\xFE") {
    *declared = "a UTF-16 or UTF-32 byte-order mark";
    return EncodingCheck::NotUtf8;
  }
  if ((!bytes.empty() && bytes[0] == '\0') || (bytes.size() > 1 && bytes[1] == '\0')) {
    *declared = "UTF-16 or UTF-32 content without a byte-order mark";
    return EncodingCheck::NotUtf8;
  }

  Cursor cursor(bytes);
  if (cursor.StartsWith(kUtf8Bom)) {
    cursor.Advance(kUtf8Bom.size());
  }
  // The XML declaration is `<?xml` followed by whitespace; `<?xml-stylesheet`
  // is an ordinary processing instruction.
  if (cursor.StartsWith("<?xml")) {
    Cursor probe = cursor;
    probe.Advance(5);
    if (IsXmlSpace(probe.Peek())) {
      while (true) {
        probe.SkipSpace();
        if (probe.StartsWith("?>")) {
          probe.Advance(2);
          break;
        }
        std::string_view name;
        std::string_view value;
        if (!probe.Attribute(&name, &value)) {
          return EncodingCheck::Unknown;
        }
        if (name == "encoding" && !EqualsIgnoreAsciiCase(value, "UTF-8") &&
            !EqualsIgnoreAsciiCase(value, "UTF8")) {
          *declared = "encoding=\"" + std::string(value) + "\"";
          return EncodingCheck::NotUtf8;
        }
      }
      cursor = probe;
    }
  }
  *body = cursor;
  return EncodingCheck::Utf8;
}

std::string LineAndColumn(std::string_view bytes, std::ptrdiff_t offset) {
  std::size_t end = offset < 0 ? 0 : static_cast<std::size_t>(offset);
  if (end > bytes.size()) {
    end = bytes.size();
  }
  std::size_t line = 1;
  std::size_t column = 1;
  for (std::size_t i = 0; i < end; ++i) {
    if (bytes[i] == '\n') {
      ++line;
      column = 1;
    } else {
      ++column;
    }
  }
  return "line " + std::to_string(line) + ", column " + std::to_string(column);
}

/// A non-negative decimal integer, as the header states it: ASCII digits
/// with optional surrounding XML whitespace.
std::optional<int> ParseRevision(const char* text) {
  std::string_view s(text);
  while (!s.empty() && IsXmlSpace(s.front())) {
    s.remove_prefix(1);
  }
  while (!s.empty() && IsXmlSpace(s.back())) {
    s.remove_suffix(1);
  }
  if (s.empty() || s.size() > 9) {
    return std::nullopt;
  }
  int value = 0;
  for (const char c : s) {
    if (c < '0' || c > '9') {
      return std::nullopt;
    }
    value = value * 10 + (c - '0');
  }
  return value;
}

std::uint32_t CountChildren(const pugi::xml_node& node, const char* name) {
  std::uint32_t count = 0;
  for (pugi::xml_node child = node.child(name); child; child = child.next_sibling(name)) {
    ++count;
  }
  return count;
}

Diagnostic Fail(std::string_view code, std::string detail) {
  return Diagnostic{code, std::move(detail)};
}

} // namespace

bool LooksLikeOpenDrive(std::string_view prefix) noexcept {
  std::string declared;
  Cursor cursor(prefix);
  if (CheckEncoding(prefix, &declared, &cursor) != EncodingCheck::Utf8) {
    return false;
  }
  while (true) {
    cursor.SkipSpace();
    if (cursor.Peek() != '<') {
      return false;
    }
    if (cursor.StartsWith("<?")) {
      cursor.Advance(2);
      if (!cursor.SkipPast("?>")) {
        return false;
      }
    } else if (cursor.StartsWith("<!--")) {
      cursor.Advance(4);
      if (!cursor.SkipPast("-->")) {
        return false;
      }
    } else if (cursor.StartsWith("<!DOCTYPE")) {
      if (!cursor.SkipDoctype()) {
        return false;
      }
    } else {
      break;
    }
  }

  // The first element: its name, then its attributes up to the end of the
  // start tag, looking for a default namespace declaration.
  cursor.Advance(1);
  if (cursor.Name() != kRootName) {
    return false;
  }
  while (true) {
    const std::size_t before = cursor.Pos();
    cursor.SkipSpace();
    if (cursor.StartsWith(">") || cursor.StartsWith("/>")) {
      return true;
    }
    if (cursor.Pos() == before) {
      return false; // An attribute must follow whitespace.
    }
    std::string_view name;
    std::string_view value;
    if (!cursor.Attribute(&name, &value) || name == "xmlns") {
      return false;
    }
  }
}

struct Document::Impl {
  pugi::xml_document xml;
  SourceInfo source;
};

Document::Document(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
Document::~Document() = default;

const SourceInfo& Document::Source() const noexcept {
  return impl_->source;
}

std::unique_ptr<Document> Document::Parse(std::string_view bytes, Diagnostic* error) {
  Diagnostic ignored;
  Diagnostic& out = error ? *error : ignored;

  std::string declared;
  Cursor body(bytes);
  if (CheckEncoding(bytes, &declared, &body) == EncodingCheck::NotUtf8) {
    out = Fail(codes::kNotUtf8, "the document is not UTF-8 (" + declared + ")");
    return nullptr;
  }

  // parse_fragment keeps top-level text and allows any number of top-level
  // elements, so that both can be rejected below; without it pugixml drops
  // the text and accepts several elements anyway.
  auto impl = std::make_unique<Impl>();
  const pugi::xml_parse_result parsed = impl->xml.load_buffer(bytes.data(), bytes.size(),
      pugi::parse_default | pugi::parse_fragment, pugi::encoding_utf8);
  if (!parsed) {
    out = Fail(codes::kMalformedXml, std::string("the document is not well-formed XML: ") +
                                         parsed.description() + " at " +
                                         LineAndColumn(bytes, parsed.offset));
    return nullptr;
  }
  pugi::xml_node root;
  for (pugi::xml_node node : impl->xml.children()) {
    if (node.type() == pugi::node_element) {
      if (root) {
        out = Fail(codes::kMalformedXml, std::string("the document is not well-formed XML: a second "
                                                     "top-level element <") +
                                             node.name() + "> follows <" + root.name() + ">");
        return nullptr;
      }
      root = node;
    } else if (node.type() == pugi::node_pcdata || node.type() == pugi::node_cdata) {
      out = Fail(codes::kMalformedXml,
          "the document is not well-formed XML: text outside the root element");
      return nullptr;
    }
  }

  if (!root) {
    out = Fail(codes::kMalformedXml, "the document is not well-formed XML: it has no element");
    return nullptr;
  }
  if (std::string_view(root.name()) != kRootName) {
    out = Fail(codes::kWrongRoot,
        std::string("the root element is <") + root.name() + ">, not <OpenDRIVE>");
    return nullptr;
  }
  if (pugi::xml_attribute ns = root.attribute("xmlns")) {
    out = Fail(codes::kWrongRoot, std::string("the root element <OpenDRIVE> is in the namespace \"") +
                                      ns.value() + "\"; OpenDRIVE defines none");
    return nullptr;
  }

  const pugi::xml_node header = root.find_child(
      [](const pugi::xml_node& node) { return node.type() == pugi::node_element; });
  if (std::string_view(header.name()) != "header") {
    out = Fail(codes::kMissingHeader,
        header ? std::string("the first child of <OpenDRIVE> is <") + header.name() +
                     ">, not <header>"
               : std::string("<OpenDRIVE> has no <header>"));
    return nullptr;
  }

  int revision[2] = {0, 0};
  const char* const names[2] = {"revMajor", "revMinor"};
  for (int i = 0; i < 2; ++i) {
    const pugi::xml_attribute attribute = header.attribute(names[i]);
    if (!attribute) {
      out = Fail(codes::kMalformedVersion, std::string("<header> has no ") + names[i]);
      return nullptr;
    }
    const std::optional<int> value = ParseRevision(attribute.value());
    if (!value) {
      out = Fail(codes::kMalformedVersion, std::string("<header ") + names[i] + "=\"" +
                                               attribute.value() +
                                               "\"> is not a non-negative integer");
      return nullptr;
    }
    revision[i] = *value;
  }
  if (revision[0] != kVersionMajor || revision[1] < kMinVersionMinor ||
      revision[1] > kMaxVersionMinor) {
    out = Fail(codes::kUnsupportedVersion,
        "OpenDRIVE " + std::to_string(revision[0]) + "." + std::to_string(revision[1]) +
            " is not supported; this reader accepts 1." + std::to_string(kMinVersionMinor) +
            " to 1." + std::to_string(kMaxVersionMinor));
    return nullptr;
  }

  SourceInfo& source = impl->source;
  source.format = "opendrive";
  source.versionMajor = revision[0];
  source.versionMinor = revision[1];
  source.name = header.attribute("name").value();
  source.date = header.attribute("date").value();
  source.vendor = header.attribute("vendor").value();
  source.version = header.attribute("version").value();

  return std::unique_ptr<Document>(new Document(std::move(impl)));
}

ElementCounts Document::CountElements() const {
  ElementCounts counts;
  const pugi::xml_node root = impl_->xml.document_element();
  counts.roads = CountChildren(root, "road");
  counts.junctions = CountChildren(root, "junction");
  for (pugi::xml_node road = root.child("road"); road; road = road.next_sibling("road")) {
    const pugi::xml_node lanes = road.child("lanes");
    for (pugi::xml_node section = lanes.child("laneSection"); section;
         section = section.next_sibling("laneSection")) {
      for (const char* group : {"left", "center", "right"}) {
        for (pugi::xml_node side = section.child(group); side; side = side.next_sibling(group)) {
          counts.lanes += CountChildren(side, "lane");
        }
      }
    }
  }
  return counts;
}

} // namespace openstrata::rn::odr
