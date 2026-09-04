// nib · nib.cpp — the console, while there is no window yet. Stage 0a is the changeset port, so
// the only verbs are the ones that let a person poke at it: read one, apply one, check one.
#include "changeset.h"
#include "doc.h"
#include "ingest.h"
#include "resident.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace nib {
int run_selftest();                          // selftest.cpp
int run_editor(const std::string& path);     // edit.cpp

namespace {

const char* kUsage =
    "nib %s - a writing surface with no send key on either side\n"
    "\n"
    "  nib --selftest                  the changeset port, against Etherpad's own vectors\n"
    "  nib --unpack CS                 split a changeset into oldLen, newLen, ops and charBank\n"
    "  nib --ops CS                    the operations, one per line\n"
    "  nib --apply CS TEXT             apply a changeset to a document\n"
    "  nib --check CS                  validate it, canonical form included\n"
    "  nib --edit [FILE]               the window\n"
    "  nib --ingest FILE [opts]        compile a file as if typed; print the percept stream\n"
    "        --lane L --chars N --quiet-ms T --tick-s S --burst N --burst-ms M --counts\n"
    "  nib --resident FILE [opts]      run the mind over it; print what each seat wanted\n"
    "        --model P --ctx N --gpu-layers N --all --verbose --allow-cpu   (needs the GPU)\n"
    "\n"
    "Stage 1b: the resident computes hold/emit and records it. It cannot speak.\n";

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
        for (const auto& p : out) printf("[%s] %s\n", p.lane.c_str(), p.text.c_str());

    printf("\n%zu bytes -> %llu percepts (%llu ticks) · longest %zu · N=%zu T=%lldms tick=%llds\n",
           text.size(), (unsigned long long)c.percepts(), (unsigned long long)c.ticks(),
           widest, cfg.chars, (long long)cfg.quiet_ms, (long long)cfg.idle_tick_s);
    printf("bytes in %llu == out %llu  %s\n",
           (unsigned long long)c.typed_in(), (unsigned long long)c.typed_out(),
           c.typed_in() == c.typed_out() ? "(nothing lost)" : "MISMATCH - A PERCEPT WAS LOST");
    if (widest > kChunkMax) printf("WARNING: a percept exceeds the %zu-byte Delta bound\n", kChunkMax);
    return c.typed_in() == c.typed_out() ? 0 : 3;
}

// Run the resident over a file, or over lines given on stdin, and print what each seat wanted.
// This is Stage 1b's whole surface: the margins, and no way to act on them.
int do_resident(int argc, char** argv) {
    std::string path, lane = "bo";
    Resident::Config rc;
    Compiler::Config cc;
    bool show_all = false;
    for (int i = 2; i < argc; ++i) {
        const std::string f = argv[i];
        if (f == "--model" && i + 1 < argc) rc.model = argv[++i];
        else if (f == "--llama-dir" && i + 1 < argc) rc.llama_dir = argv[++i];
        else if (f == "--ctx" && i + 1 < argc) rc.n_ctx = atoi(argv[++i]);
        else if (f == "--gpu-layers" && i + 1 < argc) rc.n_gpu_layers = atoi(argv[++i]);
        else if (f == "--lane" && i + 1 < argc) lane = argv[++i];
        else if (f == "--chars" && i + 1 < argc) cc.chars = (size_t)atoll(argv[++i]);
        else if (f == "--verbose") rc.verbose = true;
        else if (f == "--allow-cpu") rc.allow_cpu = true;
        else if (f == "--all") show_all = true;
        else if (path.empty()) path = f;
    }
    if (path.empty()) { fprintf(stderr, "nib: --resident needs a file\n"); return 2; }

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
    const uint64_t t0 = auricle::fusor::now_ms();
    printf("loading %s (ctx %d, %d gpu layers) ...\n", rc.model.c_str(), rc.n_ctx, rc.n_gpu_layers);
    fflush(stdout);
    if (!res.start(rc, err)) { fprintf(stderr, "nib: %s\n", err.c_str()); return 2; }
    printf("loaded in %.1f s · %s · devices: %s%s\n\n",
           (double)(auricle::fusor::now_ms() - t0) / 1000.0, res.model_desc().c_str(),
           res.devices().c_str(), res.have_gpu() ? "" : "  (CPU ONLY - this will be slow)");

    // The pad compiles the file exactly as it would compile typing, so what the resident sees
    // here is byte-identical to what it would see from the window.
    Compiler comp(cc);
    std::vector<Percept> ps;
    uint64_t clock = 1000;
    for (size_t i = 0; i < text.size(); i += 6) {
        const size_t n = 6 < text.size() - i ? 6 : text.size() - i;
        comp.typed(lane, text.substr(i, n), clock, ps);
        clock += 40;
    }
    comp.flush(clock, ps);

    std::vector<Judgment> js;
    const uint64_t r0 = auricle::fusor::now_ms();
    for (const auto& p : ps) {
        const size_t before = js.size();
        res.feed(p.lane, p.text, p.wall_ms, 0, js);
        for (size_t k = before; k < js.size(); ++k) {
            const Judgment& j = js[k];
            if (!show_all && j.margin <= 0.0f) continue;
            printf("  %-8s %+7.2f  b=%.2f %c  %s\n", seats()[j.seat].name, (double)j.margin,
                   (double)j.bscore, j.reason, j.clause.c_str());
        }
    }
    res.finish(js);
    const uint64_t elapsed = auricle::fusor::now_ms() - r0;

    printf("\n%zu percepts · %llu words · %llu boundaries (%llu coarsened) · %llu probes\n",
           ps.size(), (unsigned long long)res.words(), (unsigned long long)res.boundaries(),
           (unsigned long long)res.coarsened(), (unsigned long long)res.probes());
    printf("%llu of %llu probes wanted to speak; none could - Stage 1b has no emit path\n",
           (unsigned long long)res.wanted(), (unsigned long long)res.probes());
    if (res.boundaries())
        printf("probe %.0f ms per boundary (3 seats) · %llu ms probing of %llu ms wall · "
               "%d tokens of context used\n",
               (double)res.probe_ms_total() / (double)res.boundaries(),
               (unsigned long long)res.probe_ms_total(), (unsigned long long)elapsed,
               res.context_used());
    return 0;
}

}  // namespace
}  // namespace nib

int main(int argc, char** argv) {
    using namespace nib;
    const std::string a = argc > 1 ? argv[1] : "--help";
    if (a == "--selftest") return run_selftest();
    if (a == "--edit") return run_editor(argc > 2 ? argv[2] : "");
    if (a == "--ingest") return do_ingest(argc, argv);
    if (a == "--resident") return do_resident(argc, argv);
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
    printf(kUsage, "0.1.0");
    return 0;
}
