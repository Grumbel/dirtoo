// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/filter/parser.hpp"

#include "dirtoo/filter/predicates.hpp"

#include <cctype>

namespace dirtoo::filter {
namespace {

bool is_glob_pattern(std::string_view s)
{
  return s.find('*') != std::string_view::npos || s.find('?') != std::string_view::npos;
}

class Parser {
public:
  explicit Parser(std::string_view in)
      : in_(in)
  {
  }

  std::expected<MatchFuncPtr, ParseError> parse()
  {
    skip_ws();
    if (eof()) {
      return std::make_shared<AlwaysTrue>();
    }
    auto result = parse_or();
    if (!result) {
      return result;
    }
    skip_ws();
    if (!eof()) {
      return std::unexpected(ParseError{"unexpected input", pos_});
    }
    return result;
  }

private:
  std::string_view in_;
  std::size_t pos_ = 0;

  [[nodiscard]] bool eof() const { return pos_ >= in_.size(); }
  [[nodiscard]] char peek() const { return eof() ? '\0' : in_[pos_]; }
  char get() { return eof() ? '\0' : in_[pos_++]; }

  void skip_ws()
  {
    while (!eof() && std::isspace(static_cast<unsigned char>(peek()))) {
      ++pos_;
    }
  }

  bool match_keyword(std::string_view kw)
  {
    skip_ws();
    if (in_.size() - pos_ < kw.size()) {
      return false;
    }
    for (std::size_t i = 0; i < kw.size(); ++i) {
      const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(in_[pos_ + i])));
      const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(kw[i])));
      if (a != b) {
        return false;
      }
    }
    // Keyword boundary
    const std::size_t after = pos_ + kw.size();
    if (after < in_.size()) {
      const char c = in_[after];
      if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == ':') {
        return false;
      }
    }
    pos_ = after;
    return true;
  }

  std::expected<MatchFuncPtr, ParseError> parse_or()
  {
    auto left = parse_and();
    if (!left) {
      return left;
    }
    std::vector<MatchFuncPtr> parts;
    parts.push_back(*left);
    while (true) {
      skip_ws();
      if (!match_keyword("or")) {
        break;
      }
      auto right = parse_and();
      if (!right) {
        return right;
      }
      parts.push_back(*right);
    }
    if (parts.size() == 1) {
      return parts.front();
    }
    return std::make_shared<OrMatch>(std::move(parts));
  }

  std::expected<MatchFuncPtr, ParseError> parse_and()
  {
    auto left = parse_unary();
    if (!left) {
      return left;
    }
    std::vector<MatchFuncPtr> parts;
    parts.push_back(*left);
    while (true) {
      skip_ws();
      if (eof() || peek() == ')') {
        break;
      }
      // Stop before OR at this level
      const std::size_t save = pos_;
      if (match_keyword("or")) {
        pos_ = save;
        break;
      }
      // Optional AND keyword
      match_keyword("and");
      skip_ws();
      if (eof() || peek() == ')') {
        break;
      }
      // Another OR check after optional and
      const std::size_t save2 = pos_;
      if (match_keyword("or")) {
        pos_ = save2;
        break;
      }
      auto right = parse_unary();
      if (!right) {
        return right;
      }
      parts.push_back(*right);
    }
    if (parts.size() == 1) {
      return parts.front();
    }
    return std::make_shared<AndMatch>(std::move(parts));
  }

  std::expected<MatchFuncPtr, ParseError> parse_unary()
  {
    skip_ws();
    if (peek() == '-' || peek() == '^') {
      get();
      auto inner = parse_unary();
      if (!inner) {
        return inner;
      }
      return std::make_shared<NotMatch>(*inner);
    }
    if (match_keyword("not")) {
      auto inner = parse_unary();
      if (!inner) {
        return inner;
      }
      return std::make_shared<NotMatch>(*inner);
    }
    return parse_primary();
  }

  std::expected<MatchFuncPtr, ParseError> parse_primary()
  {
    skip_ws();
    if (peek() == '(') {
      get();
      auto inner = parse_or();
      if (!inner) {
        return inner;
      }
      skip_ws();
      if (peek() != ')') {
        return std::unexpected(ParseError{"expected ')'", pos_});
      }
      get();
      return inner;
    }

    if (peek() == '"' || peek() == '\'') {
      auto str = parse_quoted();
      if (!str) {
        return std::unexpected(str.error());
      }
      return word_to_match(*str);
    }

    // command or word
    auto atom = parse_atom();
    if (!atom) {
      return std::unexpected(atom.error());
    }
    return atom;
  }

  std::expected<std::string, ParseError> parse_quoted()
  {
    const char quote = get();
    std::string out;
    while (!eof()) {
      const char c = get();
      if (c == quote) {
        return out;
      }
      if (c == '\\' && !eof()) {
        out.push_back(get());
      } else {
        out.push_back(c);
      }
    }
    return std::unexpected(ParseError{"unterminated string", pos_});
  }

  std::expected<MatchFuncPtr, ParseError> parse_atom()
  {
    // Read until whitespace or special paren, but allow ':' inside command
    skip_ws();
    if (eof()) {
      return std::unexpected(ParseError{"expected term", pos_});
    }
    std::string token;
    while (!eof()) {
      const char c = peek();
      if (std::isspace(static_cast<unsigned char>(c)) || c == '(' || c == ')') {
        break;
      }
      // Stop at AND/OR handled by higher level — they are separate tokens only if
      // separated by spaces, so we read continuous token here.
      if (c == '\\' && pos_ + 1 < in_.size()) {
        ++pos_;
        token.push_back(get());
        continue;
      }
      token.push_back(get());
    }
    if (token.empty()) {
      return std::unexpected(ParseError{"empty token", pos_});
    }

    // Reject pure keywords if they leaked
    const auto lower = [&] {
      std::string s = token;
      for (char& ch : s) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      }
      return s;
    }();
    if (lower == "and" || lower == "or" || lower == "not") {
      return std::unexpected(ParseError{"unexpected keyword '" + token + "'", pos_});
    }

    const auto colon = token.find(':');
    if (colon != std::string::npos && colon > 0) {
      const std::string cmd = token.substr(0, colon);
      const std::string arg = token.substr(colon + 1);
      return command_to_match(cmd, arg);
    }
    return word_to_match(token);
  }

  MatchFuncPtr word_to_match(const std::string& word)
  {
    if (is_glob_pattern(word)) {
      return make_glob(word, false);
    }
    // Implicit *word* substring via glob
    return make_glob("*" + word + "*", false);
  }

  MatchFuncPtr command_to_match(const std::string& cmd, const std::string& arg)
  {
    std::string c = cmd;
    for (char& ch : c) {
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    // Case-sensitive forms (capital command names) must be checked before the
    // lowercased aliases; otherwise e.g. "Contains" lowers to "contains" and
    // always takes the insensitive branch.
    if (cmd == "Glob" || cmd == "G") {
      return make_glob(arg, true);
    }
    if (c == "glob" || c == "g") {
      return make_glob(arg, false);
    }
    if (cmd == "Regex" || cmd == "Re" || cmd == "R" || cmd == "Rx") {
      return make_regex(arg, true);
    }
    if (c == "regex" || c == "re" || c == "r" || c == "rx") {
      return make_regex(arg, false);
    }
    if (c == "tag") {
      return make_tag(arg);
    }
    if (c == "tagged") {
      return make_tagged(arg);
    }
    if (c == "size") {
      return make_size(arg);
    }
    if (c == "type" || c == "t") {
      return make_type(arg);
    }
    if (c == "width" || c == "w") {
      return make_width(arg);
    }
    if (c == "height" || c == "h") {
      return make_height(arg);
    }
    if (c == "aspect" || c == "ar" || c == "ratio") {
      return make_aspect(arg);
    }
    if (c == "duration" || c == "dur") {
      return make_duration(arg);
    }
    if (c == "framerate" || c == "fps" || c == "fr") {
      return make_framerate(arg);
    }
    if (cmd == "Fuzzy" || cmd == "Fuz" || cmd == "Fuzz" || cmd == "F") {
      return make_fuzzy(arg, true);
    }
    if (c == "fuzzy" || c == "fuz" || c == "fuzz" || c == "f") {
      return make_fuzzy(arg, false);
    }
    if (c == "length" || c == "len") {
      return make_length(arg);
    }
    if (c == "date") {
      return make_date(arg);
    }
    if (cmd == "Contains") {
      return make_contains(arg, true);
    }
    if (c == "contains") {
      return make_contains(arg, false);
    }
    if (cmd == "Containsre" || cmd == "ContainsRe" || cmd == "ContainsRegex" || cmd == "Cre") {
      return make_contains_regex(arg, true);
    }
    if (c == "containsre" || c == "contains_regex" || c == "cre") {
      return make_contains_regex(arg, false);
    }
    if (c == "containsfuzzy" || c == "contains_fuzzy" || c == "cfuzzy" || c == "cfuz") {
      return make_contains_fuzzy(arg);
    }
    if (c == "random") {
      return make_random(arg);
    }
    if (c == "charset" || c == "encoding") {
      return make_charset(arg);
    }
    if (c == "pages" || c == "page") {
      return make_pages(arg);
    }
    if (c == "filecount" || c == "files" || c == "file_count") {
      return make_filecount(arg);
    }
    if (c == "time") {
      return make_time(arg);
    }
    if (c == "weekday" || c == "wday") {
      return make_weekday(arg);
    }
    // Unknown command → never match (visible failure)
    return std::make_shared<AlwaysFalse>();
  }
};

} // namespace

