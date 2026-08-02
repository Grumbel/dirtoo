// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/archive/archive_index.hpp"

#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <set>

namespace dirtoo::archive {

/// Parse `bsdtar -tvf` / `tar -tvf` lines: mode links owner group size date… name
std::vector<ArchiveEntry> parse_tv_lines(const QString& output)
{
  std::vector<ArchiveEntry> out;
  const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  out.reserve(static_cast<std::size_t>(lines.size()));
  for (const QString& raw : lines) {
    const QString line = raw.trimmed();
    if (line.isEmpty() || line.startsWith(QLatin1String("total "))) {
      continue;
    }
    const bool is_dir = line.startsWith(QLatin1Char('d'));
    const QStringList parts =
        line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (parts.size() < 6) {
      continue;
    }
    bool ok = false;
    const qulonglong size = parts[4].toULongLong(&ok);
    int name_idx = 8;
    if (parts.size() <= 8) {
      name_idx = parts.size() - 1;
    }
    if (parts.size() > 7 && parts[5].contains(QLatin1Char('-'))) {
      name_idx = 7;
    }
    if (name_idx >= parts.size()) {
      name_idx = parts.size() - 1;
    }
    QString name = parts.mid(name_idx).join(QLatin1Char(' '));
    if (name.startsWith(QLatin1String("./"))) {
      name = name.mid(2);
    }
    if (name.endsWith(QLatin1Char('/'))) {
      name.chop(1);
    }
    if (name.isEmpty()) {
      continue;
    }
    ArchiveEntry e;
    e.path = std::filesystem::path{name.toStdString()}.lexically_normal();
    e.is_directory = is_dir;
    e.size = ok ? static_cast<std::uint64_t>(size) : 0;
    if (!e.path.empty()) {
      out.push_back(std::move(e));
    }
  }
  return out;
}

/// Parse `unzip -l` listing (Length / Date / Time / Name table).
std::vector<ArchiveEntry> parse_unzip_l(const QString& output)
{
  std::vector<ArchiveEntry> out;
  const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  bool in_table = false;
  for (const QString& raw : lines) {
    const QString line = raw.trimmed();
    if (line.startsWith(QLatin1String("------"))) {
      in_table = !in_table;
      continue;
    }
    if (!in_table) {
      continue;
    }
    const QStringList parts =
        line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (parts.size() < 4) {
      continue;
    }
    if (parts[0].compare(QLatin1String("Length"), Qt::CaseInsensitive) == 0) {
      continue;
    }
    bool ok = false;
    const qulonglong size = parts[0].toULongLong(&ok);
    QString name = parts.mid(3).join(QLatin1Char(' '));
    if (name.isEmpty()) {
      continue;
    }
    ArchiveEntry e;
    e.is_directory = name.endsWith(QLatin1Char('/'));
    if (e.is_directory) {
      name.chop(1);
    }
    e.path = std::filesystem::path{name.toStdString()}.lexically_normal();
    e.size = ok ? static_cast<std::uint64_t>(size) : 0;
    if (!e.path.empty()) {
      out.push_back(std::move(e));
    }
  }
  return out;
}


std::vector<ArchiveEntry> parse_tv_listing_text(const std::string& text)
{
  return parse_tv_lines(QString::fromStdString(text));
}

std::vector<ArchiveEntry> parse_unzip_listing_text(const std::string& text)
{
  return parse_unzip_l(QString::fromStdString(text));
}



std::expected<std::vector<ArchiveEntry>, std::string>
list_archive_entries(const std::filesystem::path& archive_file)
{
  // Flake guarantees libarchive; no CLI fallback.
  return list_archive_entries_libarchive(archive_file);
}

std::vector<fs::FileInfo>
fileinfos_for_prefix(const fs::Location& archive_location,
                     const std::vector<ArchiveEntry>& entries)
{
  const std::filesystem::path prefix = archive_location.entry_path().lexically_normal();
  const std::string prefix_str = prefix.generic_string();

  std::set<std::string> seen;
  std::vector<fs::FileInfo> result;

  for (const auto& entry : entries) {
    std::string rel = entry.path.generic_string();
    if (!prefix_str.empty()) {
      if (rel == prefix_str) {
        continue;
      }
      const std::string needed = prefix_str + "/";
      if (!rel.starts_with(needed)) {
        continue;
      }
      rel = rel.substr(needed.size());
    }
    if (rel.empty()) {
      continue;
    }

    const auto slash = rel.find('/');
    std::string name;
    bool is_dir = false;
    std::uint64_t size = 0;
    if (slash == std::string::npos) {
      name = rel;
      is_dir = entry.is_directory;
      size = entry.size;
    } else {
      name = rel.substr(0, slash);
      is_dir = true;
    }

    if (!seen.insert(name).second) {
      continue;
    }

    result.push_back(
        fs::FileInfo::synthetic(archive_location.join(name), name, is_dir, size));
  }

  return result;
}

std::expected<std::filesystem::path, std::string>
extract_member(const std::filesystem::path& archive_file,
               const std::filesystem::path& member,
               const std::filesystem::path& dest_dir)
{
  // Flake guarantees libarchive; no CLI fallback.
  return extract_member_libarchive(archive_file, member, dest_dir);
}

} // namespace dirtoo::archive
