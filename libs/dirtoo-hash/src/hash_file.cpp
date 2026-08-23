// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dirtoo/hash/hash_file.hpp"

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>

namespace dirtoo::hash {
namespace {

std::string to_hex(const unsigned char* data, std::size_t len)
{
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.resize(len * 2);
  for (std::size_t i = 0; i < len; ++i) {
    out[i * 2] = kHex[data[i] >> 4];
    out[i * 2 + 1] = kHex[data[i] & 0x0F];
  }
  return out;
}

// CRC-32 (ISO 3309 / PNG / zip polynomial 0xEDB88320).
std::uint32_t crc32_update(std::uint32_t crc, const unsigned char* data, std::size_t len)
{
  static std::array<std::uint32_t, 256> table{};
  static bool table_ready = false;
  if (!table_ready) {
    for (std::uint32_t i = 0; i < 256; ++i) {
      std::uint32_t c = i;
      for (int k = 0; k < 8; ++k) {
        c = (c & 1U) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
      }
      table[i] = c;
    }
    table_ready = true;
  }
  crc = ~crc;
  for (std::size_t i = 0; i < len; ++i) {
    crc = table[(crc ^ data[i]) & 0xFFU] ^ (crc >> 8);
  }
  return ~crc;
}

std::string crc32_hex(std::uint32_t crc)
{
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out(8, '0');
  for (int i = 7; i >= 0; --i) {
    out[static_cast<std::size_t>(i)] = kHex[crc & 0x0FU];
    crc >>= 4;
  }
  return out;
}

} // namespace

std::optional<FileDigests> hash_file(const std::filesystem::path& path, const HashOptions& options,
                                    HashError* error)
{
  std::error_code ec;
  const auto st = std::filesystem::symlink_status(path, ec);
  if (ec) {
    if (error) {
      error->message = "stat failed: " + ec.message();
    }
    return std::nullopt;
  }
  if (!std::filesystem::is_regular_file(st)) {
    if (error) {
      error->message = "not a regular file";
    }
    return std::nullopt;
  }

  const auto size = static_cast<std::uint64_t>(std::filesystem::file_size(path, ec));
  if (ec) {
    if (error) {
      error->message = "file_size failed: " + ec.message();
    }
    return std::nullopt;
  }

  FileDigests out;
  out.size = size;
  {
    const auto ftime = std::filesystem::last_write_time(path, ec);
    if (!ec) {
      // file_clock ticks — only used for equality with a later stat of the same file.
      out.mtime_ns = static_cast<std::int64_t>(ftime.time_since_epoch().count());
    }
  }

  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error) {
      error->message = "cannot open file";
    }
    return std::nullopt;
  }

  EVP_MD_CTX* md5_ctx = EVP_MD_CTX_new();
  EVP_MD_CTX* sha1_ctx = EVP_MD_CTX_new();
  EVP_MD_CTX* sha256_ctx = EVP_MD_CTX_new();
  if (md5_ctx == nullptr || sha1_ctx == nullptr || sha256_ctx == nullptr) {
    EVP_MD_CTX_free(md5_ctx);
    EVP_MD_CTX_free(sha1_ctx);
    EVP_MD_CTX_free(sha256_ctx);
    if (error) {
      error->message = "EVP_MD_CTX_new failed";
    }
    return std::nullopt;
  }

  if (EVP_DigestInit_ex(md5_ctx, EVP_md5(), nullptr) != 1
      || EVP_DigestInit_ex(sha1_ctx, EVP_sha1(), nullptr) != 1
      || EVP_DigestInit_ex(sha256_ctx, EVP_sha256(), nullptr) != 1) {
    EVP_MD_CTX_free(md5_ctx);
    EVP_MD_CTX_free(sha1_ctx);
    EVP_MD_CTX_free(sha256_ctx);
    if (error) {
      error->message = "EVP_DigestInit_ex failed";
    }
    return std::nullopt;
  }

  std::uint32_t crc = 0;
  std::vector<char> buf(1 << 16);
  std::uint64_t read_total = 0;
  std::uint64_t last_progress_emit = 0;

  while (in) {
    if (options.should_cancel && options.should_cancel()) {
      EVP_MD_CTX_free(md5_ctx);
      EVP_MD_CTX_free(sha1_ctx);
      EVP_MD_CTX_free(sha256_ctx);
      if (error) {
        error->message = "cancelled";
      }
      return std::nullopt;
    }
    in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    const auto n = static_cast<std::size_t>(in.gcount());
    if (n == 0) {
      break;
    }
    const auto* bytes = reinterpret_cast<const unsigned char*>(buf.data());
    crc = crc32_update(crc, bytes, n);
    EVP_DigestUpdate(md5_ctx, bytes, n);
    EVP_DigestUpdate(sha1_ctx, bytes, n);
    EVP_DigestUpdate(sha256_ctx, bytes, n);
    read_total += n;
    if (options.on_progress) {
      // Throttle: every ~256 KiB or final byte so UI stays smooth without flood.
      constexpr std::uint64_t kStep = 256ULL * 1024;
      if (read_total == size || read_total - last_progress_emit >= kStep
          || last_progress_emit == 0) {
        last_progress_emit = read_total;
        options.on_progress(read_total, size);
      }
    }
  }

  unsigned char md5[EVP_MAX_MD_SIZE];
  unsigned char sha1[EVP_MAX_MD_SIZE];
  unsigned char sha256[EVP_MAX_MD_SIZE];
  unsigned int md5_len = 0, sha1_len = 0, sha256_len = 0;
  EVP_DigestFinal_ex(md5_ctx, md5, &md5_len);
  EVP_DigestFinal_ex(sha1_ctx, sha1, &sha1_len);
  EVP_DigestFinal_ex(sha256_ctx, sha256, &sha256_len);
  EVP_MD_CTX_free(md5_ctx);
  EVP_MD_CTX_free(sha1_ctx);
  EVP_MD_CTX_free(sha256_ctx);

  out.crc32_hex = crc32_hex(crc);
  out.md5_hex = to_hex(md5, md5_len);
  out.sha1_hex = to_hex(sha1, sha1_len);
  out.sha256_hex = to_hex(sha256, sha256_len);
  return out;
}


