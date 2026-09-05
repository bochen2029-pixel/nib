// nib · twin.h — the replay twin (Stage 4b, SPEC 6.1.6).
//
// `nib --twin TAPE` re-drives a tape's world through a turn-based policy, offline, on the same
// weights: a fresh context per wake, the seed and the human's percepts so far prefilled in the
// serve format, one judgment per wake through the pinned probe, the same cue, sampler, cap and
// manners — and prints, wake by wake, what the twin did beside what the resident did at the
// boundaries inside that turn, the gap between them, the percepts the twin was blind to while it
// composed, and the table of which scaffold was in play in which arm. This is the matched-input
// half of the twin race (D1's four treatments: no decode-on-delta, no persistent KV, no mid-decode
// abort, no boundary-grain judgment); the product's mode switch is the other half, observational.
//
// The pure parts — reading the rows onto one clock and choosing the wakes — are here so that
// --selftest fires at them with no model.
#pragma once
#include "tape.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nib {

// One thing on the tape, on one clock (sessions chained so it never runs backwards, as the fold
// chains them). `kind` is a percept's kind (w · d · s · t) or the row kind for the resident's own
// record (judgment · emit · refused · abort · ask).
struct TwinEv {
    uint64_t t = 0;
    std::string kind;
    std::string lane, text, seat, why;
    char trigger = 0;
    uint64_t i = 0;          // the boundary, for the resident's rows
    float m[3] = { 0, 0, 0 }; // a judgment's three margins, in seat order
    float margin = 0;        // an emit's, a refusal's, an abort's
};

// The rows onto one clock. `model` and `lane` are what the session rows say; `model` stays empty
// if no session row is on the tape.
void twin_collect(const std::vector<TapeRow>& rows, std::vector<TwinEv>& out, std::string& model, std::string& lane);

// The wake policy: a wake `pause_ms` after every human percept that no other human percept follows
// within `pause_ms` — the resident's own floor, applied to the twin, so the twin is asked exactly
// when the resident's floor would have opened. Times are the tape's clock.
std::vector<uint64_t> twin_wakes_by_pause(const std::vector<TwinEv>& ev, uint64_t pause_ms);
// The tape's own `ask` rows (a TURN-BASED session): the twin is asked when the hand asked.
std::vector<uint64_t> twin_wakes_by_ask(const std::vector<TwinEv>& ev);
// A schedule: every `every_ms` from the first human percept to the last.
std::vector<uint64_t> twin_wakes_every(const std::vector<TwinEv>& ev, uint64_t every_ms);

int do_twin(int argc, char** argv);

}  // namespace nib
