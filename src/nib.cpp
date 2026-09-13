// nib · nib.cpp — the console. The window is `--edit`; everything else is a way to poke at one
// part of the machine without the others: the changeset port, the compiler, the resident, and the
// gates that stand in front of the resident.
#include "changeset.h"
#include "doc.h"
#include "ingest.h"
#include "resident.h"
#include "tape.h"
#include "util.h"

#include <windows.h>

#include "ggml-backend.h"   // --about enumerates the devices the backends brought up

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace nib {
int run_selftest();                          // selftest.cpp
int run_editor(const std::string& path, bool monologue = false);   // edit.cpp

namespace {

const char* kUsage =
    "nib %s - a writing surface with no send key on either side\n"
    "\n"
    "  nib --selftest                  the oracle: the port, the document, the compiler, the gates\n"
    "  nib --edit [FILE]               the window\n"
    "  nib --monologue [FILE]          the screen saver with a person in it: full screen, the mind switched\n"
    "                                  on by the launch, Dave holding the floor from the first line;\n"
    "                                  typing anywhere is the interruption, Ctrl+Shift+M ends it\n"
    "  nib --about                     what this build is: version, serve hash, DLLs, the module gate\n"
    "  nib --verify TAPE.jsonl         walk a tape's chain (any tape in the family's format)\n"
    "  nib --unpack CS                 split a changeset into oldLen, newLen, ops and charBank\n"
    "  nib --ops CS                    the operations, one per line\n"
    "  nib --check CS                  validate it, canonical form included (exit 2 if not)\n"
    "  nib --apply CS TEXT             apply a changeset to a document\n"
    "  nib --splice TEXT START NDEL INS  the changeset for one edit, and the result\n"
    "  nib --ingest FILE [opts]        compile a file as if typed; print the percept stream\n"
    "        --lane L --chars N --quiet-ms T --tick-s S --burst N --burst-ms M --counts\n"
    "  nib --resident FILE [opts]      run the mind over it; print what each seat wanted\n"
    "        --model P --ctx N --gpu-layers N --seed N --all --verbose --allow-cpu   (needs the GPU)\n"
    "        --script   FILE is a script: a line is typed; '- text' is removed; '# N' is N s of quiet\n"
    "        --emit     LET IT SPEAK: every seat whose margin clears zero composes one sentence\n"
    "        --saver [--saver-lines N]   after the file, the screen saver: the SPEAKER is addressed as\n"
    "                   world and holds the floor, renewing its want at every line it says, until N\n"
    "                   lines or the manners refuse it - the MONOLOGUE HORIZON (needs --emit)\n"
    "        --dave     DAVE MODE: the same gate, the same seat, a different voice. Lines are\n"
    "                   composed with the unpinned dave cue and every row says cue: dave, or\n"
    "                   cue: dave+saver for a renewal under --saver, which it composes with (needs --emit)\n"
    "\n"
    "Without --emit the resident computes hold/emit and records it, and no sampler exists in the\n"
    "process. In the window, Ctrl+Shift+A switches it on and off; off unloads the model.\n";

int do_unpack(const std::string& cs) {
    Unpacked u;
    std::string err;
    if (!unpack(cs, u, err)) { fprintf(stderr, "nib: %s\n", err.c_str()); return 2; }
    printf("oldLen   %lld\nnewLen   %lld\nops      %s\ncharBank %s\n",
           (long long)u.old_len, (long long)u.new_len, u.ops.c_str(), u.char_bank.c_str());
    return 0;
}

int do_ops(const std::string& cs) {
    Unpacked u;
    std::string err;
    if (!unpack(cs, u, err)) { fprintf(stderr, "nib: %s\n", err.c_str()); return 2; }
    std::vector<Op> ops;
    if (!deserialize_ops(u.ops, ops, err)) { fprintf(stderr, "nib: %s\n", err.c_str()); return 2; }
    for (const Op& o : ops)
        printf("  %c  chars %-6lld lines %-4lld attribs %s\n", o.opcode, (long long)o.chars, (long long)o.lines,
               o.attribs.empty() ? "-" : o.attribs.c_str());
    return 0;
}

int do_check(const std::string& cs) {
    std::string err;
    if (!check_rep(cs, err)) { printf("not canonical: %s\n", err.c_str()); return 2; }
    printf("canonical\n");
    return 0;
}

// The family's verifier, as glance prints it: INTACT with the row count and the head, or the
// first broken row named. Exit 0 or 3.
int do_verify(const std::string& path) {
    uint64_t rows = 0, bad = 0;
    std::string head, err;
    const bool ok = Tape::verify_file(path, rows, bad, head, err);
    if (ok) printf("%s: INTACT, %llu rows, head %s\n", path.c_str(), (unsigned long long)rows, head.substr(0, 16).c_str());
    else printf("%s: BROKEN at row %llu - %s\n", path.c_str(), (unsigned long long)bad, err.c_str());
    return ok ? 0 : 3;
}

// What this build is, with no model loaded: the version, the serve-format pin, and the two gates
// that stand in front of the resident — which DLLs came up, and whether a network module is in
// the process once they have. This is the receipt the status line's "0 bytes egress" rests on.
int do_about(int argc, char** argv) {
    std::string llama_dir = "C:/llama.cpp";
    bool load = true;
    for (int i = 2; i < argc; ++i) {
        const std::string f = argv[i];
        if (f == "--llama-dir" && i + 1 < argc) llama_dir = argv[++i];
        else if (f == "--no-load") load = false;
    }
    char exe[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    printf("nib %s\n%s\n", kVersion, exe);
    printf("serve hash 0x%016llx (pin 0x%016llx) %s\n",
           (unsigned long long)serve_hash(), (unsigned long long)kServeHashPin,
           serve_hash() == kServeHashPin ? "- verbatim" : "- DRIFTED");
    printf("seats     ");
    for (size_t i = 0; i < seat_count(); ++i) printf("%s%s", i ? ", " : "", seats()[i].name);
    printf("\n");

    std::string mods, bad;
    size_t count = 0;
    module_gate(mods, count, bad);
    printf("modules   %zu before any backend loads%s%s\n", count, bad.empty() ? "" : " - NETWORK: ", bad.c_str());
    if (!load) return bad.empty() ? 0 : 3;

    std::string loaded, err;
    if (!load_backends(llama_dir, loaded, err)) { printf("backends  FAILED: %s\n", err.c_str()); return 2; }
    printf("backends  %s\n", loaded.c_str());
    printf("devices   ");
    for (size_t i = 0; i < ggml_backend_dev_count(); ++i)
        printf("%s%s", i ? ", " : "", ggml_backend_dev_name(ggml_backend_dev_get(i)));
    printf("\n");
    const bool ok = module_gate(mods, count, bad);
    printf("modules   %zu after the backends loaded\n", count);
    printf("gate      %s\n", ok ? "no network module in the process - 0 bytes egress is structurally true"
                                : ("NETWORK MODULE PRESENT: " + bad + " - the resident would refuse to start").c_str());
    return ok ? 0 : 3;
}

// Compile a file as if it were typed, and print the percept stream exactly as the trunk would
// receive it: one "[lane] text" line per Delta. No model, no GPU, no window — this is how the
// compiler's N and T get looked at against real prose before they are chosen (SPEC 14.3).
int do_ingest(int argc, char** argv) {
    std::string path, lane = "bo";
    Compiler::Config cfg;
    int64_t burst_ms = 40;          // simulated gap between typed bursts
    size_t burst = 6;               // characters per burst, as a hand would deliver them
    bool quiet_only = false;
    for (int i = 2; i < argc; ++i) {
        const std::string f = argv[i];
        if (f == "--lane" && i + 1 < argc) lane = argv[++i];
        else if (f == "--chars" && i + 1 < argc) cfg.chars = (size_t)atoll(argv[++i]);
        else if (f == "--quiet-ms" && i + 1 < argc) cfg.quiet_ms = atoll(argv[++i]);
        else if (f == "--tick-s" && i + 1 < argc) cfg.idle_tick_s = atoll(argv[++i]);
        else if (f == "--burst" && i + 1 < argc) burst = (size_t)atoll(argv[++i]);
        else if (f == "--burst-ms" && i + 1 < argc) burst_ms = atoll(argv[++i]);
        else if (f == "--counts") quiet_only = true;
        else if (path.empty()) path = f;
    }
    if (path.empty()) { fprintf(stderr, "nib: --ingest needs a file\n"); return 2; }
    if (burst == 0) burst = 1;

    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { fprintf(stderr, "nib: cannot open %s\n", path.c_str()); return 2; }
    std::string text;
    char buf[8192];
    size_t got;
    while ((got = fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, got);
    fclose(f);

    Compiler c(cfg);
    std::vector<Percept> out;
    uint64_t clock = 1000;
    for (size_t i = 0; i < text.size(); i += burst) {
        const size_t n = burst < text.size() - i ? burst : text.size() - i;
        c.typed(lane, text.substr(i, n), clock, out);
        clock += (uint64_t)burst_ms;
    }
    c.flush(clock, out);

    size_t widest = 0;
    for (const auto& p : out) if (p.text.size() > widest) widest = p.text.size();
    // A deletion already carries its own marker and a tick its own brackets, so the line is
    // printed exactly as the trunk will see it and nothing is added here.
    if (!quiet_only)
        for (const auto& p : out) printf("[%s] %s\n", delta_lane(p), p.text.c_str());

    printf("\n%zu bytes -> %llu percepts (%llu ticks) · longest %zu · N=%zu T=%lldms tick=%llds\n",
           text.size(), (unsigned long long)c.percepts(), (unsigned long long)c.ticks(),
           widest, cfg.chars, (long long)cfg.quiet_ms, (long long)cfg.idle_tick_s);
    printf("bytes in %llu == out %llu  %s\n",
           (unsigned long long)c.typed_in(), (unsigned long long)c.typed_out(),
           c.typed_in() == c.typed_out() ? "(nothing lost)" : "MISMATCH - A PERCEPT WAS LOST");
    if (widest > kChunkMax) printf("WARNING: a percept exceeds the %zu-byte Delta bound\n", kChunkMax);
    return c.typed_in() == c.typed_out() ? 0 : 3;
}

// Run the resident over a file and print what each seat wanted. This is Stage 1b's whole
// surface: the margins, and no way to act on them. A run that hit the window wall or failed a
// decode says so and exits non-zero: it is not a record.
int do_resident(int argc, char** argv) {
    std::string path, lane = "bo";
    Resident::Config rc;
    Compiler::Config cc;
    bool show_all = false, script = false, saver = false, dave = false;
    int saver_lines = 12;
    for (int i = 2; i < argc; ++i) {
        const std::string f = argv[i];
        if (f == "--saver") { saver = true; rc.emit = true; }
        else if (f == "--saver-lines" && i + 1 < argc) saver_lines = atoi(argv[++i]);
        else if (f == "--saver-pinned-cue") rc.saver_cue = false;   // measure the horizon under the pinned cue alone
        else if (f == "--dave") { dave = true; rc.emit = true; rc.dave = true; }
        else if (f == "--model" && i + 1 < argc) rc.model = argv[++i];
        else if (f == "--llama-dir" && i + 1 < argc) rc.llama_dir = argv[++i];
        else if (f == "--ctx" && i + 1 < argc) rc.n_ctx = atoi(argv[++i]);
        else if (f == "--gpu-layers" && i + 1 < argc) rc.n_gpu_layers = atoi(argv[++i]);
        else if (f == "--seed" && i + 1 < argc) rc.seed = (uint32_t)strtoul(argv[++i], nullptr, 10);
        else if (f == "--lane" && i + 1 < argc) lane = argv[++i];
        else if (f == "--chars" && i + 1 < argc) cc.chars = (size_t)atoll(argv[++i]);
        else if (f == "--tick-s" && i + 1 < argc) cc.idle_tick_s = atoll(argv[++i]);
        else if (f == "--verbose") rc.verbose = true;
        else if (f == "--allow-cpu") rc.allow_cpu = true;
        else if (f == "--all") show_all = true;
        else if (f == "--script") script = true;
        else if (f == "--emit") rc.emit = true;
        else if (path.empty()) path = f;
    }
    if (path.empty()) { fprintf(stderr, "nib: --resident needs a file\n"); return 2; }
    if (rc.n_ctx < Resident::kMinCtx) {
        fprintf(stderr, "nib: --ctx %d is below the minimum of %d (the seed alone is ~430 tokens)\n",
                rc.n_ctx, Resident::kMinCtx);
        return 2;
    }

    std::string text;
    {
        FILE* fp = fopen(path.c_str(), "rb");
        if (!fp) { fprintf(stderr, "nib: cannot open %s\n", path.c_str()); return 2; }
        char b[8192];
        size_t n;
        while ((n = fread(b, 1, sizeof b, fp)) > 0) text.append(b, n);
        fclose(fp);
    }

    printf("serve hash 0x%016llx (pin 0x%016llx) %s\n",
           (unsigned long long)serve_hash(), (unsigned long long)kServeHashPin,
           serve_hash() == kServeHashPin ? "- verbatim" : "- DRIFTED");

    Resident res;
    std::string err;
    // --saver-pinned-cue exists to measure the horizon with NO unpinned cue in the frame, and
    // --dave is one. Silently letting Dave's tail win would contaminate the control run with a
    // different unpinned string, so the combination is refused rather than decided implicitly.
    if (dave && !rc.saver_cue) {
        fprintf(stderr, "nib: --saver-pinned-cue measures the horizon with no unpinned cue; --dave is one. Pick one.\n");
        return 2;
    }
    const uint64_t t0 = auricle::fusor::now_ms();
    printf("loading %s (ctx %d, %d gpu layers) ...\n", rc.model.c_str(), rc.n_ctx, rc.n_gpu_layers);
    fflush(stdout);
    if (!res.start(rc, err)) { fprintf(stderr, "nib: %s\n", err.c_str()); return 2; }
    printf("loaded in %.1f s · %s\nbackends: %s · devices: %s%s · %zu modules, no network DLL\n\n",
           (double)(auricle::fusor::now_ms() - t0) / 1000.0, res.model_desc().c_str(),
           res.backends().c_str(), res.devices().c_str(),
           res.have_gpu() ? "" : "  (CPU ONLY - this will be slow)", res.module_count());
    if (dave) {
        res.set_dave(true);
        printf("DAVE MODE: the gate is the pinned probe and the seed has not moved; only the cue that\n"
               "phrases a line is different. dave cue 0x%016llx (unpinned, not in the serve hash).\n\n",
               (unsigned long long)dave_cue_hash());
    }

    // The pad compiles the file exactly as it would compile typing, so what the resident sees
    // here is byte-identical to what it would see from the window. A script is the same stream
    // with two extra kinds of line: "- text" is a deletion, "# N" is N seconds of quiet — the
    // two experiments the review asked for (do deletions move the margins; are ticks perceived).
    Compiler comp(cc);
    std::vector<Percept> ps;
    uint64_t clock = 1000;
    if (script) {
        size_t i = 0;
        while (i < text.size()) {
            size_t j = text.find('\n', i);
            if (j == std::string::npos) j = text.size();
            std::string line = text.substr(i, j - i);
            i = j + 1;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            if (line.rfind("# ", 0) == 0) { clock += (uint64_t)atoll(line.c_str() + 2) * 1000ull; comp.idle(clock, ps); continue; }
            if (line.rfind("- ", 0) == 0) { comp.removed(lane, line.substr(2), clock, ps); clock += 40; continue; }
            comp.typed(lane, line + "\n", clock, ps);
            clock += 40 * (uint64_t)(line.size() / 6 + 1);
        }
    } else {
        for (size_t i = 0; i < text.size(); i += 6) {
            const size_t n = 6 < text.size() - i ? 6 : text.size() - i;
            comp.typed(lane, text.substr(i, n), clock, ps);
            clock += 40;
        }
    }
    comp.flush(clock, ps);

    std::vector<Judgment> js;
    const uint64_t r0 = auricle::fusor::now_ms();
    for (const auto& p : ps) {
        const size_t before = js.size();
        res.feed(delta_lane(p), p.text, p.wall_ms, 0, js);
        for (size_t k = before; k < js.size(); ++k) {
            const Judgment& j = js[k];
            if (!show_all && j.margin <= 0.0f) continue;
            printf("  %-8s %+7.2f  b=%.2f %c  %s\n", seats()[j.seat].name, (double)j.margin,
                   (double)j.bscore, j.reason, j.clause.c_str());
        }
        // The CLI has no hand to yield the floor: nobody is typing, so the floor is always open and
        // a want is composed the moment it exists. In the window the wire waits for the pause.
        res.speak_wants();
        // what it said, and what the manners would not let it say twice. The CLI has no document
        // for a line to come back through, so it feeds each line back itself (SPEC 6.3.6).
        for (const Emission& e : res.take_emissions()) {
            printf("  %-8s %+7.2f  ->  \"%s\"   (%d tok, %llu ms, stop %c, %s cue)\n", seats()[e.seat].name,
                   (double)e.margin, e.say.c_str(), e.toks, (unsigned long long)e.gen_ms, e.stop, cue_name(e.cue));
            res.own_line(seats()[e.seat].name, e.say, e.wall_ms);
        }
        for (const Suppressed& s : res.take_suppressed())
            printf("  %-8s %+7.2f  --  suppressed (%s%s%s): \"%s\"\n", seats()[s.seat].name,
                   (double)s.margin, s.why.c_str(), s.by.empty() ? "" : " by ", s.by.c_str(), s.say.c_str());
        if (res.failed() || res.window_full()) break;
    }
    res.finish(js);
    res.speak_wants();
    for (const Emission& e : res.take_emissions()) {
        printf("  %-8s %+7.2f  ->  \"%s\"   (%d tok, %llu ms, stop %c, %s cue)\n", seats()[e.seat].name,
               (double)e.margin, e.say.c_str(), e.toks, (unsigned long long)e.gen_ms, e.stop, cue_name(e.cue));
        res.own_line(seats()[e.seat].name, e.say, e.wall_ms);
    }
    for (const Suppressed& s : res.take_suppressed())
        printf("  %-8s %+7.2f  --  suppressed (%s%s%s): \"%s\"\n", seats()[s.seat].name,
               (double)s.margin, s.why.c_str(), s.by.empty() ? "" : " by ", s.by.c_str(), s.say.c_str());

    if (saver && !res.failed() && !res.window_full()) {
        // THE SCREEN SAVER'S NULL (docs/BRAINSTORMS_2026-09-05.md §4): the resident holds the floor.
        // The standing instruction is world on the host's lane; the SPEAKER answers it; each line
        // it says comes back through own_line, which renews the want; the run ends at
        // `saver_lines` lines, or when the manners have refused the seat and it has nothing left
        // to say — THE MONOLOGUE HORIZON, the number the next tune is scored on. No hand, so the
        // floor is always open; in the window the human's typing is the interruption.
        printf("\n--- the screen saver · [%s] %s · renewals under the %s cue\n", kSaverLane, kSaverAddress,
               rc.saver_cue ? "SAVER" : "PINNED");
        res.set_saver(true);
        const uint64_t supp0 = res.suppressed(), s0 = auricle::fusor::now_ms();
        std::vector<Judgment> js2;
        clock += 2000;
        res.feed(kSaverLane, std::string(kSaverAddress) + "\n", clock, 0, js2);
        auto show = [&](const std::vector<Judgment>& v) {
            for (const Judgment& j : v)
                if (show_all || j.margin > 0.0f)
                    printf("  %-8s %+7.2f  b=%.2f %c  %s\n", seats()[j.seat].name, (double)j.margin,
                           (double)j.bscore, j.reason, j.clause.c_str());
        };
        show(js2);
        int said = 0;
        while (said < saver_lines && res.wants_pending() && !res.failed() && !res.window_full()) {
            js2.clear();
            res.speak_wants(nullptr, &js2);
            show(js2);
            for (const Emission& e : res.take_emissions()) {
                printf("  %-8s %+7.2f  ->  \"%s\"   (%d tok, %llu ms, stop %c%s)\n", seats()[e.seat].name,
                       (double)e.margin, e.say.c_str(), e.toks, (unsigned long long)e.gen_ms, e.stop,
                       ssprintf(", %s cue", cue_name(e.cue)).c_str());
                res.own_line(seats()[e.seat].name, e.say, e.wall_ms);
                if (e.seat == kSaverSeat) ++said;
            }
            for (const Suppressed& s : res.take_suppressed())
                printf("  %-8s %+7.2f  --  suppressed (%s%s%s): \"%s\"\n", seats()[s.seat].name,
                       (double)s.margin, s.why.c_str(), s.by.empty() ? "" : " by ", s.by.c_str(), s.say.c_str());
        }
        printf("monologue: %d lines said in %.1f s · %llu retried · %llu refused · horizon %s\n", said,
               (double)(auricle::fusor::now_ms() - s0) / 1000.0, (unsigned long long)res.saver_retried(),
               (unsigned long long)(res.suppressed() - supp0),
               res.wants_pending() ? "not reached (the cap)" : "REACHED: the manners refused the seat and it held");
        res.set_saver(false);
    }
    const uint64_t elapsed = auricle::fusor::now_ms() - r0;

    printf("\n%zu percepts · %llu words · %llu ticks · %llu boundaries (%llu coarsened) · %llu probes\n",
           ps.size(), (unsigned long long)res.words(), (unsigned long long)res.ticks(),
           (unsigned long long)res.boundaries(), (unsigned long long)res.coarsened(),
           (unsigned long long)res.probes());
    if (rc.emit)
        printf("%llu of %llu probes wanted to speak; %llu said something, %llu were held by the manners; "
               "%llu own lines heard on the trunk\n",
               (unsigned long long)res.wanted(), (unsigned long long)res.probes(),
               (unsigned long long)res.emitted(), (unsigned long long)res.suppressed(),
               (unsigned long long)res.own_lines());
    else
        printf("%llu of %llu probes wanted to speak; none could - no sampler exists without --emit\n",
               (unsigned long long)res.wanted(), (unsigned long long)res.probes());
    if (res.boundaries())
        printf("probe %.0f ms per boundary (3 seats) · %llu ms probing of %llu ms wall · "
               "%d tokens of context used\n",
               (double)res.probe_ms_total() / (double)res.boundaries(),
               (unsigned long long)res.probe_ms_total(), (unsigned long long)elapsed,
               res.context_used());
    if (res.failed()) {
        printf("\nFAILED: %s - this run is not a record\n", res.failure().c_str());
        return 5;
    }
    if (res.window_full()) {
        printf("\nWINDOW FULL at %d of %d tokens: %llu words were never perceived - this run is not a "
               "valid record (Stage 1b has no molt; raise --ctx or shorten the stream)\n",
               res.context_used(), rc.n_ctx, (unsigned long long)res.dropped_words());
        return 4;
    }
    return 0;
}

}  // namespace
}  // namespace nib

int main(int argc, char** argv) {
    using namespace nib;
    const std::string a = argc > 1 ? argv[1] : "--help";
    if (a == "--selftest") return run_selftest();
    if (a == "--edit") return run_editor(argc > 2 ? argv[2] : "");
    if (a == "--monologue") return run_editor(argc > 2 ? argv[2] : "", true);
    if (a == "--about") return do_about(argc, argv);
    if (a == "--ingest") return do_ingest(argc, argv);
    if (a == "--resident") return do_resident(argc, argv);
    if (a == "--unpack" && argc > 2) return do_unpack(argv[2]);
    if (a == "--ops" && argc > 2) return do_ops(argv[2]);
    if (a == "--check" && argc > 2) return do_check(argv[2]);
    if (a == "--verify" && argc > 2) return do_verify(argv[2]);
    if (a == "--splice" && argc > 5) {
        const std::string orig = argv[2], ins = argv[5];
        const long long start = atoll(argv[3]), ndel = atoll(argv[4]);
        const std::string cs = make_splice(orig, start, ndel, ins);
        std::string err;
        if (!check_rep(cs, err)) { fprintf(stderr, "nib: the splice is not canonical: %s\n", err.c_str()); return 2; }
        std::string out;
        if (!apply_to_text(cs, orig, out, err)) { fprintf(stderr, "nib: %s\n", err.c_str()); return 2; }
        printf("%s\n%s\n", cs.c_str(), out.c_str());
        return 0;
    }
    if (a == "--apply" && argc > 3) {
        std::string out, err;
        if (!apply_to_text(argv[2], argv[3], out, err)) { fprintf(stderr, "nib: %s\n", err.c_str()); return 2; }
        printf("%s\n", out.c_str());
        return 0;
    }
    printf(kUsage, kVersion);
    return a == "--help" ? 0 : 2;
}
