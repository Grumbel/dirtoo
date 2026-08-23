// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <iostream>
#include <string>
#include <string_view>

namespace dtjson {

inline void write_escaped(std::ostream& out, std::string_view s)
{
  out << '"';
  for (unsigned char c : s) {
    switch (c) {
    case '"':
      out << "\\\"";
      break;
    case '\\':
      out << "\\\\";
      break;
    case '\b':
      out << "\\b";
      break;
    case '\f':
      out << "\\f";
      break;
    case '\n':
      out << "\\n";
      break;
    case '\r':
      out << "\\r";
      break;
    case '\t':
      out << "\\t";
      break;
    default:
      if (c < 0x20) {
        static const char* hex = "0123456789abcdef";
        out << "\\u00" << hex[c >> 4] << hex[c & 0xf];
      } else {
        out << static_cast<char>(c);
      }
      break;
    }
  }
  out << '"';
}

inline void write_kv_string(std::ostream& out, std::string_view key, std::string_view value,
                            bool& first)
{
  if (!first) {
    out << ',';
  }
  first = false;
  write_escaped(out, key);
  out << ':';
  write_escaped(out, value);
}

inline void write_kv_uint(std::ostream& out, std::string_view key, unsigned long long value,
                          bool& first)
{
  if (!first) {
    out << ',';
  }
  first = false;
  write_escaped(out, key);
  out << ':' << value;
}

inline void write_kv_double(std::ostream& out, std::string_view key, double value, bool& first)
{
  if (!first) {
    out << ',';
  }
  first = false;
  write_escaped(out, key);
  out << ':' << value;
}

inline void write_kv_bool(std::ostream& out, std::string_view key, bool value, bool& first)
{
  if (!first) {
    out << ',';
  }
  first = false;
  write_escaped(out, key);
  out << ':' << (value ? "true" : "false");
}

} // namespace dtjson
