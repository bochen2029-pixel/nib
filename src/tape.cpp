// nib · tape.cpp — BLAKE2b-256, canonical JSON and the chain, ported from fray (from glance).
#include "tape.h"

#include <bcrypt.h>   // SHA-256 for the model's hash: the platform's, not a second transcription

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace nib {

// ---------------------------------------------------------------- files (util.h)
bool read_file(const std::string& path, std::string& out) {
    out.clear();
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    GetFileSizeEx(f, &sz);
    out.resize((size_t)sz.QuadPart);
    size_t got = 0;
    while (got < out.size()) {
        DWORD n = 0;
        if (!ReadFile(f, out.data() + got, (DWORD)(out.size() - got), &n, nullptr) || !n) break;
        got += n;
    }
    CloseHandle(f);
    out.resize(got);
    return true;
}

bool make_dirs(const std::string& path) {
    std::string p;
    for (size_t i = 0; i < path.size(); ++i) {
        p += path[i];
        if ((path[i] == '\\' || path[i] == '/') && p.size() > 3) CreateDirectoryA(p.c_str(), nullptr);
    }
    if (!p.empty()) CreateDirectoryA(p.c_str(), nullptr);
    const DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

bool file_stat(const std::string& path, uint64_t& size, uint64_t& mtime) {
    WIN32_FILE_ATTRIBUTE_DATA d{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &d)) return false;
    size = ((uint64_t)d.nFileSizeHigh << 32) | d.nFileSizeLow;
    mtime = ((uint64_t)d.ftLastWriteTime.dwHighDateTime << 32) | d.ftLastWriteTime.dwLowDateTime;
    return true;
}

bool replace_file_retry(const std::string& src, const std::string& dst, bool write_through, std::string& err) {
    DWORD last = 0;
    for (int attempt = 0; attempt < 20; ++attempt) {
        const DWORD flags = MOVEFILE_REPLACE_EXISTING | (write_through ? MOVEFILE_WRITE_THROUGH : 0);
        if (MoveFileExA(src.c_str(), dst.c_str(), flags)) return true;
        last = GetLastError();
        Sleep(5);
    }
    err = ssprintf("could not replace %s (error %lu after 20 tries)", dst.c_str(), (unsigned long)last);
    return false;
}

bool write_file_atomic(const std::string& path, std::string_view bytes, std::string& err) {
    const std::string tmp = path + ".tmp";
    HANDLE f = CreateFileA(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) { err = "cannot create " + tmp; return false; }
    size_t put = 0;
    bool ok = true;
    while (put < bytes.size()) {
        DWORD n = 0;
        if (!WriteFile(f, bytes.data() + put, (DWORD)(bytes.size() - put), &n, nullptr)) { ok = false; break; }
        put += n;
    }
    if (ok) ok = FlushFileBuffers(f) != FALSE;
    CloseHandle(f);
    if (!ok) { DeleteFileA(tmp.c_str()); err = "the write failed: " + tmp; return false; }
    return replace_file_retry(tmp, path, true, err);
}

// The model file, read once end to end through CNG's SHA-256, 4 MiB at a time. A GGUF is gigabytes,
// so this is paid on the resident's thread while the state reads Loading, and the cost is recorded
// on the tape beside the digest rather than hidden.
bool sha256_file(const std::string& path, std::string& hex_out, uint64_t& bytes, std::string& err) {
    hex_out.clear();
    bytes = 0;
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (f == INVALID_HANDLE_VALUE) { err = "cannot open " + path; return false; }
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hh = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
        CloseHandle(f);
        err = "SHA-256 is not available from the platform";
        return false;
    }
    if (BCryptCreateHash(alg, &hh, nullptr, 0, nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        CloseHandle(f);
        err = "cannot create the hash";
        return false;
    }
    std::vector<unsigned char> buf((size_t)4 << 20);
    bool ok = true;
    for (;;) {
        DWORD n = 0;
        if (!ReadFile(f, buf.data(), (DWORD)buf.size(), &n, nullptr)) { ok = false; err = "read failed: " + path; break; }
        if (!n) break;
        if (BCryptHashData(hh, buf.data(), n, 0) < 0) { ok = false; err = "hashing failed"; break; }
        bytes += n;
    }
    unsigned char out[32]{};
    if (ok && BCryptFinishHash(hh, out, sizeof out, 0) < 0) { ok = false; err = "hashing failed"; }
    BCryptDestroyHash(hh);
    BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(f);
    if (ok) hex_out = hex(out, sizeof out);
    return ok;
}

bool sha256_file_cached(const std::string& path, const std::string& cache_path, std::string& hex_out,
                        uint64_t& bytes, std::string& err, bool& cached) {
    cached = false;
    uint64_t size = 0, mtime = 0;
    if (!file_stat(path, size, mtime)) { err = "cannot stat " + path; return false; }
    std::string text;
    if (read_file(cache_path, text)) {
        size_t i = 0;
        while (i < text.size()) {
            size_t j = text.find('\n', i);
            if (j == std::string::npos) j = text.size();
            std::string line = text.substr(i, j - i);
            i = j + 1;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            // digest \t size \t mtime \t path
            std::vector<std::string> f;
            size_t p = 0;
            for (int k = 0; k < 3; ++k) {
                const size_t t = line.find('\t', p);
                if (t == std::string::npos) break;
                f.push_back(line.substr(p, t - p));
                p = t + 1;
            }
            if (f.size() != 3) continue;
            f.push_back(line.substr(p));
            if (f[3] == path && f[1] == std::to_string((unsigned long long)size) && f[2] == std::to_string((unsigned long long)mtime) && f[0].size() == 64) {
                hex_out = f[0];
                bytes = size;
                cached = true;
                return true;
            }
        }
    }
    if (!sha256_file(path, hex_out, bytes, err)) return false;
    const size_t k = cache_path.find_last_of("\\/");
    if (k != std::string::npos) make_dirs(cache_path.substr(0, k));
    HANDLE h = CreateFileA(cache_path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        const std::string line = hex_out + "\t" + std::to_string((unsigned long long)size) + "\t" +
                                 std::to_string((unsigned long long)mtime) + "\t" + path + "\n";
        DWORD n = 0;
        WriteFile(h, line.data(), (DWORD)line.size(), &n, nullptr);
        CloseHandle(h);
    }
    return true;
}

// ---------------------------------------------------------------- BLAKE2b (RFC 7693)
namespace {
constexpr uint64_t kIV[8] = { 0x6a09e667f3bcc908ull, 0xbb67ae8584caa73bull, 0x3c6ef372fe94f82bull, 0xa54ff53a5f1d36f1ull,
                              0x510e527fade682d1ull, 0x9b05688c2b3e6c1full, 0x1f83d9abfb41bd6bull, 0x5be0cd19137e2179ull };
constexpr uint8_t kSigma[12][16] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
    { 11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
    { 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
    { 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
    { 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 },
    { 12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11 },
    { 13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10 },
    { 6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5 },
    { 10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
};
inline uint64_t rotr64(uint64_t x, unsigned n) { return (x >> n) | (x << (64 - n)); }
inline uint64_t load64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];
    return v;
}
inline void store64(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; ++i) { p[i] = (uint8_t)(v & 0xFF); v >>= 8; }
}
inline void G(uint64_t* v, int a, int b, int c, int d, uint64_t x, uint64_t y) {
    v[a] = v[a] + v[b] + x;
    v[d] = rotr64(v[d] ^ v[a], 32);
    v[c] = v[c] + v[d];
    v[b] = rotr64(v[b] ^ v[c], 24);
    v[a] = v[a] + v[b] + y;
    v[d] = rotr64(v[d] ^ v[a], 16);
    v[c] = v[c] + v[d];
    v[b] = rotr64(v[b] ^ v[c], 63);
}
}  // namespace

void Blake2b::init(size_t outlen) {
    outlen_ = std::min<size_t>(std::max<size_t>(outlen, 1), 64);
    for (int i = 0; i < 8; ++i) h_[i] = kIV[i];
    h_[0] ^= 0x01010000ull ^ (uint64_t)outlen_;   // depth 1, fanout 1, no key
    t_[0] = t_[1] = 0;
    buflen_ = 0;
    memset(buf_, 0, sizeof buf_);
}

void Blake2b::compress(const uint8_t* block, bool last) {
    uint64_t m[16], v[16];
    for (int i = 0; i < 16; ++i) m[i] = load64(block + i * 8);
    for (int i = 0; i < 8; ++i) { v[i] = h_[i]; v[i + 8] = kIV[i]; }
    v[12] ^= t_[0];
    v[13] ^= t_[1];
    if (last) v[14] = ~v[14];
    for (int r = 0; r < 12; ++r) {
        const uint8_t* s = kSigma[r];
        G(v, 0, 4, 8, 12, m[s[0]], m[s[1]]);
        G(v, 1, 5, 9, 13, m[s[2]], m[s[3]]);
        G(v, 2, 6, 10, 14, m[s[4]], m[s[5]]);
        G(v, 3, 7, 11, 15, m[s[6]], m[s[7]]);
        G(v, 0, 5, 10, 15, m[s[8]], m[s[9]]);
        G(v, 1, 6, 11, 12, m[s[10]], m[s[11]]);
        G(v, 2, 7, 8, 13, m[s[12]], m[s[13]]);
        G(v, 3, 4, 9, 14, m[s[14]], m[s[15]]);
    }
    for (int i = 0; i < 8; ++i) h_[i] ^= v[i] ^ v[i + 8];
}

void Blake2b::update(const void* data, size_t n) {
    const uint8_t* p = (const uint8_t*)data;
    while (n > 0) {
        if (buflen_ == 128) {
            // a full buffer is compressed only when more data follows: the last block is final
            t_[0] += 128;
            if (t_[0] < 128) t_[1]++;
            compress(buf_, false);
            buflen_ = 0;
        }
        const size_t take = std::min<size_t>(128 - buflen_, n);
        memcpy(buf_ + buflen_, p, take);
        buflen_ += take;
        p += take;
        n -= take;
    }
}

void Blake2b::final(uint8_t* out) {
    t_[0] += buflen_;
    if (t_[0] < buflen_) t_[1]++;
    memset(buf_ + buflen_, 0, 128 - buflen_);
    compress(buf_, true);
    uint8_t full[64];
    for (int i = 0; i < 8; ++i) store64(full + i * 8, h_[i]);
    memcpy(out, full, outlen_);
}

std::string hex(const uint8_t* p, size_t n) {
    static const char* d = "0123456789abcdef";
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) { s += d[p[i] >> 4]; s += d[p[i] & 15]; }
    return s;
}

