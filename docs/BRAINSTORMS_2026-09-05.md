# nib — three brainstorms, 2026-09-05 evening

*Filed 2026-09-05 from the session that built Stages 2 and 3 (`fdcdbabd`, and its fork `c1cad9db`).
The operator asked, after the 0.10.0 handoff, for thoughts on three ideas that had been hashed out
with other sessions before nib existed. They were answered in chat and nowhere else; this file is
the record. The questions are the operator's (voice-dictated, summarised here); the answers are the
session's, kept whole because they are the thinking and not a report of it. Nothing here is a
commitment. Where an idea has a cheap null experiment, that is the part worth building first, and
each section ends with it. Act I's order is unchanged: Stage 4, then the week of real work.*

---

## 1 · Voice as a second lane

**The question.** Two people in a room, each on a laptop, both typing into one shared pad — and
talking. Typing is not the only bandwidth: they chat about dinner while writing a story, and
sometimes the talk is about the document and changes what gets written. All of it real time, none
of it turn-based. FUSOR fuses lanes. With NVIDIA's open ASR and TTS, could nib be that: type with
the resident and talk with it at once, the two channels sometimes crossing and sometimes not?

**The answer.**

Genuinely good idea, and more load-bearing than "add voice later." Sorted by how much it changes.

*The part that is already built.* Voice is not a new modality for this architecture. It is a lane
that already exists. The trunk takes `[lane] text` lines, the compiler closes a clause on a lane
change, the self-echo filter is per lane, and the whole reason those laws exist is that several
sources converge on one context. nib currently feeds that fusion point from exactly one lane.
Speech would be the second.

Two things make that unusually clean. The lane contract in the converged kernel already carries a
`grain` field with `commit` and `forming` as values, which is precisely the distinction streaming
speech recognition forces on you: partial hypotheses that get revised, then a final one that
commits. And the un-say built today is the thing that makes full-duplex speech tractable rather
than the other way round. In audio, "what reached the air" stops being a console approximation and
becomes physically true, and the ability to abandon a half-formed response when the human's
sentence resolves differently is exactly the behaviour Moshi has and nothing with a document has.

*The genuinely new problem, which nobody has had to solve.* Not recognition, not synthesis.
**Channel selection.** When a seat's margin clears zero, which channel does it go on?

That choice carries meaning, and humans make it unconsciously. Speech is ephemeral, soft,
interruptible, and does not touch the artifact. Text is committed, permanent, attributable, and
does. Saying "wait, is that postgres 16?" out loud is a different social act from typing the
correction into someone's paragraph. The crisscross is really a description of people choosing
channels fluidly, dozens of times a minute, without deciding to.

This gives voice a job nib currently cannot do. The gutter is a stand-in for the continuer, the
class of act that never takes the floor. Voice makes the continuer literal. It is the natural home
for everything the floor rule currently refuses, which is a lot: right now a seat that wants to
speak while the hand is moving simply does not.

And the costs are asymmetric in a way that yields a principled rule rather than a tuned constant:

| | text | speech |
|---|---|---|
| a twenty-word line occupies | about 1 s | about 8 s |
| can be withdrawn cleanly | yes, it was never in the file | no, it is in the room |

So the margin threshold for speech should be strictly higher than for text. That is not
arbitrary, it is expected utility with different costs per channel, which is the same Horvitz
argument the review already uses for the emit gate.

*What breaks, and what it implies.*

- **The floor does not generalise.** nib's floor is a dependency test in op-space: did a keystroke
  touch the byte span this emission depends on. Speech has no coordinates and no ops. There is
  nothing to test. Floor control for the audio channel has to come from the turn-taking literature
  instead, and those two floors then interact, which is a mechanism nobody has built.
- **The crisscross is a relational judgment, so it is not free.** Deciding whether the pizza
  conversation is relevant to the database document is exactly the kind of judgment the estate's
  own arity law says never comes cheap from the forward pass. This is the main research question,
  and it is where to expect the first honest failure.
- **Put recognition and synthesis in separate processes.** The lane spool was designed for this. A
  speech process writes frames to a file; nib reads them as another lane. One exe, one model in it,
  no sockets, and the no-network law survives untouched. Putting three models in one process on a
  shared 16 GB card is how you rediscover the 47× silent CPU fallback.
- **Authorship gets harder.** In a room with two people and a machine that talks, the audio channel
  has no visual marker of who is speaking. The rule that it is never ambiguous who is on the other
  end is a design constraint on the voice, not a nicety.

