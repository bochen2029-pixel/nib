// nib · changeset.cpp — the port. See changeset.h for what this is and is not.
#include "changeset.h"

#include <cstdio>

namespace nib {

namespace {

std::string fail(std::string& err, const std::string& what) {
    if (err.empty()) err = what;
    return std::string();
}

bool is36(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z'); }

// count the newlines in [p, p+n)
int64_t count_nl(const std::string& s, size_t at, size_t n) {
    int64_t k = 0;
    for (size_t i = at; i < at + n && i < s.size(); ++i)
        if (s[i] == '\n') ++k;
    return k;
}

}  // namespace

// ---- base 36 --------------------------------------------------------------------------------
// JavaScript's Number.prototype.toString(36), lower case. Negative values never reach here in a
// well-formed changeset; if one does, it is written with a leading '-' as JS would, so the bug
// surfaces in the output instead of silently wrapping.
std::string num_to_string(int64_t n) {
    if (n == 0) return "0";
    const bool neg = n < 0;
    uint64_t v = neg ? (uint64_t)(-n) : (uint64_t)n;
    char buf[16];
    int i = 0;
    while (v) {
        const int d = (int)(v % 36);
        buf[i++] = (char)(d < 10 ? '0' + d : 'a' + (d - 10));
        v /= 36;
    }
    std::string s;
    if (neg) s += '-';
    while (i) s += buf[--i];
    return s;
}

int64_t parse_num(const std::string& s) {
    int64_t v = 0;
    for (char c : s) {
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
        else break;   // parseInt stops at the first character that is not a digit in the radix
        v = v * 36 + d;
    }
    return v;
}

// ---- Op ------------------------------------------------------------------------------------
std::string Op::str() const {
    if (!opcode) return std::string();   // `Op.toString` throws; the caller here never emits a null op
    std::string s = attribs;
    if (lines) { s += '|'; s += num_to_string(lines); }
    s += opcode;
    s += num_to_string(chars);
    return s;
}

// ---- deserialize_ops -------------------------------------------------------------------------
// Etherpad's regex is /((?:\*[0-9a-z]+)*)(?:\|([0-9a-z]+))?([-+=])([0-9a-z]+)|(.)/g — a scanner
// rather than <regex> here, because this runs on every keystroke and the grammar is four tokens
// wide. The alternation's last branch is the error path: any character that cannot start an op is
// invalid, except '$', which ends the ops and begins the bank.
bool deserialize_ops(const std::string& ops, std::vector<Op>& out, std::string& err) {
    out.clear();
    size_t i = 0;
    while (i < ops.size()) {
        const size_t start = i;
        Op op;
        // (\*[0-9a-z]+)*
        while (i < ops.size() && ops[i] == '*') {
            const size_t a = i++;
            while (i < ops.size() && is36(ops[i])) ++i;
            if (i == a + 1) { err = "invalid operation: " + ops.substr(start); return false; }
            op.attribs.append(ops, a, i - a);
        }
        // (?:\|([0-9a-z]+))?
        if (i < ops.size() && ops[i] == '|') {
            const size_t a = ++i;
            while (i < ops.size() && is36(ops[i])) ++i;
            if (i == a) { err = "invalid operation: " + ops.substr(start); return false; }
            op.lines = parse_num(ops.substr(a, i - a));
        }
        // ([-+=])([0-9a-z]+)
        if (i >= ops.size()) { err = "invalid operation: " + ops.substr(start); return false; }
        const char c = ops[i];
        if (c == '$') return true;             // the bank begins; the ops are done
        if (c != '=' && c != '-' && c != '+') { err = "invalid operation: " + ops.substr(start); return false; }
        op.opcode = c;
        ++i;
        const size_t a = i;
        while (i < ops.size() && is36(ops[i])) ++i;
        if (i == a) { err = "invalid operation: " + ops.substr(start); return false; }
        op.chars = parse_num(ops.substr(a, i - a));
        out.push_back(std::move(op));
    }
    return true;
}

// ---- unpack / pack ---------------------------------------------------------------------------
bool unpack(const std::string& cs, Unpacked& out, std::string& err) {
    // /Z:([0-9a-z]+)([><])([0-9a-z]+)/, anchored at the start — the JS regex has an empty
    // alternative that makes a non-match yield an empty string, which `unpack` then rejects.
    if (cs.size() < 5 || cs[0] != 'Z' || cs[1] != ':') { err = "not a changeset: " + cs; return false; }
    size_t i = 2;
    const size_t a = i;
    while (i < cs.size() && is36(cs[i])) ++i;
    if (i == a || i >= cs.size() || (cs[i] != '>' && cs[i] != '<')) { err = "not a changeset: " + cs; return false; }
    out.old_len = parse_num(cs.substr(a, i - a));
    const int sign = cs[i] == '>' ? 1 : -1;
    ++i;
    const size_t b = i;
    while (i < cs.size() && is36(cs[i])) ++i;
    if (i == b) { err = "not a changeset: " + cs; return false; }
    out.new_len = out.old_len + sign * parse_num(cs.substr(b, i - b));
    const size_t ops_start = i;
    size_t ops_end = cs.find('$');
    if (ops_end == std::string::npos) ops_end = cs.size();
    if (ops_end < ops_start) { err = "not a changeset: " + cs; return false; }
    out.ops = cs.substr(ops_start, ops_end - ops_start);
    out.char_bank = ops_end + 1 <= cs.size() ? cs.substr(ops_end + 1) : std::string();
    return true;
}

std::string pack(int64_t old_len, int64_t new_len, const std::string& ops, const std::string& bank) {
    const int64_t diff = new_len - old_len;
    const std::string d = diff >= 0 ? ">" + num_to_string(diff) : "<" + num_to_string(-diff);
    return "Z:" + num_to_string(old_len) + d + ops + "$" + bank;
}

// ---- apply_to_text ----------------------------------------------------------------------------
bool apply_to_text(const std::string& cs, const std::string& str, std::string& out, std::string& err) {
    Unpacked u;
    if (!unpack(cs, u, err)) return false;
    if ((int64_t)str.size() != u.old_len) {
        err = "mismatched apply: " + std::to_string(str.size()) + " / " + std::to_string(u.old_len);
        return false;
    }
    std::vector<Op> ops;
    if (!deserialize_ops(u.ops, ops, err)) return false;
    out.clear();
    out.reserve((size_t)(u.new_len > 0 ? u.new_len : 0));
    size_t bank = 0, at = 0;
    for (const Op& op : ops) {
        const size_t n = (size_t)op.chars;
        switch (op.opcode) {
            case '+':
                if (bank + n > u.char_bank.size()) { err = "insert runs past the charBank"; return false; }
                if (count_nl(u.char_bank, bank, n) != op.lines) { err = "newline count is wrong in op +"; return false; }
                out.append(u.char_bank, bank, n);
                bank += n;
                break;
            case '-':
                if (at + n > str.size()) { err = "delete runs past the document"; return false; }
                if (count_nl(str, at, n) != op.lines) { err = "newline count is wrong in op -"; return false; }
                at += n;
                break;
            case '=':
                if (at + n > str.size()) { err = "keep runs past the document"; return false; }
                if (count_nl(str, at, n) != op.lines) { err = "newline count is wrong in op ="; return false; }
                out.append(str, at, n);
                at += n;
                break;
            default:
                err = "unknown opcode";
                return false;
        }
    }
    out.append(str, at, str.size() - at);   // the implicit trailing keep
    return true;
}

// ---- the assemblers ---------------------------------------------------------------------------
void MergingAssembler::flush(bool end_document) {
    if (!buf_.opcode) return;
    // A trailing bare keep is left implicit — apply_to_text copies the remainder anyway, and the
    // canonical form says so. Anything else is emitted, followed by the in-line characters that
    // were buffered behind a multiline op.
    if (end_document && buf_.opcode == '=' && buf_.attribs.empty()) {
        // dropped on purpose
    } else {
        out_ += buf_.str();
        if (extra_) {
            Op tail = buf_;
            tail.chars = extra_;
            tail.lines = 0;
            out_ += tail.str();
            extra_ = 0;
        }
    }
    buf_.opcode = 0;
}

void MergingAssembler::append(const Op& op) {
    if (op.chars <= 0) return;
    if (buf_.opcode == op.opcode && buf_.attribs == op.attribs) {
        if (op.lines > 0) {
            // this op ends on a newline, so everything buffered fuses into one multiline op
            buf_.chars += extra_ + op.chars;
            buf_.lines += op.lines;
            extra_ = 0;
            return;
        }
        if (buf_.lines == 0) { buf_.chars += op.chars; return; }   // both in-line
        extra_ += op.chars;                                        // in-line text after a multiline op
        return;
    }
    flush(false);
    buf_ = op;
}

void SmartAssembler::flush_keeps() {
    out_ += keep_.str();
    keep_.clear();
}

void SmartAssembler::flush_plus_minus() {
    // deletes before inserts: the order is part of the canonical form, not a preference
    out_ += minus_.str();
    minus_.clear();
    out_ += plus_.str();
    plus_.clear();
}

void SmartAssembler::append(const Op& op) {
    if (!op.opcode || !op.chars) return;
    if (op.opcode == '-') {
        if (last_ == '=') flush_keeps();
        minus_.append(op);
        length_change_ -= op.chars;
    } else if (op.opcode == '+') {
        if (last_ == '=') flush_keeps();
        plus_.append(op);
        length_change_ += op.chars;
    } else if (op.opcode == '=') {
        if (last_ != '=') flush_plus_minus();
        keep_.append(op);
    }
    last_ = op.opcode;
}

std::string SmartAssembler::str() {
    flush_plus_minus();
    flush_keeps();
    return out_;
}

// ---- check_rep ---------------------------------------------------------------------------------
bool check_rep(const std::string& cs, std::string& err) {
    Unpacked u;
    if (!unpack(cs, u, err)) return false;
    std::vector<Op> ops;
    if (!deserialize_ops(u.ops, ops, err)) return false;

    SmartAssembler assem;
    int64_t old_pos = 0, calc_new_len = 0;
    size_t bank = 0;
    for (const Op& o : ops) {
        switch (o.opcode) {
            case '=':
                old_pos += o.chars;
                calc_new_len += o.chars;
                break;
            case '-':
                old_pos += o.chars;
                if (old_pos > u.old_len) { err = std::to_string(old_pos) + " > " + std::to_string(u.old_len) + " in " + cs; return false; }
                break;
            case '+': {
                if ((int64_t)(u.char_bank.size() - bank) < o.chars) { err = "invalid changeset: not enough chars in charBank"; return false; }
                const int64_t nl = count_nl(u.char_bank, bank, (size_t)o.chars);
                if (nl != o.lines) { err = "invalid changeset: number of newlines in insert op does not match the charBank"; return false; }
                if (o.lines != 0 && u.char_bank[bank + (size_t)o.chars - 1] != '\n') {
                    err = "invalid changeset: multiline insert op does not end with a newline";
                    return false;
                }
                bank += (size_t)o.chars;
                calc_new_len += o.chars;
                if (calc_new_len > u.new_len) { err = std::to_string(calc_new_len) + " > " + std::to_string(u.new_len) + " in " + cs; return false; }
                break;
            }
            default:
                err = "invalid changeset: unknown opcode";
                return false;
        }
        assem.append(o);
    }
    calc_new_len += u.old_len - old_pos;
    if (calc_new_len != u.new_len) { err = "invalid changeset: claimed length does not match actual length"; return false; }
    if (bank != u.char_bank.size()) { err = "invalid changeset: excess characters in the charBank"; return false; }
    assem.end_document();
    const std::string normalized = pack(u.old_len, calc_new_len, assem.str(), u.char_bank);
    if (normalized != cs) {
        err = "invalid changeset: not in canonical form (" + normalized + " != " + cs + ")";
        return false;
    }
    return true;
}

}  // namespace nib
