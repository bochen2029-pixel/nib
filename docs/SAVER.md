# nib — the screen saver: the resident holds the floor

*Branch `saver`, built 2026-09-05 from 0.10.3 by the operator's word. The design is
`docs/BRAINSTORMS_2026-09-05.md` §4; the normative clauses are SPEC 6.5; the measurements are
below and in the devlog. This file is what the mode is, what it cost, and what it measured.*

---

## What it is

Press `Ctrl+Shift+M`. The mind switches on if it is off. A single line arrives on the pad as
**world**, on the host's lane:

```
[host] Watcher, talk to me about anything until I interrupt.
```

The SPEAKER's mandate is to answer when directly addressed, so it answers — and then it keeps
going, one sentence at a time, each appended as its own `[SPEAKER] ` line at the tail of the
document. You type wherever you like. Your typing *is* the interruption: the seat sees it between
two generated tokens, and either goes on, or takes the sentence back mid-word, or lets its next
sentence be about what you just said. Press `Ctrl+Shift+M` again and it stops.

**The three positions of one axis.** TURN-BASED speaks when asked, RESIDENT on evidence, the SAVER
unless stopped. Same seat, same seed, same sampler; only the trigger differs.

## What was already there, and what had to be built

Nearly all of it existed. The seam of Stage 3 drains the ring after every generated token; the
re-probe on a fresh fork decides whether the sentence still stands; the forming plane renders it;
own speech reaches the trunk through the document; the manners persist across the switch.

Four things were new:

| what | why |
|---|---|
| **A want that renews itself** | A want is born at a boundary and dies when composed. The saver's seat wants again the moment its own line joins the trunk, so the monologue continues at all. |
| **The kill read in both directions** | The seam killed a sentence when the margin fell to zero. In the saver a sentence may *begin* below zero, and the flip that matters is upward: the world just said something the seat itself wants to answer. One law — the judgment changed **sign** — read both ways. |
| **The per-block floor** | SPEC 6.3.2 always meant "a block the human is touching"; the wire enforced it globally. The saver composes while you type elsewhere, and its line is refused only when your last keystroke touched the tail. |
| **A continuation cue** | See below. This is the one that cost a measurement to justify. |

## The measurement: the monologue horizon

**The horizon is the number of sentences said before the manners refuse the seat and it holds.**
It is the number the next tune is scored on, and the reason the brainstorm put say-it-once ahead
of every other idea on the checklist.

Measured on this box, 2026-09-05, `Qwen3.5-9B-emit-v11-Q5_K_M`, `n_ctx` 16384, q8_0 KV, the CLI's
`--resident FILE --saver`, three pads: an empty file, four lines of notes, and `tests/margins.txt`.

**Under the pinned speak-cue alone (`--saver-pinned-cue`): one sentence.** Every renewal came back
as a near-verbatim repeat of the line before it, and the manners refused it:

```
SPEAKER  +3.59  ->  "I've been monitoring the stream; let me know if you need a second pair of eyes on anything."
SPEAKER  +1.19  --  suppressed (repeat): "I've been monitoring the stream; let me know if you need a second pair of eyes on anything."
monologue: 1 lines said · horizon REACHED
```

The reason is in the cue's own words. It asks for *one sentence about what you just perceived in
the stream* — and a seat holding the floor has just perceived **its own last line**. The pinned cue
is a cue for interrupting, and it does that job exactly as tuned. It is not a cue for continuing.

**Under the saver's continuation cue, with the paraphrase valve tuned once (below): one to eight
sentences, and they develop a thought rather than restate one.** The cue is not in the serve hash
and is not pinned; it asks for one more sentence that adds something new. The gate that makes the
seat speak is still the pinned probe, the sampler is still the pinned chain, and every row says
`cue: saver`.

| pad | lines said | seconds | retried | refused |
|---|---|---|---|---|
| empty file | **8** | 4.6 | 2 | 3 |
| four lines of notes | **3** | 2.4 | 1 | 2 |
| `tests/margins.txt`, after the ordinary run | **1** | 1.8 | 1 | 4 |

The empty pad, in full, is the one to read — it is eight sentences with nothing to talk about
except the fact of watching, and it develops:

```
SPEAKER  +3.59  "I've been monitoring the stream; let me know if you need a second pair of eyes on anything."
SPEAKER  +1.19  "Let me know if you'd like a quick status check on any of the active threads."
SPEAKER  -1.44  "The active threads look stable; is there anything specific you'd like to dive into right now?"
SPEAKER  +0.35  "That's a fair ask — I'll keep it to myself until something clearly stands out."
SPEAKER  -1.95  "Given the stability of the active threads, a good time to re-evaluate the exit strategy would be now rather than later."
SPEAKER  -2.50  "The exit strategy re-evaluation is timely, but it might be worth confirming that the data pipeline is still within acceptable variance…"
SPEAKER  -2.26  "One thing that stands out is that the variance margin is tighter than usual, so a second look at the pipeline might be warranted…"
SPEAKER  -2.54  "That tighter variance margin could be a sign of the pipeline hitting its performance ceiling, not just normal fluctuation."
monologue: 8 lines said in 4.6 s · horizon REACHED: the manners refused the seat and it held
```

