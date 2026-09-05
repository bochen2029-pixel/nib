// nib · util.h — the small shared helpers. Pure std; no windows.h here. The shape follows
// C:\fray\src\util.h so the family's tools read alike.
#pragma once
#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace nib {

// One version string, so the binary cannot report a number the repository has moved past.
inline constexpr const char* kVersion = "0.10.0";

inline std::string ssprintf(const char* f, ...) {
    va_list ap;
    va_start(ap, f);
    char buf[4096];
    const int n = vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return std::string(buf, n > 0 ? (size_t)std::min<int>(n, sizeof buf - 1) : 0);
}

bool read_file(const std::string& path, std::string& out);   // whole file, bytes; false if absent
bool make_dirs(const std::string& path);                     // every component, as the tape's directory needs
bool file_stat(const std::string& path, uint64_t& size, uint64_t& mtime);   // size and last-write time (FILETIME ticks)
// Write-then-replace, so a reader never sees a torn file. The rename is retried (a reader holding
// the destination open makes MoveFileEx fail with access denied for the microseconds it holds it),
// and a failure after the retries is a reported error, never a silent one (K5's F3).
bool replace_file_retry(const std::string& src, const std::string& dst, bool write_through, std::string& err);
bool write_file_atomic(const std::string& path, std::string_view bytes, std::string& err);
// SHA-256 of a whole file, streamed, as lowercase hex — the model's hash for the tape (CLAUDE.md
// rule 8), in the one digest every operator can check with a stock tool (`certutil -hashfile`).
bool sha256_file(const std::string& path, std::string& hex_out, uint64_t& bytes, std::string& err);
// The same, remembered: a 6.6 GB GGUF costs 17 s to hash (2026-09-05). The cache file holds one
// line per model — digest, size, mtime, path — and a hit on size and mtime is the digest; a miss
// hashes and appends. `cached` says which happened, and the tape says so too.
bool sha256_file_cached(const std::string& path, const std::string& cache_path, std::string& hex_out,
                        uint64_t& bytes, std::string& err, bool& cached);

}  // namespace nib