*Two opinions about sequencing.*

**The cheapest honest experiment needs no speech at all.** Two typed lanes into one trunk: the
document, and a side channel. Talk to it in the side channel while writing in the document, and see
whether it holds two topics without confusing them and whether the crisscross lands. If it cannot
do that with two text lanes, recognition and synthesis will not rescue it. That is a day's work
against what exists now, and it is the real null.

**Do not do this before the week of real work, and specifically not before say-it-once is fixed.**
Today's measurement is the argument: in one long session the seats composed fourteen sentences,
said six, and the other eight were the same two lines proposed over and over, caught only by the
suppression ladder. In text that is a refused row on a tape that nobody sees. Out loud it is a
machine repeating itself in the room, and it would be intolerable within a minute. The disposition
fix is a prerequisite for voice, not a parallel track.

One last thought worth keeping. Voice and text map almost too neatly onto the two switches: asking
out loud is where turn-based feels natural, and noticing unasked is where residency earns its
keep. A version with both channels would let you feel the difference between the two modes in a
single session, which is a better instrument for the twin race than a toggle.

**The null, when its time comes:** two typed lanes into one trunk, a day against what exists.

---

## 2 · The two-gear method: escalation

**The question.** FUSOR need not be the only gear. A second gear could be a larger local model
(turn-based, not always on) or a frontier model over an API — a very fast one (Cerebras-class,
thousands of tokens a second) or the most capable one available. The 9B becomes a hybrid: still
the perpetual watcher, but able to escalate one or several calls out and, when the returns come,
pick one and stream it back into a lane. How would it decide to escalate, how does a return get
back in, what if several come back, how long does it wait when nothing is turn-based, and can this
ride the same backbone as everything else — including the voice idea above?

**The answer.**

Yes, and a working implementation of this has been read: K5 has gear 2 already, and the lift map
refused it for nib with a one-line reason. Thought about properly, that reason was too cheap.

What K5 does, precisely: a thin margin writes a brief to a file with the clause, the last twelve
lines of held context, and a question. Something else answers on the spool as lane `counsel`,
tagged with the brief id. The kernel never opens a socket. On return it computes how far the trunk
has moved since the question was asked, and if the drift exceeds a token budget the answer is
discarded as a typed negative on the tape. The admitted answer enters the trunk as `[counsel] …`,
testimony with a source, never belief.

That answers "how does it stream back in" completely. It comes back as a lane. Everything
downstream is unchanged, which is the whole point of the fusion design.

*The split to make, which K5 does not.* K5 escalates the **judgment**: I cannot rank this instant,
tell me whether to speak. The more valuable escalation is the opposite one, and the distinction
matters more than any other choice here.

The 9B's judgment is the thing that was tuned and is the product. Its phrasing and its factual
reach are the weakest parts of it. Today's runs show exactly that: the catches are correct and the
lines are generic. So:

- **Escalating the judgment** risks laundering an untuned gate through a bigger model. Thin margins
  are common, so it fires often, and it papers over the thing the next tune is supposed to fix.
  Restraint is the product; buying it wholesale is a way of never learning it.
- **Escalating the composition** keeps the gate local and pinned. The seat decides it will speak,
  with its own calibrated margin, and only asks for better words. It fires exactly as often as it
  speaks, which today is a handful per hour of writing. That is an affordable bill and a much
  smaller trust surface.

Build the second and treat the first as a research question.

*Timing, and why you never wait.* The clock already exists, and it comes from the interaction
rather than a configuration. A seat's want is recorded at the boundary, but nothing is composed
until the hand has been still for the floor window. **Dispatch at the want, deadline at the
floor.** If the answer is back before the pause opens, use it. If not, compose locally or hold.
Nothing blocks, nothing polls, and the deadline is the human's own pause rather than a number
someone picked.

That also splits the two targets cleanly, because they are different products:

| target | round trip for a twenty-word line | fits inside a two-second pause |
|---|---|---|
| fast inference, Cerebras class | well under a second | yes |
| frontier, normal speed | three to ten seconds | no |

The fast one can participate in the same floor window and write in the same breath. The slow one
cannot, and should be treated as K5 treats counsel: it arrives late, it is subject to the
staleness gate, and it enters as testimony to be judged rather than as an answer to be printed.

