// nib · tape.h — the family's append-only, hash-chained record, ported verbatim in behaviour from
// C:\fray\src\tape.h (itself a port of C:\glance\src\tape.h, which is REGISTRAR's core/tape.py
// format). One verifier reads every tape the family writes: glance's, caseclock's, fray's, nib's.
//
//   line 1   {"case_id":"...", ...}                                    (the header, unchained)
//   line n   {"at":A,"body":{...},"digest":D,"kind":K,"prev":P,"seq":S}
//   digest = blake2b-256( prev + "\0" + canonical({"at":A,"body":B,"kind":K,"seq":S}) )
//   canonical = json.dumps(sort_keys=True, separators=(",",":"), ensure_ascii=False)
//
// nib's tape is the record of what the resident perceived and judged, beside the document it
// perceived. Every changeset, every percept, every judgment with its margins, every switch and
// every coefficient goes on it (SPEC 8.1.2), and a reader can always tell which machine was on
// the other end (8.1.3). The chain is what makes "the tape is the proof" a sentence with teeth:
// `nib --verify` and `glance --verify` both read it, and a flipped byte names its row.
#pragma once
#include "util.h"

#include <windows.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nib {

// BLAKE2b (RFC 7693), unkeyed, any digest length up to 64 — hashlib.blake2b(digest_size=n).
// The family's own transcription, checked against hashlib's vectors in --selftest, including the
// 127/128/129-byte cases where a transcription goes wrong (the last block is never compressed early).
struct Blake2b {
    void init(size_t outlen);
    void update(const void* data, size_t n);
    void final(uint8_t* out);
private:
    void compress(const uint8_t* block, bool last);
    uint64_t h_[8]{};
    uint64_t t_[2]{};
    uint8_t buf_[128]{};
    size_t buflen_ = 0;
    size_t outlen_ = 32;
};
std::string blake2b_hex(std::string_view data, size_t outlen = 32);
std::string hex(const uint8_t* p, size_t n);

// canonical JSON as Python writes it: sorted keys, no spaces, non-ASCII raw, C0 controls escaped
namespace canon {
std::string str(std::string_view utf8);
std::string num(int64_t v);
std::string flt(double v);                                   // Python repr: shortest round trip, "1.0" not "1"
std::string boolean(bool v);
std::string obj(std::vector<std::pair<std::string, std::string>> kv);   // values are already-serialized fragments; keys sorted by bytes
std::string arr(const std::vector<std::string>& items);
}  // namespace canon

class Tape {
public:
    // Open for appending. A new file gets the header; an existing one is verified first and the
    // chain continues from its head (a broken chain refuses to open — say so, do not paper over it).
    bool open(const std::string& path, const std::string& case_id, const std::vector<std::pair<std::string, std::string>>& header_extra, std::string& err);
    // Append one row. The chain advances in memory at once; the bytes wait in a buffer until
    // `flush`, which writes them and pushes them to the disk — so a keystroke never pays for a
    // write, and the caller decides the durability cadence (the window flushes every 120 ms and at
    // every judgment; a crash loses at most that, and never a partial row).
    bool append(const std::string& kind, int64_t at, const std::string& body_canonical);
    void flush();
    void close();
    size_t buffered() const { return buf_.size(); }
    bool is_open() const { return h_ != INVALID_HANDLE_VALUE; }
    std::string head() const { return head_; }
    uint64_t rows() const { return seq_; }
    std::string path() const { return path_; }

    static std::string payload(uint64_t seq, const std::string& kind, int64_t at, const std::string& body_canonical);
    static std::string digest(const std::string& prev, const std::string& payload);
    static std::string row(uint64_t seq, const std::string& kind, int64_t at, const std::string& body_canonical, const std::string& prev, const std::string& dig);
    // Walk a file: rows verified, the first bad seq (or ~0), the head digest. False on any break.
    static bool verify_file(const std::string& path, uint64_t& rows, uint64_t& bad_seq, std::string& head, std::string& err);
    static bool verify_text(std::string_view text, uint64_t& rows, uint64_t& bad_seq, std::string& head, std::string& err);

private:
    HANDLE h_ = INVALID_HANDLE_VALUE;
    std::string path_;
    std::string head_ = std::string(64, '0');
    std::string buf_;
    uint64_t seq_ = 0;
};

}  // namespace nib
