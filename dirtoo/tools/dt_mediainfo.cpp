// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/filter/media_probe.hpp"

#include <iostream>
#include <string_view>

#ifndef DIRTOO_VERSION
#  define DIRTOO_VERSION "0.0.0-unknown"
#endif

namespace {

void usage()
{
  std::cerr << "usage: dt-mediainfo <path> [path…]\n"
               "  Print media metadata (width, height, duration, fps, pages, archive file count).\n"
               "  Uses ffprobe / pdfinfo / bsdtar (see $DIRTOO_FFPROBE). Python: programs/mediainfo.py.\n";
}

void print_info(const std::filesystem::path& path, const dirtoo::filter::MediaInfo& info)
{
  std::cout << path.string() << '\n';
  if (info.width) {
    std::cout << "  width: " << *info.width << '\n';
  }
  if (info.height) {
    std::cout << "  height: " << *info.height << '\n';
  }
  if (info.duration_ms) {
    const auto ms = *info.duration_ms;
    std::cout << "  duration_ms: " << ms << " (" << (ms / 1000.0) << " s)\n";
  }
  if (info.framerate) {
    std::cout << "  framerate: " << *info.framerate << '\n';
  }
  if (info.pages) {
    std::cout << "  pages: " << *info.pages << '\n';
  }
  if (info.file_count) {
    std::cout << "  file_count: " << *info.file_count << '\n';
  }
}

} // namespace

int main(int argc, char* argv[])
{
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == "--version" || std::string_view(argv[i]) == "-V") {
      std::cout << "dirtoo " DIRTOO_VERSION "\n";
      return 0;
    }
  }

  int argi = 1;
  if (argi < argc
      && (std::string_view(argv[argi]) == "--help" || std::string_view(argv[argi]) == "-h")) {
    usage();
    return 0;
  }
  if (argi >= argc) {
    usage();
    return 2;
  }

  int failures = 0;
  for (; argi < argc; ++argi) {
    const std::filesystem::path path{argv[argi]};
    auto info = dirtoo::filter::probe_media(path);
    if (!info) {
      std::cerr << path.string() << ": no media metadata\n";
      ++failures;
      continue;
    }
    print_info(path, *info);
  }
  return failures == 0 ? 0 : 1;
}
