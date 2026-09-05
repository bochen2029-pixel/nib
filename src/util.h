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
inline constexpr const char* kVersion = "0.7.0";

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
// SHA-256 of a whole file, streamed, as lowercase hex — the model's hash for the tape (CLAUDE.md
// rule 8), in the one digest every operator can check with a stock tool (`certutil -hashfile`).
bool sha256_file(const std::string& path, std::string& hex_out, uint64_t& bytes, std::string& err);

}  // namespace nib