**On several returns.** Do not pick the most fluent. Score each candidate by its likelihood under
the resident's own trunk and take the one most consistent with the world it actually perceived.
That is a unary quantity the forward pass already computes and throws away, it needs no new
calibration, and it does not touch the pinned probe frame. Then record every candidate and the
winning score, so the choice is auditable rather than a preference.

The law that makes all of this safe is the one the corpus already has: remote proposes, local
disposes. If a frontier answer were written straight into the document you would have built a
router, not a resident. It has to arrive as world, be judged, and be capable of being held.

*The cost you would be paying, stated plainly.* This ends the strongest promise the product
currently makes. The status line says `0 B egress` and that is structurally true today, enforced
at build and at runtime. The moment gear 2 dials out, what leaves is not a summary, it is the
person's actual writing plus the held tail.

Three things make that survivable, and all three already exist. Put the dialling process outside
nib so the binary still links no socket and the module gate still passes. Make the egress counter
read real bytes. And record the exact payload in the tape, so the claim upgrades from "nothing
left" to "here is precisely what left, and when, and why." Almost nothing ships that second claim,
and nib can, because the tape is already there.

With voice on top, the split is neat: the fast gear takes the spoken channel, where latency is felt
most and a generic line is most obvious, and the slow gear takes the written one, where arriving
late is tolerable because text is spatial and can attach to the paragraph it is about.

**The first experiment, and its null.** One target, composition only, dispatched at the want and
deadlined by the floor. Measure one thing: do escalated lines survive in the document longer than
local ones, by character identity, at one hour. The label is free and the pairing is clean,
because both arms share a trunk and a boundary. The null to beat is the 9B alone. If escalated
lines do not survive better, the gear is theatre and you have learned that cheaply.

**And the reason to build it even if the answer is marginal.** Every escalation produces a pair:
what the resident would have said, and what a much better model said, on the same trunk at the
same instant, with the human's subsequent behaviour as the label. That is distillation data
accruing from ordinary work. Gear 2 is not only a crutch for the 9B, it is the teacher for the
next tune of it. The reactor builds the reactor, which is the corpus's own line, and this is the
first mechanism in this project that would actually do it.

**Standing constraints, from CLAUDE.md:** no frontier API in Act I or Act II (rule 9); when it
arrives it is behind a flag, the room shows a permanent badge, and the egress counter stops reading
zero. The dialler lives outside the exe so rule 2's gates keep passing.

---

## 3 · Vision as a floor sensor, and lanes at different speeds

**The question.** The most ambitious version: one more modality. With a person in the room you
can look up and see their face — distracted, looking at you, or not. Tavus already lets an AI see
you through the webcam. Not the avatar half (seeing its face matters less) but the other way: a
FUSOR-based real-time vision lane watching the person through the webcam while they type and talk.
And the lanes will not all run at the same speed, so they have to be calibrated to work together.
Brainstorm all of it, deeply.

**The answer.**

The most useful thing to say is that vision is **not a third content lane**. It is a floor sensor.

A webcam tells you almost nothing about the document and almost everything about *when*. Is the
attention here. Did it leave. Did it come back. Is the person addressing you or thinking aloud.
Every one of those is an answer to the one question nib currently guesses at with a two-second
constant, and the guess is the weakest mechanism in the whole system. Framed that way, the camera
does not make the resident more fluent — it makes its **silence better informed**, which is the
only thing this project claims is scarce.

*The shape it already has a slot for.* The tick. `[tick +45s]` is already a non-linguistic,
non-content event that enters as world, is decoded raw, and **never fires a probe round**. Vision
events are that exact shape: `[gaze away 12s]`, `[gaze back]`, `[face absent 3m]`. World,
perceived, cheap, and judged never.

And the split that matters follows from it: vision events **enter the trunk as world**, and
separately they **modulate the floor gate**, which is a serialiser rule and not a model call. The
model never decides whether you are paying attention; a detector reports and a deterministic gate
uses it. That keeps the mechanism where hazards are unreachable rather than forbidden.

*The arity trap, and why no VLM belongs in this loop.* "Is he distracted" is a relational claim.
The corpus's own law says every entry in the graveyard is a relational claim made with a unary
instrument, and a VLM asked that question at 5 Hz is the next entry. But **gaze on/off screen, face
present, head pose, hands on keys** are near-unary — properties of a frame or a short window,
cheap, and calibratable against a labelled outcome.

