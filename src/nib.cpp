// nib · nib.cpp — the console, while there is no window yet. Stage 0a is the changeset port, so
// the only verbs are the ones that let a person poke at it: read one, apply one, check one.
#include "changeset.h"
#include "doc.h"
#include "ingest.h"

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
    "\n"
    "Stage 1: the pad compiles a world. There is no resident in the process yet.\n";

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

}  // namespace
}  // namespace nib

int main(int argc, char** argv) {
    using namespace nib;
    const std::string a = argc > 1 ? argv[1] : "--help";
    if (a == "--selftest") return run_selftest();
    if (a == "--edit") return run_editor(argc > 2 ? argv[2] : "");
    if (a == "--ingest") return do_ingest(argc, argv);
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
