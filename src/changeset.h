// nib · changeset.h — Etherpad's Easysync changeset format, ported to C++.
//
// NOT a new design. This is `C:\etherpad-develop\src\static\js\Changeset.ts` and its companions
// (`Op.ts`, `OpAssembler.ts`, `MergingOpAssembler.ts`, `SmartOpAssembler.ts`, `ChangesetUtils.ts`)
// re-expressed in the language the client is written in, so that nib ships one exe with no Node
// runtime and no localhost server. The algorithm, the wire format and the canonical form are
// Etherpad's; the tests are Etherpad's too, and `--selftest` asserts against them by equality.
//
// The format, in one paragraph. A changeset is a string `Z:<oldLen><sign><magnitude><ops>$<bank>`
// where the numbers are lower-case base 36 and the sign is `>` (the document grew) or `<` (it
// shrank). The ops are a sequence of `<attribs>[|<lines>]<opcode><chars>`, with `=` keeping the
// next `chars` characters of the base, `-` removing them, and `+` inserting `chars` characters
// taken in order from the bank. `|<lines>` says how many of those characters are newlines, and
// when it is non-zero the last of them must be a newline. `<attribs>` is zero or more `*<n>`
// references into the pad's attribute pool.
//
//   Z:z>1|2=m=b*0|1+1$\n
//   oldLen 35, newLen 36; keep 22 chars spanning 2 lines; keep 11 more; insert 1 newline with
//   attribute 0, taken from the bank.
//
// **Canonical form is load-bearing** and it is why the assemblers below are ported rather than
// approximated: `check_rep` re-serialises the ops it just read and asserts the result is
// byte-identical to its input. Two changesets that mean the same thing but serialise differently
// are not interchangeable on the wire, so the merging rules — which ops fuse, in which order
// deletes and inserts are emitted, and the trailing bare keep that is left implicit — are part of
// the format, not an optimisation.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace nib {

// ---- base 36, as `ChangesetUtils.numToString` / `parseNum` ---------------------------------
std::string num_to_string(int64_t n);
int64_t parse_num(const std::string& s);

// ---- Op, as `Op.ts` ------------------------------------------------------------------------
struct Op {
    char opcode = 0;        // '=' keep · '-' remove · '+' insert · 0 = none
    int64_t chars = 0;      // how many characters the op covers
    int64_t lines = 0;      // how many of them are newlines; if non-zero the last one is a newline
    std::string attribs;    // "*0*1o" — references into the pool, in the order written

    std::string str() const;   // attribs + ("|" lines)? + opcode + chars, all base 36
    void clear() { opcode = 0; chars = 0; lines = 0; attribs.clear(); }
};

// ---- the unpacked shape, as `unpack()` ------------------------------------------------------
struct Unpacked {
    int64_t old_len = 0, new_len = 0;
    std::string ops;        // the serialised operations, without the leading header or the bank
    std::string char_bank;  // the characters the insert ops draw from, in order
};

// Every entry point reports failure the same way: false, with `err` saying what was wrong and
// where. A changeset that does not parse is one nobody should be applying, so nothing here
// guesses, repairs or tolerates.

bool deserialize_ops(const std::string& ops, std::vector<Op>& out, std::string& err);
bool unpack(const std::string& cs, Unpacked& out, std::string& err);
std::string pack(int64_t old_len, int64_t new_len, const std::string& ops, const std::string& bank);

// Apply a changeset to a document. `str.size()` must equal the changeset's `old_len`.
bool apply_to_text(const std::string& cs, const std::string& str, std::string& out, std::string& err);

// The validator, and the port's own gate: it checks the lengths, the bank, the newline counts,
// and then that the changeset is in canonical form — because a re-serialisation that differs by a
// byte means this port and Etherpad disagree about the format.
bool check_rep(const std::string& cs, std::string& err);

// ---- the assemblers, as `OpAssembler` / `MergingOpAssembler` / `SmartOpAssembler` ------------
// They exist to define canonical form. `SmartAssembler` groups deletes before inserts and keeps
// separately, and each `MergingAssembler` fuses adjacent ops that share an opcode and attributes.
class MergingAssembler {
public:
    void append(const Op& op);
    void end_document() { flush(true); }
    std::string str() { flush(false); return out_; }
    void clear() { out_.clear(); buf_.clear(); extra_ = 0; }

private:
    void flush(bool end_document);
    std::string out_;
    Op buf_;
    int64_t extra_ = 0;   // in-line chars buffered after a multiline op: [xxx\n, yyy, zzz\n] fuses
};

class SmartAssembler {
public:
    void append(const Op& op);
    void end_document() { keep_.end_document(); }
    std::string str();
    int64_t length_change() const { return length_change_; }

private:
    void flush_plus_minus();
    void flush_keeps();
    MergingAssembler minus_, plus_, keep_;
    std::string out_;
    char last_ = 0;
    int64_t length_change_ = 0;
};

// ---- making changesets, as `opsFromText` / `makeSplice` / `Builder` ------------------------
// Reading a changeset is half the format; an editor has to WRITE them, and every keystroke is a
// splice. These are the write half, and they are what the buffer will call.

// Split `text` into the one or two ops that represent it under `opcode`. Two, when the text
// contains a newline: the format requires that a multiline op END on a newline, so the tail after
// the last one becomes its own in-line op.
void ops_from_text(char opcode, const std::string& text, const std::string& attribs, std::vector<Op>& out);

// The changeset that turns `orig` into `orig[0,start) + ins + orig[start+ndel, …)`.
// `start` and `ndel` are clamped to the document rather than rejected, as Etherpad clamps them.
std::string make_splice(const std::string& orig, int64_t start, int64_t ndel, const std::string& ins,
                        const std::string& attribs = std::string());

// Incremental construction, for a caller that walks the document rather than splicing it once.
// The ops must cover the base document in order; `str()` seals it.
class Builder {
public:
    explicit Builder(int64_t old_len) : old_len_(old_len) {}
    Builder& keep(int64_t chars, int64_t lines = 0, const std::string& attribs = std::string());
    Builder& keep_text(const std::string& text, const std::string& attribs = std::string());
    Builder& insert(const std::string& text, const std::string& attribs = std::string());
    Builder& remove(int64_t chars, int64_t lines = 0);
    std::string str();

private:
    int64_t old_len_;
    SmartAssembler assem_;
    std::string bank_;
};

}  // namespace nib