std::optional<FileDigests> hash_file_quick(const std::filesystem::path& path,
                                           const QuickHashOptions& options, HashError* error)
{
  std::error_code ec;
  const auto st = std::filesystem::symlink_status(path, ec);
  if (ec) {
    if (error) {
      error->message = "stat failed: " + ec.message();
    }
    return std::nullopt;
  }
  if (!std::filesystem::is_regular_file(st)) {
    if (error) {
      error->message = "not a regular file";
    }
    return std::nullopt;
  }

  const auto size = static_cast<std::uint64_t>(std::filesystem::file_size(path, ec));
  if (ec) {
    if (error) {
      error->message = "file_size failed: " + ec.message();
    }
    return std::nullopt;
  }

  std::optional<std::int64_t> mtime_ns;
  {
    const auto ftime = std::filesystem::last_write_time(path, ec);
    if (!ec) {
      mtime_ns = static_cast<std::int64_t>(ftime.time_since_epoch().count());
    }
  }

  const std::uint64_t window =
      std::max<std::uint64_t>(4096, options.window_bytes == 0 ? (1ULL << 20) : options.window_bytes);

  // Small enough: one sequential full hash is fine.
  if (size <= window * 3) {
    HashOptions full;
    full.should_cancel = options.should_cancel;
    auto digests = hash_file(path, full, error);
    if (digests && mtime_ns) {
      digests->mtime_ns = mtime_ns;
    }
    return digests;
  }

  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error) {
      error->message = "open failed";
    }
    return std::nullopt;
  }

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (ctx == nullptr || EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
    if (ctx) {
      EVP_MD_CTX_free(ctx);
    }
    if (error) {
      error->message = "EVP_DigestInit failed";
    }
    return std::nullopt;
  }

  auto feed = [&](const void* data, std::size_t n) { EVP_DigestUpdate(ctx, data, n); };

  const char magic[] = "dirtoo-quick-v1";
  feed(magic, sizeof(magic) - 1);
  const std::uint64_t size_le = size;
  feed(&size_le, sizeof(size_le));

  std::vector<unsigned char> buf(static_cast<std::size_t>(std::min(window, std::uint64_t{1ULL << 20})));
  if (buf.size() < static_cast<std::size_t>(window) && window <= (1ULL << 22)) {
    buf.resize(static_cast<std::size_t>(window));
  }

  auto read_at = [&](std::uint64_t offset, std::uint64_t len) -> bool {
    if (options.should_cancel && options.should_cancel()) {
      return false;
    }
    in.clear();
    in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!in) {
      return false;
    }
    std::uint64_t left = len;
    while (left > 0) {
      if (options.should_cancel && options.should_cancel()) {
        return false;
      }
      const std::size_t chunk =
          static_cast<std::size_t>(std::min<std::uint64_t>(left, buf.size()));
      in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(chunk));
      const auto got = static_cast<std::size_t>(in.gcount());
      if (got == 0) {
        return false;
      }
      feed(buf.data(), got);
      left -= got;
    }
    return true;
  };

  const std::uint64_t mid = size / 2 > window / 2 ? size / 2 - window / 2 : 0;
  const std::uint64_t tail = size - window;
  if (!read_at(0, window) || !read_at(mid, window) || !read_at(tail, window)) {
    EVP_MD_CTX_free(ctx);
    if (error) {
      error->message = (options.should_cancel && options.should_cancel()) ? "cancelled"
                                                                          : "sample read failed";
    }
    return std::nullopt;
  }

  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int md_len = 0;
  if (EVP_DigestFinal_ex(ctx, md, &md_len) != 1) {
    EVP_MD_CTX_free(ctx);
    if (error) {
      error->message = "EVP_DigestFinal failed";
    }
    return std::nullopt;
  }
  EVP_MD_CTX_free(ctx);

  FileDigests out;
  out.size = size;
  out.mtime_ns = mtime_ns;
  out.sha256_hex = to_hex(md, md_len);
  out.crc32_hex = "quick"; // marker: sample digest, not a real CRC
  return out;
}

} // namespace dirtoo::hash