So: a small dedicated detector emits a closed vocabulary of events; the **trunk** does the
relating, because the trunk is the only thing that holds the document, the typing rhythm and the
conversation at once. That is also the right side of Addendum Q's split — a phrasebook *compiles*
into the predicted vocabulary and inherits the prediction channel, where projected frames buy a
projector and no token-surprise. Cite the 2023 prior art on closed phrasebooks and its 3.3× tax
rather than claiming it.

*Lane calibration, which is the real question.* Lanes differ on four axes, and only the first is
the one people think about.

| lane | rate | latency to certainty | revisable | may trigger a judgment? |
|---|---|---|---|---|
| keystrokes | bursty, ~8/s | zero — each is a commit | no | yes (punctuation, N, T) |
| speech partials | 3–10/s | 200–800 ms | **heavily** | **no** |
| speech finals | ~0.2/s | at the endpoint | no | **yes — the endpoint is a better boundary than punctuation** |
| vision events | 30 fps raw → ~0.05/s debounced | 100–500 ms | hysteretic | **never** |
| gear-2 return | rare | 0.3–10 s | no | yes, as testimony |

**Latency.** You cannot have both zero delay and correct order — the trunk is append-only, so
order is destiny. The resolution is already a law in the corpus, pointed the other way: *the world
never dilates for a slow model; a late thought is a dropped thought and the drop is an event on the
record.* Its mirror is **the trunk never waits for a slow lane.** Run a commit horizon a few
hundred milliseconds behind real time; every lane stamps events with when they *happened*, not
when they were parsed; at the horizon everything older commits in event order, and anything
arriving after its slot is committed with a `late` marker or dropped with a count. The lane
contract's `t_mono_ns` field exists for exactly this and nib does not use it yet.

**Revisability.** Streaming recognition revises its hypotheses, and that is the forming/committed
seam already built, arriving from the other direction. Partials are `grain: forming` — rendered,
never on the trunk. Only commits enter. **Only commits kill** and *only commits enter* turn out to
be the same law.

**Judgment rate, which is the scarce one.** A probe round is 107–122 ms on a quiet card and
312–327 on a busy one. One lane produces a boundary every sentence or so. Four lanes could probe
continuously, and "judgment rides the free tail of the ingest pass" stops being true. Vision must
never trigger; speech endpoints should, because a prosodic endpoint is a *better* boundary than
punctuation; and there needs to be a global probes-per-minute budget with the coarsening recorded,
the way backlog coarsening already is.

**Bandwidth.** The trunk is a fixed window shared by every lane. A chatty vision lane spends
tokens that the document does not get, and the molt watermark arrives sooner. Every lane needs a
token budget, and the tape should record tokens-per-lane so you can see who is eating the context.

*The constraint nobody will like.* The serve format is byte-frozen and the tune only ever saw
`[lane] text`. There is no field for time. **The only temporal primitive v11 knows is the tick**,
so all cross-lane timing has to be expressed as ticks between events — seconds of resolution,
paid in tokens. Anything richer (`[bo +2.3s]`, per-lane rates, overlapping speech) changes the
serve bytes and breaks the pin, which means a retune.

That is not a blocker, it is a sequencing fact, and it has a pleasant consequence: build the lanes
coarsely now, and the multi-lane tape *is* the v12 corpus — with the human's subsequent behaviour
as the label. Same argument as gear 2 being the teacher for the next tune rather than only a
crutch for this one.

*Where vision genuinely earns its keep.*

- **Addressing.** The SPEAKER's mandate is "when directly addressed", currently detected from the
  literal word "Watcher". Gaze plus speech is how humans mark address, and it disambiguates the
  crisscross: talking while looking at the camera is probably to it; talking while looking at the
  paragraph is probably thinking aloud about the paragraph.
- **The latency budget for gear 2.** If the person just looked away, a slow frontier call is free;
  if they are staring at the screen waiting, only the fast gear can play. **Attention is the
  deadline** — a measured one, rather than the keystroke-gap proxy the floor uses now.
- **Breakpoint hierarchy.** The review already wants the floor to be a hierarchy rather than one
  constant, because within-word, between-word and end-of-paragraph pauses differ by an order of
  magnitude in resumption cost. A camera measures the thing that constant is standing in for.

*The discipline that keeps it honest.* Rule 5 governs the machine's own tells. What governs
*claims about the person* is the instrument law, and the precedent is already in the corpus:
surprisal is **logged and gates nothing**, because it was measured inverted twice. Vision goes in
the same way — every event on the tape from day one, gating nothing, until you can show on the
tape that "gaze away > N" predicts "the emission was deleted within the hour". Only then does it
move the floor.

