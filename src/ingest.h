// nib · ingest.h — the compiler, and PadSource.
//
// This is the seam where a document becomes a world. The pad produces edits; the resident
// consumes typed percepts off a lock-free ring. Nothing here touches a model, and nothing here
// may be skipped under load: judgment cadence is negotiable, ingest is not (SPEC 5.1.4).
//
// The interface is auricle's, used UNMODIFIED (SPEC 5.1.1) — `C:\auricle\src\fusor\source.h`.
// nib includes that header rather than copying it, so the two cannot drift.
//
// THREE MEASURED FACTS about that seam, established on this box 2026-09-04 before a line of the
// compiler was written. Each of them is a place where the obvious code would lose data silently:
//
//   1. `sizeof(Delta)` is 528, not the 512 its own comment claims (8 + 16 + 2 + 496 = 522,
//      padded to 528 by the uint64_t). Harmless, but the number is quoted in two specs.
//   2. `fill_delta` reserves the final payload byte for a NUL, so feeding it exactly kPayloadMax
//      (496) bytes yields len = 495 WITH NO SIGNAL. The safe bound is 495, and that is what
//      `kChunkMax` is. SPEC 5.1.7 said 496, which is off by one in the direction that loses a
//      byte quietly — the exact failure CLAUDE.md rule 7 forbids.
//   3. `lane` is a strncpy into 16 bytes, so a lane longer than 15 characters is truncated,
//      also silently. Lanes are checked against that bound at the door.
//
// And a fourth, from Stage 1c: a Delta carries no `kind` and no position. What the resident needs
// beyond the bytes — which percept this is, which revision and which span of the document it
// came from — rides a second ring in lockstep with the first (`PerceptMeta`), pushed only when
// the Delta itself was pushed, so the two can never disagree about order.
#pragma once

