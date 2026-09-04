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
#pragma once

#include "fusor/source.h"   // auricle::fusor::{Delta, DeltaRing, StreamingTextSource, fill_delta}

#include <cstdint>
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
};

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

    // Text arrived. Appends to the pending clause and emits whatever is complete.
    void typed(const std::string& lane, const std::string& text, uint64_t now_ms,
               std::vector<Percept>& out);

    // Text left. Emitted immediately as its own percept, after flushing anything pending, because
    // the order in which the world happened is part of the world.
    void removed(const std::string& lane, const std::string& text, uint64_t now_ms,
                 std::vector<Percept>& out);

    // No edit arrived. Emits the pending clause once T ms of quiet have passed.
    void idle(uint64_t now_ms, std::vector<Percept>& out);

    // End of stream, or a lane change: whatever is pending is real and must not be discarded.
    void flush(uint64_t now_ms, std::vector<Percept>& out);

    bool has_pending() const { return !pending_.empty(); }
    size_t pending_size() const { return pending_.size(); }

    // The falsifier's arithmetic (SPEC 11.6). Every byte that entered must leave in some percept:
    // typed_in() == typed_out() and removed_in() == removed_out(), always, with nothing pending.
    uint64_t typed_in() const { return typed_in_; }
    uint64_t typed_out() const { return typed_out_; }
    uint64_t removed_in() const { return removed_in_; }
    uint64_t removed_out() const { return removed_out_; }
    uint64_t percepts() const { return percepts_; }
    uint64_t ticks() const { return ticks_; }

private:
    void emit(const std::string& lane, std::string text, uint64_t now_ms, char kind,
              std::vector<Percept>& out);
    void maybe_tick(uint64_t now_ms, std::vector<Percept>& out);
    void drain(uint64_t now_ms, std::vector<Percept>& out);
    void push_pending(uint64_t now_ms, std::vector<Percept>& out);

    Config cfg_;
    std::string pending_;        // the clause being accumulated
    std::string pending_lane_;
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
// calls poll. Between them is auricle's SPSC ring, so the editor never blocks on the mind and the
// mind never blocks on the editor.
//
// **NEVER PUT ONE OF THESE ON THE STACK.** It embeds the 1024-slot ring by value, and a Delta is
// 528 bytes, so a PadSource is ~528 KB — over half of a default 1 MB thread stack. Declaring one
// as a local overflows the stack at construction, which presents as an instant silent crash with
// no output at all (exit 0xC00000FD). Found exactly that way on 2026-09-04. Heap-allocate it.
class PadSource final : public auricle::fusor::StreamingTextSource {
public:
    explicit PadSource(Compiler::Config c = Compiler::Config{}) : comp_(std::move(c)) {}

    // --- producer side --------------------------------------------------------------------
    void typed(const std::string& lane, const std::string& text, uint64_t now_ms);
    void removed(const std::string& lane, const std::string& text, uint64_t now_ms);
    void idle(uint64_t now_ms);
    void flush(uint64_t now_ms);

    // The resident's own seats. A delta on one of these lanes MUST NOT be fed back (SPEC 5.1.6):
    // in a pad the resident writes into the buffer it reads, so the filter lives here, at the
    // source, and not downstream where it would already have cost a decode.
    void add_seat(const std::string& lane);
    bool is_seat(const std::string& lane) const;

    // --- consumer side (StreamingTextSource) ------------------------------------------------
    bool poll(auricle::fusor::Delta& out) override { return ring_.try_pop(out); }
    void stop() override {}
    const char* name() const override { return "pad"; }

    // --- what the status line and the tape read ----------------------------------------------
    size_t pending() const { return ring_.size(); }          // how far behind the mind is running
    uint64_t pushed() const { return pushed_; }
    uint64_t dropped() const { return dropped_; }            // ring full — counted LOUDLY (rule 7)
    uint64_t echoes() const { return echoes_; }              // self-echo filtered at the door
    uint64_t truncated_lanes() const { return trunc_lanes_; }
    const Compiler& compiler() const { return comp_; }

private:
    void ship(std::vector<Percept>& ps);

    Compiler comp_;
    auricle::fusor::DeltaRing ring_;
    std::vector<std::string> seats_;
    std::vector<Percept> scratch_;
    uint64_t pushed_ = 0, dropped_ = 0, echoes_ = 0, trunc_lanes_ = 0;
};

}  // namespace nib
