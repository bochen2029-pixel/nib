# nib — blueprint

*Design 0.1 · 2026-09-03 · Bo Chen (operator) + Claude Opus 5 (synthesis) · MIT · Windows 10
1809+ / 11, x64, C++20, one exe + llama.cpp. **Act I: no network stack, single user, local
weights.** Act II: the LAN.*

---

## §1 · The one job, and the observation it rests on

**A writing surface with no send key on either side.**

Etherpad did not fail as technology. It failed as a *matching problem*: its entire value was
watching thought form, and that value was locked behind simultaneity — the scarcest thing in
collaboration. Asynchronous tools won because they did not require two people to be free in the
same instant. Real-time editing was fine; the other end of it was empty.

A resident mind is the first thing that is always free. So the first build is not a collaboration
tool with an assistant bolted on. It is **the second party**, and the network can wait.

Two properties make the fit exact, and neither alone is enough:

- **On the way in**, a pad is already a stream of small ordered edits — which is precisely what
  decode-on-delta consumes. `FileTailSource` becomes `PadSource` and the loop is unchanged.
- **On the way out**, a pad already renders another author's characters as they arrive — which is
  precisely how a streaming mind speaks.

The two halves are the same channel at the same grain. That is why this reads as a person rather
than a tool, and why it could not have been built on a chat box.

**And there is no system prompt.** The system prompt is an artifact of amnesia: a turn-based model
must be re-told who it is because it forgot. You have never handed a colleague one. What the
resident is *for* stays owner-set — the seat's mandate, per FUSOR's law that purpose is imported
and never learned — but the subject, the context and the shared history accrete from the writing
itself. Disposition is configured; content is grown.

---

## §2 · Anatomy (Act I)

```
 keystrokes ──▶ [buffer + op log] ──▶ [compiler] ──▶ [resident: fusord's loop] ──┐
                     ▲     │             tokens         hold · emit · abort      │
                     │     └──▶ [tape: ops + judgments, one hash chain] ◀────────┘
                     │                                                           │
                     └───────────── [floor control] ◀── forming region ◀─────────┘
```

| module | owns |
|---|---|
| `doc` | the buffer, character identities, the op log, replay, undo |
| `edit` | the editor proper: view, cursor, selection, find, files, DPI, the status line |
| `compile` | ops to tokens: the batcher, the modality's buy-in (§4) |
| `resident` | fusord's loop, lifted: segmenter, hold/emit on the free tail, the seat, the molt |
| `floor` | where an emit may land and when (§5) — a serializer rule, not a policy |
| `tape` | append-only, hash-chained; ops, judgments, margins, aborts, mode changes |
| `hud` | the status line, authorship colouring, the forming region's rendering |

### The document model

Even with one user, **the document is an op log, not a string.** Each character carries a stable,
unique, ordered identity from the moment it is typed. Act I has one writer and therefore one
trivial serializer; Act II swaps that serializer for convergence **without changing the document
model**. Building a string editor now and retrofitting identities later is the single decision that
would cost a rewrite, so it is made on day one — honestly, for Act II's sake, not because Act I is
concurrent.

---

## §3 · The two switches (the operator's, 2026-09-03)

A partner who is always there is also an always-open channel. Measured: the vise at dial zero
produced about 59 distinct conditions an hour on a stream — and a stream is not somebody's elbow.
The disposition tune's 63.4 % to 6.7 % is not a refinement here; it is the difference between a
colleague and a haunting. Two switches, both on the status line, both on the tape:

**1 · AI — on / off.** Off is *off*: no ingest, no trunk, no context held; the process is a text
editor. Not muted, not paused. The status line says so and the tape records the transition.

**2 · RESIDENT / TURN-BASED.** Same weights, same seat, same mandate. In resident mode judgment
rides the free tail of ingest and the mind speaks on evidence. In turn-based mode it speaks only
when addressed, and the trigger is a key you press.