#include "fusor/source.h"   // auricle::fusor::{Delta, DeltaRing, StreamingTextSource, fill_delta}

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace nib {

// The largest text a Delta carries losslessly. See fact 2 above: NOT kPayloadMax.
inline constexpr size_t kChunkMax = auricle::fusor::kPayloadMax - 1;   // 495
// The longest lane that survives fill_delta's strncpy intact. See fact 3.
inline constexpr size_t kLaneUsable = auricle::fusor::kLaneMax - 1;    // 15

// What leaves the pad. One percept becomes one Delta, which the resident reads as one bracketed
// line: the trunk sees "\n[lane] text" and splits it into words itself (fusord.cpp:723-746).
// That is why a percept is a CLAUSE and not a word — see the note on Config::chars.
struct Percept {
    std::string lane;
    std::string text;
    uint64_t    wall_ms = 0;
    char        kind = 'w';   // 'w' typed world · 'd' a deletion · 't' an idle tick
    // Where in the document it came from: the byte span [a, b) in the coordinates of revision
    // `rev` (the revision the edit produced). A deletion's span is empty at the point it left;
    // a tick's is the point the pad was at. Judgments inherit these, and the floor rule of
    // Stage 2 is a test on them.
    uint64_t    id = 0;
    uint64_t    rev = 0;
    size_t      a = 0, b = 0;
    bool        folded = false;   // re-perceived by the fold at switch-on, not compiled live
};

// The lane a percept travels on. A tick is not anyone's speech: it goes on the EMPTY lane, which
// the resident reads as "decode this line raw, and judge nothing" — so the trunk sees
// "\n[tick +Ns]" byte-identical to fusord.cpp:712 rather than "\n[bo] [tick +45s]" wrapped in a
// speaker's line, and a tick never fires a probe round (anti-turn exemption 4, fusord.cpp:709-719).
// The kind itself does not survive the ring — `Delta` has no field for it (SPEC 5.1.7) — so the
// lane carries the one bit that matters until auricle's `Delta` gains a `kind` in its padding.
inline const char* delta_lane(const Percept& p) { return p.kind == 't' ? "" : p.lane.c_str(); }

// What rides beside a Delta. Trivially copyable, because it rides an SPSC ring.
struct PerceptMeta {
    uint64_t id;
    uint64_t rev;
    uint64_t wall_ms;
    uint32_t a, b;
    char     kind;
};
using MetaRing = ::auricle::SpscRing<PerceptMeta, 1024>;

// ---------------------------------------------------------------------------------------------
// The compiler. Pure, single-threaded, no ring, no clock of its own — every entry point takes the
// time as an argument, so the whole thing is deterministic and testable without a GPU or a wait.
class Compiler {
public:
    struct Config {
        // N and T are **[OPEN — SPEC 14.3]**: they are to be MEASURED against real typing, not
        // chosen. The values below are provisional and are not evidence of anything.
        //
        // N is deliberately clause-sized rather than word-sized. SPEC 5.1.2 reads as though a
        // word boundary should emit a percept; it must not. The resident prepends "\n[lane] " per
        // Delta and runs a final judge when the line ends, so one percept per word would hand the
        // trunk one bracketed line per word and probe on every one. That is the over-segmentation
        // fusord.cpp:242-254 was tightened to escape, measured on 2026-08-12. A word boundary is
        // therefore where a percept may END, never a reason for it to end.
        size_t  chars       = 160;
        int64_t quiet_ms    = 500;
        // Ticks-as-world (SPEC 5.1.9), byte-identical to fusord.cpp:712 — "[tick +Ns]". Silence
        // is not absence and it is not a question. 0 disables.
        int64_t idle_tick_s = 30;
        // How a deletion reaches the trunk. A removal is information (SPEC 5.1.3) and the world is
        // never edited (5.1.5), so the removed text arrives intact with a marker saying it left.
        // **[OPEN]** — this marker is off-distribution for v11 and is a decision, not a finding.
        std::string removed_mark = "(removed) ";
    };

    explicit Compiler(Config c = Config{}) : cfg_(std::move(c)) {}
    const Config& config() const { return cfg_; }

    // Text arrived at document offset `pos`, producing revision `rev`. Appends to the pending
    // clause and emits whatever is complete. Text that does not continue the pending clause's
    // span (the hand jumped elsewhere) closes it first: a jump is a boundary.
    void typed(const std::string& lane, const std::string& text, uint64_t now_ms,
               size_t pos, uint64_t rev, std::vector<Percept>& out);

    // Text left, at `pos`. Emitted immediately as its own percept, after flushing anything
    // pending, because the order in which the world happened is part of the world.
    void removed(const std::string& lane, const std::string& text, uint64_t now_ms,
                 size_t pos, uint64_t rev, std::vector<Percept>& out);

    // The sequential forms, for a stream with no positions of its own (a file compiled as if
    // typed, a script): the text continues where the last one ended.
    static constexpr size_t kContinue = (size_t)-1;
    void typed(const std::string& lane, const std::string& text, uint64_t now_ms, std::vector<Percept>& out) {
        typed(lane, text, now_ms, kContinue, last_rev_, out);
    }
    void removed(const std::string& lane, const std::string& text, uint64_t now_ms, std::vector<Percept>& out) {
        removed(lane, text, now_ms, kContinue, last_rev_, out);
    }

    // No edit arrived. Emits the pending clause once T ms of quiet have passed.
    void idle(uint64_t now_ms, std::vector<Percept>& out);

    // End of stream, or a lane change: whatever is pending is real and must not be discarded.
    void flush(uint64_t now_ms, std::vector<Percept>& out);

    // A tick the caller has measured itself — the resume tick (SPEC 5.1.9.2): how long the world
    // went on while the resident was away, told to it before anything that happened meanwhile.
    void tick(uint64_t gap_s, uint64_t now_ms, std::vector<Percept>& out);
    // After a replay whose clock was not this one's: the next gap is measured from now, so the
    // first live percept does not read as a silence the length of the replay's whole history.
    void resync_clock(uint64_t now_ms) { last_percept_ms_ = now_ms; last_input_ms_ = now_ms; }

    bool has_pending() const { return !pending_.empty(); }
    size_t pending_size() const { return pending_.size(); }

    // The falsifier's arithmetic (SPEC 5.1.11). Every byte that entered must leave in some percept:
    // typed_in() == typed_out() and removed_in() == removed_out(), always, with nothing pending.
    uint64_t typed_in() const { return typed_in_; }
    uint64_t typed_out() const { return typed_out_; }
    uint64_t removed_in() const { return removed_in_; }
    uint64_t removed_out() const { return removed_out_; }
    uint64_t percepts() const { return percepts_; }
    uint64_t ticks() const { return ticks_; }

private:
    void emit(const std::string& lane, std::string text, uint64_t now_ms, char kind,
              size_t a, uint64_t rev, std::vector<Percept>& out);
    void maybe_tick(uint64_t now_ms, std::vector<Percept>& out);
    void drain(uint64_t now_ms, std::vector<Percept>& out);
    void push_pending(uint64_t now_ms, std::vector<Percept>& out);

    Config cfg_;
    std::string pending_;        // the clause being accumulated
    std::string pending_lane_;
    size_t pending_at_ = 0;      // document offset of pending_'s first byte, in pending_rev_'s frame
    uint64_t pending_rev_ = 0;
    size_t last_pos_ = 0;        // where the pad was last, for a tick's point
    uint64_t last_rev_ = 0;
    uint64_t last_input_ms_ = 0;
    uint64_t last_percept_ms_ = 0;

    uint64_t typed_in_ = 0, typed_out_ = 0;
    uint64_t removed_in_ = 0, removed_out_ = 0;
    uint64_t percepts_ = 0, ticks_ = 0;
};

// ---------------------------------------------------------------------------------------------
// Helpers the compiler uses, exposed because the selftest fires directly at them.

// The largest prefix of `s` that is <= limit bytes and does not split a UTF-8 sequence.
size_t utf8_safe_cut(const std::string& s, size_t limit);
// The last word boundary at or before `limit`, or 0 if the run has none (a long unbroken token).
size_t last_word_cut(const std::string& s, size_t limit);
// Where a sentence ends within `s` (one past '.', '!' or '?' at a word end, or past a newline),
// or npos. Matches fusord's TIGHTENED boundary set: ';' and ':' are syntax, not thought.
size_t sentence_cut(const std::string& s);

// ---------------------------------------------------------------------------------------------
// PadSource — the pad, presented to the resident as a live stream.
//
// Producer side (the editor thread) calls typed/removed/idle. Consumer side (the resident thread)
// calls poll, then poll_meta. Between them are two SPSC rings in lockstep, so the editor never
// blocks on the mind and the mind never blocks on the editor.
//
// **NEVER PUT ONE OF THESE ON THE STACK.** It embeds the 1024-slot ring by value, and a Delta is
// 528 bytes, so a PadSource is ~560 KB — over half of a default 1 MB thread stack. Declaring one
// as a local overflows the stack at construction, which presents as an instant silent crash with
// no output at all (exit 0xC00000FD). Found exactly that way on 2026-09-04. Heap-allocate it.
//
// A full ring no longer drops: the percept waits in a SPOOL on the producer's side and is pushed
// when the ring has room (`pump`, from the editor's timer). Delay is legal — judgment may be
// delayed; the world is never edited — and the spool's depth is on the status line. Only the
// spool's own cap (a quarter million percepts) drops, and that is counted loudly as before.
class PadSource final : public auricle::fusor::StreamingTextSource {
public:
    explicit PadSource(Compiler::Config c = Compiler::Config{}) : comp_(std::move(c)) {}

    // --- producer side --------------------------------------------------------------------
    void typed(const std::string& lane, const std::string& text, uint64_t now_ms, size_t pos, uint64_t rev);
    void removed(const std::string& lane, const std::string& text, uint64_t now_ms, size_t pos, uint64_t rev);
    void typed(const std::string& lane, const std::string& text, uint64_t now_ms) { typed(lane, text, now_ms, Compiler::kContinue, 0); }
    void removed(const std::string& lane, const std::string& text, uint64_t now_ms) { removed(lane, text, now_ms, Compiler::kContinue, 0); }
    void idle(uint64_t now_ms);
    void flush(uint64_t now_ms);
    void pump();                        // move spooled percepts onto the rings while they have room

    // The fold (Stage 1c): the document's history replayed into the compiler at switch-on. Between
    // fold_begin and fold_end nothing is shipped; fold_end ships the LAST percepts that fit
    // `budget_bytes` and counts the rest as skipped, loudly — a window is only so long, and until
    // the molt (Stage 2) a resident that joins a long document joins it part-way, and says so.
    //
    // Stage 1d: the model loads for seconds, and the hand keeps typing. Between fold_begin and
    // the history fold the pad IGNORES the hand's live calls: every one of them is in the
    // document's log with its own clock, and the fold replays the log (or the tape) at Ready, so
    // compiling them here as well would perceive them twice. The history is compiled between
    // fold_history_begin and fold_history_end — the switch-on knows what history only once the
    // model has said whether it restored — and fold_end ships it and returns the pad to Live.
    void fold_begin();
    void fold_history_begin();
    void fold_history_end();
    void fold_end(size_t budget_bytes);
    void tick(uint64_t gap_s, uint64_t now_ms);          // compiled at once (into the fold while folding)
    void resync_clock(uint64_t now_ms) { comp_.resync_clock(now_ms); }
    uint64_t fold_shipped() const { return fold_shipped_; }
    uint64_t fold_skipped() const { return fold_skipped_; }
    uint64_t fold_skipped_bytes() const { return fold_skipped_bytes_; }

    // The resident's own seats. A delta on one of these lanes MUST NOT be fed back (SPEC 5.1.6):
    // in a pad the resident writes into the buffer it reads, so the filter lives here, at the
    // source, and not downstream where it would already have cost a decode.
    void add_seat(const std::string& lane);
    bool is_seat(const std::string& lane) const;

    // --- consumer side (StreamingTextSource) ------------------------------------------------
    bool poll(auricle::fusor::Delta& out) override { return ring_.try_pop(out); }
    bool poll_meta(PerceptMeta& out) { return meta_.try_pop(out); }
    void stop() override {}
    const char* name() const override { return "pad"; }

    // --- what the status line and the tape read ----------------------------------------------
    size_t pending() const { return ring_.size(); }          // how far behind the mind is running
    size_t spooled() const { return spool_.size(); }         // waiting for room on the ring
    uint64_t pushed() const { return pushed_; }
    uint64_t dropped() const { return dropped_; }            // the spool's cap — counted LOUDLY (rule 7)
    uint64_t echoes() const { return echoes_; }              // self-echo filtered at the door
    uint64_t truncated_lanes() const { return trunc_lanes_; }
    const Compiler& compiler() const { return comp_; }
    // Every percept shipped since the last call, for the tape (the editor thread appends them).
    std::vector<Percept> take_shipped();

private:
    void ship(std::vector<Percept>& ps);
    bool push_one(const Percept& p);

    Compiler comp_;
    auricle::fusor::DeltaRing ring_;
    MetaRing meta_;
    std::vector<std::string> seats_;
    std::vector<Percept> scratch_;
    std::deque<Percept> spool_;
    std::vector<Percept> shipped_;      // for the tape
    std::vector<Percept> fold_;         // held between fold_begin and fold_end
    // Live: the hand's calls compile and ship. Wait: the model is loading; the calls are the log's
    // to replay and are ignored here. Hist: the fold is compiling history into `fold_`.
    enum class Mode { Live, Wait, Hist } mode_ = Mode::Live;
    uint64_t next_id_ = 1;
    uint64_t pushed_ = 0, dropped_ = 0, echoes_ = 0, trunc_lanes_ = 0;
    uint64_t fold_shipped_ = 0, fold_skipped_ = 0, fold_skipped_bytes_ = 0;
    static constexpr size_t kSpoolMax = 262144;
};

}  // namespace nib