Two hard lines. **Frames never touch the tape** — only derived events; the tape records what was
*perceived*, not what was *seen*, and an append-only durable record of a person's face is not a
thing to build by accident. And the camera process stays outside the binary, so the module gate
still passes and the egress counter still reads real bytes. That claim gets much more valuable
and much more load-bearing with a camera in the room.

On the avatar: the asymmetry is correct and it is principled, not just scoping. Every expression
on a face is a tell, and rule 5 says a tell must correspond to a real internal event. The only
honest face is one whose expressiveness renders the margin — which is the gutter, with eyes.
Better to keep the gutter.

**And the self-echo law arrives in physical form:** with a microphone and a speaker, its own
voice comes back through the mic and the recogniser transcribes it. Acoustic echo cancellation and
`is_seat(lane)` are the same law at two layers, and the corpus already knows what happens when
the mind fails to recognise its own speech — it deliberates about interrupting itself.

*What to expect to break first.* The crisscross judgment, because it is relational. Hysteresis,
because raw gaze is jittery and an undebounced lane is fusord's `;`-and-`:` over-firing all over
again. And authorship — a camera can perceive a *second person* who never touched the keyboard,
which is Act II arriving through a side door before the rules for it exist.

**The cheapest experiment, and it needs no camera.** There is already a perfect attention signal
with zero hardware and zero privacy cost: **window focus.** `[focus] left 40s` / `[focus] back`
has exactly the shape, exactly the semantics, and is perfectly reliable. Wire that as a second
non-linguistic lane, let it modulate the floor, and measure whether emissions timed to a
focus-return survive longer than emissions timed to a keystroke pause.

If attention-as-a-lane does not help when the signal is *noiseless*, a webcam's noisy gaze will
not rescue it. That is the null, and it is an evening's work against what exists today.

---

## 4 · The screen saver: the resident holds the floor

**The question** (added the same evening, from a voice note; on the checklist, not to be built now).
An Easter egg: a screen-saver mode in which the resident kernel is spun up with one standing
instruction — just start typing. It talks about whatever, without stopping, because by definition
it can. The human can interrupt at any time, and "interrupt" means: move the caret to a different
line and start typing. Etherpad is concurrent character by character, so both can be typing into
the pad at once on different lines (and either could overwrite the other, if they wanted to troll).
The resident sees the human typing and decides what to do: ignore it and keep going; pause and
watch what the human is doing; or keep typing on its own line while the human types on theirs, and
change what it is talking about in response — the way a person would.

**The answer.**

*What it is, in nib's words.* Everything nib has built so far has one polarity: silence is the
default, the seat speaks on evidence at a boundary, and the hand holds the floor by moving. The
screen saver inverts the polarity: **the resident holds the floor by default, and the human's
typing is the interruption.** That is the same floor problem seen from the other chair, and it is
worth having for that reason alone — the floor rule was written for a resident that must not write
into the human's sentence; this mode asks what the resident does when the human writes into *its*
session. And the three positions on the initiative axis fall out cleanly: TURN-BASED speaks when
asked, RESIDENT speaks on evidence, the SAVER speaks unless stopped. Stage 4's switch has a third
position waiting for it, and the same discipline applies — same seat, same seed, same sampler,
only the trigger differs — so a flip into and out of the saver is still a paired sample.

*What already exists.* Nearly all of it, which is the surprise. The seam of Stage 3 drains intake
after every generated token; a human line that lands mid-sentence is on the trunk before the next
token; the speaking seat is re-probed on a fresh fork of the updated trunk and the sentence dies if
the margin flips or the world settles the point. Read those as the three verbs the note asks for:

| the note says | the mechanism, already built |
|---|---|
| ignore it and keep going | the re-probe comes back positive; the sentence continues to its close |
| pause and see what the human is doing | the re-probe comes back at or below zero; the sentence is killed mid-word, the seat holds until the human's next boundary |
| keep typing while the human types, and change the subject | the sentence finishes (it was conditioned on the world at fork time); the *next* one is composed off a trunk that now holds the human's words, so it is about them if the seat judges so |

