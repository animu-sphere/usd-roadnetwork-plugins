// SPDX-License-Identifier: Apache-2.0
// The OpenDRIVE reader without OpenUSD (ADR 0006, FMT-01 section 3): the
// CanRead() sniff, each container, root, header, and version failure with its
// ODR- code, the accepted version range, and the header scan.
#include "io/Diagnostics.h"
#include "io/Reader.h"

#include <cstdio>
#include <memory>
#include <string>
#include <string_view>

namespace {

using namespace openstrata::rn;
using namespace openstrata::rn::odr;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
  }
}

std::string WithVersion(const std::string& major, const std::string& minor) {
  return "<OpenDRIVE><header revMajor=\"" + major + "\" revMinor=\"" + minor + "\"/></OpenDRIVE>";
}

/// Expects Parse() to fail with `code`.
void ExpectFailure(const std::string& xml, std::string_view code, const std::string& what) {
  Diagnostic error;
  const std::unique_ptr<Document> document = Document::Parse(xml, &error);
  Check(!document, what + ": parsed, expected " + std::string(code));
  if (!document) {
    Check(error.code == code, what + ": got " + std::string(error.code) + " (" + error.detail +
                                  "), expected " + std::string(code));
    Check(!error.detail.empty(), what + ": no detail");
  }
}

std::unique_ptr<Document> ExpectSuccess(const std::string& xml, const std::string& what) {
  Diagnostic error;
  std::unique_ptr<Document> document = Document::Parse(xml, &error);
  Check(document != nullptr, what + ": " + std::string(error.code) + " " + error.detail);
  return document;
}

void TestSniff() {
  const std::string bom = "\xEF\xBB\xBF";
  const struct {
    std::string text;
    bool expected;
    const char* what;
  } cases[] = {
      {"<OpenDRIVE>", true, "bare root"},
      {"<OpenDRIVE/>", true, "empty root"},
      {"\n\t <OpenDRIVE >", true, "leading whitespace"},
      {bom + "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<OpenDRIVE>", true, "BOM and declaration"},
      {"<?xml version='1.0' encoding='utf-8' standalone='yes'?><OpenDRIVE>", true,
          "single-quoted declaration, lower-case encoding"},
      {"<?xml version=\"1.0\"?><!-- a > b --><?pi data?><OpenDRIVE>", true, "comment and PI"},
      {"<?xml-stylesheet href=\"a.xsl\"?><OpenDRIVE>", true, "xml-stylesheet is not a declaration"},
      {"<!DOCTYPE OpenDRIVE [ <!ENTITY x \"a>b\"> <!-- ] > --> <?p ]?> ]><OpenDRIVE>", true,
          "doctype with an internal subset"},
      {"<!DOCTYPE OpenDRIVE SYSTEM \"odr>.dtd\"><OpenDRIVE>", true, "doctype with a system ID"},
      {"<OpenDRIVE xmlns:odr=\"urn:x\" a='1'>", true, "a prefix declaration is not a namespace"},
      {"", false, "empty"},
      {"hello<OpenDRIVE>", false, "text before the root"},
      {"<OpenSCENARIO>", false, "another root"},
      {"<OpenDRIVEx>", false, "a longer name"},
      {"<odr:OpenDRIVE xmlns:odr=\"urn:x\">", false, "a prefixed root"},
      {"<OpenDRIVE xmlns=\"urn:x\">", false, "a default namespace"},
      {"<OpenDRIVE a=\"1\"b=\"2\">", false, "attributes without whitespace"},
      {"<OpenDRIVE a=\"1", false, "a start tag cut off"},
      {"<!-- unterminated <OpenDRIVE>", false, "an unterminated comment"},
      {"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?><OpenDRIVE>", false, "Latin-1 declared"},
      {"<?xml version=\"1.0\" encoding=\"UTF-16\"?><OpenDRIVE>", false, "UTF-16 declared"},
      {std::string("\xFF\xFE<\0O\0", 6), false, "UTF-16 LE BOM"},
      {std::string("\xFE\xFF\0<\0O", 6), false, "UTF-16 BE BOM"},
      {std::string("<\0O\0", 4), false, "UTF-16 without a BOM"},
  };
  for (const auto& c : cases) {
    Check(LooksLikeOpenDrive(c.text) == c.expected, std::string("sniff: ") + c.what);
  }

  // A comment that pushes the root past the sniff window: CanRead() sees
  // only the first kSniffBytes, so it says no, while the whole document is
  // OpenDRIVE.
  const std::string longComment =
      "<!--" + std::string(kSniffBytes, 'x') + "-->" + WithVersion("1", "8");
  Check(!LooksLikeOpenDrive(std::string_view(longComment).substr(0, kSniffBytes)),
      "sniff: root beyond the window");
  Check(LooksLikeOpenDrive(longComment), "sniff: the same document without the window");
  Check(ExpectSuccess(longComment, "long comment") != nullptr, "parse: long comment");
}

