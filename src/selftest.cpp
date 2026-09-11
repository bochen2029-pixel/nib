// nib · selftest.cpp — the changeset port's oracle.
//
// The vectors are Etherpad's, not invented here: the worked example in
// `doc/api/changeset_library.md`, the attribute-string example beside it, and the composed
// changesets in `src/tests/backend-new/specs/easysync-*.ts`. Where a number is asserted it is
// asserted by equality, because a format is not something to be nearly right about.
#include "changeset.h"
#include "doc.h"
#include "ingest.h"
#include "resident.h"
#include "tape.h"
#include "util.h"
#include "wire.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <memory>
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

std::string scratch_path(const char* name) {
    char tmp[MAX_PATH]{};
    GetTempPathA(MAX_PATH, tmp);
    const std::string dir = std::string(tmp) + "nib-selftest";
    make_dirs(dir);
    return dir + "\\" + name;
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

    section("undo and redo — by group, appended, never truncated");
    {
        // Undo takes back one thing a person DID, not one character. A burst of typing with no
        // pause, contiguous, by the same hand, is one group. The window driver found this the
        // hard way: undo used to unpick a word letter by letter, which is technically correct
        // and unusable.
        Doc d;
        std::string err;
        d.splice(0, 0, "abc", "me", err);
        d.splice(3, 0, "def", "me", err);
        check(d.undo_groups() == 1, ssprintf("two contiguous appends with no pause are ONE group (%zu)", d.undo_groups()));

        const std::string full = d.text();
        const size_t revs_before = d.revisions();
        const bool u1 = d.undo("me", err);
        check(u1 && d.text().empty(), u1 ? "one undo takes back the whole burst (left: \"" + d.text() + "\")" : "undo failed: " + err);
        check(d.revisions() > revs_before,
              ssprintf("and the log GREW rather than shrank: %zu revisions, was %zu", d.revisions(), revs_before));

        const bool r1 = d.redo("me", err);
        check(r1 && d.text() == full, r1 ? "one redo puts the whole burst back: " + d.text() : "redo failed: " + err);

        std::string out, e;
        const bool ok = d.replay(out, e);
        check(ok && out == d.text(), ok ? ssprintf("the whole log, %zu revisions including the undo and redo, replays byte-exact", d.revisions()) : "replay failed: " + e);
    }

    section("what closes an undo group");
    {
        std::string err;
        {   // a pause: the world moved on between the two, so they are two things
            Doc d;
            d.splice(0, 0, "abc", "me", err);
            Sleep(Doc::kGroupMs + 150);
            d.splice(3, 0, "def", "me", err);
            check(d.undo_groups() == 2, ssprintf("a pause longer than %lld ms starts a new group (%zu)", (long long)Doc::kGroupMs, d.undo_groups()));
            d.undo("me", err);
            check(d.text() == "abc", "so one undo leaves the first burst standing: \"" + d.text() + "\"");
        }
        {   // a newline: a person who pressed Enter finished a thought
            Doc d;
            d.splice(0, 0, "abc", "me", err);
            d.splice(3, 0, "\n", "me", err);
            d.splice(4, 0, "def", "me", err);
            check(d.undo_groups() >= 2, ssprintf("a newline closes the group (%zu groups)", d.undo_groups()));
            d.undo("me", err);
            check(d.text() == "abc\n", "so one undo leaves the finished line: \"" + d.text() + "\"");
        }
        {   // moving away: an edit somewhere else is a different act
            Doc d;
            d.splice(0, 0, "abcdef", "me", err);
            d.splice(0, 0, "X", "me", err);   // back at the start, not contiguous with the last
            check(d.undo_groups() == 2, ssprintf("an edit that is not contiguous starts a new group (%zu)", d.undo_groups()));
        }
        {   // typing then deleting are different kinds, and do not fuse
            Doc d;
            d.splice(0, 0, "abc", "me", err);
            d.splice(2, 1, "", "me", err);
            check(d.undo_groups() == 2, ssprintf("typing then deleting are two groups (%zu)", d.undo_groups()));
            d.undo("me", err);
            check(d.text() == "abc", "and undoing the delete restores the character: \"" + d.text() + "\"");
        }
        {   // a different hand: the resident's edit never joins a person's burst
            Doc d;
            d.splice(0, 0, "abc", "me", err);
            d.splice(3, 0, "def", "resident", err);
            check(d.undo_groups() == 2, ssprintf("a different author starts a new group (%zu)", d.undo_groups()));
        }
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
                if (d.undo("me", err)) ++undos;
            } else if (roll < 3 && d.can_redo()) {
                d.redo("me", err);
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

    section("word wrap - the row index breaks lines and never a character");
    {
        // wrap off: one visual row per logical line
        {
            const std::string t = "abc\ndef\n";
            LineIndex li; li.build(t);
            RowIndex ri; ri.build(t, li, 80, false);
            check(ri.count() == 3, ssprintf("wrap off: three lines are three rows (%zu)", ri.count()));
        }
        // wrap on: a long line tiles into rows with no gap or overlap, each within the width,
        // broken at spaces — the falsifier is byte conservation, exactly as ingest's is
        {
            const std::string t = "the quick brown fox jumps over the lazy dog";
            LineIndex li; li.build(t);
            RowIndex ri; ri.build(t, li, 10, true);
            bool tiles = true; size_t at = 0; std::string re;
            for (const auto& row : ri.rows) { if (row.a != at) tiles = false; re += t.substr(row.a, row.b - row.a); at = row.b; }
            check(tiles && re == t && at == t.size(), "the rows tile the line: no gap, no overlap, every byte once");
            bool width_ok = true;
            for (const auto& row : ri.rows) if (utf8_count(t, row.a, row.b - row.a) > 10) width_ok = false;
            check(width_ok, ssprintf("no row exceeds the wrap width (%zu rows)", ri.count()));
        }
        // a word wider than the width hard-breaks, and never inside a UTF-8 sequence
        {
            const std::string t = "aa\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9zz";
            LineIndex li; li.build(t);
            RowIndex ri; ri.build(t, li, 4, true);
            bool valid = true;
            for (const auto& row : ri.rows) if (!utf8_valid(t.substr(row.a, row.b - row.a))) valid = false;
            check(valid && ri.count() >= 3, ssprintf("a word wider than the width hard-breaks on sequence boundaries (%zu rows, valid=%d)", ri.count(), valid ? 1 : 0));
        }
        // offset -> (row, col) -> offset is identity at every offset of a wrapped line, which is
        // what keeps the caret honest when Up and Down cross a soft break
        {
            const std::string t = "alpha beta gamma delta epsilon zeta eta theta iota kappa";
            LineIndex li; li.build(t);
            RowIndex ri; ri.build(t, li, 12, true);
            bool rt = true; size_t bad = 0;
            for (size_t o = 0; o <= t.size(); ++o) {
                const size_t r = ri.row_of(o), c = ri.col_of(o, t);
                if (ri.offset_of(r, c, t) != o) { rt = false; bad = o; break; }
            }
            check(rt, ssprintf("offset -> row/col -> offset is identity across a wrapped line (first miss at %zu)", bad));
        }
    }

    section("ingest - the seam's measured facts");
    {
        using namespace auricle::fusor;
        // These are properties of auricle's header, asserted here because nib's chunking depends
        // on them and a change upstream must break a test rather than a paste.
        check(sizeof(Delta) == 528,
              ssprintf("a Delta is %zu bytes (its own comment claims 512; 8+16+2+496=522 pads to 528)", sizeof(Delta)));
        check(kChunkMax == 495,
              ssprintf("the lossless payload bound is %zu, one less than kPayloadMax", kChunkMax));
        Delta d{};
        fill_delta(d, "bo", std::string(kPayloadMax, 'x'));
        check(d.len == 495,
              ssprintf("fill_delta silently truncates %zu bytes to %u - which is why we chunk at %zu",
                       kPayloadMax, (unsigned)d.len, kChunkMax));
        fill_delta(d, "bo", std::string(kChunkMax, 'x'));
        check(d.len == kChunkMax, "and a chunk of exactly kChunkMax survives whole");
        check(sizeof(PadSource) > 512u * 1024u,
              ssprintf("a PadSource is %zu KB - it embeds the ring, so it can never be a stack local",
                       sizeof(PadSource) / 1024));
    }

    section("ingest - where a percept ends");
    {
        Compiler c;
        std::vector<Percept> out;
        c.typed("bo", "The build finished green. ", 1000, out);
        check(out.size() == 1 && out[0].text == "The build finished green. ",
              out.empty() ? "no percept" : "a closed thought is one percept: \"" + out[0].text + "\"");
        check(!c.has_pending(), "and nothing is left pending behind it");

        out.clear();
        Compiler c2;
        c2.typed("bo", "for (i = 0; i < n; ++i) { f(); }", 1000, out);
        check(out.empty(),
              ssprintf("';' and ':' do NOT close a thought - fusord measured that on 2026-08-12 (%zu percepts)", out.size()));

        out.clear();
        Compiler c3;
        c3.typed("bo", "pi is 3.14 and e is 2.71", 1000, out);
        check(out.empty(), ssprintf("a decimal point is not a thought-end (%zu percepts)", out.size()));

        out.clear();
        Compiler c4;
        c4.typed("bo", "one\ntwo", 1000, out);
        check(out.size() == 1 && out[0].text == "one\n", "a newline closes a thought");

        out.clear();
        Compiler c5;
        c5.typed("bo", "Really?! Yes.", 1000, out);
        check(out.size() == 1 && out[0].text == "Really?! ",
              out.empty() ? "no percept" : "a cluster of terminators closes once: \"" + out[0].text + "\"");
    }

    section("ingest - N characters, and T milliseconds of quiet");
    {
        Compiler::Config cfg;
        cfg.chars = 20;
        cfg.quiet_ms = 500;
        Compiler c(cfg);
        std::vector<Percept> out;
        c.typed("bo", "alpha beta gamma delta epsilon", 1000, out);
        check(!out.empty(), ssprintf("a clause past N is emitted without waiting (%zu percepts)", out.size()));
        bool word_safe = true;
        for (const auto& p : out)
            if (!p.text.empty() && p.text.back() != ' ' && p.text.back() != '\n') word_safe = false;
        check(word_safe, "and every one of them ends on a word boundary, never mid-word");

        out.clear();
        Compiler c2(cfg);
        c2.typed("bo", "half", 1000, out);
        check(out.empty(), "a partial word waits");
        c2.idle(1200, out);
        check(out.empty(), "still waits while the quiet is shorter than T");
        c2.idle(1600, out);
        check(out.size() == 1 && out[0].text == "half",
              out.empty() ? "the partial word never arrived" : "T ms of quiet flushes it: \"" + out[0].text + "\"");
    }

    section("ingest - nothing is ever truncated");
    {
        Compiler c;
        std::vector<Percept> out;
        const std::string huge(4000, 'z');   // one unbroken token, far past a Delta
        c.typed("bo", huge, 1000, out);
        c.flush(1000, out);
        std::string rebuilt;
        bool bounded = true;
        for (const auto& p : out) {
            rebuilt += p.text;
            if (p.text.size() > kChunkMax) bounded = false;
        }
        check(bounded, ssprintf("an unbroken 4000-byte run becomes %zu percepts, none over the bound", out.size()));
        check(rebuilt == huge, "and reassembling them returns the original byte for byte");

        out.clear();
        Compiler c2;
        std::string utf8;
        for (int i = 0; i < 400; ++i) utf8 += "\xE2\x80\x94";   // em dashes, 3 bytes each
        c2.typed("bo", utf8, 1000, out);
        c2.flush(1000, out);
        // The property is that each chunk is STANDALONE-VALID UTF-8, not that its last byte looks
        // a certain way: the final byte of a well-formed em dash (E2 80 94) is 0x94, which IS a
        // continuation byte. The first version of this check tested that and failed a correct
        // chunker - the same mistake, in the same shape, as the selection check the window driver
        // caught. Assert the property, not the appearance.
        auto utf8_valid = [](const std::string& t) {
            size_t i = 0;
            while (i < t.size()) {
                const unsigned char b = static_cast<unsigned char>(t[i]);
                size_t need = 0;
                if (b < 0x80) need = 0;
                else if ((b & 0xE0) == 0xC0) need = 1;
                else if ((b & 0xF0) == 0xE0) need = 2;
                else if ((b & 0xF8) == 0xF0) need = 3;
                else return false;                       // a continuation byte, or illegal lead
                if (need > 0 && i + need >= t.size()) return false;   // sequence runs off the end
                for (size_t k = 1; k <= need; ++k)
                    if ((static_cast<unsigned char>(t[i + k]) & 0xC0) != 0x80) return false;
                i += need + 1;
            }
            return true;
        };
        std::string re2;
        bool clean = true;
        size_t nchunks = 0;
        for (const auto& p : out) {
            re2 += p.text;
            ++nchunks;
            if (!utf8_valid(p.text)) clean = false;
        }
        check(clean, ssprintf("every one of %zu chunks is standalone-valid UTF-8 - no boundary split a character", nchunks));
        check(re2 == utf8, "and the multi-byte text reassembles exactly");
    }

    section("ingest - a deletion is a percept");
    {
        Compiler c;
        std::vector<Percept> out;
        c.removed("bo", "the whole sentence", 1000, out);
        check(out.size() == 1 && out[0].kind == 'd',
              ssprintf("removing text produces a percept, not a silence (%zu)", out.size()));
        check(!out.empty() && out[0].text == "(removed) the whole sentence",
              out.empty() ? "nothing" : "the removed text arrives INTACT, marked: \"" + out[0].text + "\"");
        check(c.removed_in() == c.removed_out(),
              ssprintf("and the marker is counted out of band: in %llu == out %llu",
                       (unsigned long long)c.removed_in(), (unsigned long long)c.removed_out()));

        out.clear();
        Compiler c2;
        c2.typed("bo", "abc", 1000, out);
        c2.removed("bo", "x", 1001, out);
        check(out.size() == 2 && out[0].text == "abc" && out[1].kind == 'd',
              ssprintf("a deletion flushes what was pending first - the order things happened is the world (%zu)", out.size()));
    }

    section("ingest - silence enters as world");
    {
        Compiler::Config cfg;
        cfg.idle_tick_s = 30;
        Compiler c(cfg);
        std::vector<Percept> out;
        c.typed("bo", "first. ", 1000, out);
        out.clear();
        c.typed("bo", "second. ", 1000 + 45000, out);   // 45 s later
        bool found = false;
        for (const auto& p : out)
            if (p.kind == 't' && p.text == "[tick +45s]") found = true;
        check(found, out.empty() ? "no percepts at all" : "a 45 s gap enters as \"" + out[0].text + "\", byte-identical to fusord.cpp:712");
        check(c.ticks() == 1, ssprintf("counted once (%llu)", (unsigned long long)c.ticks()));

        out.clear();
        Compiler c3(cfg);
        c3.typed("bo", "a. ", 1000, out);
        out.clear();
        c3.typed("bo", "b. ", 3000, out);   // 2 s: not silence
        bool any = false;
        for (const auto& p : out) if (p.kind == 't') any = true;
        check(!any, "a two-second pause is not silence and produces no tick");
    }

    section("ingest - a lane change never fuses two hands");
    {
        Compiler c;
        std::vector<Percept> out;
        c.typed("bo", "mine", 1000, out);
        c.typed("watcher", "theirs", 1001, out);
        check(out.size() >= 1 && out[0].lane == "bo" && out[0].text == "mine",
              out.empty() ? "nothing" : "the previous hand's clause is closed first: [" + out[0].lane + "] " + out[0].text);
    }

    section("ingest - the falsifier: every byte that entered leaves");
    {
        // Stage 1's falsifier is "a percept dropped without a loud count". Stated as arithmetic:
        // with nothing pending, the bytes in must equal the bytes out, over a stream nobody chose
        // by hand. Deterministic generator, so a failure names a case to re-run.
        uint64_t z = 0x243F6A8885A308D3ull;
        auto next = [&z]() {
            z += 0x9E3779B97F4A7C15ull;
            uint64_t x = z;
            x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
            x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
            return x ^ (x >> 31);
        };
        auto rnd = [&next](uint64_t n) { return n ? (uint64_t)(next() % n) : 0ull; };

        Compiler c;
        std::vector<Percept> out;
        uint64_t clock = 1000;
        long typed_n = 0, removed_n = 0, idled = 0;
        std::string reassembled, removed_all;
        for (int i = 0; i < 4000; ++i) {
            clock += rnd(900);
            const uint64_t roll = rnd(10);
            if (roll < 7) {
                std::string s;
                const size_t len = (size_t)rnd(30) + 1;
                for (size_t k = 0; k < len; ++k) {
                    const uint64_t r = rnd(32);
                    if (r < 24) s += (char)('a' + r);
                    else if (r < 28) s += ' ';
                    else if (r < 29) s += '.';
                    else if (r < 30) s += '\n';
                    else s += "\xE2\x80\x94";           // an em dash, to keep UTF-8 in the stream
                }
                c.typed("bo", s, clock, out);
                reassembled += s;
                ++typed_n;
            } else if (roll < 9) {
                const std::string s(1 + (size_t)rnd(20), 'q');
                c.removed("bo", s, clock, out);
                removed_all += s;
                ++removed_n;
            } else {
                c.idle(clock, out);
                ++idled;
            }
        }
        c.flush(clock, out);

        std::string got_typed, got_removed;
        for (const auto& p : out) {
            if (p.kind == 'w') got_typed += p.text;
            else if (p.kind == 'd') got_removed += p.text.substr(c.config().removed_mark.size());
        }
        check(c.typed_in() == c.typed_out(),
              ssprintf("%ld typings, %ld removals, %ld idles: bytes in %llu == bytes out %llu",
                       typed_n, removed_n, idled,
                       (unsigned long long)c.typed_in(), (unsigned long long)c.typed_out()));
        check(got_typed == reassembled,
              ssprintf("and the percepts reassemble into exactly what was typed (%zu bytes)", got_typed.size()));
        check(got_removed == removed_all,
              ssprintf("and every removed byte came through intact (%zu bytes)", got_removed.size()));
        check(!c.has_pending(), "with nothing stranded in the compiler at the end");
    }

    section("PadSource - the filter at the door, and the loud count");
    {
        // ~528 KB of ring: heap, never stack (see the note in ingest.h).
        auto srcp = std::make_unique<PadSource>();
        PadSource& src = *srcp;
        src.add_seat("watcher");
        src.typed("bo", "a person types. ", 1000);
        src.typed("watcher", "the resident writes. ", 1001);
        src.typed("WATCHER", "and again, differently cased. ", 1002);
        check(src.echoes() == 2,
              ssprintf("the resident's own lane never re-enters, case-insensitively (%llu filtered)",
                       (unsigned long long)src.echoes()));
        check(src.pushed() >= 1, ssprintf("the person's words did (%llu pushed)", (unsigned long long)src.pushed()));

        auricle::fusor::Delta d{};
        const bool got = src.poll(d);
        check(got && std::string(d.lane) == "bo",
              got ? "and what comes off the ring is on the person's lane: [" + std::string(d.lane) + "]"
                  : "nothing came off the ring");
        check(got && std::string(d.payload, d.len) == "a person types. ",
              got ? "carrying the percept byte for byte: \"" + std::string(d.payload, d.len) + "\""
                  : "no payload");
    }

    section("PadSource - a full ring spools, in order, and nothing is swallowed");
    {
        // CLAUDE.md rule 7: a dropped percept is the turn reborn inside the loop. The ring holds
        // 1024; nothing polls it here, so the overflow is deliberate and must be VISIBLE - since
        // Stage 1c as a spool that waits for room, never as a drop.
        auto srcp = std::make_unique<PadSource>();
        PadSource& src = *srcp;
        for (int i = 0; i < 3000; ++i) src.typed("bo", "word. ", 1000 + (uint64_t)i);
        src.flush(9000);
        check(src.spooled() > 0 && src.dropped() == 0,
              ssprintf("the ring filled: %llu pushed, %zu SPOOLED, %llu dropped",
                       (unsigned long long)src.pushed(), src.spooled(), (unsigned long long)src.dropped()));
        check(src.pushed() + src.spooled() == src.compiler().percepts(),
              ssprintf("and every percept is accounted for: %llu pushed + %zu spooled == %llu compiled",
                       (unsigned long long)src.pushed(), src.spooled(), (unsigned long long)src.compiler().percepts()));
        check(src.pending() == auricle::fusor::DeltaRing::capacity(),
              ssprintf("the ring is full at its capacity of %zu", src.pending()));
        // the consumer drains; the spool follows, in order, until it is empty
        auricle::fusor::Delta d{};
        PerceptMeta m{};
        uint64_t last_id = 0;
        bool ordered = true;
        size_t got = 0;
        for (int round = 0; round < 4; ++round) {
            while (src.poll(d)) { src.poll_meta(m); if (m.id <= last_id) ordered = false; last_id = m.id; ++got; }
            src.pump();
        }
        check(got == src.compiler().percepts() && src.spooled() == 0 && ordered,
              ssprintf("drained and pumped: %zu percepts came off in order, the spool is empty", got));
    }

    section("ingest - spans, jumps, and the fold");
    {
        // A percept knows where in the document it came from, and a hand that jumps elsewhere
        // closes the clause it was in: the span must stay contiguous or it means nothing.
        Compiler c;
        std::vector<Percept> out;
        c.typed("bo", "Hello ", 1000, 0, 1, out);
        c.typed("bo", "world. ", 1001, 6, 2, out);
        check(out.size() == 1 && out[0].a == 0 && out[0].b == 13 && out[0].rev == 2,
              out.empty() ? "no percept" : ssprintf("a clause typed in two bursts spans [%zu,%zu) at rev %llu", out[0].a, out[0].b, (unsigned long long)out[0].rev));
        out.clear();
        c.typed("bo", "tail", 1002, 13, 3, out);
        c.typed("bo", "HEAD ", 1003, 0, 4, out);   // a jump to the start closes "tail" first
        check(out.size() == 1 && out[0].text == "tail" && out[0].a == 13 && out[0].b == 17,
              out.empty() ? "no percept" : "a jump closes the pending clause where it stood: \"" + out[0].text + "\"");
        out.clear();
        c.removed("bo", "HEAD ", 1004, 0, 5, out);
        check(out.size() >= 1 && out.back().kind == 'd' && out.back().a == 0 && out.back().b == 0,
              "a deletion's span is empty at the point it left");

        // The fold: the log replayed through a fresh pad reproduces what the pad compiled live -
        // deletions and order included - which is what makes switching the resident on a resume
        // rather than a snapshot.
        Doc d;
        std::string err;
        d.splice(0, 0, "The build is green. ", "bo", err);
        d.splice(20, 0, "Ship it Friday. ", "bo", err);
        d.splice(4, 5, "", "bo", err);            // "build" leaves
        d.splice(4, 0, "release", "bo", err);
        auto srcp = std::make_unique<PadSource>();
        const size_t revs = fold_log(d, *srcp, "bo");
        srcp->flush(mono_ms());
        std::vector<Percept> ps = srcp->take_shipped();
        std::string typed, removed;
        for (const auto& p : ps) { if (p.kind == 'w') typed += p.text; else if (p.kind == 'd') removed += p.text.substr(srcp->compiler().config().removed_mark.size()); }
        check(revs == 4 && typed == "The build is green. Ship it Friday. release" && removed == "build",
              ssprintf("%zu revisions folded: typed \"%s\", removed \"%s\"", revs, typed.c_str(), removed.c_str()));
        check(srcp->compiler().typed_in() == srcp->compiler().typed_out() && srcp->compiler().removed_in() == srcp->compiler().removed_out(),
              "and the fold conserves every byte, both ways");
        // a budget keeps the tail and counts the rest, loudly
        auto srcq = std::make_unique<PadSource>();
        srcq->fold_begin();            // the model is loading: the pad ignores live calls
        srcq->fold_history_begin();    // the history is compiled into the fold
        fold_log(d, *srcq, "bo");
        srcq->flush(mono_ms());
        srcq->fold_history_end();
        srcq->fold_end(20);
        check(srcq->fold_skipped() > 0 && srcq->fold_shipped() > 0 && srcq->fold_shipped() + srcq->fold_skipped() == srcq->compiler().percepts(),
              ssprintf("a 20-byte budget ships the last %llu percepts and counts %llu skipped (%llu bytes)",
                       (unsigned long long)srcq->fold_shipped(), (unsigned long long)srcq->fold_skipped(), (unsigned long long)srcq->fold_skipped_bytes()));
    }

    section("the resident's serve format - train equals serve, as a run that fails");
    {
        // SPEC 6.2.2. The seed, the six worked examples, the stream opener, each seat's name and
        // mandate, the probe frame and the speak-cue frame are lifted from fusord.cpp verbatim.
        // This hash covers all of them, and matching fusord's own pin is the PROOF that the lift
        // was byte-exact - not a promise in a comment that somebody copied carefully.
        //
        // It is deliberately pure string arithmetic: no model, no GPU, no DLL. A gate that only
        // fires when a 9B is loaded is a gate that stops being checked.
        const uint64_t h = serve_hash();
        check(h == kServeHashPin,
              ssprintf("the serve bytes hash to 0x%016llx and fusord pinned 0x%016llx on 2026-08-12",
                       (unsigned long long)h, (unsigned long long)kServeHashPin));
        check(seat_count() == 3, ssprintf("three seats (%zu)", seat_count()));
        check(std::string(seats()[0].name) == "SPEAKER" &&
              std::string(seats()[1].name) == "SKEPTIC" &&
              std::string(seats()[2].name) == "SENTINEL",
              "named SPEAKER, SKEPTIC, SENTINEL, in that order - the order is part of the hash");

        // A resident that has not been started judges nothing rather than crashing: --selftest
        // must never need the card.
        Resident r;
        std::vector<Judgment> js;
        r.feed("bo", "anything at all.", 1000, 0, js);
        r.finish(js);
        check(js.empty() && !r.running(),
              ssprintf("an unstarted resident is inert, so the battery never needs a GPU (%zu judgments)", js.size()));
    }

    section("UTF-8 - columns are characters, and a splice never cuts one");
    {
        // The QC of 2026-09-04 reproduced this: with byte columns, Down then Backspace over an
        // accented character removed one byte of it, and Ctrl+R still said byte-exact, because
        // the replay check verifies the log and the log recorded the cut faithfully.
        const std::string t = "ab\n\xC3\xA9\xE2\x80\x94z\n";   // "ab", then e-acute, em dash, z
        LineIndex ix;
        ix.build(t);
        check(ix.offset_of(1, 1, t) == 5, ssprintf("column 1 of line 1 is past the whole e-acute: offset %zu (want 5)", ix.offset_of(1, 1, t)));
        check(ix.offset_of(1, 2, t) == 8, ssprintf("column 2 is past the em dash: offset %zu (want 8)", ix.offset_of(1, 2, t)));
        check(ix.offset_of(1, 99, t) == 9, "a column past the end of the line clamps to its end, on a boundary");
        check(ix.col_of(5, t) == 1 && ix.col_of(8, t) == 2 && ix.col_of(9, t) == 3, "and offsets map back to character columns");
        check(utf8_count(t, 3, 6) == 3 && utf8_snap_down(t, 4) == 3 && utf8_snap_up(t, 4) == 5,
              "count, snap down and snap up agree about where the characters are");
        check(utf8_valid(t) && !utf8_valid(std::string("ab\n\xA9\n")), "the validity walk accepts the text and rejects the corrupted one");

        Doc d(t);
        std::string err;
        d.splice(4, 1, "", "me", err);   // a caller inside the e-acute: the whole character goes, not one byte of it
        check(utf8_valid(d.text()) && d.text() == "ab\n\xE2\x80\x94z\n",
              "a splice that lands inside a character removes the whole character, never half of it");
    }

    section("UTF-8 - a thousand random edits over a multi-byte alphabet");
    {
        // The property the byte-exact replay could not see: after every edit, at random BYTE
        // offsets, the text is still well-formed and the log still replays.
        uint64_t z = 0x5851F42D4C957F2Dull;
        auto next = [&z]() {
            z += 0x9E3779B97F4A7C15ull;
            uint64_t x = z;
            x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
            x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
            return x ^ (x >> 31);
        };
        auto rnd = [&next](uint64_t n) { return n ? (uint64_t)(next() % n) : 0ull; };
        const char* alphabet[] = { "a", "b", " ", "\n", "\xC3\xA9", "\xE2\x80\x94", "\xF0\x9F\x98\x80" };
        Doc d;
        std::string err;
        int edits = 0, invalid = 0, diverged = 0;
        for (int i = 0; i < 1000; ++i) {
            const uint64_t roll = rnd(10);
            if (roll < 2 && d.can_undo()) d.undo("me", err);
            else if (roll < 3 && d.can_redo()) d.redo("me", err);
            else {
                const int64_t start = (int64_t)rnd(d.size() + 1);   // any byte, even inside a character
                const int64_t ndel = (int64_t)rnd((uint64_t)((int64_t)d.size() - start) + 1);
                std::string ins;
                for (uint64_t k = 0, n = rnd(5); k < n; ++k) ins += alphabet[rnd(7)];
                if (d.splice(start, ndel, ins, "me", err)) ++edits;
            }
            if (!utf8_valid(d.text())) ++invalid;
            std::string out, e;
            if (!d.replay(out, e) || out != d.text()) ++diverged;
        }
        check(invalid == 0, ssprintf("%d edits at random byte offsets, and the text was well-formed UTF-8 after every one (%d were not)", edits, invalid));
        check(diverged == 0, ssprintf("and the log replayed byte-exact every time (%d diverged)", diverged));
    }

    section("a foreign change ends the undo history, and a group is all or nothing");
    {
        // Doc::apply is the path a resident's block takes. The stored inverses were computed
        // against text the foreign change has just altered; a same-length replacement passes every
        // length guard and would corrupt the document on the next Ctrl+Z (QC 2026-09-04, H-4).
        Doc d;
        std::string err;
        d.splice(0, 0, "hello", "me", err);
        check(d.can_undo(), "a burst can be undone");
        const std::string cs = make_splice(d.text(), 1, 3, "ELL");   // a same-length foreign replacement
        const bool ok = d.apply(cs, "SKEPTIC", err);
        check(ok && d.text() == "hELLo", ok ? "a foreign changeset applies: " + d.text() : "apply failed: " + err);
        check(!d.can_undo() && !d.can_redo(), "and it ends what the hand could take back (nothing to undo)");
        check(d.log().back().author == "SKEPTIC" && d.log().back().kind == 'a' && d.log().back().inverse.empty(),
              "the log records the author and the kind, and stores no whole-document inverse");
        std::string out, e;
        check(d.replay(out, e) && out == d.text(), "and the log still replays byte-exact");

        Doc d2;
        d2.splice(0, 0, "abc", "me", err);
        d2.splice(3, 0, "def", "me", err);
        const std::string before = d2.text();
        const size_t revs = d2.revisions();
        check(d2.undo("me", err) && d2.text().empty(), "a validated group undoes whole");
        check(d2.log().back().author == "me" && d2.log().back().kind == 'u',
              "and the undo is attributed to the hand that undid, not to a verb");
        check(d2.redo("me", err) && d2.text() == before && d2.revisions() == revs + 4,
              ssprintf("redo restores it, every step on the log (%zu revisions)", d2.revisions()));
    }

    section("ingest - a long deletion carries its marker on every chunk");
    {
        // A bare tail chunk would reach the trunk as newly typed text, the opposite of what
        // happened (QC 2026-09-04, F7). The marker is nib's, so it is still counted out of band.
        Compiler c;
        std::vector<Percept> out;
        std::string big;
        for (int i = 0; i < 300; ++i) big += "word" + std::to_string(i % 10) + " ";   // ~1,800 bytes
        c.removed("bo", big, 1000, out);
        const std::string mark = c.config().removed_mark;
        bool all_marked = true, all_bounded = true;
        std::string rebuilt;
        for (const auto& p : out) {
            if (p.kind != 'd' || p.text.compare(0, mark.size(), mark) != 0) all_marked = false;
            if (p.text.size() > kChunkMax) all_bounded = false;
            rebuilt += p.text.substr(mark.size());
        }
        check(out.size() > 1 && all_marked, ssprintf("a %zu-byte removal became %zu percepts, every one marked", big.size(), out.size()));
        check(all_bounded && rebuilt == big, "none over the Delta bound, and the removed text reassembles exactly");
        check(c.removed_in() == c.removed_out(),
              ssprintf("removed in %llu == out %llu", (unsigned long long)c.removed_in(), (unsigned long long)c.removed_out()));
    }

    section("ingest - a tick rides the empty lane, and the seats are the filter's set");
    {
        Percept tick;
        tick.kind = 't';
        tick.lane = "bo";
        tick.text = "[tick +45s]";
        check(std::string(delta_lane(tick)).empty(), "a tick's Delta has no lane: the resident decodes it raw and never judges it");
        Percept w;
        w.lane = "bo";
        check(std::string(delta_lane(w)) == "bo", "typed world keeps its lane");

        auto srcp = std::make_unique<PadSource>();
        register_seats(*srcp);
        bool all = true;
        for (size_t i = 0; i < seat_count(); ++i) {
            std::string lower = seats()[i].name;
            for (char& ch : lower) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
            if (!srcp->is_seat(seats()[i].name) || !srcp->is_seat(lower)) all = false;
        }
        check(all && srcp->is_seat("fusor") && !srcp->is_seat("bo"),
              "the window's filter set is the resident's seat set, from one source, case-insensitively");

        // the tick lands on the ring with an empty lane and its text intact
        srcp->typed("bo", "first. ", 1000);
        srcp->typed("bo", "second. ", 1000 + 45000);
        auricle::fusor::Delta d{};
        bool saw_tick = false;
        while (srcp->poll(d))
            if (d.lane[0] == 0 && std::string(d.payload, d.len) == "[tick +45s]") saw_tick = true;
        check(saw_tick, "and off the ring the tick is [tick +45s] on the empty lane");
    }

    section("the tape - BLAKE2b, canonical JSON, the chain, and the family's verifier");
    {
        // The hash vectors are caseclock's (tests/expected/hash-vectors.json, checked against
        // hashlib) and glance's. The 127/128/129-byte cases are where a transcription of RFC 7693
        // goes wrong: the last block is buffered and never compressed early.
        check(blake2b_hex("") == "0e5751c026e543b2e8ab2eb06099daa1d1e5df47778f7787faab45cdf12fe3a8", "blake2b-256 of the empty string");
        check(blake2b_hex("abc") == "bddd813c634239723171ef3fee98579b94964e3bb1cb3e427262c8c068d52319", "blake2b-256(\"abc\")");
        check(blake2b_hex("The quick brown fox jumps over the lazy dog") == "01718cec35cd3d796dd00020e0bfecb473ad23457d063b75eff29c0ffa2e58a9", "blake2b-256 of the fox");
        check(blake2b_hex(std::string(127, 'a')) == "59e2f1aba240f20aa591016f5ef429990bc9c2131dcd0d30f0ffd75ed18f317d", "127 x a: one byte short of a block");
        check(blake2b_hex(std::string(128, 'a')) == "ae2aa48507885c4c950fb809b2076f959cde9f8ea6da260d9a3587df33dac450", "128 x a: exactly one block, still the last");
        check(blake2b_hex(std::string(129, 'a')) == "2f64744a6de0d2c0b56e64cf6e29a5aaa255010d415d51c75ccc82f73dccd865", "129 x a: the first block compresses only when the second arrives");
        check(blake2b_hex(std::string(1000, 'a')) == "e00b0ddbf1e2cdaf5c898e1a5e8826ea3a2c339bcf2a478da2e5fca9ff126672", "1000 x a");
        check(blake2b_hex(std::string(200, 'a')) == "6b6e59aaf00eb730cf93de53560846722184bbd92f8368c21ffa95380c2f9fe6", "200 x a: glance's two-block case");
        check(canon::flt(1.0) == "1.0" && canon::flt(0.9) == "0.9" && canon::flt(1e-05) == "1e-05" && canon::flt(0.1 + 0.2) == "0.30000000000000004",
              "canonical floats as Python's repr");
        check(canon::obj({ { "seq", "0" }, { "kind", canon::str("note") }, { "at", "-20" } }) == "{\"at\":-20,\"kind\":\"note\",\"seq\":0}",
              "canonical objects sort their keys");
        check(canon::str("a\"b\\c\nd\x01") == "\"a\\\"b\\\\c\\nd\\u0001\"" && canon::str("\xC3\xA9") == "\"\xC3\xA9\"",
              "canonical strings: Python's escapes, non-ASCII raw");

        // The cross-implementation pin: this payload and its digests are REGISTRAR's tape.py's
        // bytes, as glance's selftest pins them. Equal digests mean nib's tape IS the family's.
        const std::string body0 = canon::obj({ { "text", canon::str("SYNTHETIC \xC3\xA9 \xE2\x80\x94 no PHI") }, { "n", "3" }, { "ok", "true" }, { "c", canon::flt(0.9) } });
        const std::string p0 = Tape::payload(0, "note", -20, body0);
        check(p0 == "{\"at\":-20,\"body\":{\"c\":0.9,\"n\":3,\"ok\":true,\"text\":\"SYNTHETIC \xC3\xA9 \xE2\x80\x94 no PHI\"},\"kind\":\"note\",\"seq\":0}", "payload: the reference's bytes");
        const std::string d0 = Tape::digest(std::string(64, '0'), p0);
        check(d0 == "404e7301fbe61b8cb50b671e3ebf2dad98635c8e78efe41f912f3ce6c7233194", "digest 0 equals REGISTRAR tape.py's");
        const std::string p1 = Tape::payload(1, "change", -19, canon::obj({ { "path", canon::str("Labs/Serology drawn") }, { "new", canon::str("21:30") }, { "old", canon::str("") } }));
        check(Tape::digest(d0, p1) == "aebaa958fbe4d12f19bc5a0017e1abc969fa54a50bdd34a37304973c93dd3ce9", "digest 1 chains from digest 0 as the reference does");

        // A tape on disk: written, verified, continued across a reopen, and localised when broken.
        const std::string path = scratch_path("tape.jsonl");
        DeleteFileA(path.c_str());
        Tape t;
        std::string e;
        check(t.open(path, "nib:selftest", { { "tool", canon::str("nib") } }, e), "a tape opens (" + e + ")");
        t.append("note", 0, body0);
        t.append("changeset", 5, canon::obj({ { "author", canon::str("bo") }, { "rev", "1" }, { "cs", canon::str("Z:0>5+5$hello") } }));
        t.append("judgment", 700, canon::obj({ { "i", "1" }, { "margins", canon::obj({ { "SPEAKER", canon::flt(-6.1) }, { "SKEPTIC", canon::flt(5.56) }, { "SENTINEL", canon::flt(-6.09) } }) } }));
        check(t.rows() == 3 && t.head() != std::string(64, '0'), "three rows appended, the head moved");
        const std::string head3 = t.head();
        t.close();
        uint64_t rows = 0, bad = 0;
        std::string head;
        check(Tape::verify_file(path, rows, bad, head, e) && rows == 3 && head == head3, "verify: intact, three rows, head matches");
        Tape t2;
        check(t2.open(path, "nib:selftest", {}, e) && t2.rows() == 3 && t2.head() == head3, "reopening verifies first and continues the chain from its head");
        t2.append("switch", 900, canon::obj({ { "which", canon::str("ai") }, { "to", canon::str("off") } }));
        t2.close();
        check(Tape::verify_file(path, rows, bad, head, e) && rows == 4, ssprintf("a fourth row chains on across the reopen (%llu rows)", (unsigned long long)rows));
        std::string text;
        read_file(path, text);
        std::vector<std::string> ls;
        for (size_t i = 0; i < text.size();) { size_t j = text.find('\n', i); if (j == std::string::npos) j = text.size(); ls.push_back(text.substr(i, j - i)); i = j + 1; }
        check(ls.size() == 5 && ls[0].rfind("{\"case_id\":\"nib:selftest\"", 0) == 0 && ls[1].rfind("{\"at\":0,\"body\":", 0) == 0 &&
              ls[1].find("\"digest\"") < ls[1].find("\"kind\"") && ls[1].find("\"kind\"") < ls[1].find("\"prev\"") && ls[1].find("\"prev\"") < ls[1].find("\"seq\""),
              "file shape: the header, then rows of exactly the six keys in sorted order");
        std::string flipped = text;
        const size_t k = flipped.find("Z:0>5+5$hello");
        flipped[k + 8] = 'j';   // one byte of row 1's body
        const std::string broken = scratch_path("tape-broken.jsonl");
        DeleteFileA(broken.c_str());
        HANDLE bf = CreateFileA(broken.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        DWORD wn = 0;
        WriteFile(bf, flipped.data(), (DWORD)flipped.size(), &wn, nullptr);
        CloseHandle(bf);
        const bool vb = Tape::verify_file(broken, rows, bad, head, e);
        check(!vb && bad == 1, ssprintf("one flipped byte is localised to its row: bad row %llu (%s)", (unsigned long long)bad, e.c_str()));
        Tape t3;
        check(!t3.open(broken, "nib:selftest", {}, e), "and a broken chain refuses to be appended to: " + e);
        printf("  the intact tape is at %s for glance --verify\n", path.c_str());

        // The model's hash on the tape is the platform's SHA-256 (CNG), streamed over the file.
        // Two FIPS 180-4 vectors, through a real file, and a missing file is a failure with a
        // reason rather than a hash of nothing.
        {
            const std::string sp = scratch_path("sha.txt");
            HANDLE sf = CreateFileA(sp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            DWORD swn = 0;
            WriteFile(sf, "abc", 3, &swn, nullptr);
            CloseHandle(sf);
            std::string hx, se;
            uint64_t nb = 0;
            const bool ok1 = sha256_file(sp, hx, nb, se);
            check(ok1 && nb == 3 && hx == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                  "sha256 of a file holding \"abc\" is FIPS 180-4's: " + hx);
            sf = CreateFileA(sp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            CloseHandle(sf);
            const bool ok0 = sha256_file(sp, hx, nb, se);
            check(ok0 && nb == 0 && hx == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "sha256 of the empty file");
            const bool okm = sha256_file(scratch_path("no-such-model.gguf"), hx, nb, se);
            check(!okm && hx.empty() && !se.empty(), "a missing model is a reported failure, not a hash: " + se);
        }
    }

    section("Stage 2 - the manners, which decide whether a line is said twice");
    {
        // content overlap drops stopwords, so two lines about the same thing show it
        check(content_overlap("the retry limit is three", "we set the retry limit to twelve") == 2,
              ssprintf("content overlap counts the words that carry the topic (%d)",
                       content_overlap("the retry limit is three", "we set the retry limit to twelve")));
        check(content_overlap("lunch is at noon", "the deploy is on Friday") == 0, "and nothing when the topics differ");
        // near-dup is fusord's own six-in-ten test, over whole words
        check(near_dup("that contradicts what we established it is postgres not mysql",
                       "that contradicts what we established it is postgres 16 not mysql 5"),
              "a line that repeats itself with two numbers added is a near-duplicate");
        check(!near_dup("the staging database is postgres", "lunch is at noon in the small room"),
              "and two different lines are not");
        // THE ACCEPTANCE BUG, in every kernel before K5's F2: "correct" inside "incorrect"
        check(looks_like_acceptance("You're right, it is postgres 16."), "\"you're right\" is an acceptance");
        check(looks_like_acceptance("good catch, my mistake"), "so is \"good catch\"");
        check(!looks_like_acceptance("that is incorrect"), "\"incorrect\" is NOT \"correct\" — whole words only");
        check(!looks_like_acceptance("the correction landed"), "nor is \"correction\"");
        check(!looks_like_acceptance("that is not correct"), "and a negation two words back cancels it");
        check(!looks_like_acceptance("no, you are right about nothing"), "\"no, you are right\" is not an acceptance");
        check(looks_like_acceptance("agreed"), "a bare \"agreed\" is one");
        check(!looks_like_acceptance("we disagreed about the schema"), "and \"disagreed\" is not, because it is one word");
    }

    section("Stage 1d - the tape read back, the torn row, the hash cache, the tick, the fold");
    {
        // canonical strings survive the round trip, and a field comes back as its raw fragment
        {
            const std::string raw = "a\"b\\c\nd\t\xC3\xA9 \x01z";
            check(canon::unstr(canon::str(raw)) == raw, "unstr(str(s)) == s, escapes and a control byte and non-ASCII included");
            check(canon::unstr("\"\\ud83d\\ude00\"") == "\xF0\x9F\x98\x80", "a surrogate pair in \\u escapes decodes to one four-byte character");
            const std::string obj = canon::obj({ { "cs", canon::str("Z:1>1$x") }, { "rev", "7" }, { "kind", canon::str("e") } });
            check(canon::field(obj, "cs") == "\"Z:1>1$x\"" && canon::field(obj, "rev") == "7" && canon::field(obj, "nope").empty(),
                  "field() returns a top-level value's raw fragment, or nothing");
        }
        // the intact scratch tape from the section above reads back row by row
        {
            const std::string path = scratch_path("tape.jsonl");
            std::vector<TapeRow> rows;
            std::string e;
            const bool ok = Tape::read_rows(path, rows, e);
            check(ok && rows.size() == 4 && rows[0].kind == "note" && rows[3].kind == "switch" && rows[3].seq == 3,
                  ssprintf("read_rows: four rows, kinds and seqs as written (%zu rows) %s", rows.size(), e.c_str()));
            uint64_t n = 0, bad = 0;
            std::string head, e2;
            Tape::verify_file(path, n, bad, head, e2);
            check(ok && rows.size() == 4 && rows[3].digest == head, "the last row's digest is the verified head");
        }
        // a torn last row - a crash inside a write - is cut off, counted, and the chain goes on
        {
            const std::string path = scratch_path("tape.jsonl");
            const std::string torn = scratch_path("tape-torn.jsonl");
            std::string text;
            read_file(path, text);
            const std::string fragment = "{\"at\":950,\"body\":{\"half\":\"a row the process died ins";
            std::string e;
            write_file_atomic(torn, text + fragment, e);
            Tape t;
            const bool opened = t.open(torn, "nib:selftest", {}, e);
            check(opened && t.torn_bytes() == fragment.size() && t.rows() == 4,
                  ssprintf("a torn last row opens: %llu bytes cut off, four rows stand (%s)", (unsigned long long)t.torn_bytes(), e.c_str()));
            t.append("note", 990, canon::obj({ { "after", canon::str("the torn row") } }));
            t.close();
            uint64_t n = 0, bad = 0;
            std::string head, e2;
            const bool intact = Tape::verify_file(torn, n, bad, head, e2);
            check(intact && n == 5, ssprintf("and the file verifies INTACT with the row appended after it (%llu rows) %s", (unsigned long long)n, e2.c_str()));
            // a complete-but-wrong last row still refuses: the torn rule is for fragments only
            std::string bad_text = text;
            const size_t k = bad_text.rfind("\"digest\":\"");
            bad_text[k + 10] = bad_text[k + 10] == 'a' ? 'b' : 'a';
            const std::string broken = scratch_path("tape-bad.jsonl");
            write_file_atomic(broken, bad_text, e);
            Tape t2;
            check(!t2.open(broken, "nib:selftest", {}, e), "a complete last row with a wrong digest still refuses: " + e);
        }
        // the hash cache: a second look at an unchanged file is remembered, a changed file is not
        {
            const std::string f = scratch_path("hashme.bin"), cache = scratch_path("hashes.txt");
            std::string e;
            write_file_atomic(f, "the quick brown fox", e);
            DeleteFileA(cache.c_str());
            std::string h1, h2, h3;
            uint64_t b1 = 0, b2 = 0, b3 = 0;
            bool c1 = true, c2 = false, c3 = true;
            const bool ok1 = sha256_file_cached(f, cache, h1, b1, e, c1);
            const bool ok2 = sha256_file_cached(f, cache, h2, b2, e, c2);
            check(ok1 && ok2 && !c1 && c2 && h1 == h2 && h1.size() == 64 && b2 == 19, "the first look hashes, the second is remembered, same digest");
            Sleep(20);
            write_file_atomic(f, "the quick brown fox jumps", e);
            const bool ok3 = sha256_file_cached(f, cache, h3, b3, e, c3);
            check(ok3 && !c3 && h3 != h1 && b3 == 25, "a file that changed size or mtime is hashed again");
        }
        // the resume tick, and the diff
        {
            Compiler c;
            std::vector<Percept> out;
            c.tick(3600, 5000, out);
            check(out.size() == 1 && out[0].kind == 't' && out[0].text == "[tick +3600s]", "a measured tick is one percept: [tick +3600s]");
            const DiffSpan d = diff_texts("hello world", "hello brave new world");
            check(d.at == 6 && d.gone.empty() && d.came == "brave new ", "diff_texts finds the one changed region");
            const DiffSpan d2 = diff_texts("caf\xC3\xA9 au lait", "caf\xC3\xA8 au lait");
            check(d2.at == 3 && d2.gone == "\xC3\xA9" && d2.came == "\xC3\xA8", "and never begins or ends inside a UTF-8 sequence");
        }
        // the pad's modes: the hand's calls during the load are not compiled; history is; then live
        {
            auto src = std::make_unique<PadSource>();
            src->fold_begin();
            src->typed("bo", "typed while loading. ", 100);
            check(src->compiler().percepts() == 0, "while the model loads the pad compiles nothing (the log has it)");
            src->fold_history_begin();
            src->typed("bo", "History one. ", 200);
            src->tick(45, 300);
            src->typed("bo", "History two. ", 400);
            src->fold_history_end();
            src->fold_end(1u << 20);
            const auto shipped = src->take_shipped();
            check(shipped.size() == 3 && shipped[0].text == "History one. " && shipped[1].kind == 't' && shipped[2].text == "History two. " && shipped[0].folded,
                  ssprintf("the history is compiled in order, ticks included, and marked folded (%zu shipped)", shipped.size()));
            src->typed("bo", "Live now. ", 500);
            check(src->take_shipped().size() == 1, "and after fold_end the pad is live again");
        }
        // the fold from the tape: rows after a bound row, an `open` row as a diff, the text carried
        {
            const std::string path = scratch_path("fold.jsonl");
            DeleteFileA(path.c_str());
            Tape t;
            std::string e;
            t.open(path, "nib:fold", {}, e);
            t.append("session_open", 0, canon::obj({ { "doc", canon::str("x") } }));
            const std::string cs1 = make_splice("", 0, 0, "Hello world. ");
            t.append("changeset", 10, canon::obj({ { "rev", "1" }, { "author", canon::str("bo") }, { "kind", canon::str("e") }, { "cs", canon::str(cs1) } }));
            const std::string bound = t.head();   // a checkpoint taken here would bind to this row
            const std::string cs2 = make_splice("Hello world. ", 13, 0, "Second thought. ");
            t.append("changeset", 800, canon::obj({ { "rev", "2" }, { "author", canon::str("bo") }, { "kind", canon::str("e") }, { "cs", canon::str(cs2) } }));
            t.append("session_close", 900, canon::obj({}));
            // a later session opened the file with one more line already in it (edited outside nib)
            t.append("session_open", 0, canon::obj({ { "doc", canon::str("x") } }));
            const std::string opened = "Hello world. Second thought. Third, from outside.";
            t.append("changeset", 5, canon::obj({ { "rev", "1" }, { "author", canon::str("open") }, { "kind", canon::str("o") }, { "cs", canon::str(make_splice("", 0, 0, opened)) } }));
            t.close();
            std::vector<TapeRow> rows;
            const bool found = rows_after(path, bound, rows, e);
            check(found && rows.size() == 4, ssprintf("rows_after finds the bound row and returns the %zu after it %s", rows.size(), e.c_str()));
            auto src = std::make_unique<PadSource>();
            src->fold_history_begin();
            std::string text = "Hello world. ";
            uint64_t clock = 1000;
            const size_t n = fold_tape(rows, text, *src, "bo", clock);
            src->fold_history_end();
            src->fold_end(1u << 20);
            src->flush(clock + 1000);   // the file's last sentence has no separator after it: pending until quiet, as live
            const auto ps = src->take_shipped();
            std::string all;
            for (const auto& p : ps) all += p.text;
            check(n == 2 && text == opened, ssprintf("two rows replayed; the text is carried to the later session's open (%zu)", n));
            check(all.find("Second thought.") != std::string::npos && all.find("Third, from outside.") != std::string::npos &&
                  all.find("Hello world") == std::string::npos,
                  "the world since the checkpoint is what was perceived: the second thought and the outside edit, not the first line");
            std::vector<TapeRow> none;
            check(!rows_after(path, std::string(64, 'f'), none, e), "a digest in no tape is refused: " + e);
        }
    }

    section("the span memory - one entry per boundary, and a want older than it is not placed at line 1");
    {
        // Until 2026-09-05 the wire remembered a span once per SEAT in a 16-slot ring: five
        // boundaries of memory, and a want older than that composed with span zero, whose block
        // then landed after the document's first line. The memory is a struct now, so this can be
        // fired at without a model: three seats per boundary count once, the depth is boundaries
        // and not judgments, and a boundary that has fallen out is reported as unknown.
        SpanMemory sm;
        for (uint32_t b = 1; b <= 100; ++b)
            for (int seat = 0; seat < 3; ++seat) sm.remember(b, 10 + b, b * 10, b * 10 + 5);   // the three seats of one boundary
        check(sm.held() == SpanMemory::kDepth, ssprintf("100 boundaries judged by three seats: %zu held, the depth is boundaries not judgments", sm.held()));
        uint64_t rev = 0;
        uint32_t a = 0, b = 0;
        check(sm.find(100, rev, a, b) && rev == 110 && a == 1000 && b == 1005, "the newest boundary is found with its revision and span");
        const uint32_t oldest = 100 - (uint32_t)SpanMemory::kDepth + 1;
        check(sm.find(oldest, rev, a, b) && rev == 10 + oldest, ssprintf("and so is boundary %u, the oldest still held", oldest));
        check(!sm.find(oldest - 1, rev, a, b) && rev == 0 && a == 0 && b == 0, ssprintf("boundary %u has fallen out and is reported unknown, never as span zero", oldest - 1));
        check(!sm.find(0, rev, a, b), "boundary 0 is never a boundary");
        SpanMemory one;
        one.remember(7, 3, 40, 60); one.remember(7, 3, 40, 60); one.remember(7, 3, 40, 60);
        check(one.held() == 1 && one.find(7, rev, a, b) && a == 40 && b == 60, "three seats of one boundary occupy one slot");
    }

    section("own speech through the document - the sanctioned door, and the fold attributing by author");
    {
        // Through 0.10.1 the thread committed its own line to the trunk before the editor had
        // ruled, and the fold replayed every revision on the hand's lane, so a seat's block came
        // back as "[bo] [SKEPTIC] ...". Now a seat's line is a percept of its own kind, made only
        // when the document holds it, and the fold attributes by author (SPEC 5.1.6 amended,
        // 6.2.11.3.1). None of this needs a model: the resident's half is one raw decode.
        {
            Compiler c;
            std::vector<Percept> out;
            c.typed("bo", "The Pacific is", 1000, 0, 1, out);
            c.own("SKEPTIC", "The Pacific is the largest ocean.", 1500, 20, 2, out);
            check(out.size() == 2 && out[0].kind == 'w' && out[0].text == "The Pacific is" && out[1].kind == 's' && out[1].lane == "SKEPTIC",
                  ssprintf("a seat's line follows the pending clause as a percept of kind s (%zu percepts)", out.size()));
            check(out.size() == 2 && out[1].a == 20 && out[1].b == 20 + out[1].text.size() && out[1].rev == 2,
                  "carrying the block's revision and the span of the line's own bytes");
            check(c.typed_in() == c.typed_out() && c.typed_in() == 14, "and the world's arithmetic does not count the resident's bytes");
        }
        {
            auto srcp = std::make_unique<PadSource>();
            register_seats(*srcp);
            srcp->typed("SKEPTIC", "typed on a seat's lane", 1000);
            srcp->own("SKEPTIC", "said through the door", 1001, 0, 1);
            check(srcp->echoes() == 1 && srcp->pushed() == 1,
                  ssprintf("typed on a seat's lane is still an echo (%llu); own is a percept (%llu pushed)",
                           (unsigned long long)srcp->echoes(), (unsigned long long)srcp->pushed()));
            auricle::fusor::Delta d{};
            PerceptMeta m{};
            const bool got = srcp->poll(d);
            srcp->poll_meta(m);
            check(got && std::string(d.lane) == "SKEPTIC" && m.kind == 's' && std::string(d.payload, d.len) == "said through the door",
                  "and off the ring it rides the seat's lane with kind s in lockstep");
        }
        {
            // the fold from the log: a seat-authored revision comes back as own speech with its
            // prefix and newline stripped; a literal prefix typed by the hand stays the hand's
            Doc doc;
            std::string err;
            doc.splice(0, 0, "The Pacific is the smallest ocean.\n", "bo", err);
            const std::string block = "[SKEPTIC] The Pacific is the largest ocean, not the smallest.\n";
            const bool applied = doc.apply(make_splice(doc.text(), (int64_t)doc.size(), 0, block), "SKEPTIC", err);
            doc.splice((int64_t)doc.size(), 0, "[SKEPTIC] a spoof, typed by the hand.\n", "bo", err);
            auto srcf = std::make_unique<PadSource>();
            register_seats(*srcf);
            const size_t revs = fold_log(doc, *srcf, "bo");
            srcf->flush(mono_ms());
            int own = 0, spoof_w = 0, seat_w = 0;
            std::string own_text;
            size_t own_a = 0, own_b = 0;
            for (const auto& p : srcf->take_shipped()) {
                if (p.kind == 's') { ++own; own_text = p.text; own_a = p.a; own_b = p.b; }
                if (p.kind == 'w' && p.text.find("a spoof") != std::string::npos) ++spoof_w;
                if (p.kind == 'w' && p.text.find("largest ocean") != std::string::npos) ++seat_w;
            }
            check(applied && revs == 3 && own == 1 && own_text == "The Pacific is the largest ocean, not the smallest.",
                  ssprintf("the fold replays the seat's block as its own speech, prefix and newline off (%d): \"%s\"", own, own_text.c_str()));
            check(own_a == 35 + 10 && own_b == own_a + own_text.size(), ssprintf("on the line's own bytes in the document [%zu,%zu)", own_a, own_b));
            check(seat_w == 0, "and never as the hand's words");
            check(spoof_w == 1, "while a literal [SKEPTIC] typed by the hand stays the hand's");
        }
        {
            // the same through the tape's rows, which carry the author
            const std::string path = scratch_path("fold-own.jsonl");
            DeleteFileA(path.c_str());
            Tape t;
            std::string e;
            t.open(path, "nib:fold-own", {}, e);
            t.append("session_open", 0, canon::obj({ { "doc", canon::str("x") } }));
            const std::string bound = t.head();
            const std::string cs1 = make_splice("", 0, 0, "Hello world.\n");
            t.append("changeset", 10, canon::obj({ { "rev", "1" }, { "author", canon::str("bo") }, { "kind", canon::str("e") }, { "cs", canon::str(cs1) } }));
            const std::string cs2 = make_splice("Hello world.\n", 13, 0, "[SPEAKER] Hello yourself.\n");
            t.append("changeset", 900, canon::obj({ { "rev", "2" }, { "author", canon::str("SPEAKER") }, { "kind", canon::str("a") }, { "cs", canon::str(cs2) } }));
            t.close();
            std::vector<TapeRow> rows;
            const bool found = rows_after(path, bound, rows, e);
            auto src = std::make_unique<PadSource>();
            register_seats(*src);
            src->fold_history_begin();
            std::string text;
            uint64_t clock = 1000;
            const size_t n = fold_tape(rows, text, *src, "bo", clock);
            src->fold_history_end();
            src->fold_end(1u << 20);
            src->flush(clock + 1000);
            int own = 0, hand_said_seat = 0;
            std::string own_lane, own_text;
            for (const auto& p : src->take_shipped()) {
                if (p.kind == 's') { ++own; own_lane = p.lane; own_text = p.text; }
                if (p.kind == 'w' && p.text.find("[SPEAKER]") != std::string::npos) ++hand_said_seat;
            }
            check(found && n == 2 && own == 1 && own_lane == "SPEAKER" && own_text == "Hello yourself." && hand_said_seat == 0,
                  ssprintf("the tape's fold attributes by author too: %d own line on %s, %d on the hand's lane", own, own_lane.c_str(), hand_said_seat));
        }
    }

    section("the manners survive the switch - exported, imported, and the ladder as a pure check");
    {
        // A restored resident had its own lines on its trunk and an empty suppression ladder, so a
        // seat could say its piece again after the switch (the SKEPTIC's Pacific line, twice, in
        // two lives, 2026-09-05). The ladder and its memory need no model, so the round trip and
        // the refusals it produces are checked here on an unstarted resident.
        Resident r;
        std::string by;
        check(std::string(r.manners_allows(1, "The Pacific is the largest ocean.", "the pacific", by)).empty(),
              "a fresh resident allows any line");
        const std::string lines =
            "m1.say\tThe Pacific is actually the largest ocean, not the smallest.\n"
            "m1.clause\tActually, the Pacific is the smallest ocean on Earth.\n"
            "m1.age_ms\t5000\nm1.since_i\t2\nm1.resolved\t0\nm1.open\t1\n"
            "m2.say\tDropping the users table is irreversible without a backup.\n"
            "m2.clause\tI am going to drop the users table.\n"
            "m2.age_ms\t400000\nm2.since_i\t3\nm2.resolved\t1\nm2.open\t0\n";
        r.manners_import(lines);
        check(std::string(r.manners_allows(1, "The Pacific is the largest ocean on Earth, not the smallest.", "the pacific again", by)) == "repeat",
              "a restored seat will not repeat the line it said five seconds ago: repeat");
        check(std::string(r.manners_allows(2, "The users table cannot be dropped without a backup.", "drop it", by)) == "resolved",
              "and a line the world settled stays settled across the switch: resolved");
        check(std::string(r.manners_allows(1, "Lunch is at noon in the small room.", "lunch", by)).empty(),
              "while a new topic is allowed");
        const std::string other = r.manners_allows(0, "Dropping the users table is irreversible without a backup.", "drop", by);
        check(other == "repeat_other" && by == "SENTINEL", "and another seat may not say what the SENTINEL already said: " + other + " by " + by);
        const std::string out = r.manners_export();
        check(out.find("m1.say\tThe Pacific is actually") != std::string::npos && out.find("m2.resolved\t1") != std::string::npos &&
              out.find("m2.open\t0") != std::string::npos && out.find("m0.") == std::string::npos,
              "export round-trips what was imported, and nothing for a seat that never spoke");
        Resident r2;
        r2.manners_import("m1.say\tOld news.\nm1.clause\tx\nm1.age_ms\t1000\nm1.since_i\t99\nm1.resolved\t0\nm1.open\t1\n");
        check(std::string(r2.manners_allows(1, "Old news.", "y", by)).empty(),
              "a line said more boundaries ago than the window holds may be said again");
    }

    section("the screen saver - the seat that interrupts nobody, and the mode as a switch");
    {
        // The saver's seat holds the floor by the human's leave, so the refractory budget — an
        // interruption budget — does not apply to it; repeat, repeat_other and resolved still do.
        Resident r;
        std::string by;
        r.manners_import("m0.say\tThe Pacific is the largest ocean.\nm0.clause\tx\nm0.age_ms\t1000\nm0.since_i\t0\nm0.resolved\t0\nm0.open\t1\n");
        const std::string a = "The Atlantic is smaller than the Pacific.";
        check(std::string(r.manners_allows(0, a, "y", by)) == "refractory", "a restatement one second after the seat's last line is refractory for an interrupting seat");
        check(std::string(r.manners_allows(0, a, "y", by, false)).empty(), "and allowed for the saver's seat, which interrupts nobody");
        check(std::string(r.manners_allows(0, "The Pacific is the largest ocean on Earth.", "y", by, false)) == "repeat",
              "while a repeat is still a repeat, saver or not");
        check(!r.saver() && !r.saver_wants(), "the saver is off by default and no want is pending");
        r.set_saver(true);
        check(r.saver() && !r.saver_wants(), "on is a state, not a want: nothing is composed until the seat is addressed");
        r.set_saver(false);
        check(!r.saver(), "and off is off");
        check(std::string(kSaverLane) == "host" && std::string(kSaverAddress).find("Watcher") == 0 && kSaverSeat == 0,
              "the standing instruction addresses the Watcher on the host's lane, and the saver's seat is the SPEAKER");
    }

    section("dave mode - a third cue, and the pin that must not move");
    {
        // WHO, not WHEN. Everything here is pure: no model, no card, no DLL. The whole claim of the
        // mode is that it changes the phrasing and nothing else, and these are the checks that the
        // claim is structurally true rather than merely intended.
        check(serve_hash() == kServeHashPin,
              "the serve hash is still fusord's pin with a third cue in the file - the gate is untouched");

        // The tails are distinct strings, and the unpinned two are NOT the pinned one.
        check(std::string(cue_tail('p')) != std::string(cue_tail('s')) &&
              std::string(cue_tail('p')) != std::string(cue_tail('d')) &&
              std::string(cue_tail('s')) != std::string(cue_tail('d')),
              "the three cues are three different strings");

        // An unknown letter must fall back to the PINNED tail and never to a mode's words.
        check(std::string(cue_tail('z')) == std::string(cue_tail('p')) &&
              std::string(cue_tail(0)) == std::string(cue_tail('p')),
              "a cue letter this build does not know composes with the pinned tail, never with a mode's");

        // And it must never be LABELLED pinned, which is the failure the label exists to prevent.
        check(std::string(cue_name('p')) == "pinned" && std::string(cue_name('s')) == "saver" &&
              std::string(cue_name('d')) == "dave" && std::string(cue_name(0)) == "none" &&
              std::string(cue_name('z')) == "?",
              "every cue names itself on the tape, and an unknown one is '?' and not 'pinned'");

        // Every tail must close the assistant turn the same way, or the frame the tune saw is gone.
        const std::string close = "<|im_end|>\n<|im_start|>assistant\n<think>\n\n</think>\n\n";
        bool ends = true;
        for (char c : { 'p', 's', 'd' }) {
            const std::string t = cue_tail(c);
            ends = ends && t.size() >= close.size() && t.compare(t.size() - close.size(), close.size(), close) == 0;
        }
        check(ends, "all three cues end with the same assistant-turn opener - train and serve agree");

        // Dave's cue carries Dave and not a watcher, and says so before the persona begins.
        const std::string d = cue_tail('d');
        check(d.find("You are Dave.") != std::string::npos &&
              d.find("Set that seat aside") != std::string::npos,
              "the dave cue sets the seat aside in the open and then says who is speaking");
        check(d.find("\xE2\x80\x94") == std::string::npos,
              "and contains no em dash, which the persona itself forbids");

        // THE RESERVE (resident.h kTailReserve). A byte bound, so this needs no tokenizer: no BPE
        // token for ASCII prose averages under two characters, so bytes/2 is a safe upper bound on
        // the token count. If a tail ever outgrows the reserve, this fails here and not on the card.
        size_t worst = 0;
        for (char c : { 'p', 's', 'd' }) worst = worst > strlen(cue_tail(c)) ? worst : strlen(cue_tail(c));
        check((long long)(worst / 2) + 28 + 64 < kTailReserve,
              ssprintf("the longest cue is %zu bytes, under the %lld-token tail reserve with the sentence after it",
                       worst, (long long)kTailReserve));

        // The receipt for an unpinned string: a stable, non-zero hash that is not the serve pin.
        const uint64_t dh = dave_cue_hash();
        check(dh != 0 && dh != kServeHashPin && dh == dave_cue_hash(),
              ssprintf("the dave cue has its own stable hash 0x%016llx, which is not the pin",
                       (unsigned long long)dh));
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