std::string blake2b_hex(std::string_view data, size_t outlen) {
    Blake2b b;
    b.init(outlen);
    b.update(data.data(), data.size());
    uint8_t out[64];
    b.final(out);
    return hex(out, outlen);
}

// ---------------------------------------------------------------- canonical JSON
namespace canon {

std::string str(std::string_view utf8) {
    std::string o = "\"";
    for (unsigned char c : utf8) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            case '\b': o += "\\b"; break;
            case '\f': o += "\\f"; break;
            default:
                if (c < 0x20) o += ssprintf("\\u%04x", c);
                else o += (char)c;
        }
    }
    return o + "\"";
}

std::string num(int64_t v) { return std::to_string(v); }

std::string flt(double v) {
    if (v != v) return "NaN";
    if (v == HUGE_VAL) return "Infinity";
    if (v == -HUGE_VAL) return "-Infinity";
    char buf[40];
    for (int p = 1; p <= 17; ++p) {
        snprintf(buf, sizeof buf, "%.*g", p, v);
        if (strtod(buf, nullptr) == v) break;
    }
    std::string s = buf;
    if (s.find_first_of(".eE") == std::string::npos) s += ".0";
    // Python prints exponents as e+16 / e-05; %g does the same
    return s;
}

std::string boolean(bool v) { return v ? "true" : "false"; }