void TestContainer() {
  ExpectFailure("<OpenDRIVE><header revMajor=\"1\" revMinor=\"8\"", codes::kMalformedXml,
      "unterminated start tag");
  ExpectFailure("<OpenDRIVE><header revMajor=\"1\" revMinor=\"8\"/></OpenSCENARIO>",
      codes::kMalformedXml, "mismatched end tag");
  ExpectFailure("", codes::kMalformedXml, "empty document");
  ExpectFailure("<!-- only a comment -->", codes::kMalformedXml, "no element");
  ExpectFailure(WithVersion("1", "8") + "<OpenDRIVE/>", codes::kMalformedXml, "two root elements");
  ExpectFailure(WithVersion("1", "8") + "trailing", codes::kMalformedXml, "text after the root");

  ExpectFailure("<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>" + WithVersion("1", "8"),
      codes::kNotUtf8, "Latin-1 declared");
  ExpectFailure(std::string("\xFF\xFE<\0O\0", 6), codes::kNotUtf8, "UTF-16 BOM");
  ExpectFailure(std::string("<\0O\0", 4), codes::kNotUtf8, "UTF-16 without a BOM");
  Check(ExpectSuccess("\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"UTF-8\"?>" + WithVersion("1", "8"),
            "UTF-8 BOM") != nullptr,
      "parse: UTF-8 BOM");

  ExpectFailure("<OpenSCENARIO><header revMajor=\"1\" revMinor=\"8\"/></OpenSCENARIO>",
      codes::kWrongRoot, "another root");
  ExpectFailure("<odr:OpenDRIVE xmlns:odr=\"urn:x\"><header revMajor=\"1\" revMinor=\"8\"/></odr:OpenDRIVE>",
      codes::kWrongRoot, "a prefixed root");
  ExpectFailure("<OpenDRIVE xmlns=\"urn:x\"><header revMajor=\"1\" revMinor=\"8\"/></OpenDRIVE>",
      codes::kWrongRoot, "a default namespace");

  // An entity declared in the DOCTYPE is not expanded (FMT-01 section 6).
  const std::unique_ptr<Document> entity = ExpectSuccess(
      "<!DOCTYPE OpenDRIVE [<!ENTITY big \"expanded\">]><OpenDRIVE><header revMajor=\"1\" "
      "revMinor=\"8\" name=\"&big;\"/></OpenDRIVE>",
      "doctype entity");
  if (entity) {
    Check(entity->Source().name == "&big;", "a DOCTYPE entity was expanded: " + entity->Source().name);
  }
}