That second switch is worth more than livability, and this part gets built deliberately:
**it is the twin race, instrumented in the product.** The one number the estate says matters next
is a resident against a maximally-good turn-based twin on identical weights, blind-graded. Every
user who flips that toggle during real work produces a paired sample — same person, same document,
same task, same model, one variable. The tape already records the mode; nothing extra is needed
except the discipline that keeps the halves comparable:

> **The seat, the seed and the sampler are identical across the toggle. Only the trigger differs.**

Break that and the toggle becomes a preference instead of an experiment.

---

## §4 · Ingest: typing as a modality

FUSOR's rule is that a modality buys in through a compiler into the vocabulary that perception and
prediction share, and everything after the compiler is already paid for. Typing buys in cheaply,
because it is already text.

The compiler's only real decision is **grain**. Per keystroke is wrong — the mind would ingest
`t`, `h`, `e`. Etherpad batched at roughly seven characters or half a second for its own network
reasons, which is coincidentally near the right order here. So: emit a percept at whichever comes
first — a word boundary, N characters, or T milliseconds of quiet — and tokenize that.

Three things the compiler must never do:

- **Drop.** Ingest is unconditional; only judgment cadence is modulated. A ring that overflows
  counts the loss loudly, because a silently dropped percept is the turn reborn inside the loop.
- **Hide deletions.** A person backspacing a sentence is a percept, and often the most informative
  one in the stream. Deletions enter as events, not as a silently reconciled string.
- **Editorialise.** No summarising, no cleaning, no "the user seems to be writing about X". The
  world is never edited.

Silence enters as world rather than as a question: idle ticks, exactly as `fusord` does it.

---

## §5 · Floor control — the genuinely unsolved part

The emit gate decides *whether* to speak. It does not decide **where the words go and when**, in a
buffer somebody else is also typing into. Two humans negotiate this continuously and unconsciously.
Nobody has had to solve it for an entity with no turns sharing a surface with humans who also have
no turns. Treat this as the research problem of the build, not a detail.

**Act I's rule, deliberately crude and enforced by construction:**

- The resident writes **its own blocks**, never inside a human's paragraph.
- An emit targeting a block with a human keystroke inside the floor window (start at 2 s) is
  **refused before it is composed** — the serializer rejects it; the gate never gets a chance to be
  polite about it.
- The resident's block is inserted after the block the human most recently completed, and it never
  moves text a human wrote.

Later stages may earn the right to write inline. Act I does not get it, because an assistant that
rewrites the sentence you are typing is intolerable in a way no accuracy makes up for.

**Pacing.** The mind must not dilate the world any more than the world dilates for the mind. A
9B-class model on this card out-types every human in the room. Emission is paced — but pacing must
express a real internal state and never perform one (CLAUDE.md rule 5). Holds are silence. Aborts
are visible retraction. A thin margin may legitimately slow emission, because the mind is closer to
changing its own. An invented stutter is a lie, and it is the exact place a demo would cheat.

---

## §6 · Un-saying, made visible

This is the act no turn-based system can perform: a sentence killed mid-word when the world
contradicts it, in 13 microseconds, with the killed words on the tape — while the turn-based twin
on identical weights missed the same moment by 65.7 seconds. In a chat box it is impossible, or it
looks like a bug. **In a pad it is an ordinary, legible event**, because the protocol already
renders characters appearing and being withdrawn and the history already records them.

- The forming sentence occupies a **provisional region**: rendered, visibly distinct, never
  committed, never saved, never in the file.
- On abort the characters are withdrawn and the abort is committed to the tape with what was
  formed, what reached the air, and why it died.
- On commit the region becomes ordinary text carrying the resident's authorship.

This is the demonstration, and it needs no narration: a person watches the machine begin a
sentence, sees their own next keystroke contradict it, and sees it taken back.

---

## §7 · Act II — the LAN, and what replaces the build gate

Only after one person alone finds the pad worth using.

- **Discovery:** IPv4 only. A periodic UDP beacon and a peer table with a TTL. No IPv6, no mDNS
  dependency, no configuration.