The forming plane renders the sentence as it is sampled; own speech commits through the document
once a sentence closes; the human's caret and the resident's insertion point are two positions in
one op log, which Act I's single serialiser interleaves for free — the concurrency the note
attributes to Etherpad is, in nib, the fact that every character is a changeset and the editor
thread is the only writer. (Act II needs `follow` for two *machines*; two hands on one machine
need nothing.)

*What is missing, and it is two things.* First, **a want that renews itself.** Today a want is
born at a boundary and dies when it is composed; in the saver, the end of the resident's own
sentence is a boundary at which the same seat wants again, unless the world has just spoken. That
is one rule in `speak_wants`, and it is the whole of "keeps talking". Second, **the per-block
floor** (`docs/BACKLOG.md`, Stage 2's open item): the saver composes *while* the hand moves, so the
global refusal has to become the one the spec always meant — never into a block the human is
touching, freely into its own. The commit-time gate already tests the span; the compose-time gate
is what changes.

*What it must not do, and the laws that say so.*

- **The seed is byte-frozen, so the standing instruction cannot be a prompt.** "Just start typing
  about anything" is a *mandate*, and the mandates are inside the serve hash: change one and the
  resident refuses to start (rule 10). The instruction has to arrive as **world**, on the human's
  lane — `[bo] Watcher, talk to me about anything until I interrupt.` — which is a direct address,
  the SPEAKER's own mandate, and off-distribution for nothing. What the saver then talks about is
  what is in the pad; an empty pad talks about the address line, and a page of the operator's notes
  talks about the notes, which is the more interesting screen saver anyway. A seat with a different
  disposition (a NARRATOR) is a retune, not a runtime.
- **Rule 5 holds absolutely.** The resident types at the sampler's cadence and no other. Its
  "pause to see what you are doing" is a real re-probe that came back negative; its "ignore" is a
  real one that came back positive. A saver that inserted a hesitation because a human was typing
  would be a demo that stopped being evidence, and this mode is the one most tempting to fake.
- **Rule 6 holds in the file.** The resident's paragraphs begin `[SEAT] `; the human's are bare;
  nobody reading the pad afterwards wonders who was talking to whom.
- **It is a switch, not a tick.** A screen saver starts when the human has been away, and nib
  already knows that — `[tick +300s]` is world. But ticks never trigger a probe round (anti-turn
  exemption 4), and a mode that woke on a timer would be the poll reborn. The saver is a state,
  entered by a key and recorded on the tape like the AI switch and wrap (rule 8), and the status
  line says SAVER while it is on. An idle-time entry is a *policy* over that switch, decided later
  and recorded the same way.
- **The manners are the part that will fail first, and that is the point.** The seats perseverate:
  one long session composed fourteen sentences of which eight were the same two lines. A monologue
  is the harshest possible test of that disposition, and the harness will refuse repeat after
  repeat until the seat has nothing new to say. So the saver is also an instrument: **the monologue
  horizon** — sentences said before the manners refuse everything — is a number the next tune can
  be scored on, and the saver's tape is a disposition soak accruing from an Easter egg.

*Where it is unusual.* Nobody ships a text editor whose idle state is a mind thinking aloud on the
page in its own paragraph, while you type in yours, with the interruption negotiated by judgment
rather than by a send key. The closest things are voice assistants that keep talking until barged
in (Moshi's full-duplex stance), and those have no page. On a page the human can *edit* the
monologue — delete a sentence of it, and that is a percept on the human's lane, which the resident
perceives as its own words being thrown out. That feedback loop is the label valve of the review's
§5.4, delivered by the screen saver at a rate a working session never reaches.

**The null, and the order.** In the CLI first: `--resident --emit --saver` — after every emission
the same seat wants again; measure the monologue horizon on this box, on an empty file and on the
README. It needs the self-renewing want and nothing else, an evening against what exists. Then the
window: the switch, the per-block floor, the resident's block at the end of the document and the
human anywhere else. None of it before Stage 4, whose third switch position this is.

---

## The order, across all four

Fix say-it-once first, because a machine repeating itself is tolerable in a refused tape row and
intolerable out loud — and the screen saver's monologue horizon is the instrument that measures it.
Then focus-as-a-lane as the null for attention. Then two typed lanes for the crisscross. Then the
screen saver in the window, as the third position of Stage 4's switch. Then gear 2, composition
only, one target, deadlined by the floor. Then speech. Then vision, cheap detectors only. None of it
before Stage 4 and the week of real work; all of it after the week has said whether one person
alone leaves the thing switched on.
