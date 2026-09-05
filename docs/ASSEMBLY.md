# nib — the assembly, after reading what already exists

*2026-09-03. Supersedes BLUEPRINT §2 and §7 where they disagree. Written after the operator
pointed out that Etherpad is on this box at `C:\etherpad-develop` and that `C:\auricle` already
builds a resident that ingests a live stream and projects words onto the screen. Both corrections
below are mine to make: I proposed building things that exist.*

---

## 1 · What is already on this machine

| piece | where | state |
|---|---|---|
| Etherpad, full source | `C:\etherpad-develop` | 19 MB, **Node.js / TypeScript**, pnpm workspace |
| the changeset core | `src/static/js/Changeset.ts` + `Op.ts` `Builder.ts` `OpIter.ts` `MergingOpAssembler.ts` `TextLinesMutator.ts` `AttributePool.ts` | the actual algorithm, with tests in `src/tests/` |
| the resident daemon | `C:\auricle\src\fusor\fusord.cpp` → `build\Release\fusord.exe` | **built and run live** (first hours banked 2026-08-12) |
| **the intake seam** | `C:\auricle\src\fusor\source.h` | `StreamingTextSource` · `Delta` · `SpscRing` · `FileTailSource` |
| the tape / propose-dispose | `C:\auricle\src\fabric\fabric.h` | hash chain, op_id idempotency, contradicts∩dep abort |
| the emit surface | `C:\auricle\src\overlay\hud.cpp` | the teleprompter — the resident's words on screen |
| ears | `src\capture` (WASAPI) · `src\asr` (sherpa) | audio → text lane, working |
| three minds on one trunk | `C:\auricle\src\app\syncytium.cpp` | Speaker · Skeptic · Sentinel, emergent abort |

**Both halves of the loop already have a working precedent in the tree.** Audio streams in through
a `StreamingTextSource`; words stream out through the HUD. nib swaps the driver on one end and the
renderer on the other. That is the whole insight and it is why this is assembly.

---

## 2 · Correction one: Etherpad is Node, so "wrap it" is three different projects

This is the fact that decides the shape, and it was not in the blueprint. Etherpad Lite is a
Node.js server with a browser client. "Wrap it in a C/C++ client" resolves three ways:

**(a) Bundle the server.** Ship `node.exe` plus the Etherpad tree as a child process on localhost;
the native client speaks Etherpad's socket.io protocol to it. You inherit OT correctness, the
timeslider, authorship, the attribute pool and the plugin ecosystem for free. You also inherit: a
Node runtime in the installer, roughly 300 MB of `node_modules`, a pnpm build chain, an HTTP
server on localhost — **which kills the no-network-stack law on day one** — and a client that
*still* has to implement changeset apply/compose and attribute-pool sync to render anything. Most
of the work, plus the runtime.

**(b) Port the changeset core to C++.** Roughly 1,500 lines of TypeScript across
`Changeset.ts` and its half-dozen companions. The format is stable, documented, and the repository
ships test vectors that become the port's oracle by equality. You keep the algorithm, the wire
format, and the tests; you express them in the language the client is already written in. One exe,
no Node, no localhost server, no sockets in Act I — and a wire format a real Etherpad server could
still talk to later if that ever matters.

**(c) Keep the browser.** Rejected: it is the thing being replaced.

**Take (b).** Porting a proven, golden-tested algorithm is *reusing* the wheel. Designing a fresh
CRDT would have been reinventing it — see the next correction.

---

## 3 · Correction two: OT, not CRDT — I was reasoning without this box in front of me

The earlier recommendation was CRDT, on the argument that eliminating the setup step and then
reintroducing "who is the server" would be perverse. That reasoning was sound in the abstract and
wrong here, because it priced the wrong thing. **The cost of CRDT is designing and verifying
something new. Etherpad's OT is already verified**, with vectors in the repo, and it is sitting on
this disk.

So: **OT, with a serializer per document.**

- **In Act I the serializer is free.** One machine, one document thread — the app *is* the server,
  trivially, and the question does not arise.