std::string obj(std::vector<std::pair<std::string, std::string>> kv) {
    std::sort(kv.begin(), kv.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::string o = "{";
    for (size_t i = 0; i < kv.size(); ++i) {
        if (i) o += ",";
        o += str(kv[i].first) + ":" + kv[i].second;
    }
    return o + "}";
}

std::string arr(const std::vector<std::string>& items) {
    std::string o = "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) o += ",";
        o += items[i];
    }
    return o + "]";
}

static void put_utf8(std::string& o, uint32_t cp) {
    if (cp < 0x80) o += (char)cp;
    else if (cp < 0x800) { o += (char)(0xC0 | (cp >> 6)); o += (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { o += (char)(0xE0 | (cp >> 12)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
    else { o += (char)(0xF0 | (cp >> 18)); o += (char)(0x80 | ((cp >> 12) & 0x3F)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
}

std::string unstr(std::string_view lit) {
    std::string o;
    if (lit.size() < 2 || lit.front() != '"' || lit.back() != '"') return std::string(lit);
    for (size_t i = 1; i + 1 < lit.size(); ++i) {
        const char c = lit[i];
        if (c != '\\') { o += c; continue; }
        if (i + 2 >= lit.size()) break;
        const char e = lit[++i];
        switch (e) {
            case '"': o += '"'; break;
            case '\\': o += '\\'; break;
            case '/': o += '/'; break;
            case 'b': o += '\b'; break;
            case 'f': o += '\f'; break;
            case 'n': o += '\n'; break;
            case 'r': o += '\r'; break;
            case 't': o += '\t'; break;
            case 'u': {
                if (i + 4 >= lit.size()) break;
                uint32_t cp = (uint32_t)strtoul(std::string(lit.substr(i + 1, 4)).c_str(), nullptr, 16);
                i += 4;
                if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < lit.size() && lit[i + 1] == '\\' && lit[i + 2] == 'u') {
                    const uint32_t lo = (uint32_t)strtoul(std::string(lit.substr(i + 3, 4)).c_str(), nullptr, 16);
                    if (lo >= 0xDC00 && lo <= 0xDFFF) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); i += 6; }
                }
                put_utf8(o, cp);
                break;
            }
            default: o += e; break;
        }
    }
    return o;
}

}  // namespace canon

// ---------------------------------------------------------------- the chain
std::string Tape::payload(uint64_t seq, const std::string& kind, int64_t at, const std::string& body_canonical) {
    return canon::obj({ { "seq", canon::num((int64_t)seq) }, { "kind", canon::str(kind) }, { "at", canon::num(at) }, { "body", body_canonical } });
}

std::string Tape::digest(const std::string& prev, const std::string& pay) {
    Blake2b b;
    b.init(32);
    b.update(prev.data(), prev.size());
    const uint8_t z = 0;
    b.update(&z, 1);
    b.update(pay.data(), pay.size());
    uint8_t out[32];
    b.final(out);
    return hex(out, 32);
}

std::string Tape::row(uint64_t seq, const std::string& kind, int64_t at, const std::string& body_canonical, const std::string& prev, const std::string& dig) {
    return canon::obj({ { "seq", canon::num((int64_t)seq) }, { "kind", canon::str(kind) }, { "at", canon::num(at) }, { "body", body_canonical }, { "prev", canon::str(prev) }, { "digest", canon::str(dig) } });
}

namespace {

// top-level fields of one JSON object row as raw fragments (strings keep their quotes)
bool top_fields(std::string_view row, std::map<std::string, std::string>& out) {
    out.clear();
    size_t i = 0;
    auto ws = [&] { while (i < row.size() && (row[i] == ' ' || row[i] == '\t' || row[i] == '\r' || row[i] == '\n')) ++i; };
    ws();
    if (i >= row.size() || row[i] != '{') return false;
    ++i;
    for (;;) {
        ws();
        if (i < row.size() && row[i] == '}') return true;
        if (i >= row.size() || row[i] != '"') return false;
        // key
        size_t k = i + 1;
        std::string key;
        while (k < row.size() && row[k] != '"') {
            if (row[k] == '\\' && k + 1 < row.size()) { key += row[k + 1]; k += 2; continue; }
            key += row[k++];
        }
        if (k >= row.size()) return false;
        i = k + 1;
        ws();
        if (i >= row.size() || row[i] != ':') return false;
        ++i;
        ws();
        // value: balanced scan
        const size_t start = i;
        if (row[i] == '"') {
            ++i;
            while (i < row.size() && row[i] != '"') { if (row[i] == '\\') ++i; ++i; }
            if (i >= row.size()) return false;
            ++i;
        } else if (row[i] == '{' || row[i] == '[') {
            int depth = 0;
            bool q = false;
            for (; i < row.size(); ++i) {
                const char c = row[i];
                if (q) { if (c == '\\') ++i; else if (c == '"') q = false; continue; }
                if (c == '"') q = true;
                else if (c == '{' || c == '[') ++depth;
                else if (c == '}' || c == ']') { if (--depth == 0) { ++i; break; } }
            }
        } else {
            while (i < row.size() && row[i] != ',' && row[i] != '}') ++i;
        }
        out[key] = std::string(row.substr(start, i - start));
        ws();
        if (i < row.size() && row[i] == ',') { ++i; continue; }
        if (i < row.size() && row[i] == '}') return true;
        return false;
    }
}

std::string unq(const std::string& s) {
    // the strings we compare are hex digests and kinds: quotes off, simple escapes only
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        std::string o;
        for (size_t i = 1; i + 1 < s.size(); ++i) {
            if (s[i] == '\\' && i + 2 < s.size()) { ++i; o += s[i] == 'n' ? '\n' : s[i] == 't' ? '\t' : s[i]; }
            else o += s[i];
        }
        return o;
    }
    return s;
}

}  // namespace

std::string canon::field(std::string_view object, std::string_view key) {
    std::map<std::string, std::string> f;
    if (!top_fields(object, f)) return {};
    const auto it = f.find(std::string(key));
    return it == f.end() ? std::string() : it->second;
}

bool Tape::read_rows(const std::string& path, std::vector<TapeRow>& out, std::string& err) {
    out.clear();
    std::string text;
    if (!read_file(path, text)) { err = "cannot read " + path; return false; }
    size_t i = 0;
    bool first = true;
    while (i < text.size()) {
        size_t j = text.find('\n', i);
        if (j == std::string::npos) j = text.size();
        std::string_view line(text.data() + i, j - i);
        i = j + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.remove_suffix(1);
        if (line.empty()) continue;
        if (first) { first = false; continue; }
        std::map<std::string, std::string> f;
        if (!top_fields(line, f) || !f.count("seq") || !f.count("kind") || !f.count("body") || !f.count("digest")) {
            err = ssprintf("row %zu: not a tape row", out.size());
            return false;
        }
        TapeRow r;
        r.seq = strtoull(f["seq"].c_str(), nullptr, 10);
        r.kind = unq(f["kind"]);
        r.at = strtoll(f["at"].c_str(), nullptr, 10);
        r.body = f["body"];
        r.digest = unq(f["digest"]);
        out.push_back(std::move(r));
    }
    return true;
}

bool Tape::verify_text(std::string_view text, uint64_t& rows, uint64_t& bad_seq, std::string& head, std::string& err) {
    rows = 0;
    bad_seq = ~0ull;
    head = std::string(64, '0');
    size_t i = 0;
    bool first = true;
    uint64_t expect = 0;
    while (i < text.size()) {
        size_t j = text.find('\n', i);
        if (j == std::string_view::npos) j = text.size();
        std::string_view line = text.substr(i, j - i);
        i = j + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.remove_suffix(1);
        if (line.empty()) continue;
        if (first) { first = false; continue; }   // the header
        std::map<std::string, std::string> f;
        if (!top_fields(line, f) || !f.count("seq") || !f.count("kind") || !f.count("at") || !f.count("body") || !f.count("prev") || !f.count("digest")) {
            err = ssprintf("row %llu: not a tape row", (unsigned long long)expect);
            bad_seq = expect;
            return false;
        }
        const uint64_t seq = strtoull(f["seq"].c_str(), nullptr, 10);
        if (seq != expect) { err = ssprintf("row %llu: seq is %llu; rows are missing or reordered", (unsigned long long)expect, (unsigned long long)seq); bad_seq = expect; return false; }
        if (unq(f["prev"]) != head) { err = ssprintf("row %llu: prev digest does not match row %llu", (unsigned long long)seq, (unsigned long long)seq - 1); bad_seq = seq; return false; }
        const std::string pay = canon::obj({ { "seq", f["seq"] }, { "kind", f["kind"] }, { "at", f["at"] }, { "body", f["body"] } });
        const std::string want = digest(head, pay);
        if (unq(f["digest"]) != want) { err = ssprintf("row %llu (%s): the body has been altered since it was written", (unsigned long long)seq, unq(f["kind"]).c_str()); bad_seq = seq; return false; }
        head = want;
        ++expect;
        ++rows;
    }
    return true;
}

bool Tape::verify_file(const std::string& path, uint64_t& rows, uint64_t& bad_seq, std::string& head, std::string& err) {
    std::string text;
    if (!read_file(path, text)) { err = "cannot read " + path; rows = 0; bad_seq = ~0ull; return false; }
    return verify_text(text, rows, bad_seq, head, err);
}

bool Tape::open(const std::string& path, const std::string& case_id, const std::vector<std::pair<std::string, std::string>>& header_extra, std::string& err) {
    close();
    const size_t k = path.find_last_of("\\/");
    if (k != std::string::npos) make_dirs(path.substr(0, k));
    std::string existing;
    const bool exists = read_file(path, existing) && !existing.empty();
    torn_bytes_ = 0;
    bool needs_newline = false;
    if (exists) {
        uint64_t rows = 0, bad = 0;
        std::string head;
        if (!verify_text(existing, rows, bad, head, err)) {
            // A torn last line — no newline after it, the process died inside the write — is not a
            // row. If everything before it verifies, the fragment is cut off, counted, and the chain
            // continues from the last complete row; the caller writes the `warn` row. A file that
            // fails anywhere else, or whose last line is complete and wrong, still refuses.
            if (!existing.empty() && existing.back() != '\n') {
                const size_t nl = existing.find_last_of('\n');
                const size_t cut = nl == std::string::npos ? 0 : nl + 1;
                std::string prefix = existing.substr(0, cut);
                std::string err2;
                if (verify_text(prefix, rows, bad, head, err2)) {
                    torn_bytes_ = existing.size() - cut;
                    HANDLE t = CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (t == INVALID_HANDLE_VALUE) { err = path + ": cannot truncate the torn row"; return false; }
                    LARGE_INTEGER pos{};
                    pos.QuadPart = (LONGLONG)cut;
                    SetFilePointerEx(t, pos, nullptr, FILE_BEGIN);
                    SetEndOfFile(t);
                    CloseHandle(t);
                    existing = std::move(prefix);
                    err.clear();
                } else {
                    err = path + ": " + err;
                    return false;
                }
            } else {
                err = path + ": " + err;
                return false;
            }
        }
        if (!existing.empty() && existing.back() != '\n') needs_newline = true;   // a row cut exactly at its end: never append onto it
        seq_ = rows;
        head_ = head;
    } else {
        seq_ = 0;
        head_ = std::string(64, '0');
    }
    h_ = CreateFileA(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h_ == INVALID_HANDLE_VALUE) { err = "cannot open " + path + ssprintf(" (error %lu)", (unsigned long)GetLastError()); return false; }
    path_ = path;
    if (needs_newline) { DWORD n = 0; WriteFile(h_, "\n", 1, &n, nullptr); }
    if (!exists) {
        std::vector<std::pair<std::string, std::string>> kv = header_extra;
        kv.emplace_back("case_id", canon::str(case_id));
        const std::string line = canon::obj(kv) + "\n";
        DWORD n = 0;
        WriteFile(h_, line.data(), (DWORD)line.size(), &n, nullptr);
    }
    return true;
}

bool Tape::append(const std::string& kind, int64_t at, const std::string& body_canonical) {
    if (h_ == INVALID_HANDLE_VALUE) return false;
    const std::string pay = payload(seq_, kind, at, body_canonical);
    const std::string dig = digest(head_, pay);
    buf_ += row(seq_, kind, at, body_canonical, head_, dig);
    buf_ += "\n";
    head_ = dig;
    ++seq_;
    return true;
}

void Tape::flush() {
    if (h_ == INVALID_HANDLE_VALUE) return;
    if (!buf_.empty()) {
        DWORD n = 0;
        WriteFile(h_, buf_.data(), (DWORD)buf_.size(), &n, nullptr);
        buf_.clear();
    }
    FlushFileBuffers(h_);
}

void Tape::close() {
    if (h_ != INVALID_HANDLE_VALUE) { flush(); CloseHandle(h_); }
    h_ = INVALID_HANDLE_VALUE;
}

}  // namespace nib
