// nib · selftest.cpp — the changeset port's oracle.
//
// The vectors are Etherpad's, not invented here: the worked example in
// `doc/api/changeset_library.md`, the attribute-string example beside it, and the composed
// changesets in `src/tests/backend-new/specs/easysync-*.ts`. Where a number is asserted it is
// asserted by equality, because a format is not something to be nearly right about.
#include "changeset.h"
#include "doc.h"

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace nib {

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const std::string& what) {
    if (ok) ++g_pass;
    else ++g_fail;
    printf("  %s  %s\n", ok ? "ok  " : "FAIL", what.c_str());
    fflush(stdout);
}
void section(const char* s) { printf("\n%s\n", s); }

std::string ssprintf(const char* f, ...) {
    va_list ap;
    va_start(ap, f);
    char buf[2048];
    const int n = vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return std::string(buf, n > 0 ? (size_t)(n < (int)sizeof buf ? n : (int)sizeof buf - 1) : 0);
}

std::string ops_summary(const std::vector<Op>& ops) {
    std::string s;
    for (const Op& o : ops) {
        if (!s.empty()) s += " ";
        s += o.opcode;
        s += std::to_string(o.chars);
        if (o.lines) s += "/" + std::to_string(o.lines) + "L";
        if (!o.attribs.empty()) s += o.attribs;
    }
    return s;
}

}  // namespace