- **Rooms:** anyone hosts. Listed · unlisted (join by exact name) · hidden. **The host is a
  bouncer, not a bottleneck** — it advertises and admits; once you are in, editing is peer to peer,
  so the room survives the host closing their laptop.
- **Passphrases gate readability, not merely admission.** Derive a room key from the passphrase and
  encrypt the room's traffic with it. Otherwise anyone on the segment reads the room anyway and
  "password-protected" is theatre.
- **Convergence:** the character identities from §2 start earning their keep. The resident's tape
  becomes *what this mind perceived and did*, not what the room objectively was. Say that plainly:
  it is a real weakening of the tape's claim and the honest price of having no serializer.
- **Late percepts:** on a healed partition, edits arrive from the past. A resident that already
  emitted on an incomplete view must withdraw — so partition-heal is wired into the abort path
  deliberately rather than discovered there.
- **The build gate changes.** Act I forbids sockets outright; Act II cannot. What replaces it: the
  linker gate narrows to *no HTTP client, no DNS resolver*, and the socket layer refuses any
  destination outside the machine's own subnet by construction, with `--about` printing the
  receipt. Egress to the internet stays impossible in the binary, not in a setting.
- **Known deployment truth:** school and campus networks very often run client isolation on the
  access points precisely to stop student devices talking to each other. That kills UDP discovery
  dead. Find out before promising a classroom, and know what the fallback is.

Three seats on one trunk (Speaker · Skeptic · Sentinel — fork at 0 MiB, co-decode at 1.208×) is
what makes moderator *and* contributor *and* watcher affordable in one room on one host GPU. That
is Act III, and it is why the seat abstraction exists in Act I with only one seat in it.

---

## §8 · Stages and falsifiers

- **Stage 0 — the editor, with no model in the process.** Buffer, op log, character identities,
  replay, undo, files, find, DPI, the status line. *Falsifier: replaying the op log does not
  reproduce the document byte-exact.*
- **Stage 1 — ingest, and a resident that only holds.** The compiler, the trunk, the segmenter,
  hold/emit computed and recorded with **emit disabled**. You watch the margins move against your
  own writing before it ever writes a word. *Falsifier: a percept dropped without a loud count, or
  margins that do not move with content.*
- **Stage 2 — emit, in its own blocks, with floor control.** *Falsifier: one emit lands inside a
  block a human touched inside the floor window; or forming text survives a save or a crash.*
- **Stage 3 — un-saying, visible.** *Falsifier: a killed sentence that is not on the tape, or that
  leaves residue in the buffer, or a saved file that ever contained a word that was un-said.*
- **Stage 4 — the two switches, and the paired record.** *Falsifier: the two modes differ in seat,
  seed or sampler — at which point the toggle is a preference and not an experiment.*
- **Stage 5 — a week of real writing.** Emissions per hour at somebody's elbow, holds per emit, how
  often the un-say fired and whether it was right. *Falsifier: the honest answer to "did you leave
  it on" is no.*
- **Act II — the LAN**, per §7, each part with its own falsifier.

**The falsifier for the whole idea arrives at Stage 5 and not before:** if the operator, alone with
it for a week of real work, does not leave it turned on, then nothing in Act II saves it — and the
finding is published beside the laws it bought.

---

## §9 · What is honest about the limits

- The illusion is an illusion, and the laws exist so that it is never a *deception*: authorship is
  per character, the mode is on the status line, and the tape says which machine was on the other
  end.
- A partner always free, never bored, that remembers everything and writes with you at three in the
  morning is more available than any person. That is the value and the hazard in one sentence. The
  estate's laws are about honesty of mechanism; this tool needs one about honesty of relationship,
  and CLAUDE.md rule 6 is it. Cheap now, impossible to retrofit once people are attached.
- This buys presence and co-writing. It does not buy generalisation quality, hierarchical planning,
  or grounding — and the page says so in the same small caps the rest of the estate uses.
