# nib — ROADMAP

*Rev 0.4 · 2026-09-03. Each stage carries a **falsifier**: the observation that would say the stage
failed. A stage is not done because its code exists; it is done when its falsifier has been fired
at and did not go off. Dates are the day a stage went green on this box.*

**Legend** — ✔ done · ◑ in progress · ○ not started · ⨯ deliberately not doing

---

## Where it stands

| act | stage | state | version |
|---|---|---|---|
| **I** | 0a · the changeset port, read half | ✔ 2026-09-03 | 0.1.0 |
| **I** | 0b · the write half | ✔ 2026-09-03 | 0.2.0 |
| **I** | 0c · the editor shell | ✔ 2026-09-03 | 0.3.0 |
| **I** | 0d · save, selection, the theme | ✔ 2026-09-03 | 0.4.0 |
| **I** | 0e · the window driver | ○ | |
| **I** | 1 · ingest, and a resident that only holds | ○ | |
| **I** | 2 · emission, with floor control | ○ | |
| **I** | 3 · un-saying, made visible | ○ | |
| **I** | 4 · the two switches, and the paired record | ○ | |
| **I** | 5 · **a week of real work** — the falsifier for the whole idea | ○ | |
| **II** | 6 · discovery | ○ | |
| **II** | 7 · rooms | ○ | |
| **II** | 8 · convergence | ○ | |
| **III** | 9 · three seats | ○ | |

**71 checks green.** The exe is 216 KB and links kernel32, user32, gdi32, comdlg32 — no network
DLL, enforced at build.

---

## Act I — one person, one resident, no network

### ✔ Stage 0a — the changeset port, read half · 0.1.0

base36, `Op`, `deserialize_ops`, `unpack`/`pack`, `apply_to_text`, the three assemblers,
`check_rep`. Ported from `Changeset.ts` and companions rather than designed.

*Falsifier: a changeset Etherpad accepts that nib rejects, or a re-serialisation that differs by a
byte.* — Did not fire. 41 checks against Etherpad's own vectors, including four negative cases
that pin canonical form (a written-out trailing keep, unfused adjacent keeps, a wrong claimed
length, excess bank characters).

### ✔ Stage 0b — the write half · 0.2.0

`ops_from_text`, `make_splice`, `Builder`.

*Falsifier: a splice whose changeset is not canonical, or which does not apply to the string
ordinary surgery produces.* — Did not fire over **ten thousand random splices**, from a
deterministic generator. Exact bytes are pinned too, so a drift in the encoding is caught rather
than tolerated.

### ✔ Stage 0c — the editor shell · 0.3.0

`doc.cpp` (the document as an op log) and `edit.cpp` (Win32/GDI). Typing, navigation, undo, redo,
the status line, `Ctrl+R`.

*Falsifier: replaying the op log does not reproduce the document byte-exact.* — Did not fire over
**a thousand random edits interleaved with undos** (767 edits, 214 undos, 965 revisions), checked
after every single one. The runtime check is `Ctrl+R`, so the person using it can fire the
falsifier themselves at any moment.

### ✔ Stage 0d — save, selection, the theme · 0.4.0

Atomic save, open, CRLF and BOM preservation, the unsaved-changes prompt. Selection by keyboard and
mouse, clipboard. `nib.theme` read at startup, `tools/theme_detect.py` to derive it from an image.
The `WM_APP+1` / `NIB_LOG` driver seam.

*Falsifier: a save that can lose the previous file, or a file whose conventions nib silently
changed.* — Not yet fired at: **the driver is Stage 0e and save is currently verified by hand.**
This is the one place where the stage is marked done ahead of its evidence, and it is marked so
deliberately rather than quietly.

### ○ Stage 0e — the window driver

A driver that posts `WM_APP+1` commands and reads `NIB_LOG` and the file on disk: type, select,
replace, save, undo, reopen, compare bytes. Closes the gap left by 0d.

**It must not synthesise global input.** The first attempt did, and global keystrokes land wherever
the focus happens to be — which can type into another application's window. That is why the seam
exists.

*Falsifier: a save path that the driver cannot reach without global input, or a save that loses a
byte across a close-and-reopen cycle.*

### ○ Stage 1 — ingest, and a resident that only holds

`PadSource` implementing `StreamingTextSource`; the compiler (§5 of SPEC); the trunk, the
segmenter, hold/emit computed and recorded with **emission disabled**. The margins move against
your own typing before the thing ever writes a word.

*Falsifier: a percept dropped without a loud count, or margins that do not move with content.*

Why emission is disabled here: it is the cheapest possible way to find out whether the resident
perceives sanely, and it cannot embarrass itself while you are finding out.

### ○ Stage 2 — emission, with floor control

The resident writes in its own blocks. An emission targeting a block a human has touched within the
floor window is refused before it is composed.

