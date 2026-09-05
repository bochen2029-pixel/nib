// nib · twin.cpp — the replay twin. See twin.h.
#include "twin.h"

#include "resident.h"
#include "util.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace nib {

namespace {

double fnum(const std::string& body, const char* key) {
    const std::string f = canon::field(body, key);
    return f.empty() ? 0.0 : strtod(f.c_str(), nullptr);
}
std::string fstr(const std::string& body, const char* key) { return canon::unstr(canon::field(body, key)); }

bool is_human(const TwinEv& e) { return e.kind == "w" || e.kind == "d"; }

bool is_seat_name(const std::string& a) {
    for (int s = 0; s < 3; ++s) {
        const char* n = seats()[s].name;
        size_t i = 0;
        for (; i < a.size() && n[i]; ++i) {
            char x = a[i], y = n[i];
            if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
            if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
            if (x != y) break;
        }
        if (i == a.size() && n[i] == 0) return true;
    }
    return false;
}

// The hand's clock: every keystroke is a changeset row on the tape, and the resident's floor is a
// keystroke clock, so the wake policy reads those. A tape with no keystroke rows (a script's) falls
// back to the human percepts.
std::vector<uint64_t> hand_times(const std::vector<TwinEv>& ev) {
    std::vector<uint64_t> k;
    for (const TwinEv& e : ev) if (e.kind == "k") k.push_back(e.t);
    if (k.empty()) for (const TwinEv& e : ev) if (is_human(e)) k.push_back(e.t);
    return k;
}

}  // namespace

void twin_collect(const std::vector<TapeRow>& rows, std::vector<TwinEv>& out, std::string& model, std::string& lane) {
    out.clear();
    uint64_t base = 0, last = 0;
    uint64_t sess_epoch = 0, sess_last_at = 0;   // the session's wall-clock start, and how far its own clock ran
    for (const TapeRow& r : rows) {
        const uint64_t at = (uint64_t)(r.at < 0 ? 0 : r.at);
        uint64_t t = base + at;
        if (r.kind == "session_open") {
            // Sessions are chained so the clock never runs backwards. The gap between them is the
            // wall clock's where both rows carry an epoch — the human really was away that long,
            // and a wake policy should see it — and one millisecond where they do not (the fold's
            // rule: a gap is never invented).
            const uint64_t epoch = (uint64_t)fnum(r.body, "epoch_ms");
            uint64_t gap = 1;
            if (epoch && sess_epoch && epoch > sess_epoch + sess_last_at) gap = epoch - (sess_epoch + sess_last_at);
            base = last + gap;
            t = base + at;
            sess_epoch = epoch;
            sess_last_at = 0;
            if (lane.empty()) lane = fstr(r.body, "lane");
        }
        if (at > sess_last_at) sess_last_at = at;
        if (t < last) { base = last + 1 - at; t = last + 1; }
        last = t;
        TwinEv e;
        e.t = t;
        if (r.kind == "session") {
            if (model.empty()) model = fstr(r.body, "model");
            e.kind = "on";   // a resident came up here: wakes before the first are paired against nobody
        } else if (r.kind == "changeset") {
            // a keystroke, when the hand made it: the floor's own clock. A seat's block and the
            // open row are not the hand.
            const std::string author = fstr(r.body, "author");
            if (author == "open" || is_seat_name(author)) continue;
            e.kind = "k";
        } else if (r.kind == "percept") {
            e.kind = fstr(r.body, "kind");
            e.lane = fstr(r.body, "lane");
            e.text = fstr(r.body, "text");
            // A folded percept is the fold re-perceiving history at a switch-on: world the tape
            // already holds as a live percept of an earlier life, unless nothing perceived it live
            // (the paragraph typed before the first switch-on; an edit made while nib was closed).
            // The twin's world is every human edit once, so a folded duplicate is skipped. The
            // revision rides in `why`, which a percept does not otherwise use.
            e.why = canon::field(r.body, "rev");
            if (canon::field(r.body, "folded") == "true" && (e.kind == "w" || e.kind == "d")) {
                bool seen = false;
                for (const TwinEv& x : out)
                    if (x.kind == e.kind && x.text == e.text && x.lane == e.lane && x.why == e.why) { seen = true; break; }
                if (seen) continue;
            }
        } else if (r.kind == "tick") {
            e.kind = "t";
            e.text = fstr(r.body, "text");
        } else if (r.kind == "judgment") {
            e.kind = "judgment";
            e.i = (uint64_t)fnum(r.body, "i");
            e.text = fstr(r.body, "clause");
            const std::string mg = canon::field(r.body, "margins");
            for (int s = 0; s < 3; ++s) e.m[s] = (float)fnum(mg, seats()[s].name);
        } else if (r.kind == "emit" || r.kind == "refused" || r.kind == "abort") {
            e.kind = r.kind;
            e.i = (uint64_t)fnum(r.body, "i");
            e.seat = fstr(r.body, "seat");
            e.text = r.kind == "abort" ? fstr(r.body, "aired") : fstr(r.body, "say");
            e.margin = (float)fnum(r.body, "m");
            e.why = fstr(r.body, "why");
            const std::string tg = fstr(r.body, "trigger");
            e.trigger = tg.empty() ? 0 : tg[0];
        } else if (r.kind == "ask") {
            e.kind = "ask";
        } else {
            continue;
        }
        out.push_back(std::move(e));
    }
}