void TestHeader() {
  ExpectFailure("<OpenDRIVE/>", codes::kMissingHeader, "no children");
  ExpectFailure("<OpenDRIVE><!-- c --> text </OpenDRIVE>", codes::kMissingHeader, "no child element");
  ExpectFailure("<OpenDRIVE><road/><header revMajor=\"1\" revMinor=\"8\"/></OpenDRIVE>",
      codes::kMissingHeader, "header not first");

  ExpectFailure("<OpenDRIVE><header revMinor=\"8\"/></OpenDRIVE>", codes::kMalformedVersion,
      "no revMajor");
  ExpectFailure("<OpenDRIVE><header revMajor=\"1\"/></OpenDRIVE>", codes::kMalformedVersion,
      "no revMinor");
  for (const char* minor : {"", "8.0", "-1", "+8", "eight", "1e1", "0x8", "1234567890"}) {
    ExpectFailure(WithVersion("1", minor), codes::kMalformedVersion,
        std::string("revMinor=\"") + minor + "\"");
  }

  for (const auto& version : {std::pair<const char*, const char*>{"1", "3"}, {"1", "10"},
           {"1", "0"}, {"0", "8"}, {"2", "0"}}) {
    ExpectFailure(WithVersion(version.first, version.second), codes::kUnsupportedVersion,
        std::string("version ") + version.first + "." + version.second);
  }
  for (int minor = kMinVersionMinor; minor <= kMaxVersionMinor; ++minor) {
    const std::unique_ptr<Document> document =
        ExpectSuccess(WithVersion("1", std::to_string(minor)), "1." + std::to_string(minor));
    if (document) {
      Check(document->Source().versionMajor == 1 && document->Source().versionMinor == minor,
          "version 1." + std::to_string(minor) + " recorded");
    }
  }
  Check(ExpectSuccess(WithVersion(" 1 ", "\t08\n"), "whitespace and a leading zero") != nullptr,
      "parse: whitespace and a leading zero");
}

void TestHeaderScan() {
  const std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<OpenDRIVE>
  <header revMajor="1" revMinor="4" name="a &amp; b" version="1.00" date="Tue Jun 30 14:55:47 2020" vendor="hand">
    <geoReference><![CDATA[+proj=tmerc]]></geoReference>
  </header>
  <road id="1" length="10" junction="-1">
    <lanes>
      <laneSection s="0">
        <left><lane id="2"/><lane id="1"/></left>
        <center><lane id="0"/></center>
        <right><lane id="-1"/></right>
      </laneSection>
      <laneSection s="5">
        <center><lane id="0"/></center>
        <right><lane id="-1"/></right>
      </laneSection>
    </lanes>
  </road>
  <road id="2" length="5" junction="7"/>
  <junction id="7"/>
  <controller id="c"/>
</OpenDRIVE>
)";
  const std::unique_ptr<Document> document = ExpectSuccess(xml, "header scan");
  if (!document) {
    return;
  }
  const SourceInfo& source = document->Source();
  Check(source.format == "opendrive", "format");
  Check(source.versionMajor == 1 && source.versionMinor == 4, "version");
  Check(source.name == "a & b", "name is the attribute value: " + source.name);
  Check(source.version == "1.00", "the network version is a string");
  Check(source.date == "Tue Jun 30 14:55:47 2020", "date is opaque");
  Check(source.vendor == "hand", "vendor");
  Check(!source.geoReference && !source.offset, "the georeference is left to the decoder");

  const ElementCounts counts = document->CountElements();
  Check(counts.roads == 2, "road count " + std::to_string(counts.roads));
  Check(counts.junctions == 1, "junction count " + std::to_string(counts.junctions));
  Check(counts.lanes == 6, "lane count " + std::to_string(counts.lanes));

  const std::unique_ptr<Document> bare = ExpectSuccess(WithVersion("1", "8"), "bare header");
  if (bare) {
    Check(bare->Source().name.empty() && bare->Source().vendor.empty(), "absent attributes are empty");
    const ElementCounts none = bare->CountElements();
    Check(none.roads == 0 && none.junctions == 0 && none.lanes == 0, "empty counts");
  }
}

} // namespace

int main() {
  TestSniff();
  TestContainer();
  TestHeader();
  TestHeaderScan();
  if (failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  std::printf("roadnetwork-opendrive reader: all checks passed\n");
  return 0;
}
