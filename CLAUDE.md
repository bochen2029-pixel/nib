# CLAUDE.md — session rules for nib

*The name is provisional — a nib is the point where the writing actually touches the paper. Rename
freely; nothing below depends on it.*

You are implementing **nib**: a plain-text editor for Windows in which a **resident mind writes
beside you in real time**, and later, on a LAN, other people do too. The machine-wide rules in
`C:\Users\user\.claude\CLAUDE.md` apply on top. Read `README.md` → `docs/BLUEPRINT.md` before
writing code, and read these before writing a line of the loop:

- `C:\NEW\FUSOR_MASTER-ONEPAGER_2026-08-23_FABLE5.md` — the substrate, its laws, its receipts.
- `C:\auricle_snapshots\20260813-131533\src\fusor\fusord.cpp` — **the loop. Lift it; do not
  re-derive it.** Its seed, its seat probe and its speak-cue are byte-frozen (train ≡ serve); the
  daemon refuses to start if the hash moves. That refusal comes with us.
- `C:\auricle\src\fabric\fabric.h` — propose/dispose, the hash-chained tape, contradicts∩dep.
- `C:\auricle\src\app\blackbox.cpp` — the tape as the deliverable; the trace as the proof.

## The one job

**A writing surface with no send key on either side.** You type; it perceives as you type. It
writes; you watch the words form. Neither of you takes a turn. Everything else — rooms, a LAN,
several people, several seats — is Act II and after.

## Where this tool departs from the family, and why

facet, vramtop, everywho, caseclock, glance and fray all forbid a model inside the tool. **nib is
the opposite: the model is the point.** Those tools *report*; this one *participates*. That is a
deliberate break and it has to be paid for, so the compensating laws below are not decoration —
they are the price of the exception.

## Hard rules

1. **C or C++ only, one exe, OS APIs plus llama.cpp.** No runtime, no browser, no Electron, no
   web view. `build.bat`, `/W4 /WX`, zero warnings.
2. **Act I links no network stack.** `dumpbin /dependents` fails the build on `ws2_32`, `wininet`,
   `winhttp`, `urlmon`. The model is local, the pad is local, the tape is local, and the status
   line says `0 bytes egress` because it is structurally true. **Act II must link sockets — that
   is the moment this gate relaxes, and §7 of the blueprint says exactly what replaces it.**
3. **Forming text never persists.** The resident's half-written sentence is rendered but not
   committed. Save mid-formation and you get the committed document. A crash loses nothing that
   was ever real. Reflex partials never persist — the file format enforces it, not a code path.
4. **The resident never writes into a block a human is touching.** Not a policy, a serializer
   rule: an emit targeting a block with a human keystroke inside the floor window is refused
   before it is composed. Hazards unreachable by construction, never forbidden by manners.
5. **Never simulate a human tell that does not correspond to a real internal event.** No fake
   typing hesitation, no invented typos, no "thinking…" that isn't thinking. The resident's holds
   *are* its silences and its aborts *are* its retractions; those are real and they are enough. A
   demo that fakes a pause is a demo that has stopped being evidence.
6. **It is never ambiguous who is on the other end.** Authorship is per character, permanent, and
   visible; the resident's blocks are marked in the buffer, in the file, and on the tape. This
   holds when the pad is shared with people who did not start it.
7. **Percepts are never dropped, and deletions are percepts.** A human backspacing a sentence is
   information — often the most interesting information in the stream. Judgment may be delayed;
   the world is never edited. A ring that overflows counts the loss loudly.
8. **Every state that changes what the resident is goes on the tape**: the AI switch, the
   resident/turn-based mode, the seat's mandate, the model's hash, every coefficient. A reader of
   the tape can always tell which machine was on the other end.
9. **No frontier API in Act I or Act II.** When it arrives (Act III, behind a flag), the room shows
   a permanent badge and the egress counter stops reading zero. Nobody in a shared pad discovers
   after the fact that their words left the building.
10. **The seed is byte-frozen.** `SEED_SYS`, `SEED_EXAMPLES`, `SEED_OPEN`, the seat probe and the
    speak-cue are copied from `fusord.cpp` verbatim; the build hashes them and refuses to run if a
    character drifted. v11's dial-0 calibration is off-distribution otherwise.

## Build and test discipline

- `--selftest` before every commit; stages and their falsifiers are BLUEPRINT §6.
- The editor is testable without a GPU: Stage 0 is the buffer, the op log and replay, with no
  model in the process at all. **Every stage below Stage 2 must stay runnable with `--no-model`,**
  because a test battery that needs a 9B on a busy card is a battery that stops being run.
- Numbers get measured on this box and carry their date: keystroke-to-render, ingest latency,
  emit rate per hour of real writing, tokens per second, VRAM.
- The devlog is a lab notebook: what was tried, what was measured, what was decided, traps the day
  they bite.

## This machine

Windows 11 24H2, 225 % DPI, VS 2022, an RTX 4070 Ti SUPER usually shared with llama-server and a
speech stack. `C:/llama.cpp` is the build; `C:/models/` holds the weights. The Bash tool's
heredocs mangle apostrophes and backslashes — write files with the Write tool, patch with short
Python, use Bash to run things. Forward slashes in shell commands.

## Refusals

A "thinking…" animation that is not thinking: no. Faked typing hesitation: no. A cloud model in
Act I or II: no. Silently keeping a forming sentence because it looked good: no. Shipping the LAN
act before one person alone finds the pad worth using: no — that is the whole point of the order.
