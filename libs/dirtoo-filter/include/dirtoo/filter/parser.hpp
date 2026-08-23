// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dirtoo/filter/match_func.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace dirtoo::filter {

struct ParseError {
  std::string message;
  std::size_t position = 0;
};

/// Parse a filter expression into a MatchFunc.
///
/// Grammar (recursive descent; fixes missing parentheses from the Python DSL):
///   expr     := or_expr
///   or_expr  := and_expr ( (OR|or) and_expr )*
///   and_expr := unary ( (AND|and)? unary )*   // juxtaposition implies AND
///   unary    := ( '-' | '^' | NOT|not )? primary
///   primary  := '(' expr ')' | command | quoted | word
///   command  := name ':' argument
///
/// Bare words / quoted strings match the basename (substring, or glob if * or ? present).
/// Built-in commands: glob, Glob, regex, Regex, re, size, type, t
[[nodiscard]] std::expected<MatchFuncPtr, ParseError> parse_filter(std::string_view input);

/// Human-readable short help for the expression language (plain text; CLI/logs).
[[nodiscard]] std::string filter_help_text();

/// Qt-rich-text / HTML help for the expression language (GUI dialogs).
[[nodiscard]] std::string filter_help_html();

} // namespace dirtoo::filter