std::expected<MatchFuncPtr, ParseError> parse_filter(std::string_view input)
{
  Parser p{input};
  return p.parse();
}

std::string filter_help_text()
{
  // Plain-text fallback (CLI / logs). Prefer filter_help_html() in the GUI.
  return R"(Filter expression language

Terms (juxtaposition = AND, OR joins alternatives):
  Inclusive ranges use lo-hi or lo..hi (e.g. duration:3-10m, width:800..1920).
  For duration, a unit only on the high side applies to both (3-10m = 3m–10m).
  word            basename contains word (case-insensitive)
  "quoted"        same, allows spaces
  glob:*.png      glob match (also: g:)
  Glob:*.PNG      case-sensitive glob
  regex:^a.*      regex on basename (re:, r:)
  size:>1M        size compare (K/M/G); also size:10K-2M or size:10K..2M
  tag:work        files tagged "work" (needs checksum + dt-tag)
  tagged:yes|no   any tags / no tags (checksum cache lookup only)
  type:dir        type:file|dir|video|image|archive|audio (t:)
  width:>=1920    image/video width (needs ffprobe)
  height:=1080    image/video height
  aspect:16:9     width/height ratio (also ar:, ratio:; >1.5, =4:3)
  duration:>1m    media duration (seconds; 1h2m / 1:30); also duration:3-10m
  framerate:>30   video frame rate (fps)
  fuzzy:speling   n-gram fuzzy basename (threshold 0.5); fuzzy:x@0.6
  Fuzzy:Speling   case-sensitive fuzzy
  length:>10      basename character length (len:)
  date:today      mtime date (also date:>=2024-01-01, date:2024-*-01)
  time:>=15:00    mtime time of day (local HH:MM)
  weekday:mon     mtime weekday (mon–sun or 0–6); weekday:>=fri
  contains:foo    file content substring, case-insensitive (max 1MiB)
  Contains:Foo    case-sensitive content match
  containsre:a.*b content regex (cre:); Containsre: case-sensitive
  containsfuzzy:speling  fuzzy content lines (cfuzzy:); optional @0.6
  random:0.5      match with probability
  charset:ascii   basename encodable as charset (utf-8, latin1, …)
  pages:>=10      PDF page count (pdfinfo or scan)
  filecount:>5    archive member count (files: / file_count:)
  -term           exclude / NOT term   (also ^term or not term)
  ( a OR b )      grouping with parentheses

Examples:
  *.png OR *.jpg
  type:file size:>1M
  tag:work OR tagged:no
  (readme OR license) -*.bak
  glob:*.cpp regex:main
  length:>20 contains:TODO
  date:today type:file
)";
}

