// nib · nib.cpp — the console, while there is no window yet. Stage 0a is the changeset port, so
// the only verbs are the ones that let a person poke at it: read one, apply one, check one.
#include "changeset.h"

#include <cstdio>
#include <string>

namespace nib {
int run_selftest();   // selftest.cpp

namespace {

const char* kUsage =
    "nib %s - a writing surface with no send key on either side\n"
    "\n"
    "  nib --selftest                  the changeset port, against Etherpad's own vectors\n"
    "  nib --unpack CS                 split a changeset into oldLen, newLen, ops and charBank\n"
    "  nib --ops CS                    the operations, one per line\n"
    "  nib --apply CS TEXT             apply a changeset to a document\n"
    "  nib --check CS                  validate it, canonical form included\n"
    "\n"
    "Stage 0a: the format only. There is no editor yet, and no resident.\n";

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

}  // namespace
}  // namespace nib

int main(int argc, char** argv) {
    using namespace nib;
    const std::string a = argc > 1 ? argv[1] : "--help";
    if (a == "--selftest") return run_selftest();
    if (a == "--version") { printf("nib 0.1.0\n"); return 0; }
    if (a == "--unpack" && argc > 2) return do_unpack(argv[2]);
    if (a == "--ops" && argc > 2) return do_ops(argv[2]);
    if (a == "--check" && argc > 2) {
        std::string err;
        if (!check_rep(argv[2], err)) { fprintf(stderr, "nib: %s\n", err.c_str()); return 1; }
        printf("canonical\n");
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