The notes pad is three, and the chain is a real one: postgres 16 and the migration script → the
script assumes a schema that may not exist → so the first run fails and the retry logic must
handle *not found* rather than treat it as transient. The margins pad is one, and correctly so:
the SPEAKER had already said its piece in the ordinary run before the saver started, and its own
history refuses the restatement.

Two things in these transcripts are worth more than the horizon itself.

**The margins go negative and the mode keeps speaking, on the record.** The empty pad's run starts
at `+3.59` and crosses to `−1.44` by the third line: the seat is asked, at every renewal, what it
would do on its own, and after two or three lines it answers *hold* — and the record says so beside
every line. That is the honest shape for this mode. The trigger is the mode, never a margin the
mode invented, and a reader of the tape can see exactly how much of the monologue the resident
itself wanted. Two lines out of eight, on that run.

**The manners are what ends it, and they end it correctly.** The refusals are real repeats and
real restatements, not arbitrary. The disposition, not the harness, is the limit — which is the
estate's standing position on say-it-once since 2026-08-12, now with a number attached.

### The manners, tuned once, with the reason

The paraphrase valve refuses a line sharing `dup_overlap` content words with the seat's last. That
constant is **3**, measured for a seat *restating a catch* in a watch-room. A monologue about one
document shares that document's nouns in every sentence, and at 3 the valve refused

> "postgres 16 is fast, but the migration script assumes a schema that might not exist yet"

as a repeat of

> "the staging database is postgres 16, and the migration script is written for it"

which is a different claim about the same subject. The saver's seat uses **5**
(`saver_dup_overlap`); the six-in-ten near-duplicate test is unchanged and still catches a line
said twice. Both constants are on the session row of the tape. Measured effect on the empty pad:
**three lines at 3, eight at 5**, and the three-line run's refusals were paraphrases while the
eight-line run's are real repeats.

## What is honest about it

- **Rule 5 holds absolutely.** The saver types at the sampler's cadence and no other. Its pause is
  a re-probe that came back one way; its continuation is a re-probe that came back the other. It
  invents no hesitation and no tell.
- **The pin does not move.** The seed, the seats, the probe frame and the speak-cue frame are
  byte-frozen and `serve_hash()` still computes fusord's `0xe7ffa5704ba31076`. The saver's cue is
  a *runtime* string that composes a line after the pinned gate has decided; it is off the pin by
  construction, it is declared on the session row, and every line it produced says so.
- **The margin is not laundered.** A negative margin under the saver is printed and taped as a
  negative margin. Nothing rewrites it to justify the mode.
- **Rule 6 holds in the file.** Every line the resident writes is its own `[SPEAKER] ` block; the
  human's paragraphs are bare; the file is a valid lane stream.
- **It is a switch, never a timer.** A screen saver that woke on idle time would be the poll
  reborn inside the loop; silence is world (SPEC 5.1.9) and never a clock that wakes the mind.
  Entry on idle time would be a policy over this switch, decided later and recorded the same way.

## What it is for, beyond being an Easter egg

It is the **disposition soak** the estate has never had. A monologue is the harshest possible test
of say-it-once, it runs unattended, and its tape is a corpus of exactly the failure the next tune
must fix — with the seat's own margin beside every line, which is the label. Two numbers fall out
of ordinary use: the horizon, and the ratio of lines the seat itself wanted to lines the mode
extracted.

## The oracles

```
build.bat                                        /W4 /WX, zero warnings, both gates
nib.exe --selftest                               243 passed, 0 failed   (six at the saver, no model)
python tools/drive.py --exe C:/nib-saver/nib.exe  30 passed, 0 failed
python tools/drive.py --exe … --ai                78 passed, 0 failed   (eleven at the saver; needs the card)
nib.exe --resident FILE --saver [--saver-lines N] [--saver-pinned-cue]
```

The window case switches the mode on, waits for two lines said unasked, asserts they are the tail
of the document and the human's lines untouched, types on the human's first line while the
monologue runs and asserts every byte landed there, asserts the resident answered the
interruption by going on or by taking a sentence back, switches off and asserts at most one line
already in flight followed, then reads the saver's switch rows, the host's one line and the emit
rows off the tape and verifies its chain.

## Open, on this branch

- **The horizon is short and that is the finding**, not a defect to engineer around. Widening the
  manners would buy a longer monologue of worse lines; the fix is a tune.
- The saver's cue is unpinned and therefore uncalibrated. If the mode is kept, the honest end state
  is a *tuned* continuation disposition, at which point the cue joins the pin.
- Entry on idle time, and a resumption that reads the room (the vision brainstorm's `[focus]` lane
  would be the right trigger, and it is the null that needs no camera).
- The other two seats keep the global floor while the saver runs; whether a SKEPTIC should be able
  to interrupt the SPEAKER's monologue is a real question the week would answer.