int run_selftest() {
    printf("nib --selftest · the changeset port (Etherpad Easysync, C++)\n");

    section("base 36");
    {
        // JavaScript's Number.prototype.toString(36), which is what the format is written in
        const struct { int64_t n; const char* s; } v[] = {
            { 0, "0" }, { 1, "1" }, { 9, "9" }, { 10, "a" }, { 35, "z" },
            { 36, "10" }, { 35 * 36 + 35, "zz" }, { 1295, "zz" }, { 1296, "100" },
        };
        bool ok = true;
        std::string bad;
        for (const auto& x : v) {
            if (num_to_string(x.n) != x.s) { ok = false; bad = std::to_string(x.n) + " -> " + num_to_string(x.n) + ", expected " + x.s; break; }
            if (parse_num(x.s) != x.n) { ok = false; bad = std::string(x.s) + " -> " + std::to_string(parse_num(x.s)); break; }
        }
        check(ok, ok ? "nine values round-trip base 36 both ways" : bad);
        // the header of the documented example: 'z' is 35, and that is the document's old length
        check(parse_num("z") == 35, "z parses as 35, the old length in Etherpad's worked example");
    }

    section("unpack — Etherpad's worked example");
    {
        // doc/api/changeset_library.md:
        //   unpack('Z:z>1|2=m=b*0|1+1$\n')
        //   -> { oldLen: 35, newLen: 36, ops: '|2=m=b*0|1+1', charBank: '\n' }
        const std::string cs = "Z:z>1|2=m=b*0|1+1$\n";
        Unpacked u;
        std::string err;
        const bool ok = unpack(cs, u, err);
        check(ok, ok ? "it unpacks" : "unpack failed: " + err);
        check(u.old_len == 35, "oldLen is 35 (got " + std::to_string(u.old_len) + ")");
        check(u.new_len == 36, "newLen is 36 (got " + std::to_string(u.new_len) + ")");
        check(u.ops == "|2=m=b*0|1+1", "ops are |2=m=b*0|1+1 (got " + u.ops + ")");
        check(u.char_bank == "\n", "the charBank is one newline");
        check(pack(u.old_len, u.new_len, u.ops, u.char_bank) == cs, "pack is unpack's inverse, byte for byte");
    }

    section("deserializeOps — the same example");
    {
        // the documentation prints exactly these three ops:
        //   Op { opcode: '=', chars: 22, lines: 2, attribs: '' }
        //   Op { opcode: '=', chars: 11, lines: 0, attribs: '' }
        //   Op { opcode: '+', chars: 1,  lines: 1, attribs: '*0' }
        std::vector<Op> ops;
        std::string err;
        const bool ok = deserialize_ops("|2=m=b*0|1+1", ops, err);
        check(ok, ok ? "the ops parse" : "parse failed: " + err);
        check(ops.size() == 3, "three ops (got " + std::to_string(ops.size()) + ")");
        if (ops.size() == 3) {
            check(ops[0].opcode == '=' && ops[0].chars == 22 && ops[0].lines == 2 && ops[0].attribs.empty(),
                  "op 1 is = 22 chars over 2 lines");
            check(ops[1].opcode == '=' && ops[1].chars == 11 && ops[1].lines == 0 && ops[1].attribs.empty(),
                  "op 2 is = 11 chars, in-line");
            check(ops[2].opcode == '+' && ops[2].chars == 1 && ops[2].lines == 1 && ops[2].attribs == "*0",
                  "op 3 inserts 1 newline carrying attribute *0");
            std::string round;
            for (const Op& o : ops) round += o.str();
            check(round == "|2=m=b*0|1+1", "the three ops re-serialise to the same string (" + round + ")");
        }
    }

    section("deserializeOps — the atext attribute string");
    {
        // the same encoding is used for a pad's attribute string; the docs print these five ops
        std::vector<Op> ops;
        std::string err;
        const bool ok = deserialize_ops("*0*1+9*0|1+1*0*1*2+b|1+1*0+b|2+2", ops, err);
        check(ok && ops.size() == 6, ok ? "six ops (got " + std::to_string(ops.size()) + ")" : "parse failed: " + err);
        if (ops.size() == 6) {
            check(ops[0].chars == 9 && ops[0].lines == 0 && ops[0].attribs == "*0*1", "9 chars with author and bold");
            check(ops[2].chars == 11 && ops[2].attribs == "*0*1*2", "11 chars with three attributes");
            check(ops[5].chars == 2 && ops[5].lines == 2 && ops[5].attribs.empty(), "two newlines, unattributed");
            check(ops_summary(ops) == "+9*0*1 +1/1L*0 +11*0*1*2 +1/1L +11*0 +2/2L", "the whole run reads back: " + ops_summary(ops));
        }
    }

    section("applyToText");
    {
        std::string out, err;
        // insert one attributed newline at position 33 of a 35-character document
        const std::string doc = "0123456789\n0123456789\n0123456789\nx";   // 34 chars... build it exactly
        (void)doc;
        // a small, hand-checkable case first
        bool ok = apply_to_text("Z:5>1=2+1$X", "abcde", out, err);
        check(ok && out == "abXcde", ok ? "inserting X after 2 chars gives " + out : "failed: " + err);
        ok = apply_to_text("Z:5<2=1-2$", "abcde", out, err);
        check(ok && out == "ade", ok ? "deleting 2 chars after 1 gives " + out : "failed: " + err);
        ok = apply_to_text("Z:5>0=5$", "abcde", out, err);
        check(ok && out == "abcde", ok ? "an all-keep changeset is the identity" : "failed: " + err);
        // the length assertion must fire rather than guess
        ok = apply_to_text("Z:5>1=2+1$X", "abcdef", out, err);
        check(!ok && err.find("mismatched apply") != std::string::npos,
              !ok ? "a document of the wrong length is refused: " + err : "it was NOT refused");
        // a multiline insert, with the newline count checked against the bank
        ok = apply_to_text("Z:3>2=1|1+2$X\n", "ab\n", out, err);
        check(ok && out == "aX\nb\n", ok ? "a multiline insert lands: " + std::string("aX\\nb\\n") : "failed: " + err);
        ok = apply_to_text("Z:3>2=1+2$X\n", "ab\n", out, err);
        check(!ok, !ok ? "an insert whose lines disagree with its bank is refused: " + err : "it was NOT refused");
    }

    section("checkRep — the canonical form");
    {
        std::string err;
        // Etherpad's own compose test uses these three as valid inputs and output
        const char* good[] = { "Z:2>1*1+1*1=1$x", "Z:3>0*0|1=3$", "Z:2>1+1*0|1=2$x", "Z:z>1|2=m=b*0|1+1$\n" };
        for (const char* cs : good) {
            const bool ok = check_rep(cs, err);
            check(ok, ok ? std::string("canonical: ") + cs : std::string(cs) + " rejected: " + err);
            err.clear();
        }
        // a trailing bare keep is implicit — writing it out is NOT canonical, and that is the
        // check that proves this port agrees with Etherpad about the format rather than merely
        // parsing it
        const bool nc = check_rep("Z:5>1=2+1=2$X", err);
        check(!nc && err.find("canonical") != std::string::npos,
              !nc ? "a written-out trailing keep is refused as non-canonical" : "it was accepted, which is wrong");
        err.clear();
        // two adjacent keeps that share attributes must have been fused
        const bool nf = check_rep("Z:5>0=2=3$", err);
        const std::string nf_err = err;
        check(!nf, !nf ? "unfused adjacent keeps are refused: " + nf_err.substr(0, 46) : "unfused keeps were accepted");
        err.clear();
        // a claimed length that does not match the ops
        const bool nl = check_rep("Z:5>9=2+1$X", err);
        check(!nl && err.find("claimed length") != std::string::npos,
              !nl ? "a wrong claimed length is caught: " + err : "a wrong length was accepted");
        err.clear();
        // characters left over in the bank
        const bool nb = check_rep("Z:5>1=2+1$XY", err);
        check(!nb && err.find("excess characters") != std::string::npos,
              !nb ? "excess bank characters are caught: " + err : "excess bank was accepted");
    }

    section("the assemblers — fusing is part of the format");
    {
        SmartAssembler a;
        Op k1; k1.opcode = '='; k1.chars = 2;
        Op k2; k2.opcode = '='; k2.chars = 3;
        a.append(k1);
        a.append(k2);
        check(a.str() == "=5", "two adjacent keeps fuse into =5 (got " + a.str() + ")");

        SmartAssembler b;
        Op ins; ins.opcode = '+'; ins.chars = 1;
        Op del; del.opcode = '-'; del.chars = 2;
        b.append(ins);
        b.append(del);
        check(b.str() == "-2+1", "a delete and an insert emit as -2+1, deletes first (got " + b.str() + ")");

        SmartAssembler c;
        Op bold; bold.opcode = '='; bold.chars = 2; bold.attribs = "*1";
        Op plain; plain.opcode = '='; plain.chars = 2;
        c.append(bold);
        c.append(plain);
        check(c.str() == "*1=2=2", "keeps with different attributes do not fuse (got " + c.str() + ")");

        // [xxx\n, yyy, zzz\n] fuses to one multiline op, per MergingOpAssembler's own comment
        MergingAssembler m;
        Op a1; a1.opcode = '+'; a1.chars = 4; a1.lines = 1;
        Op a2; a2.opcode = '+'; a2.chars = 3;
        Op a3; a3.opcode = '+'; a3.chars = 4; a3.lines = 1;
        m.append(a1);
        m.append(a2);
        m.append(a3);
        check(m.str() == "|2+b", "xxx\\n yyy zzz\\n fuses to |2+b (got " + m.str() + ")");

        // and [xxx\n, yyy] does not — the in-line tail is emitted separately
        MergingAssembler m2;
        m2.append(a1);
        m2.append(a2);
        check(m2.str() == "|1+4+3", "xxx\\n yyy stays two ops (got " + m2.str() + ")");
    }

    section("makeSplice — the write half");
    {
        std::string err, out;
        // insert, delete, replace, each hand-checkable by reading the string
        struct Case { const char* orig; int64_t start, ndel; const char* ins; const char* want; };
        const Case cases[] = {
            { "abcde", 2, 0, "X",   "abXcde" },     // insert
            { "abcde", 1, 2, "",    "ade" },        // delete
            { "abcde", 1, 2, "YZ",  "aYZde" },      // replace
            { "abcde", 0, 0, "X",   "Xabcde" },     // at the start
            { "abcde", 5, 0, "X",   "abcdeX" },     // at the end
            { "abcde", 0, 5, "",    "" },           // the whole document
            { "",      0, 0, "hi",  "hi" },         // into an empty document
            { "ab\n",  3, 0, "c\n", "ab\nc\n" },    // a multiline insert
            { "a\nb\n", 0, 2, "",   "b\n" },        // deleting across a newline
        };
        int ok_apply = 0, ok_canon = 0;
        std::string first_bad;
        for (const Case& c : cases) {
            const std::string cs = make_splice(c.orig, c.start, c.ndel, c.ins);
            std::string e2;
            const bool canon = check_rep(cs, e2);
            if (canon) ++ok_canon;
            else if (first_bad.empty()) first_bad = std::string(c.orig) + ": " + cs + " — " + e2;
            std::string got;
            std::string e3;
            const bool applied = apply_to_text(cs, c.orig, got, e3);
            if (applied && got == c.want) ++ok_apply;
            else if (first_bad.empty()) first_bad = std::string(c.orig) + " -> " + got + ", wanted " + c.want;
        }
        const int n = (int)(sizeof cases / sizeof cases[0]);
        check(ok_canon == n, ok_canon == n ? ssprintf("all %d splices are canonical", n) : first_bad);
        check(ok_apply == n, ok_apply == n ? ssprintf("all %d splices apply to the expected text", n) : first_bad);

        // the exact bytes, so a change in the encoding is caught and not merely tolerated
        check(make_splice("abcde", 2, 0, "X") == "Z:5>1=2+1$X", "insert encodes as Z:5>1=2+1$X (got " + make_splice("abcde", 2, 0, "X") + ")");
        check(make_splice("abcde", 1, 2, "") == "Z:5<2=1-2$", "delete encodes as Z:5<2=1-2$ (got " + make_splice("abcde", 1, 2, "") + ")");
        check(make_splice("abcde", 1, 2, "YZ") == "Z:5>0=1-2+2$YZ", "replace puts the delete first (got " + make_splice("abcde", 1, 2, "YZ") + ")");

        // clamping, as Etherpad clamps: past the end means the end, not an error
        const std::string clamped = make_splice("abc", 99, 99, "X");
        const bool capp = apply_to_text(clamped, "abc", out, err);
        check(capp && out == "abcX", capp ? "a splice past the end clamps to the end: " + out : "failed: " + err);

        // a no-op splice is the identity changeset, and it is still canonical
        const std::string noop = make_splice("abcde", 2, 0, "");
        std::string e4;
        const bool noop_ok = check_rep(noop, e4);
        check(noop_ok && noop == "Z:5>0$", noop_ok ? "an empty splice is the identity Z:5>0$ (got " + noop + ")" : "not canonical: " + e4);
    }

    section("the Builder");
    {
        // the Builder must reach the same bytes as makeSplice for the same edit — two roads, one
        // encoding, because a document built op by op and one spliced in place are the same change
        Builder b(5);
        b.keep(2).insert("X");
        const std::string built = b.str();
        const std::string spliced = make_splice("abcde", 2, 0, "X");
        check(built == spliced, "keep(2).insert(X) equals makeSplice: " + built + " vs " + spliced);

        Builder b2(5);
        b2.keep(1).remove(2);
        check(b2.str() == make_splice("abcde", 1, 2, ""), "keep(1).remove(2) equals the delete splice (" + b2.str() + ")");

        // a multiline insert through the Builder carries its line count
        Builder b3(3);
        b3.keep_text("ab\n").insert("c\n");
        std::string out3, err3;
        const std::string cs3 = b3.str();
        const bool ok3 = apply_to_text(cs3, "ab\n", out3, err3);
        check(ok3 && out3 == "ab\nc\n", ok3 ? "the Builder's multiline insert applies (" + cs3 + ")" : "failed: " + err3);
        std::string e5;
        const bool canon3 = check_rep(cs3, e5);
        check(canon3, canon3 ? "and it is canonical" : "not canonical: " + e5);
    }

    section("makeSplice — ten thousand random splices");
    {
        // The property that matters, checked by construction rather than by example: for a random
        // document and a random splice, the changeset must be CANONICAL and must apply to exactly
        // the string ordinary surgery produces. A deterministic generator, so a failure names a
        // seed somebody can re-run.
        uint64_t z = 0x9E3779B97F4A7C15ull;
        auto next = [&z]() {
            z += 0x9E3779B97F4A7C15ull;
            uint64_t x = z;
            x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
            x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
            return x ^ (x >> 31);
        };
        auto rnd = [&next](uint64_t n) { return n ? (uint64_t)(next() % n) : 0ull; };
        auto text = [&rnd](size_t len) {
            std::string s;
            for (size_t i = 0; i < len; ++i) {
                const uint64_t r = rnd(28);
                s += r < 26 ? (char)('a' + r) : '\n';   // newlines at roughly one in fourteen
            }
            return s;
        };

        int tried = 0, canon_fail = 0, apply_fail = 0;
        std::string first_bad;
        for (int i = 0; i < 10000; ++i) {
            const std::string orig = text((size_t)rnd(40));
            const int64_t start = (int64_t)rnd(orig.size() + 1);
            const int64_t ndel = (int64_t)rnd((uint64_t)((int64_t)orig.size() - start) + 1);
            const std::string ins = text((size_t)rnd(8));
            ++tried;

            const std::string cs = make_splice(orig, start, ndel, ins);
            std::string e;
            if (!check_rep(cs, e)) {
                ++canon_fail;
                if (first_bad.empty()) first_bad = ssprintf("i=%d %s [%lld,%lld) + %s -> %s: %s", i, orig.c_str(),
                                                            (long long)start, (long long)ndel, ins.c_str(), cs.c_str(), e.c_str());
                continue;
            }
            const std::string want = orig.substr(0, (size_t)start) + ins + orig.substr((size_t)(start + ndel));
            std::string got, e2;
            if (!apply_to_text(cs, orig, got, e2) || got != want) {
                ++apply_fail;
                if (first_bad.empty()) first_bad = ssprintf("i=%d %s [%lld,%lld) + %s -> got %s, wanted %s (%s)", i, orig.c_str(),
                                                            (long long)start, (long long)ndel, ins.c_str(), got.c_str(), want.c_str(), e2.c_str());
            }
        }
        check(canon_fail == 0, canon_fail == 0 ? ssprintf("%d random splices are all canonical", tried) : ssprintf("%d not canonical, first: %s", canon_fail, first_bad.c_str()));
        check(apply_fail == 0, apply_fail == 0 ? ssprintf("%d random splices all apply to the expected text", tried) : ssprintf("%d wrong, first: %s", apply_fail, first_bad.c_str()));
    }

    section("the document — Stage 0's falsifier");
    {
        // The promise: the log, folded from the empty document, reproduces the text byte for byte.
        // It is checked after EVERY edit, not once at the end, so a divergence names the keystroke
        // that caused it rather than the session that contained it.
        Doc d;
        std::string err;
        const char* script[] = { "hello", " world", "\n", "second line", "\n\nfourth" };
        int steps = 0, diverged = 0;
        std::string first_bad;
        for (const char* piece : script) {
            if (!d.splice((int64_t)d.size(), 0, piece, "me", err)) { first_bad = err; break; }
            ++steps;
            std::string out, e;
            if (!d.replay(out, e) || out != d.text()) {
                ++diverged;
                if (first_bad.empty()) first_bad = "after step " + std::to_string(steps) + ": " + e;
            }
        }
        check(steps == 5 && diverged == 0,
              diverged == 0 ? "five appends, and the log replays byte-exact after each" : first_bad);
        check(d.text() == "hello world\nsecond line\n\nfourth", "the text is what was typed: " + d.text());
        check(d.revisions() == 5, ssprintf("five revisions on the log (%zu)", d.revisions()));

        // an edit in the middle, which is where an append-only log usually goes wrong
        const bool mid = d.splice(5, 6, "!! ", "me", err);
        std::string out, e;
        const bool ok = d.replay(out, e);
        check(mid && ok && out == d.text(), mid ? (ok ? "an edit in the middle still replays: " + d.text().substr(0, 20) : "replay failed: " + e) : "splice failed: " + err);
    }

    section("undo and redo — appended, never truncated");
    {
        Doc d;
        std::string err;
        d.splice(0, 0, "abc", "me", err);
        d.splice(3, 0, "def", "me", err);
        const std::string full = d.text();
        const size_t revs_before = d.revisions();

        const bool u1 = d.undo(err);
        check(u1 && d.text() == "abc", u1 ? "undo takes the document back to " + d.text() : "undo failed: " + err);
        check(d.revisions() == revs_before + 1,
              ssprintf("and the log GREW rather than shrank: %zu revisions, was %zu", d.revisions(), revs_before));

        const bool r1 = d.redo(err);
        check(r1 && d.text() == full, r1 ? "redo restores " + d.text() : "redo failed: " + err);

        // the log still replays after undo and redo have been through it
        std::string out, e;
        const bool ok = d.replay(out, e);
        check(ok && out == d.text(), ok ? ssprintf("the whole log, %zu revisions including the undo, replays byte-exact", d.revisions()) : "replay failed: " + e);

        // undo twice, then a new edit: the redo stack is gone, and the log is still whole
        d.undo(err);
        d.undo(err);
        check(d.text().empty(), "two undos empty the document (" + std::to_string(d.text().size()) + " chars)");
        const bool fork = d.splice(0, 0, "xyz", "me", err);
        check(fork && !d.can_redo(), fork ? "a new edit after an undo forks the future and drops the redos" : "splice failed: " + err);
        std::string out2, e2;
        const bool ok2 = d.replay(out2, e2);
        check(ok2 && out2 == "xyz", ok2 ? "and the log still replays to " + out2 : "replay failed: " + e2);
    }

    section("the document — a thousand random edits");
    {
        // The same property under abuse: random splices at random offsets, with the replay checked
        // every time. This is the check that would catch an inverse computed against the wrong
        // text, which is the subtle way an append-only undo goes wrong.
        uint64_t z = 0xD1B54A32D192ED03ull;
        auto next = [&z]() {
            z += 0x9E3779B97F4A7C15ull;
            uint64_t x = z;
            x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
            x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
            return x ^ (x >> 31);
        };
        auto rnd = [&next](uint64_t n) { return n ? (uint64_t)(next() % n) : 0ull; };

        Doc d;
        std::string err;
        int edits = 0, undos = 0, diverged = 0;
        std::string first_bad;
        for (int i = 0; i < 1000; ++i) {
            const uint64_t roll = rnd(10);
            if (roll < 2 && d.can_undo()) {
                if (d.undo(err)) ++undos;
            } else if (roll < 3 && d.can_redo()) {
                d.redo(err);
            } else {
                const int64_t start = (int64_t)rnd(d.size() + 1);
                const int64_t ndel = (int64_t)rnd((uint64_t)((int64_t)d.size() - start) + 1);
                std::string ins;
                for (uint64_t k = 0, n = rnd(6); k < n; ++k) {
                    const uint64_t r = rnd(28);
                    ins += r < 26 ? (char)('a' + r) : '\n';
                }
                if (d.splice(start, ndel, ins, "me", err)) ++edits;
            }
            std::string out, e;
            if (!d.replay(out, e) || out != d.text()) {
                ++diverged;
                if (first_bad.empty()) first_bad = ssprintf("diverged at i=%d after %d edits: %s", i, edits, e.c_str());
            }
        }
        check(diverged == 0, diverged == 0
                                 ? ssprintf("%d edits and %d undos, and the log replayed byte-exact every single time (%zu revisions)", edits, undos, d.revisions())
                                 : first_bad);
    }

    section("the line index");
    {
        const std::string t = "one\ntwo\n\nfour";
        LineIndex ix;
        ix.build(t);
        check(ix.count() == 4, ssprintf("four lines (%zu)", ix.count()));
        check(ix.line_len(0, t) == 3 && ix.line_len(2, t) == 0 && ix.line_len(3, t) == 4,
              "line lengths drop the newline: 3, 0, 4");
        check(ix.line_of(0) == 0 && ix.line_of(3) == 0 && ix.line_of(4) == 1 && ix.line_of(t.size()) == 3,
              "an offset maps to its line, and the newline belongs to the line it ends");
        check(ix.offset_of(1, 2, t) == 6, ssprintf("line 1 column 2 is offset 6 (got %zu)", ix.offset_of(1, 2, t)));
        check(ix.offset_of(0, 99, t) == 3, "a column past the end of a line clamps to its end");
    }

    section("refusals");
    {
        std::string err;
        Unpacked u;
        check(!unpack("hello", u, err), "a string that is not a changeset is refused");
        err.clear();
        std::vector<Op> ops;
        const bool bad_opcode = deserialize_ops("=2!3", ops, err);
        check(!bad_opcode, "an invalid opcode is refused: " + err);
        err.clear();
        const bool no_len = deserialize_ops("=", ops, err);
        check(!no_len, "an opcode with no length is refused: " + err);
    }

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 3 : 0;
}

}  // namespace nib
