# nib — a writing surface with no send key on either side

**You type; it perceives as you type. It writes; you watch the words form. Neither of you takes a
turn.** A plain-text editor for Windows in which a resident mind writes beside you in real time —
and later, on a LAN, other people do too.

> **Status: 0.6.1 · Stages 0a–1b built and green (2026-09-04).** The Easysync port, the op-log
> document, the window, the ingest compiler and a resident that only holds — 144 checks in the exe
> and 27 from the window driver, run before every commit. Stage 2 (emission) is next. The normative
> spec is `docs/SPEC.md`; the stages and their falsifiers are `docs/ROADMAP.md`; the rules are
> `CLAUDE.md`; the review that set the current work order is
> `docs/CRYSTALLIZATION_2026-09-04_FABLE5-1.md`. The name is provisional.
>
> One exe, like Notepad++: no runtime, no browser, no script to start it, no Node. (Node appears
> once, at development time only, as the oracle behind `tools/etherpad_harness`.)

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

No network stack until discovery is built: the model is local, the pad is local, the tape is local,
the build fails if a socket is ever linked, and the resident refuses to start if a network module
has entered the process (`nib --about` prints the receipt). When the LAN arrives it is **on by
default** — the shipped pad finds its peers on the local subnet, and the toggle that turns that off
is on the status line and the tape like the other two. No cloud model in Act I or II. Forming text is never saved. The
resident never writes into a paragraph you are touching. Nothing simulates a human tell that does
not correspond to a real internal event — no fake hesitation, no invented typos, no "thinking…"
that is not thinking. It is never ambiguous who is on the other end.

## Where it sits

Built on **FUSOR** — the substrate for a mind that does not take turns: decode-on-delta, judgment
on the free tail of the ingest pass, an append-only hash-chained tape, and a seam between forming
and committed that makes un-saying possible. The loop is lifted from `fusord.cpp`, not re-derived;
its seed is byte-frozen and the build refuses to start if a character drifts.

MIT · Access Intellect LLC.