std::vector<uint64_t> twin_wakes_by_pause(const std::vector<TwinEv>& ev, uint64_t pause_ms) {
    std::vector<uint64_t> w;
    const std::vector<uint64_t> hand = hand_times(ev);
    for (size_t i = 0; i < hand.size(); ++i) {
        const bool last = i + 1 == hand.size();
        if (last || hand[i + 1] > hand[i] + pause_ms) w.push_back(hand[i] + pause_ms);
    }
    return w;
}

std::vector<uint64_t> twin_wakes_by_ask(const std::vector<TwinEv>& ev) {
    std::vector<uint64_t> w;
    for (const TwinEv& e : ev) if (e.kind == "ask") w.push_back(e.t);
    return w;
}

std::vector<uint64_t> twin_wakes_every(const std::vector<TwinEv>& ev, uint64_t every_ms) {
    std::vector<uint64_t> w;
    if (every_ms == 0) return w;
    const std::vector<uint64_t> hand = hand_times(ev);
    if (hand.empty()) return w;
    for (uint64_t t = hand.front() + every_ms; t <= hand.back() + every_ms; t += every_ms) w.push_back(t);
    return w;
}

namespace {

int seat_index(const std::string& name) {
    for (int s = 0; s < 3; ++s) if (name == seats()[s].name) return s;
    return -1;
}

std::string secs(uint64_t ms) { return ssprintf("%.1f", ms / 1000.0); }

}  // namespace

