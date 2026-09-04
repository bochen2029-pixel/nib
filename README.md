# nib — a writing surface with no send key on either side

**You type; it perceives as you type. It writes; you watch the words form. Neither of you takes a
turn.** A plain-text editor for Windows in which a resident mind writes beside you in real time —
and later, on a LAN, other people do too.

> **Status: blueprint (0.0.0).** Nothing is built. The design is in `docs/BLUEPRINT.md`, the
> session rules in `CLAUDE.md`. The name is provisional.

## Why this, and why now

Etherpad did not fail as technology. It failed as a matching problem. Its whole value was watching
thought form, and that value was locked behind simultaneity — the scarcest thing in collaboration.
Asynchronous tools won because they never required two people to be free in the same instant. The
real-time editing was fine; the other end of it was empty.

A resident mind is the first thing that is always free.

So this is not a collaboration tool with an assistant bolted on. It is the second party, and the
network can wait. What makes the fit exact is that both directions already stream at the same
grain: a pad's input is a stream of small ordered edits, which is what a decode-on-delta loop
consumes; a pad's output already renders another author's characters as they arrive, which is how a
streaming mind speaks. Neither half alone would have been enough.

**And there is no system prompt.** The system prompt is an artifact of amnesia — a turn-based model
has to be re-told who it is because it forgot. You have never handed a colleague one. What the
resident is *for* stays owner-set; the subject, the context and the shared history accrete from the
writing itself.

## Two switches, on the status line and on the tape

**AI on / off**, where off means off — no ingest, no context held, a plain text editor.

**RESIDENT / TURN-BASED**, same weights and same seat, differing only in what makes it speak:
evidence, or a key you press. That switch is the livability control *and* the twin race
instrumented in the product — every flip during real work is a paired sample with one variable.

## The thing to watch for

A sentence the machine begins, and takes back mid-word, because your next keystroke contradicted
it. That act is the one no turn-based system can perform. In a chat box it is impossible or looks
like a bug; in a pad it is an ordinary event the surface already knows how to render — and the
withdrawn words stay on the tape.

## What it will not do

No network stack in Act I: the model is local, the pad is local, the tape is local, and the build
fails if a socket is ever linked. No cloud model in Act I or II. Forming text is never saved. The
resident never writes into a paragraph you are touching. Nothing simulates a human tell that does
not correspond to a real internal event — no fake hesitation, no invented typos, no "thinking…"
that is not thinking. It is never ambiguous who is on the other end.

## Where it sits

Built on **FUSOR** — the substrate for a mind that does not take turns: decode-on-delta, judgment
on the free tail of the ingest pass, an append-only hash-chained tape, and a seam between forming
and committed that makes un-saying possible. The loop is lifted from `fusord.cpp`, not re-derived;
its seed is byte-frozen and the build refuses to start if a character drifts.

MIT · Access Intellect LLC.