std::string filter_help_html()
{
  return R"(<html><body style="font-family:sans-serif; font-size:11pt;">
<h2>Filter expression language</h2>
<p>Combine terms with <b>juxtaposition</b> (AND) or the keyword
<code>OR</code>. Prefix a term with <code>-</code>, <code>^</code>, or
<code>not</code> to exclude it. Group with parentheses.</p>

<h3>Basic matching</h3>
<table cellspacing="4" cellpadding="2">
<tr><td><code>word</code></td>
    <td>Basename contains <i>word</i> (case-insensitive)</td></tr>
<tr><td><code>"quoted text"</code></td>
    <td>Same, allows spaces</td></tr>
<tr><td><code>glob:*.png</code> / <code>g:</code></td>
    <td>Glob match (case-insensitive)</td></tr>
<tr><td><code>Glob:*.PNG</code> / <code>G:</code></td>
    <td>Case-sensitive glob</td></tr>
<tr><td><code>regex:^a.*</code> / <code>re:</code> / <code>r:</code></td>
    <td>Regular expression on basename</td></tr>
<tr><td><code>fuzzy:speling</code> / <code>f:</code></td>
    <td>N-gram fuzzy basename (default threshold 0.5; optional
        <code>@0.6</code>)</td></tr>
<tr><td><code>Fuzzy:Speling</code></td>
    <td>Case-sensitive fuzzy match</td></tr>
<tr><td><code>length:&gt;10</code> / <code>len:</code></td>
    <td>Basename character length</td></tr>
</table>

<h3>File attributes</h3>
<table cellspacing="4" cellpadding="2">
<tr><td><code>tag:work</code></td>
    <td>Files with tag <i>work</i> (checksum cache + tags DB; no hashing)</td></tr>
<tr><td><code>tagged:yes</code> / <code>tagged:no</code></td>
    <td>Any tags / no tags on the file</td></tr>
<tr><td><code>size:&gt;1M</code></td>
    <td>Size compare (<code>K</code>/<code>M</code>/<code>G</code>);
        also ranges like <code>size:10K-2M</code> / <code>size:10K..2M</code></td></tr>
<tr><td><code>type:dir</code> / <code>t:video</code> / <code>type:image</code></td>
    <td><code>file</code>, <code>dir</code>, <code>video</code>, <code>image</code>,
        <code>archive</code>, or <code>audio</code> (extension-based for media)</td></tr>
<tr><td><code>date:today</code></td>
    <td>Modification date; also <code>date:&gt;=2024-01-01</code>,
        <code>date:2024-*-01</code></td></tr>
<tr><td><code>time:&gt;=15:00</code></td>
    <td>Modification time of day (local <code>HH:MM</code>)</td></tr>
<tr><td><code>weekday:mon</code> / <code>wday:</code></td>
    <td>Weekday (<code>mon</code>–<code>sun</code> or <code>0</code>–<code>6</code>);
        supports comparisons</td></tr>
<tr><td><code>charset:ascii</code></td>
    <td>Basename encodable as charset (<code>utf-8</code>, <code>latin1</code>, …)</td></tr>
<tr><td><code>random:0.5</code></td>
    <td>Match with the given probability</td></tr>
</table>

<h3>Media &amp; archives</h3>
<table cellspacing="4" cellpadding="2">
<tr><td><code>width:&gt;=1920</code> / <code>w:</code></td>
    <td>Image/video width (needs ffprobe)</td></tr>
<tr><td><code>height:=1080</code> / <code>h:</code></td>
    <td>Image/video height</td></tr>
<tr><td><code>aspect:16:9</code> / <code>ar:</code> / <code>ratio:</code></td>
<td>width÷height (e.g. <code>&gt;1.5</code>, <code>=4:3</code>)</td>
</tr>
<tr><td><code>duration:&gt;1m</code> / <code>dur:</code></td>
    <td>Media duration (seconds, or <code>1h2m</code> / <code>1:30</code>);
        ranges <code>duration:3-10m</code> / <code>3m..10m</code>
        (unit on the high side only applies to both ends)</td></tr>
<tr><td><code>framerate:&gt;30</code> / <code>fps:</code></td>
    <td>Video frame rate</td></tr>
<tr><td><code>pages:&gt;=10</code></td>
    <td>PDF page count</td></tr>
<tr><td><code>filecount:&gt;5</code> / <code>files:</code></td>
    <td>Archive member count</td></tr>
</table>

<h3>Content search</h3>
<p>Content predicates read up to 1&nbsp;MiB of each file (off the GUI thread).</p>
<table cellspacing="4" cellpadding="2">
<tr><td><code>contains:foo</code></td>
    <td>Substring, case-insensitive</td></tr>
<tr><td><code>Contains:Foo</code></td>
    <td>Substring, case-sensitive</td></tr>
<tr><td><code>containsre:a.*b</code> / <code>cre:</code></td>
    <td>Content regex</td></tr>
<tr><td><code>Containsre:</code></td>
    <td>Case-sensitive content regex</td></tr>
<tr><td><code>containsfuzzy:speling</code> / <code>cfuzzy:</code></td>
    <td>Fuzzy match against content lines (optional <code>@0.6</code>)</td></tr>
</table>

<h3>Examples</h3>
<ul>
<li><code>*.png OR *.jpg</code></li>
<li><code>type:file size:&gt;1M</code></li>
<li><code>tag:work OR tagged:no</code></li>
<li><code>(readme OR license) -*.bak</code></li>
<li><code>glob:*.cpp regex:main</code></li>
<li><code>length:&gt;20 contains:TODO</code></li>
<li><code>date:today type:file</code></li>
</ul>
</body></html>)";
}

} // namespace dirtoo::filter