*Falsifier: one emission lands inside a block a human touched inside the floor window; or forming
text survives a save or a crash.*

**This stage contains the genuinely unsolved problem.** The emit gate decides *whether* to speak;
nothing yet decides *where the words land and when*, in a buffer somebody else is typing into. Two
humans negotiate that continuously and unconsciously, and nobody has had to solve it for an entity
with no turns sharing a surface with humans who also have no turns. Act I's rule is deliberately
crude and enforced by the serialiser rather than by manners.

### ○ Stage 3 — un-saying, made visible

The provisional region: rendered, distinct, never committed, never saved. On abort the characters
are withdrawn and the abort goes on the tape.

*Falsifier: a killed sentence that is not on the tape, or that leaves residue in the buffer, or a
saved file that ever contained a word that was un-said.*

This is the demonstration the whole project is for. A person watches the machine begin a sentence,
sees their own next keystroke contradict it, and sees it taken back. In a chat box that act is
impossible or looks like a bug; in a pad it is an ordinary event the surface already renders.

### ○ Stage 4 — the two switches, and the paired record

AI on/off; RESIDENT / TURN-BASED. Both on the status line, both on the tape.

*Falsifier: the two modes differ in seat, seed or sampler — at which point the toggle is a
preference and not an experiment.*

**This stage is worth more than livability.** It is the twin race instrumented in the product: the
estate's one number that matters next is a resident against a maximally-good turn-based twin on
identical weights, and every flip of that toggle during real work is a paired sample with exactly
one variable.

### ○ Stage 5 — a week of real work

*Falsifier, and the falsifier for the entire idea: the operator, alone with it for a week, does not
leave it turned on.*

Measured: emissions per hour at somebody's elbow, holds per emission, how often the un-say fired
and whether it was right, and how often the toggle moved and which way.

Nothing in Act II rescues a failure here.

---

## Act II — the LAN

Only after Stage 5. See `docs/ASSEMBLY.md` §3 for why OT with a host, and not CRDT.

### ○ Stage 6 — discovery

IPv4 only, a UDP beacon, a peer table with a TTL, no configuration.

*Falsifier: two machines on the same subnet that do not find each other within five seconds; or a
beacon that reaches anything off the local segment.*

**Known deployment truth:** school and campus networks very often run client isolation on the
access points precisely to stop student devices talking to each other. That kills UDP discovery
dead. Find out before promising a classroom.

### ○ Stage 7 — rooms

Anyone hosts. Listed, unlisted, hidden. The passphrase gates readability, not merely admission.

*Falsifier: a room's content readable off the wire by somebody who does not have the passphrase.*

### ○ Stage 8 — convergence

`follow`, the host as serialiser, pad handoff. The differential harness against the real
`Changeset.ts` is a prerequisite, not an optional extra.

*Falsifier: two peers whose documents differ after the same edits; or a room that cannot survive
its host handing over.*

---

## Act III — the shape it was always for

### ○ Stage 9 — three seats

Speaker, Skeptic, Sentinel on one KV trunk: fork at 0 MiB, three co-decoding at 1.208× the cost of
one. Moderator *and* contributor *and* watcher in one room on one host GPU.

*Falsifier: three seats cost materially more than 1.3× one seat on this card.*

This is why the seat abstraction exists in Act I with a single seat in it.

---

## Deliberately not doing

⨯ **Bundling Node and Etherpad's server.** It would end the no-network-stack law on day one, ship
a runtime and ~300 MB of `node_modules`, and still leave the client implementing changeset apply
and compose. See ASSEMBLY §2.

⨯ **A CRDT.** Etherpad's OT is verified and on this disk; a CRDT is something new to design and
verify. If Act II proves the host-serialiser intolerable, CRDT returns — paid for by a
measurement, not an instinct. See ASSEMBLY §3.

⨯ **A cloud model in Act I or II.** The moment a room streams to a frontier API, "nothing leaves
the building" is false for everyone in it, not only for whoever flipped the switch.

⨯ **Simulated humanity.** No invented hesitation, no fake typos, no "thinking…" that is not
thinking. The resident's holds are real silences and its aborts are real retractions; that is
enough, and faking it is the exact place a demo stops being evidence.

⨯ **Rich text, plugins, a browser build, accounts, telemetry.**

---

## The order, and why it is this order

The resident half carries the entire thesis; the network half is upside. Etherpad failed as a
*matching problem* — its value needed two people free in the same instant — and a resident is the
first thing that is always free. So Act I is one person, one pad, one resident, no network at all.
If writing with it is compelling, rooms and classrooms follow. If it is not, no amount of
auto-discovery rescues it.

Within Act I, the order runs from the most bounded work to the least: a format with an existing
oracle, then a document with a mechanical falsifier, then a window, then a resident that only
listens, then one that speaks, then one that can take it back. Each stage is usable before the next
begins, and each has a falsifier that can be fired at with the code that exists at that point.