int do_twin(int argc, char** argv) {
    std::string tape_path, out_path, policy = "pause";
    uint64_t pause_ms = 2000, every_ms = 0;
    Resident::Config rc;
    bool exact = false, quiet = false, no_emit = false;
    for (int i = 2; i < argc; ++i) {
        const std::string f = argv[i];
        if (f == "--wake" && i + 1 < argc) {
            const std::string v = argv[++i];
            if (v.rfind("pause:", 0) == 0) { policy = "pause"; pause_ms = (uint64_t)(atof(v.c_str() + 6) * 1000.0); }
            else if (v == "ask") policy = "ask";
            else if (v.rfind("every:", 0) == 0) { policy = "every"; every_ms = (uint64_t)(atof(v.c_str() + 6) * 1000.0); }
            else { fprintf(stderr, "nib: --wake pause:S | ask | every:S\n"); return 2; }
        }
        else if (f == "--model" && i + 1 < argc) rc.model = argv[++i];
        else if (f == "--llama-dir" && i + 1 < argc) rc.llama_dir = argv[++i];
        else if (f == "--ctx" && i + 1 < argc) rc.n_ctx = atoi(argv[++i]);
        else if (f == "--gpu-layers" && i + 1 < argc) rc.n_gpu_layers = atoi(argv[++i]);
        else if (f == "--out" && i + 1 < argc) out_path = argv[++i];
        else if (f == "--verbose") rc.verbose = true;
        else if (f == "--allow-cpu") rc.allow_cpu = true;
        else if (f == "--no-emit") no_emit = true;
        else if (f == "--exact") exact = true;
        else if (f == "--quiet") quiet = true;
        else if (tape_path.empty()) tape_path = f;
    }
    if (tape_path.empty()) { fprintf(stderr, "nib: --twin needs a tape\n"); return 2; }
    rc.emit = !no_emit;   // a twin with no mouth is a judgment-only replay
    if (out_path.empty()) out_path = tape_path + ".twin.jsonl";

    // ---- the tape ------------------------------------------------------------------------
    std::vector<TapeRow> rows;
    std::string err;
    if (!Tape::read_rows(tape_path, rows, err)) { fprintf(stderr, "nib: %s\n", err.c_str()); return 2; }
    std::vector<TwinEv> ev;
    std::string model_on_tape, lane;
    twin_collect(rows, ev, model_on_tape, lane);
    if (lane.empty()) lane = "bo";
    if (rc.model == Resident::Config().model && !model_on_tape.empty()) rc.model = model_on_tape;
    std::vector<uint64_t> wakes = policy == "ask" ? twin_wakes_by_ask(ev)
                                : policy == "every" ? twin_wakes_every(ev, every_ms)
                                : twin_wakes_by_pause(ev, pause_ms);
    size_t human_n = 0, res_judg = 0, res_emit = 0, res_refused = 0, res_abort = 0, keys_n = 0;
    uint64_t on_since = 0;   // the first session row: the resident was not there before it
    for (const TwinEv& e : ev) {
        if (e.kind == "on") { if (!on_since) on_since = e.t; }
        else if (e.kind == "k") ++keys_n;
        else if (is_human(e)) ++human_n;
        else if (e.kind == "judgment") ++res_judg;
        else if (e.kind == "emit") ++res_emit;
        else if (e.kind == "refused") ++res_refused;
        else if (e.kind == "abort") ++res_abort;
    }
    printf("nib --twin %s\n%zu rows: %zu keystrokes, %zu human percepts once each; %zu judgments, %zu emissions, %zu refusals, %zu aborts on the resident's side%s\n",
           tape_path.c_str(), rows.size(), keys_n, human_n, res_judg, res_emit, res_refused, res_abort,
           keys_n ? "" : " (no keystroke rows: the wakes read the percepts)");
    printf("policy %s%s: %zu wakes · model %s\n", policy.c_str(),
           policy == "pause" ? (":" + secs(pause_ms) + "s").c_str() : policy == "every" ? (":" + secs(every_ms) + "s").c_str() : "",
           wakes.size(), rc.model.c_str());
    if (wakes.empty()) { printf("nothing to wake for\n"); return 0; }

    // ---- the twin ------------------------------------------------------------------------
    Resident res;
    printf("loading ...\n");
    fflush(stdout);
    if (!res.start(rc, err)) { fprintf(stderr, "nib: %s\n", err.c_str()); return 2; }
    Tape out;
    if (!out.open(out_path, "nib:twin", { { "tool", canon::str("nib") }, { "version", canon::str(kVersion) },
                                          { "tape", canon::str(tape_path) }, { "policy", canon::str(policy) },
                                          { "pause_ms", canon::num((int64_t)pause_ms) }, { "every_ms", canon::num((int64_t)every_ms) },
                                          { "model", canon::str(rc.model) }, { "exact", canon::boolean(exact) } }, err)) {
        fprintf(stderr, "nib: %s\n", err.c_str());
        return 2;
    }
    out.append("session", 0, canon::obj({ { "model", canon::str(rc.model) }, { "model_desc", canon::str(res.model_desc()) },
                                          { "serve_hash", canon::str(ssprintf("0x%016llx", (unsigned long long)serve_hash())) },
                                          { "n_ctx", canon::num(rc.n_ctx) }, { "arm", canon::str("twin") }, { "emit", canon::boolean(rc.emit) },
                                          { "gen_cap", canon::num(rc.gen_cap) }, { "gen_min", canon::num(rc.gen_min) },
                                          { "sampler", canon::str("min_p 0.05 -> temp 0.7 -> dist 11, re-seeded per wake") } }));

    struct Own { uint64_t t; int seat; std::string say; };
    std::vector<Own> own;
    size_t twin_emit = 0, twin_hold = 0, twin_supp = 0, blind_total = 0, both_emit = 0, both_hold = 0, twin_only = 0, res_only = 0;
    std::vector<double> gaps;
    uint64_t prefill_toks = 0, prefill_ms = 0, gen_ms_total = 0;
    uint64_t t_prev = 0;
    for (size_t k = 0; k < wakes.size(); ++k) {
        const uint64_t tk = wakes[k];
        // a fresh context: the seed, then the world through tk — the human's lines and the twin's
        // own, merged by time — prefilled in the serve format and judged never
        const uint64_t p0 = auricle::fusor::now_ms();
        if (!res.reseed()) { fprintf(stderr, "nib: reseed failed at wake %zu\n", k); return 5; }
        size_t oi = 0;
        std::string last_human;
        uint64_t last_human_t = 0;
        for (const TwinEv& e : ev) {
            if (e.t > tk) break;
            while (oi < own.size() && own[oi].t <= e.t) { res.own_line(seats()[own[oi].seat].name, own[oi].say, own[oi].t); ++oi; }
            if (is_human(e)) {
                res.world_line(e.lane, e.text, exact);   // --exact: word by word, in the resident's own batches
                last_human = e.text;
                last_human_t = e.t;
            } else if (e.kind == "t") {
                res.world_line(std::string(), e.text);
            }
            if (res.failed() || res.window_full()) break;
        }
        while (oi < own.size() && own[oi].t <= tk) { res.own_line(seats()[own[oi].seat].name, own[oi].say, own[oi].t); ++oi; }
        prefill_toks += (uint64_t)res.context_used();
        prefill_ms += auricle::fusor::now_ms() - p0;
        if (res.failed()) { printf("FAILED at wake %zu: %s\n", k, res.failure().c_str()); return 5; }
        if (res.window_full()) { printf("WINDOW FULL at wake %zu: the transcript no longer fits %d tokens; stopping here\n", k, rc.n_ctx); break; }
        if (last_human.empty()) { t_prev = tk; continue; }   // a wake with nothing to be asked about

        // one judgment of the whole transcript, then the same composition the resident makes
        std::vector<Judgment> js;
        res.judge_wake(last_human, js);
        float tm[3] = { -1e9f, -1e9f, -1e9f };
        for (const Judgment& j : js) tm[j.seat] = j.margin;
        const uint64_t g0 = auricle::fusor::now_ms();
        res.speak_wants(nullptr, 'w');
        const uint64_t gen_ms = auricle::fusor::now_ms() - g0;
        gen_ms_total += gen_ms;
        std::vector<Emission> em = res.take_emissions();
        std::vector<Suppressed> sp = res.take_suppressed();
        for (const Emission& e : em) {
            own.push_back(Own{ tk + e.gen_ms, e.seat, e.say });
            res.own_line(seats()[e.seat].name, e.say, tk + e.gen_ms);   // the twin hears itself, as the resident does
        }
        // what the twin was blind to: the world that arrived while it composed
        size_t blind = 0;
        for (const TwinEv& e : ev) if (is_human(e) && e.t > tk && e.t <= tk + gen_ms) ++blind;
        blind_total += blind;

        // The resident's side of the same turn. Its floor opens at the same instant the twin is
        // asked (the pause), but its line lands half a second or so later — the composition, the
        // editor's poll, the second gate — so the turn's window runs a little past both wakes:
        // rows in (t_prev + slack, tk + slack]. A line landing later than that is the next turn's.
        const uint64_t slack = 1500;
        float rm[3] = { -1e9f, -1e9f, -1e9f };
        size_t rb = 0;
        std::vector<const TwinEv*> r_emit, r_ref, r_abort;
        for (const TwinEv& e : ev) {
            if (e.t <= t_prev + (t_prev ? slack : 0) || e.t > tk + slack) continue;
            if (e.kind == "judgment") { ++rb; for (int s = 0; s < 3; ++s) if (e.m[s] > rm[s]) rm[s] = e.m[s]; }
            else if (e.kind == "emit") r_emit.push_back(&e);
            else if (e.kind == "refused") r_ref.push_back(&e);
            else if (e.kind == "abort") r_abort.push_back(&e);
        }
        // print and record
        std::string cl = last_human;
        while (!cl.empty() && (cl.back() == '\n' || cl.back() == '\r')) cl.pop_back();
        if (cl.size() > 60) cl = cl.substr(0, 57) + "...";
        const bool res_on = on_since && tk >= on_since;
        if (!quiet) printf("\nwake %zu  t=+%ss  after \"%s\"  (%d tokens prefilled; %s; the twin was blind to %zu percepts)\n",
                           k + 1, secs(tk).c_str(), cl.c_str(), res.context_used(),
                           res_on ? ssprintf("the resident judged %zu boundaries in this turn", rb).c_str() : "the resident was not on yet: it folded this later",
                           blind);
        std::vector<std::string> twin_rows;
        for (int s = 0; s < 3; ++s) {
            const Emission* said = nullptr;
            const Suppressed* held = nullptr;
            for (const Emission& e : em) if (e.seat == s) said = &e;
            for (const Suppressed& x : sp) if (x.seat == s) held = &x;
            const TwinEv* r_said = nullptr;
            for (const TwinEv* e : r_emit) if (seat_index(e->seat) == s) r_said = e;
            const bool tw = said != nullptr, rs = r_said != nullptr;
            if (res_on) {
                if (tw && rs) ++both_emit; else if (!tw && !rs && tm[s] <= 0 && rm[s] <= 0) ++both_hold; else if (tw && !rs) ++twin_only; else if (!tw && rs) ++res_only;
            }
            if (tw) ++twin_emit; else if (held) ++twin_supp; else ++twin_hold;
            double gap = 0;
            if (tw && rs) { gap = ((double)(tk + said->gen_ms) - (double)r_said->t) / 1000.0; gaps.push_back(gap); }
            if (!quiet) {
                std::string left = ssprintf("  %-8s twin %+5.1f ", seats()[s].name, (double)tm[s]);
                left += tw ? ssprintf("-> \"%s\" (%llu ms)", said->say.c_str(), (unsigned long long)said->gen_ms)
                      : held ? ssprintf("-- held (%s)", held->why.c_str())
                      : tm[s] > 0 ? "-- wanted, no line" : "-- hold";
                std::string right = res_on ? ssprintf("resident max %+5.1f ", (double)(rm[s] > -1e8f ? rm[s] : 0.0f)) : std::string("resident ");
                right += rs ? ssprintf("-> \"%s\" at +%ss (%c)", r_said->text.c_str(), secs(r_said->t).c_str(), r_said->trigger ? r_said->trigger : '?')
                       : !res_on ? "-- not on yet" : rm[s] > -1e8f ? "-- hold" : "-- no boundary";
                if (tw && rs) right += ssprintf("  gap %+.1f s", gap);
                printf("%s\n      | %s\n", left.c_str(), right.c_str());
            }
            twin_rows.push_back(canon::obj({ { "seat", canon::str(seats()[s].name) }, { "twin_m", canon::flt(tm[s]) },
                                             { "twin_say", canon::str(tw ? said->say : std::string()) },
                                             { "twin_held", canon::str(held ? held->why : std::string()) },
                                             { "res_m", canon::flt(rm[s] > -1e8f ? rm[s] : 0.0f) },
                                             { "res_say", canon::str(rs ? r_said->text : std::string()) },
                                             { "res_t", canon::num(rs ? (int64_t)r_said->t : 0) },
                                             { "gap_s", canon::flt(gap) } }));
        }
        out.append("wake", (int64_t)tk, canon::obj({ { "k", canon::num((int64_t)k + 1) }, { "after", canon::str(last_human) },
                                                    { "after_t", canon::num((int64_t)last_human_t) },
                                                    { "prefill_tokens", canon::num(res.context_used()) }, { "gen_ms", canon::num((int64_t)gen_ms) },
                                                    { "blind", canon::num((int64_t)blind) }, { "res_aborts", canon::num((int64_t)r_abort.size()) },
                                                    { "res_refused", canon::num((int64_t)r_ref.size()) }, { "seats", canon::arr(twin_rows) } }));
        t_prev = tk;
    }

    // ---- the aggregate, and the scaffold table ----------------------------------------------
    std::sort(gaps.begin(), gaps.end());
    double gmean = 0;
    for (double g : gaps) gmean += g;
    if (!gaps.empty()) gmean /= (double)gaps.size();
    printf("\n%zu wakes · twin said %zu, held %zu, manners held %zu · resident said %zu, refused %zu, took back %zu\n",
           wakes.size(), twin_emit, twin_hold, twin_supp, res_emit, res_refused, res_abort);
    printf("per seat-turn while the resident was on: both spoke %zu · both held %zu · twin only %zu · resident only %zu\n", both_emit, both_hold, twin_only, res_only);
    if (!gaps.empty())
        printf("gap when both spoke, twin minus resident: mean %+.1f s, median %+.1f s, n %zu (positive: the twin was later)\n",
               gmean, gaps[gaps.size() / 2], gaps.size());
    printf("blind: %zu human percepts landed while the twin composed and it could not see them; the resident's seam would have\n", blind_total);
    printf("cost: %llu tokens prefilled in %llu ms across the wakes, %llu ms composing\n",
           (unsigned long long)prefill_toks, (unsigned long long)prefill_ms, (unsigned long long)gen_ms_total);
    printf("\nscaffold                  resident                 twin (this replay)\n"
           "seed, seats, probe, cue   pinned 0x%016llx   the same pin, the same bytes\n"
           "sampler                   min_p .05 temp .7 dist 11  the same chain, re-seeded per wake\n"
           "cap / minimum             %d / %d                   %d / %d\n"
           "manners                   yes                      yes, the same code, persistent across wakes\n"
           "floor                     %s\n"
           "coarsening under backlog  yes                      no: one judgment per wake\n"
           "seam, un-say              yes                      no: the twin is blind while it composes (counted above)\n"
           "own speech                through the document     onto its own context at the wake\n"
           "decode-on-delta, KV kept  yes                      no: a fresh prefill per wake%s\n",
           (unsigned long long)kServeHashPin, rc.gen_cap, rc.gen_min, rc.gen_cap, rc.gen_min,
           policy == "pause" ? ("the hand's pause, " + secs(pause_ms) + " s    the same pause, as the wake").c_str()
                             : policy == "ask" ? "the hand's pause          the hand's key (the tape's ask rows)"
                                               : ("the hand's pause          a schedule, every " + secs(every_ms) + " s").c_str(),
           exact ? " (--exact: word by word, the resident's own batches)" : " (batched; --exact prefills word by word to remove the kernel's drift)");
    printf("\nobservational, not matched-input: the human wrote what they wrote beside the RESIDENT's lines, and the twin's\n"
           "lines were not on the page. Both arms saw the same human bytes; only the twin's own words differ.\n");
    out.append("end", (int64_t)t_prev, canon::obj({ { "wakes", canon::num((int64_t)wakes.size()) }, { "twin_emit", canon::num((int64_t)twin_emit) },
                                                    { "twin_hold", canon::num((int64_t)twin_hold) }, { "twin_supp", canon::num((int64_t)twin_supp) },
                                                    { "res_emit", canon::num((int64_t)res_emit) }, { "res_refused", canon::num((int64_t)res_refused) },
                                                    { "res_abort", canon::num((int64_t)res_abort) }, { "both_emit", canon::num((int64_t)both_emit) },
                                                    { "both_hold", canon::num((int64_t)both_hold) }, { "twin_only", canon::num((int64_t)twin_only) },
                                                    { "res_only", canon::num((int64_t)res_only) }, { "gap_mean_s", canon::flt(gmean) },
                                                    { "gap_n", canon::num((int64_t)gaps.size()) }, { "blind", canon::num((int64_t)blind_total) },
                                                    { "prefill_tokens", canon::num((int64_t)prefill_toks) }, { "prefill_ms", canon::num((int64_t)prefill_ms) } }));
    out.close();
    printf("record: %s\n", out_path.c_str());
    return 0;
}

}  // namespace nib