- **In Act II a room has a host that linearizes.** The honest failure mode is that a room needs its
  host, and a host that walks out ends the room or hands it over. That is legible: people
  understand "whose room is this". It is a real weakening against the earlier claim that the room
  survives the host's laptop closing — record it as a cost, not a feature.
- **Handoff is the mitigation, not leader election.** Etherpad pads carry their whole state as a
  changeset history; transferring one to a new host is a data move, not a consensus protocol. Build
  it when a room actually loses a host, not before.

If Act II proves this intolerable, CRDT is still there. It is a strictly larger project and it
should be paid for by a measured failure, not by an instinct.

---

## 4 · The seam, exactly

`C:\auricle\src\fusor\source.h` is the integration point, and it is already the right shape:

```cpp
struct StreamingTextSource {
    virtual bool poll(Delta& out) = 0;   // non-blocking drain; the resident never waits on a source
    virtual void stop() = 0;
    virtual const char* name() const = 0;
};
```

**`PadSource` is one more implementation of that interface** — the pad's changeset stream in, typed
deltas out, on the same lock-free ring, with the same back-pressure rule (`dropped_` counted
loudly, because a silently dropped percept is the turn reborn inside the loop). Call it 150 lines.
`FileTailSource` is the worked example beside it.

Three constraints the file already states, which the blueprint would have had to discover the hard
way:

1. **Self-echo (spec §5.8).** *"A delta whose lane is one of FUSOR's own seats must NEVER be fed
   back as input, or each nucleus deliberates about interrupting itself."* In a pad the resident
   writes into the very buffer it reads. This law is load-bearing here in a way it is not for a
   file tail — `PadSource` must filter the resident's own authorship out of the intake, at the
   source, not downstream.
2. **`kPayloadMax = 496`** — and both numbers this sentence used to carry were wrong, measured on
   2026-09-04 (SPEC 5.1.7): a `Delta` is **528** bytes, not 512, and `fill_delta` keeps the last
   payload byte for a NUL, so the lossless bound is **495** (`nib::kChunkMax`), not 496. Chunking
   "at that boundary" would have lost one byte per full chunk, silently. The compiler in
   BLUEPRINT §4 still gets a hard upper bound for free; it is one byte lower than the header says.
3. **The lane string is train ≡ serve.** The trunk sees `[lane] text` byte-identically to the
   soak and tune format, or v11's dial-0 calibration is off-distribution. A pad's lane naming is
   therefore not a UI decision.

---

## 5 · What is actually left to build

Ordered by risk, largest first.

1. **The editor shell.** A native Win32 text editor: buffer view, cursor, selection, undo, find,
   files, DPI, the status line. This is the genuinely new piece and the bulk of the work. Nothing
   in the estate does it; `hud.cpp` renders, it does not edit.
2. **`changeset.cpp`** — the port of §2(b), with Etherpad's own vectors as `--selftest` by
   equality. Bounded, mechanical, and testable before a single pixel is drawn.
3. **`PadSource`** — §4. Small.
4. **The emit path into the pad**: the resident's lane, the forming region, floor control
   (BLUEPRINT §5), and un-saying rendered as withdrawal (BLUEPRINT §6).
5. **Lifted verbatim, not rewritten:** fusord's loop, the segmenter, the seat and the byte-frozen
   seed, the fabric's tape, the two switches.
6. **Act II:** the UDP beacon and peer table, rooms, and the host-as-serializer of §3.

The revised order of the stages in BLUEPRINT §8 stands; only the contents of Stage 0 change — it
now means *the editor shell plus the changeset port, greened against Etherpad's vectors*, with no
model in the process.

---

## 6 · What this changes about the laws

- **BLUEPRINT §2's document model is now Etherpad's changesets**, not an invented op log with
  character identities. Same property (the document is a stream of ordered edits, replayable), a
  proven encoding, and a free test oracle.
- **CLAUDE.md rule 2 survives Act I intact** — and it survives *because* of choice (b). Bundling
  Node would have ended it immediately. Worth stating plainly: the no-network-stack law is what
  made the porting decision, not the other way round.
- **CLAUDE.md rule 6 gains a mechanism.** Etherpad already tracks authorship per character; the
  port inherits it. "It is never ambiguous who is on the other end" stops being a promise and
  becomes a property of the format.
