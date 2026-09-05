# Prior art and state of the art for **nib**'s floor-control problem

*Survey compiled 2026-09-04 for `C:/nib` (BLUEPRINT §5, SPEC §5–6). Primary sources preferred;
anything I could not verify against a primary source is marked **[UNVERIFIED]** or
**[SECONDARY]**. The system under survey: a Windows plain-text editor in which a locally resident
9B model perceives keystrokes (deletions included) as percepts, judges `{hold, emit}` on the free
tail of the ingest forward pass, writes into its own blocks in the same document, and can withdraw
a forming sentence mid-word when the next human keystroke contradicts it, logging the retraction on
an append-only hash-chained tape. No system prompt; no sockets linked in Act I.*

---

## 0 · Executive orientation

Four literatures have each solved a *part* of nib's floor problem and none has solved the whole:

| literature | solved | left open for nib |
|---|---|---|
| Conversation analysis (1970–2015) | the *descriptive* theory of who speaks when, and the ~200 ms timing budget that proves projection | nothing operational for a text surface; no theory of a *written* TRP |
| Full-duplex spoken dialogue (2024–2026) | the *architecture* of an always-listening, always-decodable agent, and benchmark metrics including **stop latency** | speech only; the "document" is ephemeral air, so there is no persistent artefact to place words *into* |
| CSCW / real-time editing (1989–2026) | *placement*: territoriality, authorship colour, awareness, concurrency control | the other party was always a human with a turn-taking instinct; the AI agent, where studied, is still request/response |
| AI co-writing UX (2019–2026) | *acceptance economics* of an inline suggestion, and the decision-theoretic threshold for when not to suggest | the suggestion is always keystroke-anchored ghost text the human must accept; nothing takes the floor, and nothing retracts |

nib's specific act — **evidence-triggered emission into a shared persistent buffer with a visible,
tape-logged retraction** — sits in the gap between rows 2 and 3. That is the honest novelty claim,
and §7 and §11(d) below say precisely how much of it is unprecedented.

---

## 1 · Conversation analysis and turn-taking

### 1.1 Sacks, Schegloff & Jefferson 1974 — the constitutive paper

**Citation.** Harvey Sacks, Emanuel A. Schegloff, Gail Jefferson, "A Simplest Systematics for the
Organization of Turn-Taking for Conversation," *Language* 50(4): 696–735, 1974.
<https://www.cambridge.org/core/journals/language/article/abs/simplest-systematics-for-the-organization-of-turntaking-for-conversation/111896B038FBA9CF7C131206B5A4C079>
(reprinted in Schenkein ed., *Studies in the Organization of Conversational Interaction*, 1978;
open copy: <https://liso-archives.liso.ucsb.edu/Jefferson/systematics_Schenkein.pdf>)

**Mechanism.** The system has two components and a rule set. The **turn-constructional component**
says turns are built out of unit-types — lexical, phrasal, clausal, sentential — whose *ends are
projectable*: a hearer can tell, before the unit finishes, roughly where it will finish. The end of
a unit-type is a **transition-relevance place (TRP)**, and speaker change becomes *relevant* (not
mandatory) there. The **turn-allocation component** is the ordered rule set applied at each TRP:
(1a) if the current speaker has selected a next speaker, that party must speak next; (1b) otherwise
any party may self-select, and **first starter gets the turn**; (1c) otherwise the current speaker
may (but need not) continue. Rule (2) re-applies the whole set at the next TRP. The system is
locally managed, party-administered, interactionally controlled, and allocation-for-a-turn rather
than allocation-of-turns. Its design consequences are the ones nib inherits whether it wants them
or not: overwhelmingly **one party talks at a time**, overlaps are **brief and repaired**, and gaps
and lapses are systematically minimised. [Rule wording paraphrased from the standard formulation;
I could not fetch the primary PDF within the size limit — **[SECONDARY]** for exact wording,
**[VERIFIED]** for venue, pagination and the TRP/TCU apparatus.]

**Mapping to nib.** nib deletes rule 1a (there is no address, no "you" selection) and deletes rule
1b's *scarcity* (a pad has no single channel: two authors can literally both write). What survives
is 1c — the *incumbent's* right to continue — and the projectability that makes TRPs computable.
nib's "closed thought" (SPEC 5.1.2.1: `.`/`!`/`?` at word end, or newline; `;` and `:` explicitly
*excluded*) is a TCU-boundary detector in all but name, and the exclusion of `;`/`:` is exactly the
kind of empirical unit-type refinement CA spent a decade doing for speech.

### 1.2 Duncan 1972 — turn-yielding cues, the operational half

**Citation.** Starkey Duncan Jr., "Some Signals and Rules for Taking Speaking Turns in
Conversations," *Journal of Personality and Social Psychology* 23(2): 283–292, 1972.
<https://www.semanticscholar.org/paper/Some-Signals-and-Rules-for-Taking-Speaking-Turns-in-Duncan/2eda53312d39fabad8b6816a563c483ed85d37b1>

**Mechanism.** From videotaped dyads (a therapist–patient interview and a two-therapist
discussion), Duncan isolated three signal classes — **turn-yielding**, **attempt-suppressing**, and
**speaker-state** — and showed the auditor's turn-attempt probability rises *monotonically with the
number of yielding cues displayed simultaneously*, while a concurrent attempt-suppressing cue (a
continuing gesticulation) drives it to zero. The canonical six yielding cues are terminal
intonation, drawl on the final syllable, a sociocentric sequence ("or something," "you know"), a
pitch/loudness drop on such a sequence, syntactic completion of a clause, and termination of a
hand gesture. [Six-cue list **[SECONDARY]**; the additive-cue finding and the three signal classes
**[VERIFIED]** via abstract and course syllabi.]

**Mapping to nib.** This is the single most directly portable result in the CA literature, because
it is *additive and countable*. nib's text-surface analogue of the cue bundle: (i) terminal
punctuation at a word end, (ii) inter-keystroke interval exceeding a pause threshold, (iii) a
newline / paragraph break, (iv) the caret leaving the paragraph (a *positional* cue with no speech
equivalent), (v) selection collapse / focus loss. Duncan's finding says the emit gate should not
use any one of these as a trigger but should *count* them — and should treat continued typing
inside the floor window as an attempt-suppressing cue that zeroes the probability regardless of how
many yielding cues fired. nib's current rule already does the suppressing half (the 2 s serializer
refusal); it does not yet do the additive half.

### 1.3 Yngve 1970 and Schegloff 1982 — the back channel and continuers

**Citations.** Victor H. Yngve, "On Getting a Word in Edgewise," *Papers from the Sixth Regional
Meeting, Chicago Linguistic Society*, pp. 567–578, 1970.
<https://www.semanticscholar.org/paper/On-getting-a-word-in-edgewise-Yngve/17eb496543b004b9c4215ab51380aae2640d5695> ·
Emanuel A. Schegloff, "Discourse as an Interactional Achievement: Some Uses of 'uh huh' and Other
Things That Come Between Sentences," in D. Tannen (ed.), *Analyzing Discourse: Text and Talk*,
Georgetown University Press, 1982.
<https://www.conversationanalysis.org/schegloff-media-archive/discourse-as-interactional-achievement-1-uh-huh-1982/>

**Mechanism.** Yngve's contribution is the *channel split*: "both the person who has the turn and
his partner are simultaneously engaged in both speaking and listening… because of the existence of
what I call the back channel, over which the person who has the turn receives short messages such
as 'yes' and 'uh-huh' **without relinquishing the turn**." Schegloff sharpens this: a **continuer**
is not a mini-turn but a *pass* — its producer displays an understanding that an extended unit is
underway and not yet complete, and by producing it declines the opportunity for a full turn. The
continuer is therefore the only utterance class in conversation that is **speech which does not
take the floor**.

**Mapping to nib.** This is the missing emission class in the blueprint. nib currently has a binary
`{hold, emit}` where emit means *compose a block*. CA says there is a third state, and it is
cheap: a floor-preserving acknowledgement. On a text surface the back channel is not a word in the
buffer — a word in the buffer *is* taking the floor. It is the margin, the gutter, the status line,
the authorship colour intensity: a channel that carries "I am following, keep going" without
occupying a character position. nib already has one (Stage 1's visible margins). It should be named
as a **continuer channel** and given the CA property explicitly: *nothing on the continuer channel
may ever become a document character.*

### 1.4 Levinson & Torreira 2015; Stivers et al. 2009; Heldner & Edlund 2010 — the timing budget

**Citations.** Stephen C. Levinson & Francisco Torreira, "Timing in turn-taking and its
implications for processing models of language," *Frontiers in Psychology* 6:731, 2015.
<https://www.frontiersin.org/journals/psychology/articles/10.3389/fpsyg.2015.00731/full> ·
Tanya Stivers, N. J. Enfield, Penelope Brown, Christina Englert, Makoto Hayashi, Trine Heinemann,
Gertie Hoymann, Federico Rossano, Jan Peter de Ruiter, Kyung-Eun Yoon, Stephen C. Levinson,
"Universals and cultural variation in turn-taking in conversation," *PNAS* 106(26): 10587–10592,
2009. <https://www.pnas.org/doi/10.1073/pnas.0903616106> ·
Mattias Heldner & Jens Edlund, "Pauses, gaps and overlaps in conversations," *Journal of Phonetics*
38(4): 555–568, 2010. <https://www.sciencedirect.com/science/article/abs/pii/S0095447010000628>

**Mechanism.** Levinson & Torreira state the puzzle that defines the field: modal between-turn gaps
are on the order of **200 ms**, but the latency to plan and initiate an utterance is **600 ms and
up**. The only resolution is that responders begin planning *during* the incoming turn and hold a
prepared response for release at the projected TRP — comprehension, planning and articulation are
overlapped, not serialised. Stivers et al. show this is not an artefact of one culture: across ten
languages from traditional indigenous communities to major world languages the response-latency
distribution has the same shape, with cultural differences quantitative only (mode near 0–200 ms
everywhere). Heldner & Edlund quantify the corpus reality — gaps, overlaps and pauses across three
conversational corpora — establishing that transitions cluster tightly around zero with short,
systematic overlaps rather than long silences. [Heldner & Edlund's exact percentages
**[UNVERIFIED]** here; venue and framing **[VERIFIED]**.]

**Mapping to nib, and the one place the analogy breaks.** This literature is the *strongest
theoretical support* for nib's core architectural claim — judgment must ride the ingest pass, not
poll after it, because the human transition budget is shorter than the generation budget (SPEC
6.2.3 is Levinson & Torreira restated in CUDA). It is also the place where the analogy is
**dangerous**: the 200 ms budget is an artefact of *speech being serial and evanescent*. Text is
neither. A pad has no cost to a 3-second gap, no cost to simultaneous production, and no
overlap-repair machinery to invoke. Any nib design that ports the 200 ms number rather than the
*reasoning behind it* is cargo-culting. What ports is the structure of the claim: **prepare early,
release at a boundary, and let the boundary be projected rather than detected.**

**Typing pauses as the only prosody.** The prosody substitute is empirical and already measured, in
a literature the blueprint does not cite: keystroke-logging writing research. The **2000 ms pause
threshold** used to delimit "bursts" of writing production originates with Wengelin (2006) and is
standard in Inputlog-based analysis; pause length varies systematically with the syntactic level of
the pause location — longer before sentences than before words, longer before words than within
words. <https://www.diva-portal.org/smash/get/diva2:834468/FULLTEXT01.pdf> ·
<https://repository.uantwerpen.be/docman/irua/e8d12b/b39f88f0.pdf> [**[SECONDARY]** for the
Wengelin 2006 attribution; the 2 s convention and the syntactic-level gradient are well attested.]
**nib's 2 s floor window and this 2 s burst threshold are numerically identical and were arrived at
independently. That is either the strongest external validation in this document or a coincidence
that will embarrass the spec — it is worth one paragraph of the devlog either way.** The gradient
result also gives nib a free upgrade: a 2 s pause *within* a word is a very different event from a
2 s pause after a full stop, and the floor rule currently cannot tell them apart.

---

## 2 · Full-duplex spoken dialogue systems, 2024–2026

This is the closest engineering analogue to un-saying, and the field has moved fast enough that
nib's Stage 3 has measurable competitors in a neighbouring modality.

### 2.1 Moshi (Kyutai, 2024) — the reference architecture

**Citation.** Alexandre Défossez, Laurent Mazaré, Manu Orsini, Amélie Royer, Patrick Pérez, Hervé
Jégou, Edouard Grave, Neil Zeghidour, "Moshi: a speech-text foundation model for real-time
dialogue," arXiv:2410.00037, September 2024. <https://arxiv.org/abs/2410.00037> ·
<https://github.com/kyutai-labs/moshi>

**Mechanism.** Three parts. **Helium**, a 7B text LM trained on 2.1T tokens, is the backbone.
**Mimi** is a streaming neural audio codec: 24 kHz in, a 12.5 Hz token representation at 1.1 kbps,
80 ms frame latency, with a distillation loss binding the first codebook to a WavLM self-supervised
representation so one codec carries semantic and acoustic information. The **multi-stream**
architecture models the user's audio and Moshi's own audio as *parallel* token streams, "which
allows for the removal of explicit speaker turns, and the modeling of arbitrary conversational
dynamics." **Inner Monologue** is the decisive trick: Moshi predicts *time-aligned text tokens as a
prefix to* its audio tokens for its own speech — the text is generated first at each frame and the
audio is conditioned on it. Theoretical latency 160 ms, 200 ms in practice.

**Why it matters to nib.** Three transfers. (i) The **parallel-stream** framing is nib's `lane`
mechanism, and it justifies SPEC 5.1.2.2 (a lane change closes the pending clause) as an
architectural principle rather than a hack. (ii) **Inner Monologue** is the *precise* answer to what
nib's forming region should be: a cheap, coarse, semantically-committing token stream that runs
*ahead* of the expensive, visible one, so that a contradiction can be detected against the plan
before the plan reaches the air. nib's forming/committed seam is Moshi's text/audio delay,
transposed. (iii) Moshi's *ability* to be interrupted comes free from the parallel streams —
there is no interruption handler, because there is no turn to interrupt. That is the same claim
nib makes and it is now an implemented one in speech.

### 2.2 Veluri et al. 2024 — SyncLLM, the clock inside the model

**Citation.** Bandhav Veluri, Benjamin N. Peloquin, Bokai Yu, Hongyu Gong, Shyamnath Gollakota,
"Beyond Turn-Based Interfaces: Synchronous LLMs as Full-Duplex Dialogue Agents," *EMNLP 2024*,
pp. 21390–21402. <https://aclanthology.org/2024.emnlp-main.1192/> · arXiv:2409.15594

**Mechanism (abstract, verbatim in part).** "the challenge of achieving full-duplex dialogue with
LLMs lies in modeling synchrony as pre-trained LLMs do not have a sense of 'time'." The fix is to
integrate time information into Llama3-8B "so that they run synchronously with the real-world
clock": the timeline is chunked into fixed intervals and the model emits tokens for *every*
interval, including intervals in which it says nothing, so silence is a generated symbol rather than
an absence. Trained on 212k hours of synthetic spoken dialogue derived from text dialogue plus 2k
hours of real speech; tolerates up to 240 ms of network latency in the simulated deployment.

**Why it matters to nib.** SyncLLM is independent, peer-reviewed corroboration of SPEC 5.1.9:
**silence must enter the model as world, not as absence.** nib's `[tick +Ns]` idle token *is*
SyncLLM's silence interval, arrived at from fusord rather than from EMNLP. Cite it; it converts a
design choice into a convergent finding.

### 2.3 LSLM, Freeze-Omni, VITA, Mini-Omni — the interruption-handling family

- Ziyang Ma et al., "Language Model Can Listen While Speaking," arXiv:2408.02622, 2024; AAAI 2025.
  <https://arxiv.org/abs/2408.02622>. **LSLM** carries a token-based decoder-only TTS speaking
  channel and a streaming SSL encoder listening channel and fuses them for autoregressive
  generation; it compares early / middle / late fusion and finds **middle fusion** best. The paper's
  explicit target is *interruption*: detecting turn-taking in real time under command-based and
  voice-based full-duplex settings.
- **Freeze-Omni**, arXiv:2411.00774, 2024. <https://arxiv.org/abs/2411.00774>. Speech input and
  output modalities attached to a **frozen** text LLM via three-stage training, with chunk-wise
  streaming input and a classification head that predicts the dialogue state (speak / listen /
  interrupt) at chunk granularity. This is the cheapest architecture in the family and the one
  closest to nib's constraint set, because the text model is never fine-tuned.
- **VITA**, arXiv:2408.05211, 2024 <https://arxiv.org/abs/2408.05211>; **Mini-Omni**,
  arXiv:2408.16725 <https://arxiv.org/html/2408.16725v1>, and Mini-Omni2, arXiv:2410.11190. These
  established the open-source baseline for "hear and talk while thinking in streaming" and for
  audio-interrupt / non-awakening interaction.

**Why it matters to nib.** Freeze-Omni's **chunk-level state classification head** is the single
most stealable serving pattern here: nib's emit gate is a state classifier over the free tail of the
ingest pass, and Freeze-Omni proves such a head can be trained and served without touching the
backbone — which is what SPEC 6.2.2's byte-frozen seed requires.

### 2.4 The benchmark layer, 2025–2026 — where "stop latency" got a definition

- Guan-Ting Lin et al., "Full-Duplex-Bench: A Benchmark to Evaluate Full-duplex Spoken Dialogue
  Models on Turn-taking Capabilities," arXiv:2503.04721, 2025.
  <https://arxiv.org/abs/2503.04721> · <https://full-duplex-bench.github.io/>. Four dimensions:
  **pause handling** (Takeover Rate; Jensen–Shannon divergence of the model's timing distribution
  against human timing), **backchanneling**, **turn-taking** (Response Latency = time from end of
  user speech to start of model response), **interruption management** (Takeover Rate, an LLM-judge
  quality score, and Latency After Interruption).
- Guan-Ting Lin, Shih-Yun Shan Kuan, Qirui Wang, Jiachen Lian, Tingle Li, Shinji Watanabe, Hung-yi
  Lee, "Full-Duplex-Bench v1.5: Evaluating Overlap Handling for Full-Duplex Speech Models,"
  arXiv:2507.23159, 2025. <https://arxiv.org/abs/2507.23159>. Four overlap scenarios — **user
  interruption, user backchannel, talking to others, background speech** — with "a comprehensive
  suite of metrics analyzing categorical dialogue behaviors, **stop and response latency**, and
  prosodic adaptation." Its headline finding is a taxonomy of two strategies models actually adopt:
  a **responsive** strategy that prioritises rapid response to user input, and a **floor-holding**
  strategy that "preserves conversational flow by filtering overlapping events."
- Peng et al., "FD-Bench," arXiv:2507.19040, Interspeech 2025 <https://arxiv.org/abs/2507.19040>;
  "FLEXI: Benchmarking Full-duplex Human-LLM Speech Interaction," arXiv:2509.22243, 2025
  <https://arxiv.org/abs/2509.22243>, which reports open/commercial gaps in "emergency awareness,
  turn terminating, and interaction latency."
- **ICASSP 2026 HumDial Challenge**: arXiv:2601.05564 (challenge) and Wang et al., "Full-Duplex
  Interaction in Spoken Dialogue Systems: A Comprehensive Study from the ICASSP 2026 HumDial
  Challenge," arXiv:2604.21406, April 2026. <https://arxiv.org/html/2604.21406v1>. Two tracks,
  eight scenarios. **Interruption** scenarios: follow-up question, **negation/dissatisfaction**,
  repetition request, topic switch, silence/termination request. **Rejection** scenarios: user
  backchannel, pause handling, third-party speech, speech directed at others. Metrics: **First
  Response Latency** ("the interval from the end of the first user question to the start of the
  first model response"), **Stop Latency**, **Response Latency**, and a behavioural classification
  of each episode as **Respond / Resume / Uncertain / Unknown** derived from ASR transcripts plus an
  LLM judge. Overall score = 40 % interruption + 40 % rejection + 20 % latency, with logarithmic
  latency normalisation. The related literature also uses **Proper Interruption Rate (PIR)**,
  **Proper Response Rate (PRR)** and **First Token Emission Delay (FTED)**. [PIR/PRR/FTED
  **[SECONDARY]** — surfaced in the search corpus around the HumDial line of work, not confirmed
  against a single primary definition.]
- Modi, Mahajan, Wetter, Welles, "EchoChain: A Full-Duplex Benchmark for State-Update Reasoning
  Under Interruptions," arXiv:2604.16456, 2026. <https://arxiv.org/pdf/2604.16456>. Tests whether a
  system **abandons outdated commitments** when the user contradicts or revises state mid-stream,
  rather than completing the utterance it had planned.
- Curated tracking list: <https://github.com/Ruiqi-Yan/Awesome-Full-Duplex-SDM>.

**The answer to the question the task asked.** *Is there a published metric for "self-interruption
when the user contradicts"?* **Yes, as of 2025–2026, and it is called stop latency.** Full-Duplex-
Bench v1.5 and the HumDial 2026 study both define it, and HumDial's *negation/dissatisfaction*
scenario is a direct speech analogue of nib's un-say trigger. EchoChain benchmarks the semantic
half — whether the abandoned plan is actually replaced by a correct one. **nib should adopt "stop
latency" as the name of its un-say metric rather than inventing one**, and should report the
Respond/Resume/Uncertain four-way behavioural classification, because that is the vocabulary a
reviewer from this field will already have. nib measures 13 µs kill latency against a 65.7 s
turn-based miss (BLUEPRINT §6); expressed as stop latency those are directly comparable to
published speech numbers, which are in the hundreds of milliseconds.

---

## 3 · Real-time collaborative editing and awareness (CSCW)

### 3.1 Dourish & Bellotti 1992 — awareness through the workspace, not around it

**Citation.** Paul Dourish & Victoria Bellotti, "Awareness and Coordination in Shared Workspaces,"
*CSCW '92*, pp. 107–114, Rank Xerox EuroPARC.
<https://www.dourish.com/publications/1992/cscw92-awareness.pdf>

**Mechanism.** A study of shared-editor use contrasting two ways of supplying awareness: **explicit
role/division mechanisms** (each user declares what they are doing, the system enforces separation)
versus **shared feedback**, in which awareness information is provided *passively through the
workspace itself* — everyone simply sees the artefact change. The finding is that shared feedback
lets users "move smoothly between close and loose collaboration, and to assign and coordinate work
dynamically," whereas explicit mechanisms impose a structure the group must then work around.

**Mapping to nib.** This is the CSCW argument *for* nib's central aesthetic and *against* half of
its floor rule. For: the resident's characters appearing in the buffer, colour-coded, is textbook
shared feedback — no separate panel, no notification, the artefact *is* the awareness channel.
Against: the 2 s serializer refusal is an *explicit* coordination mechanism, the exact class this
paper found inferior. A reviewer will note that Dourish & Bellotti's participants coordinated
successfully **without** a lock because they could see each other; nib gives the resident far better
vision than any human collaborator has, and then locks anyway.

### 3.2 Concurrency control and the floor-control tradition

- Clarence A. Ellis & Simon J. Gibbs, "Concurrency Control in Groupware Systems," *ACM SIGMOD 1989*,
  pp. 399–407. <https://dl.acm.org/doi/10.1145/67544.66963>. Introduced **operational
  transformation** in the GROVE outline editor: rather than locking, each site applies operations
  immediately and transforms incoming remote operations against concurrent local ones, preserving
  causality and convergence. This is the ancestor of Etherpad's Easysync — the changeset library
  nib already implements (SPEC §3).
- Saul Greenberg & David Marwood, "Real Time Groupware as a Distributed System: Concurrency Control
  and Its Effect on the Interface," *CSCW '94*.
  <https://grouplab.cpsc.ucalgary.ca/grouplab/uploads/Publications/Publications/1994-Concurrency.CSCW.pdf>.
  The key argument: concurrency control in groupware cannot be evaluated as a correctness problem
  alone, "because system interactions include people as well as computers" — locking, serialisation
  and their degrees of optimism each produce a *visibly different interface*, and the interface cost
  usually dominates the correctness benefit.
- Hans-Peter Dommel & J. J. Garcia-Luna-Aceves, "Floor control for multimedia conferencing and
  collaboration," *Multimedia Systems* 5: 23–38, 1997.
  <https://link.springer.com/article/10.1007/s005300050040>. The canonical formalisation: floors are
  "temporary permissions granted dynamically to collaborating users in order to mitigate race
  conditions and guarantee mutually exclusive resource usage," with a taxonomy of floor-granting
  policies (chair-controlled, first-come, round-robin, free-for-all) and architectures (centralised
  vs distributed floor holder).

**What the tradition learned.** Explicit floor passing works where the resource is genuinely
exclusive (an audio channel, a shared telepointer, a physical camera) and fails where it is not.
Text is not exclusive: two people can type into different paragraphs simultaneously with no race at
all. The field's verdict, from Ellis & Gibbs onward, was that **document editing should not use a
floor**; it should use transformation plus visibility. nib's floor window is therefore a *revival*
of a mechanism the CSCW literature deliberately abandoned for text — which is defensible only on
grounds that have nothing to do with correctness and everything to do with attention (see §5).

### 3.3 Awareness, territoriality, and what co-writers actually do

- Carl Gutwin & Saul Greenberg, "A Descriptive Framework of Workspace Awareness for Real-Time
  Groupware," *CSCW: The Journal of Collaborative Computing* 11(3–4), 2002.
  <https://dl.acm.org/doi/10.1023/A:1021271517844>. Decomposes workspace awareness into
  *who / what / where* in present tense plus *when / how / where* in past tense, and catalogues the
  perceptual mechanisms (consequential communication, feedthrough, intentional communication) by
  which each element is normally maintained. [Volume **[VERIFIED]**; page range **[UNVERIFIED]**.]
- Ida Larsen-Ledet & Henrik Korsgaard, "Territorial Functioning in Collaborative Writing," *CSCW*
  28: 391–433, 2019. <https://link.springer.com/article/10.1007/s10606-019-09359-8>. 23 interviews,
  32 researchers/students, plus revision-history visualisations. Finds that collaborative documents
  are **territorially organised**: writers claim paragraphs and sections, mark them, defend them,
  and treat unmarked incursions as violations, even when the incursion improves the text.
- Dakuo Wang, Haodan Tan, Tun Lu, "Why Users Do Not Want to Write Together When They Are Writing
  Together: Users' Rationales for Today's Collaborative Writing Practices," *PACM HCI* 1(CSCW),
  2017. <https://dl.acm.org/doi/abs/10.1145/3134742>. Writers routinely ask collaborators to *close*
  the document, or copy the text out to a private file, edit there, and paste it back — explicitly
  to escape being observed mid-formation. Several asked for a tool that lets them work in private
  with no collaborator reading work in progress.
- Jeremy Birnholtz & Steven Ibara, "Tracking changes in collaborative writing: edits, visibility and
  group maintenance," *CSCW 2012* <https://dl.acm.org/doi/10.1145/2145204.2145325>; Birnholtz,
  Steinhardt & Pavese, "Write here, write now!: an experimental study of group maintenance in
  collaborative writing," *CHI 2013*. Visibility of edits is not costless: making changes visible
  improves knowledge of who did what but can **incite social conflict**, so writers manage the
  visibility of their own edits as a face-work strategy.
- **Etherpad** (David Greenspan, Aaron Iba, J. D. Zamfirescu, AppJet, launched 19 Nov 2008;
  open-sourced 2009). <https://etherpad.org/> · <https://github.com/ether/etherpad>. Per-author
  colour applied to every character, plus a coloured caret per participant — authorship as an
  always-on, non-modal property of the text rather than a mode you enter. nib's SPEC §7 is this,
  made permanent and tape-backed.

**Mapping to nib.** Larsen-Ledet & Korsgaard is the *empirical justification* for
BLUEPRINT §5's "its own blocks, never inside a human's paragraph" — the paragraph really is the
unit of territory, measured. Wang et al. is the *warning*: the population studied would experience
a resident watching every keystroke as precisely the surveillance they were escaping. Birnholtz
adds the twist that nib's permanent per-character authorship, which the spec treats as an honesty
guarantee, is in the human case a documented source of conflict.

### 3.4 The 2026 state of the art on AI agents inside shared documents

**Citation.** Florian Lehmann, Krystsina Shauchenka, Daniel Buschek, "Collaborative Document Editing
with Multiple Users and AI Agents," *CHI '26*, Barcelona, April 2026; arXiv:2509.11826.
<https://arxiv.org/abs/2509.11826>

**Mechanism (abstract, verbatim excerpt).** "We propose integrating AI agents directly into
collaborative writing environments. Our prototype makes AI use visible to all users through two new
shared objects: user-defined agent profiles and tasks. Agent responses appear in the familiar
comment feature. In a user study (N=30), 14 teams worked on writing projects during one week.
Interaction logs and interviews show that teams incorporated agents into existing norms of
authorship, control, and coordination, rather than treating them as team members. Agent profiles
were viewed as personal territory, while created agents and outputs became shared resources."

**Why this is the most important single citation in the survey.** It is the nearest neighbour in
the CSCW frame, it is a week-long deployment (the same duration as nib's Stage 5), and its central
finding is a *negative result for nib's premise*: given an agent inside the shared document, teams
did **not** treat it as a participant; they folded it into existing norms and, decisively, the
agent's output landed **in comments, not in the body**. nib claims the body. The comparison is
unavoidable and nib must address it directly.

**Also:** Ink & Switch's **Patchwork** (Geoffrey Litt, Paul Sonnentag, Max Schöning, Adam Wiggins,
Peter van Hardenberg, Orion Henry, 2024–2026): AI bots participate as named collaborators, but
"put changes on a branch, which you can choose to partially or completely merge," shown as a diff,
with "the history timeline also show[ing] which edits came from the bot."
<https://www.inkandswitch.com/patchwork/notebook/2024-version-control/07/> ·
<https://patchwork.inkandswitch.com/>. **Patchwork is the strongest existing implementation of
nib's authorship-and-history claims and it solves floor control by refusing the floor entirely: the
bot writes on a branch.** That is the design nib is betting against.

---

## 4 · AI co-writing and inline suggestion UX

### 4.1 The trigger question — what actually causes a suggestion to appear

| system | trigger | floor discipline |
|---|---|---|
| Gmail **Smart Compose** (Chen et al., KDD '19) | every keystroke, gated by a **confidence threshold** tuned per model to hold coverage constant; strict sub-100 ms latency budget (reported 90th-percentile ~60 ms) <https://dl.acm.org/doi/10.1145/3292500.3330723> · arXiv:1906.00080 | ghost text after the caret; zero floor claim; Tab accepts |
| GitHub **Copilot** (Ziegler et al., MAPS '22) | keystroke + pause; acceptance rate 23.3 % TypeScript, 27.9 % JavaScript, 28.8 % Python; **acceptance rate, not persistence, is what correlates with felt productivity** <https://arxiv.org/abs/2205.06537> | ghost text; zero floor claim |
| **Wordcraft** (Coenen, Davis, Ippolito, Reif, Yuan, arXiv:2107.07430; Yuan, Coenen, Reif, Ippolito, *IUI '22*, <https://dl.acm.org/doi/10.1145/3490099.3511105>) | **explicit** — select text and press "replace selection", or press "generate text" | human-initiated, always |
| **CoAuthor** (Lee, Liang, Yang, *CHI '22*, <https://dl.acm.org/doi/10.1145/3491102.3502030>) | explicit key press to request 5 GPT-3 suggestions; 63 writers × 4 model instances × 1445 sessions, logged at **keystroke level** | human-initiated; the dataset is the closest public analogue to nib's tape |
| **Cursor Tab** (2025) | fires on **every** keystroke and cursor move, but the policy includes a **no-suggestion action** <https://cursor.com/blog/tab-rl> | ghost text / diff popup |

**Cursor's Tab-RL post is the most operationally useful document in this section**, because it
publishes the decision rule nib's emit gate needs. The policy's action space is
{show suggestion, show nothing}. With a 25 % acceptance target the reward is +0.75 for an accepted
suggestion, −0.25 for a rejected one, **0 for showing nothing**, so "the expected reward if the
suggestion is shown is 0.75p − 0.25(1 − p), which is positive exactly when p > 0.25." The threshold
is not hand-set; it *emerges* from policy-gradient optimisation
(∇θJ(θ) = E[∇θ log π(a|s,θ)·R(s,a)]) over on-policy user accept/reject data, with checkpoints rolled
out and data collected on a 1.5–2 hour cycle across ~400 M requests/day. Result: **21 % fewer
suggestions with a 28 % higher accept rate.** Their own summary of the lesson —
"Achieving a high accept rate isn't just about making the model smarter, but also knowing when to
suggest and when not to."

**Mapping to nib.** This is Horvitz's expected-utility principle (§5.1) reduced to two constants and
made trainable, and it is exactly the shape of nib's `{hold, emit}` gate. nib has one enormous
advantage here and one enormous disadvantage. Advantage: **nib's tape already records the ground
truth Cursor had to instrument for** — every emit, every hold, every margin, and (via un-say) every
retraction. Disadvantage: nib has one user, so on-policy RL at 400 M requests/day is unavailable;
the threshold must be calibrated, not learned, and BLUEPRINT §3's paired RESIDENT/TURN-BASED
sampling is the only estimator available.

### 4.2 What co-writing does to the human — the three findings nib must cite

1. **Draxler, Werner, Lehmann, Hoppe, Schmidt, Buschek, Welsch, "The AI Ghostwriter Effect: When
   Users Do Not Perceive Ownership of AI-Generated Text But Self-Declare as Authors,"** *ACM TOCHI*
   31(2), 2024. <https://dl.acm.org/doi/pdf/10.1145/3637875> · arXiv:2303.03283. Two studies
   (n₁=30, n₂=96): users do **not** feel ownership of AI-generated text yet **do not declare AI
   authorship publicly**; personalisation does not close the gap; greater user influence over the
   text raises felt ownership. Ownership–authorship discrepancy was *larger* for a supposed human
   ghostwriter than for an AI one.
2. **Jakesch, Bhat, Buschek, Zalmanson, Naaman, "Co-Writing with Opinionated Language Models Affects
   Users' Views,"** *CHI '23*. <https://dl.acm.org/doi/10.1145/3544548.3581196> · arXiv:2302.00560.
   N=1506. A writing assistant configured to argue one side shifted both what participants wrote
   **and their surveyed attitudes afterwards**. The authors name the mechanism **latent persuasion**:
   influence that is hard to detect because the model's opinion preferences are opaque to users,
   policymakers, and even developers.
3. **Lou, Crowley, Dodson, Yoon, "AnchoredAI: Contextual Anchoring of AI Comments Improves Writer
   Agency and Ownership,"** arXiv:2509.16128, September 2025. <https://arxiv.org/abs/2509.16128>.
   Anchors AI feedback to specific passages (an anchoring context window plus update-aware retrieval
   that survives edits) instead of a side chat; anchored feedback produced more targeted revisions
   and stronger felt agency.

**Mapping to nib.** Jakesch is the sharpest threat to nib's "no system prompt" claim: a resident
with an accreted, owner-set mandate and no visible instruction is *maximally* opaque, which is
exactly the latent-persuasion risk condition, and nib's honesty laws (per-character authorship,
mode on the status line, tape) address *provenance* but not *influence*. Draxler cuts the other
way and supports nib: felt ownership rises with user influence, and nib's un-say means the user's
keystroke is the highest-influence event in the system. AnchoredAI supports the block rule — spatial
anchoring near the relevant text beats a detached channel.

### 4.3 Proactive and always-on assistants, 2025–2026

- **"Assistance or Disruption? Exploring and Evaluating the Design and Trade-offs of Proactive AI
  Programming Support," *CHI '25*.** <https://dl.acm.org/doi/10.1145/3706598.3713357>. *Codellaborator*
  initiates assistance from editor activity and task context; within-subjects N=18. Proactive
  agents increased efficiency over prompt-only, **but incurred workflow disruptions**; presence
  indicators and interaction-context support mitigated the disruption and improved awareness of what
  the AI was doing.
- **"Need Help? Designing Proactive AI Assistants for Programming," *CHI '25*.**
  <https://dl.acm.org/doi/10.1145/3706598.3714002>.
- **Chao Zhang, Abe Davis, Chih-Wei Chen, Chin-Chia Hsu, "Designing Proactive Thought Partners for
  Writing," arXiv:2609.01588, 1 September 2026.** <https://arxiv.org/abs/2609.01588>. Verbatim from
  the abstract: "This paper studies the design space of proactive thought partners: AI agents that
  proactively offer customizable, higher-level cognitive support during writing… deployed it with 16
  participants for one week… participants configured proactive support through prospective planning,
  used suggestions for both idea generation and self-monitoring, and valued **lightweight visual
  representations alongside non-directive rhetorical framing for non-intrusive interventions**."
  [An earlier PDF extraction suggested this paper covers suggestion *retraction*; the abstract does
  not mention it — **[UNVERIFIED]**, treat as absent.]
- **TimelyAI: When Should Generative AI Assistants Intervene?** CHIWORK 2026 workshop.
  <https://dl.acm.org/doi/10.1145/3805029.3818270> — the field has now named nib's question as an
  open workshop topic.
- Products in the always-on, local, inline niche: **Cotypist** and **Typeahead** (macOS, local
  models, sub-100 ms inline suggestion in any text field, offline, no cloud).
  <https://www.producthunt.com/products/cotypist> · <https://www.producthunt.com/products/typeahead>
  [product marketing claims; **[UNVERIFIED]** as measurements.]

**The pattern across the whole of §4.** Every deployed system's trigger is either (i) a keystroke,
(ii) a timer/pause, or (iii) an explicit key. **None is evidence-triggered in nib's sense** — a
judgment computed from the *content* of the incoming stream on the same forward pass that ingests
it. The closest is Cursor's learned no-suggestion action, which is content-conditional but still
fires on the keystroke clock. This is nib's clearest defensible mechanism claim.

---

## 5 · Mixed-initiative interaction and interruption cost

### 5.1 Horvitz 1999 — the twelve principles

**Citation.** Eric Horvitz, "Principles of Mixed-Initiative User Interfaces," *CHI '99*, pp. 159–166.
<https://erichorvitz.com/chi99horvitz.pdf> · <http://erichorvitz.com/uiact.htm>

**Mechanism.** Automated service is justified only when its **expected utility exceeds** that of
leaving the action to the user, computed under explicit uncertainty about the user's goal. The
principles that bear on nib: *developing significant value-added automation* (don't automate what
direct manipulation already does well); *considering uncertainty about a user's goals*;
*considering the status of a user's attention in the timing of services*; *inferring ideal action
in light of costs, benefits, and uncertainties*; *employing dialog to resolve key uncertainties*;
*minimising the cost of poor guesses about action and timing*; *scoping precision of service to
match uncertainty*; and *providing mechanisms for efficient agent–user collaboration to refine
results*. LookOut instantiates them as a scheduling agent that computes p(user wants help) and
compares the expected utility of acting, asking, and doing nothing.

**Companion.** James F. Allen, Curry I. Guinn, Eric Horvitz, "Mixed-Initiative Interaction," *IEEE
Intelligent Systems* 14(5), 1999. <https://www.microsoft.com/en-us/research/wp-content/uploads/2016/11/mixedinit.pdf>
— Allen's taxonomy of mixed-initiative dialogue, Horvitz on uncertainty and Bayesian arbitration,
Guinn on evaluation. Frames initiative as a *negotiated* resource rather than a mode.

### 5.2 McFarlane 2002 — the four coordination methods, empirically compared

**Citation.** Daniel C. McFarlane, "Comparison of Four Primary Methods for Coordinating the
Interruption of People in Human-Computer Interaction," *Human–Computer Interaction* 17(1): 63–139,
2002. <https://www.tandfonline.com/doi/abs/10.1207/S15327051HCI1701_2> ·
<https://www.interruptions.net/literature/McFarlane-HCI02_2.pdf>

**Mechanism.** From a theory-based taxonomy of human interruption, four and only four ways exist to
coordinate an interruption:

- **Immediate** — interrupt with no prior notice; the interruption must be handled at once
  regardless of the state of the primary task.
- **Negotiated** — announce that something is pending and let the interrupted person choose when to
  take it; the interruptee holds control over the moment.
- **Mediated** — a third party (a proxy, an assistant process) decides on the person's behalf when
  to pass the interruption through.
- **Scheduled** — interruptions are delivered only on a fixed cadence.

A 36-participant experiment in an abstracted multitasking context found **negotiated interruption is
the best overall solution**, except where small differences in the timeliness of handling the
interruption are critical, in which case immediate wins. [Definitions and the headline result
**[SECONDARY]** — the primary PDF would not extract; venue, pagination and the four-method taxonomy
**[VERIFIED]**.]

### 5.3 Iqbal & Bailey — defer to the breakpoint

**Citations.** Shamsi T. Iqbal & Brian P. Bailey, "Understanding and developing models for detecting
and differentiating breakpoints during interactive tasks," *CHI '07*, pp. 697–706; "Effects of
intelligent notification management on users and their tasks," *CHI '08*
<https://interruptions.net/literature/Iqbal-CHI08.pdf>; "Oasis: A framework for linking notification
delivery to the perceptual structure of goal-directed tasks," *ACM TOCHI* 17(4), 2010.
<https://dl.acm.org/doi/abs/10.1145/1879831.1879833>

**Mechanism.** Task execution has a perceptual structure with **coarse, medium and fine
breakpoints**, inferable from interaction traces without extra hardware. Oasis realises
*defer-to-breakpoint* policies: hold a notification until the next breakpoint of at least a given
coarseness. Delivering at breakpoints lowers interruption cost — resumption lag, frustration,
errors — relative to immediate delivery, and **coarser breakpoints cost less**.

### 5.4 Mapping McFarlane's four methods onto nib's floor rule

| McFarlane method | what nib does today (SPEC 6.3.2) | what nib could do |
|---|---|---|
| **Immediate** | rejected by construction: an emit into a block touched within 2 s is refused *before composition* | this is the only method nib has actually forbidden, and correctly |
| **Negotiated** | **absent** — the human is never told an emit is pending and never gets to accept or defer it | the strongest available upgrade, and the one the literature says wins: the resident signals *I have something* on the continuer channel (§1.3) and the human's next keystroke (or a key, or the caret leaving the paragraph) disposes. This is also FUSOR's own propose/dispose idiom, so it costs nib nothing conceptually |
| **Mediated** | **this is what nib actually implements.** The serializer is a third party that decides on the human's behalf, using a fixed 2 s rule the human never sees | keep as the safety floor; it should be the backstop, not the policy |
| **Scheduled** | absent, and should stay absent — a cadence is exactly the "polling" SPEC 6.2.3 forbids | — |

**The finding to act on:** nib has built the *third-best* of the four methods and named it the
research problem. McFarlane's result plus Horvitz's "employ dialog to resolve key uncertainties"
plus Dourish & Bellotti's shared-feedback preference all point the same way — **negotiated, with
mediated as the floor.** Iqbal & Bailey supply the missing ingredient: the *breakpoint hierarchy*.
nib currently has one breakpoint class (2 s of quiet). A coarse/medium/fine hierarchy over the
keystroke stream — within-word pause, between-word pause, end-of-sentence pause, paragraph break,
caret leaving the paragraph, focus loss — is directly computable from the op log and would let the
emit gate scale its intrusiveness to the coarseness of the breakpoint it caught.

---

## 6 · Speculative and abortable generation in LLM serving

### 6.1 The abort-cheaply toolbox

- **Speculative decoding.** Yaniv Leviathan, Matan Kalman, Yossi Matias, "Fast Inference from
  Transformers via Speculative Decoding," *ICML 2023*; arXiv:2211.17192.
  <https://arxiv.org/abs/2211.17192>. A cheap draft model proposes γ tokens; the target model scores
  them in one parallel forward pass; a modified rejection-sampling rule accepts a prefix and
  resamples at the first rejection, guaranteeing the output distribution is **identical** to
  standard decoding. 2–3× on T5-XXL. (Companion: Chen et al., "Accelerating Large Language Model
  Decoding with Speculative Sampling," arXiv:2302.01318, 2023 — **[UNVERIFIED]** in this survey,
  cited from prior knowledge.)
- **KV-cache forking and prefix reuse.** Lianmin Zheng et al., "SGLang: Efficient Execution of
  Structured Language Model Programs," arXiv:2312.07104 (NeurIPS 2024); **RadixAttention** blog:
  <https://www.lmsys.org/blog/2024-01-17-sglang/>. KV cache for prompts *and generations* is retained
  in a **radix tree** with LRU eviction, giving automatic prefix search / insert / evict, so a shared
  prefix is computed once and multiple continuations fork from it at zero copy cost. vLLM's
  PagedAttention (Kwon et al., SOSP 2023) provides the same reuse through paged blocks with
  copy-on-write — **[UNVERIFIED]** in this survey, cited from prior knowledge.
- **Unbounded streaming context.** Guangxuan Xiao, Yuandong Tian, Beidi Chen, Song Han, Mike Lewis,
  "Efficient Streaming Language Models with Attention Sinks," *ICLR 2024*; arXiv:2309.17453.
  <https://arxiv.org/abs/2309.17453>. Keeping the KV of the first few "attention sink" tokens
  alongside a sliding window restores window-attention quality, enabling stable decoding over
  4M+ tokens with up to 22.2× speedup over sliding-window recomputation, **without fine-tuning**.

**Mapping to nib.** These three together are exactly the substrate a resident needs and they are all
open. Radix/paged KV reuse is what makes nib's forming region *cheap to throw away*: fork the trunk
at the ingest boundary, decode the candidate sentence on the fork, and on abort drop the fork —
nothing to roll back, the committed prefix was never touched. Speculative decoding gives the
"cheap draft ahead of an expensive verifier" pattern that Moshi's Inner Monologue uses semantically.
StreamingLLM is the answer to nib's `n_ctx = 8192` compromise (SPEC 6.2.6) when the resident window
becomes Stage 2's problem: attention sinks let a resident live in a session much longer than its
trained window without a molt.

### 6.2 Revising an in-flight generation when new input arrives

- **LiveMind: Low-latency Large Language Models with Simultaneous Inference.** arXiv:2406.14319,
  June 2024. <https://arxiv.org/abs/2406.14319>. Performs inference on **incomplete** user input by
  segmenting incoming text into chunks and caching intermediate reasoning per segment, then
  integrating when the input completes — reallocating computation into the input phase. Reported
  84.0 % average response-latency reduction on MMLU and 71.6 % on MMLU-Pro at comparable accuracy;
  with a large model inferring and a small model emitting, 37 % latency reduction and +4.30 %
  accuracy on MMLU-Pro.
- **Beyond the Turn-Based Game: Enabling Real-Time Conversations with Duplex Models.** Xinrong Zhang,
  Yingfa Chen, Shengding Hu, Xu Han, Zihang Xu, Yuanwei Xu, Weilin Zhao, Maosong Sun, Zhiyuan Liu,
  *EMNLP 2024*, pp. 11543–11557; arXiv:2406.15718. <https://aclanthology.org/2024.emnlp-main.644/>.
  **A text-side duplex model.** Queries and responses are split into **time slices**; each slice is
  either real message content or a special **idle** token denoting silence; the model consumes and
  produces slices in an interleaved schedule so it can "listen while generating output and
  dynamically adjust." This is the closest published thing to nib's compiler + tick design, in text.
- **Can Speech LLMs Think while Listening?** Yi-Jen Shih, Desh Raj, Chunyang Wu, Wei Zhou, SK Bong,
  Yashesh Gaur, Jay Mahadeokar, Ozlem Kalinli, Mike Seltzer, arXiv:2510.07497, October 2025.
  <https://arxiv.org/abs/2510.07497>. Begins reasoning **before the user finishes speaking**, gated
  by an **entropy-based "question completeness" metric** that indicates when enough input has
  arrived to start. CoT fine-tuning gave 2.4× accuracy across reasoning tasks; the entropy gate gave
  +4 % on ARC-Easy at equal latency; DPO cut latency 70 % without accuracy loss.
- **Asynchronous Reasoning: Training-Free Interactive Thinking LLMs.** George Yakushev, Nataliia
  Babina, Masoud Vahid Dastgerdi, Vyacheslav Zhdanovskiy, Denis Kuznedelev, Alina Shutova, Max
  Ryabinin, arXiv:2512.10931 (Dec 2025, rev. May 2026). <https://arxiv.org/abs/2512.10931>. Uses
  properties of positional embeddings to let a sequentially-trained LLM "simultaneously think,
  listen, and write outputs" with **no training**; reduces time-to-first-non-thinking-token from
  minutes to ≤5 s and overall delay by up to 12×.
- **DuplexMamba**, arXiv:2502.11123, 2025. <https://arxiv.org/pdf/2502.11123>. On new input during
  generation, "the model's state is duplicated to create an auxiliary decoding branch that processes
  the new input," enabling suspension of the ongoing output. [State-duplication detail
  **[SECONDARY]**.]

**The gap this section exposes.** Every one of these systems can *start early* and several can
*stop*. **None of them un-says.** Stopping is discarding tokens that have not yet been emitted, or
truncating audio that has been played and is gone. nib's un-say is different in kind because the
emitted characters are **still on screen and still addressable** — withdrawal is a visible,
reversible edit to a persistent artefact, not a cut to a stream. That difference is the technical
heart of nib's novelty claim and it should be stated in exactly those terms.

---

## 7 · Anything that already *is* nib

I searched: "always-on local LLM text editor"; "AI writes alongside you in real time in the same
document"; "non-turn-based writing assistant"; "LLM co-editor that retracts its own suggestions";
"resident model editor"; "duplex text editing LLM"; plus product-space queries for 2025–2026. **No
system matches nib's conjunction.** The three closest, in order:

### Closest #1 — Lehmann, Shauchenka & Buschek, *CHI '26* (§3.4)
**How it differs.** Agents are invoked by user-defined **tasks** (explicit request), their output
lands in the **comment** channel rather than the document body, there is no continuous perception of
keystrokes, no notion of a thought boundary, no retraction, and the model is a cloud service. Its
week-long N=30 deployment is the methodological template nib's Stage 5 should copy — and its finding
(agents folded into existing norms, not treated as members; profiles as personal territory) is the
prior expectation nib's Stage 5 must beat.

### Closest #2 — Ink & Switch **Patchwork** (§3.4)
**How it differs.** The bot is a first-class named collaborator with edits attributed in a history
timeline — nib's authorship and tape claims, already shipped in prototype. But the bot **writes on a
branch** and the human merges: floor control solved by never taking the floor. There is no
in-flight formation visible to the human, therefore no un-saying, and the trigger is a prompt.
Patchwork is the design nib is explicitly betting against, and the bet should be stated as such.

### Closest #3 — **Cotypist / Typeahead** (macOS, 2025–2026) and **Cursor Tab** (§4.1)
**How they differ.** Local weights, no cloud, inline, sub-100 ms, always running — nib's
*deployment* profile, essentially achieved. But they are autocomplete: the suggestion is anchored
to the caret, never occupies a block of its own, never persists without a Tab press, has no
authorship identity, produces no tape, and cannot retract because it never committed. Cursor Tab is
the only one with a principled *don't-suggest* action, and even that fires on the keystroke clock.

### The five-part claim, checked against everything found

| nib claim | prior art status |
|---|---|
| **Evidence-triggered emission computed on the free tail of the ingest pass** | **No match in text.** Speech has the mechanism (Moshi, Freeze-Omni's chunk state head, SyncLLM); text co-writing has only keystroke/timer/explicit triggers, plus Cursor's learned no-suggestion action. **Novel in the text-editing setting; convergent with speech.** |
| **Deletions as percepts** | **No match as a designed input channel.** CoAuthor *logs* deletions at keystroke level for analysis; Inputlog/keystroke-logging research *analyses* them; no system feeds them to a model as first-class world. **Genuinely unusual.** |
| **Visible un-saying with the retraction on an append-only tape** | **No match anywhere.** Speech benchmarks measure *stop latency* (v1.5, HumDial 2026) but stopping ≠ withdrawing already-rendered persistent characters, and no speech system keeps a hash-chained record of what was retracted. **The strongest novelty claim in the project.** |
| **No system prompt** | Unusual but not novel as an *engineering* fact (a frozen seed prefix is a system prompt by another name); the novel part is the *policy* that purpose is owner-set and context is accreted. Expect reviewers to challenge the terminology, not the design. |
| **Local-only with a linker gate that forbids sockets** | Local-only is common (Cotypist, Typeahead, Ollama-class tooling). A **build-time dependency gate that fails the build on `ws2_32`/`wininet`/`winhttp`/`urlmon`/`dnsapi`** is, as far as this survey found, unique as a *shipped, checkable* privacy guarantee. **Novel as an artefact, not as an idea.** |

---

## 8 · (a) Mechanisms nib should steal outright

| # | mechanism (source) | one-line adaptation to the text surface |
|---|---|---|
| 1 | **TRP / TCU projectability** (SSJ 1974) | nib's closed-thought detector is already this; rename it, cite it, and make it *projective* — score the probability that the current clause will close within N characters, rather than detecting closure after the fact |
| 2 | **Additive turn-yielding cues** (Duncan 1972) | replace the single 2 s test with a *cue count*: terminal punctuation + pause-length bucket + paragraph break + caret-left-paragraph + focus-loss; emit probability rises with the count |
| 3 | **Attempt-suppressing signal** (Duncan 1972) | continued typing inside the window is not merely "a refusal reason" — it is a cue that zeroes the emit probability even when every yielding cue fired. Keep the serializer refusal, but make the gate *know* about it so it stops proposing |
| 4 | **Continuers / back channel** (Yngve 1970; Schegloff 1982) | add a third emission class below `emit`: a **continuer** that renders in the margin/status line and by law can never become a document character. It is how the resident says "still with you" without taking the floor |
| 5 | **Prepare-early, release-at-boundary** (Levinson & Torreira 2015) | decode the candidate block *during* ingest and hold it; the floor rule decides release, not composition. This is already SPEC 6.2.3 — the citation makes it a principle rather than an optimisation |
| 6 | **Pause-level gradient** (keystroke-logging writing research) | a 2 s pause within a word ≠ after a word ≠ after a sentence ≠ after a paragraph. Bucket the pause by syntactic level and let the floor window vary by bucket instead of being a constant |
| 7 | **Inner Monologue** (Moshi 2024) | run a cheap text plan ahead of the visible characters; the *plan* is what gets checked against each new percept, so contradiction is detected before more characters reach the air, and the kill is cheaper and earlier |
| 8 | **Silence as a generated symbol** (SyncLLM 2024; duplex time slices, Zhang et al. 2024) | keep `[tick +Ns]`; cite both papers as convergent evidence that an idle token beats an absence |
| 9 | **Chunk-level state head on a frozen backbone** (Freeze-Omni 2024) | the emit gate is a small classifier over the ingest tail — trainable and swappable without touching the byte-frozen seed, which is exactly what SPEC 6.2.2 requires |
| 10 | **Stop latency + Respond/Resume behavioural coding** (Full-Duplex-Bench v1.5; HumDial 2026) | adopt the names and the four-way coding for nib's un-say metric instead of inventing vocabulary |
| 11 | **Expected-utility threshold made trainable** (Horvitz 1999 → Cursor Tab-RL 2025) | the emit gate's threshold is `p_useful > cost_interrupt / (value_useful + cost_interrupt)`; publish the two constants on the status line and let the tape's accept/ignore/undo record estimate `p` |
| 12 | **Negotiated interruption** (McFarlane 2002) | the resident *proposes* on the continuer channel; the human's next keystroke disposes — accept (a key, or the caret entering the resident's block), defer (keep typing), or reject (backspace). Mediated stays as the hard floor |
| 13 | **Defer-to-breakpoint with a coarseness hierarchy** (Iqbal & Bailey 2007–2010) | classify each keystroke gap as fine / medium / coarse from the op log; longer or more intrusive emissions require coarser breakpoints |
| 14 | **Shared feedback over explicit coordination** (Dourish & Bellotti 1992) | never add a modal "AI is thinking" panel; every awareness signal must be a property of the artefact — colour, position, margin — which nib's design already prefers |
| 15 | **Territory = paragraph** (Larsen-Ledet & Korsgaard 2019) | keep "own blocks, never inside a human's paragraph," and cite the measurement rather than asserting it as taste |
| 16 | **Per-character authorship colour** (Etherpad 2008) | already SPEC §7; cite Etherpad as the origin and note that nib makes it permanent and tape-backed rather than a view toggle |
| 17 | **KV forking / radix prefix reuse** (SGLang 2024; vLLM) | the forming region decodes on a *fork* of the trunk, so abort is a dropped fork with zero rollback and un-saying costs microseconds by construction |
| 18 | **Attention sinks** (StreamingLLM, ICLR 2024) | the escape hatch for the resident window when the 8192-token compromise (SPEC 6.2.6) stops being enough |
| 19 | **Entropy-gated early start** ("Think while Listening," 2025) | an entropy-derived "thought completeness" score is a cheaper and better-motivated emit trigger than a punctuation rule, and it is computed from the same forward pass |
| 20 | **Keystroke-level session logging as a public dataset** (CoAuthor, CHI '22) | nib's tape is a strictly richer CoAuthor; releasing a de-identified Stage 5 tape in CoAuthor-compatible form is the cheapest credibility purchase available |

---

## 9 · (b) The three strongest critiques from each field

### From conversation analysis
1. **You have imported the vocabulary of turn-taking into a medium that has no turn scarcity.**
   SSJ's system exists because speech is one serial channel; "one party at a time" is a *consequence*
   of physics, not a value. A pad has no such constraint. Your floor rule therefore solves a problem
   the medium does not have — and the real question, which you have not asked, is what the *unit of
   sequential relevance* is in text, since it is plainly not the turn.
2. **Your "thought boundary" is a punctuation rule, not a TRP.** TRPs are *projected* by recipients
   from syntax, prosody and pragmatics *before* completion; that projection is the entire finding.
   Detecting a full stop after it is typed is not projection, it is the trivial case, and it means
   the resident is structurally always late in a way a human recipient is not.
3. **Un-saying has a name in CA and it is not retraction — it is self-repair, and repair is
   organised, not free.** Schegloff, Jefferson & Sacks (1977) show repair has preference structure
   and a positional grammar (same-turn, transition-space, next-turn, third-position). A system that
   withdraws characters with no repair-initiation format, no account, and no repair-completion marker
   is producing a phenomenon that has no interactional shape. Human recipients will read the
   retraction *as* an action and infer a meaning from it, and you have not designed what it means.

### From full-duplex spoken dialogue
1. **You have not defined the contradiction detector, which is the whole system.** "The next
   keystroke contradicts it" is an entailment judgment. EchoChain exists precisely because models are
   bad at state-update reasoning under interruption. Your 13 µs kill is a *latency* number for a
   decision whose *accuracy* is unmeasured — and a fast wrong un-say is worse than a slow right one.
2. **Your evaluation has n = 1 and no baseline.** This field converged in eighteen months on
   Full-Duplex-Bench, FD-Bench, FLEXI and HumDial precisely because single-system demos were
   unfalsifiable. Your RESIDENT/TURN-BASED toggle is a good instinct but it is a within-subject
   design with one subject, one document type and an unblinded, invested experimenter.
3. **A 2 s hard floor is the "floor-holding" strategy that v1.5 shows costs responsiveness.** In
   speech the responsive/floor-holding trade-off is measured, not assumed. You have chosen one pole
   by fiat and named the choice a serializer rule so it cannot be tuned — which means you cannot even
   measure what the other pole would have given you.

### From CSCW
1. **The tradition abandoned floors for text on purpose, and you have re-introduced one without the
   compensating awareness affordances.** Ellis & Gibbs (1989) and everything downstream showed that
   transformation plus visibility beats mutual exclusion for documents; Dourish & Bellotti (1992)
   showed explicit coordination loses to shared feedback. Your rule is Dommel's mediated floor with
   a fixed timeout — the least adaptive point in the whole design space.
2. **Territoriality predicts your users will reject the resident's blocks regardless of quality.**
   Larsen-Ledet & Korsgaard found writers defend paragraph-level territory against *improvements*;
   Wang, Tan & Lu found writers actively flee co-presence, copying text to private files to escape
   being watched mid-formation. A resident that perceives every keystroke including deletions is the
   maximal version of the thing those participants were escaping — and your falsifier ("did you
   leave it on") will fire for territorial reasons that have nothing to do with the model's quality.
3. **CHI '26 already ran your study and got the opposite result.** Lehmann et al.'s N=30, one-week
   deployment found teams did not treat agents as members and pushed agent output into *comments*.
   You are claiming the body of the document with a smaller N and no comparison condition.

### From AI co-writing / writing-assistant UX
1. **Latent persuasion is worse, not better, without a system prompt.** Jakesch et al. (CHI '23,
   N=1506) showed an opinionated assistant shifts users' *attitudes*, and named opacity as the
   aggravating factor. Your resident has an owner-set mandate, no visible instruction, permanent
   presence, and accretes context from the user's own writing. Per-character authorship tells the
   user *who typed it*, not *what it was disposed to say*, and the tape is not read by the person
   being influenced while they are being influenced.
2. **Nothing in your design measures whether the emission was any good.** Ziegler et al. (MAPS '22)
   found acceptance rate — not persistence — is what drives perceived productivity, and Cursor
   demonstrated the whole game is knowing when *not* to suggest. Your emissions cannot be accepted or
   rejected; they are simply *there*, in the document, authored. There is no accept signal, so there
   is no `p`, so your gate cannot be calibrated even in principle.
3. **Ownership will drop and you will not notice.** Draxler et al. (TOCHI 2024) found users neither
   feel ownership of AI text nor declare it. nib's honesty laws address declaration, which was never
   the failing half; they do nothing about felt ownership, which drops as the AI's share rises. A
   week-long Stage 5 with the operator as sole subject is the worst possible instrument for
   detecting a slow shift in his own sense of authorship.

### From mixed-initiative / interruption research
1. **You built the mediated method and skipped the negotiated one, which is the empirically better
   one.** McFarlane's own conclusion. Your rule also violates Horvitz's *employ dialog to resolve key
   uncertainties* and *provide mechanisms for efficient agent–user collaboration to refine results*:
   there is no dialogue and no refinement — there is a timeout.
2. **The floor window has no utility model behind it.** Horvitz's principle is that the agent acts
   when expected utility of acting exceeds that of not acting. "2 s" encodes an implicit claim about
   the ratio of interruption cost to information value and is not derived from either. It is also
   uniform across a within-word pause and an end-of-paragraph pause, which Iqbal & Bailey showed
   differ by an order of magnitude in resumption cost.
3. **Your central refusal — never simulate a tell that has no internal correlate — forbids the one
   affordance the interruption literature says works.** Negotiated interruption requires an
   *announcement*: a pre-signal that something is pending. A resident that may only be silent or
   fully speaking has no way to announce, so its only intrusiveness setting is maximal. The
   continuer channel (§8, row 4) is the resolution — a real internal state (a held, margin-positive
   judgment) rendered honestly — but as specified today the rule blocks the fix.

---

## 10 · (c) Metrics for Stage 5, and how to compute them from the tape

nib's tape (SPEC §8) records every changeset, every judgment with its margin, every hold, every
abort, every switch transition, the model hash and the seat mandate. That is enough to compute all
of the following without adding instrumentation. Notation: `H` = human changesets, `R` = resident
changesets, `J` = judgment records, `A` = abort records.

### 10.1 Floor-control metrics (from CA and full-duplex)

| metric | definition | computation from the tape |
|---|---|---|
| **Emission latency** | resident's analogue of Response Latency | `t(first char of R block) − t(last H keystroke in the preceding burst)`. Report the full distribution and its mode, not the mean; compare its shape to the human gap distribution in the same session (Heldner & Edlund's method) |
| **Takeover rate** | fraction of human pauses ≥ θ at which the resident emitted | `|{pauses ≥ θ with an R block started inside}| / |{pauses ≥ θ}|`, swept over θ ∈ {0.5, 1, 2, 4, 8} s. This is the single most diagnostic curve in the whole battery |
| **Timing divergence (JSD)** | how unlike a human collaborator the resident's timing is | Jensen–Shannon divergence between the resident's emission-onset distribution and the human's turn-onset distribution over the same document. Directly borrowed from Full-Duplex-Bench |
| **Floor violations** | must be exactly zero | count of R changesets whose target block has an H changeset within the window. This is Stage 2's falsifier; report it as a *measured* zero, not an asserted one |
| **Near-violations** | the interesting number | emits *refused* by the serializer per hour. A high count means the gate and the floor disagree — i.e. the gate is proposing into occupied territory and the floor is quietly cleaning up. That disagreement rate is the real health metric of §5 |
| **Overlap duration** | how long two authors were simultaneously producing | total wall time with an H changeset and an R block both within 1 s. A pad permits this; the question is whether the human tolerates it |

### 10.2 Un-saying metrics (from Full-Duplex-Bench v1.5 / HumDial 2026)

| metric | definition | computation |
|---|---|---|
| **Stop latency** | the headline number, in the field's own vocabulary | `t(abort record) − t(the H keystroke that triggered it)`. Report median and p95 |
| **Characters withdrawn** | the visible cost of a retraction | from the abort record's "what was formed / what reached the air" fields; report the distribution, because withdrawing 3 characters and withdrawing 200 are different products |
| **Un-say precision** | was the kill *right*? | for each abort, did the human's subsequent writing (next 60 s) in fact contradict the killed sentence? Requires post-hoc coding — blind, by someone other than the operator, on shuffled abort records with the killed text withheld until after the judgment. **Without this number the 13 µs figure means nothing**, and this is the field's first question |
| **Miss rate** | the complement, and the one nobody reports | committed R blocks that the human deleted within 60 s — emissions that *should* have been un-said and were not. Computable exactly, from `H` deletions targeting `R` character identities |
| **Behavioural coding** | HumDial's four-way | classify each un-say episode as **Respond / Resume / Uncertain / Unknown**: did the resident replace the killed sentence with a corrected one, resume the original, stay silent, or produce something uninterpretable? |

### 10.3 Co-writing and acceptance metrics (from Copilot / Cursor / CoAuthor)

| metric | definition | computation |
|---|---|---|
| **Survival rate at 10 min / 1 h / session end** | nib's substitute for acceptance rate | fraction of resident-authored characters still present, by character identity, at each horizon. Character identities (SPEC §2) make this exact — this is a *better* measurement than the persistence measures Ziegler et al. could compute, and nib should say so |
| **Edit-distance-to-survival** | did it survive as written, or as raw material? | Levenshtein between the emitted block and its surviving descendant characters |
| **Effective p (accept probability)** | the constant Cursor learns and nib must calibrate | survival-at-1-h as the empirical estimate of `p`; then check whether the gate's margin threshold is consistent with `p > cost/(value+cost)` for the operator's stated costs. Publish both constants |
| **Emissions per hour of real writing** | already in BLUEPRINT §8 | count R blocks / wall-clock hours with AI on. Cursor's result (21 % fewer suggestions, 28 % higher accept rate) is the benchmark direction: **fewer and better beats more** |
| **Holds per emit** | already in BLUEPRINT §8 | `|J where hold| / |J where emit|`. Report jointly with the margin distribution, since holds at margin 0.49 and holds at margin 0.05 are different phenomena |
| **Human-deletion rate of own vs resident text** | the territoriality probe | compare deletion rates for H-authored and R-authored characters, normalised by age |

### 10.4 The experiment-integrity metrics (Stage 4's falsifier)

| metric | computation |
|---|---|
| **Paired-sample count and balance** | mode transitions on the tape → number of RESIDENT/TURN-BASED segments, their durations, and their content types. If the operator flips to TURN-BASED only when writing difficult passages, the paired design is confounded and the tape will show it |
| **Seat/seed/sampler identity across the toggle** | the pinned serve hash `0xe7ffa5704ba31076` recorded on both sides of every transition. Already checkable; make it a *reported* number |
| **AI-off fraction** | wall-clock with AI off ÷ total. This is the actual Stage 5 falsifier in numeric form — publish it before publishing anything else |

### 10.5 The metrics these fields would demand that nib currently cannot produce

- **Blind quality judgment** of resident emissions by someone other than the operator (every AI
  co-writing paper since CoAuthor).
- **A control condition with a human collaborator** in the same pad, to establish what emission
  rates and floor violations look like when the second party is a person (CSCW would insist).
- **Attitude/opinion pre-post measurement** for the latent-persuasion risk (Jakesch et al.); with
  n = 1 this is impossible, which is itself a publishable limitation.
- **Felt-ownership measurement** over the week (Draxler et al.) — cheap: a daily one-item scale, and
  a fair test of whether nib's honesty laws do the work they claim.

---

## 11 · (d) Candid novelty assessment

### Genuinely unprecedented (as of this survey)
1. **Visible un-saying of persistent, already-rendered text, with the retraction on an append-only
   hash-chained tape.** Speech systems stop; serving stacks abort; no system found withdraws
   characters that a human has already read from a shared document and keeps a verifiable record of
   what was withdrawn and why. This is nib's real contribution and the paper should be about this.
2. **Deletions as first-class percepts.** Keystroke-logging research and CoAuthor *record*
   deletions; nothing found *feeds* them to a model as world. Combined with the byte-conservation
   falsifier (SPEC 5.1.11 — `typed_in == typed_out`, `pushed + dropped == percepts`, checked after
   every keystroke) this is a rigour level no co-writing system in the survey approaches.
3. **A build-time linker gate as a shipped privacy guarantee.** "0 bytes egress because
   `dumpbin /dependents` fails the build on `ws2_32`" is a *checkable artefact*, not a policy claim.
   Local-only is common; a falsifiable local-only is not.
4. **The RESIDENT/TURN-BASED toggle as an instrumented twin race inside the product**, with seat,
   seed and sampler pinned by hash across the toggle so only the trigger varies. The design is
   sound; the n = 1 execution is the weakness, not the idea.

### Convergent — arrived at independently, already established elsewhere, and stronger for the citation
1. **Judgment on the ingest pass rather than polling** ⟵ Levinson & Torreira's 200 ms/600 ms puzzle;
   Moshi; SyncLLM; LiveMind; "Think while Listening"; Asynchronous Reasoning.
2. **Silence as a generated token** ⟵ SyncLLM's clock-synchronous intervals; Zhang et al.'s idle
   time slices. nib's `[tick +Ns]` is the same idea from a different lineage.
3. **Parallel author streams with no explicit speaker turns** ⟵ Moshi's multi-stream architecture;
   nib's lanes.
4. **A forming/committed seam** ⟵ Moshi's Inner Monologue (text ahead of audio); speculative
   decoding's draft/verify; SGLang's forked KV.
5. **Territory = the paragraph** ⟵ Larsen-Ledet & Korsgaard 2019, measured.
6. **Per-character authorship colour** ⟵ Etherpad 2008.
7. **A decision-theoretic emit threshold** ⟵ Horvitz 1999, made concrete by Cursor's Tab-RL reward.
8. **The 2 s window** ⟵ the standard keystroke-logging burst threshold. Same number, independent
   derivation.

### Prior art nib must cite (non-negotiable, in a paper or a README)
Sacks/Schegloff/Jefferson 1974 · Duncan 1972 · Yngve 1970 · Schegloff 1982 · Levinson & Torreira
2015 · Stivers et al. 2009 · Défossez et al. (Moshi) 2024 · Veluri et al. (SyncLLM) 2024 · Ma et al.
(LSLM) 2024 · Freeze-Omni 2024 · Zhang et al. (duplex text models) 2024 · Lin et al.
(Full-Duplex-Bench + v1.5) 2025 · HumDial 2026 · EchoChain 2026 · Ellis & Gibbs 1989 · Dourish &
Bellotti 1992 · Greenberg & Marwood 1994 · Dommel & Garcia-Luna-Aceves 1997 · Gutwin & Greenberg
2002 · Posner & Baecker 1992 · Larsen-Ledet & Korsgaard 2019 · Wang, Tan & Lu 2017 · Birnholtz &
Ibara 2012 · Lehmann, Shauchenka & Buschek 2026 · Etherpad 2008 · Chen et al. (Smart Compose) 2019 ·
Coenen/Yuan et al. (Wordcraft) 2021–22 · Lee, Liang & Yang (CoAuthor) 2022 · Ziegler et al. 2022 ·
Cursor Tab-RL 2025 · Draxler et al. 2024 · Jakesch et al. 2023 · Zhang et al. (Proactive Thought
Partners) 2026 · Horvitz 1999 · Allen/Guinn/Horvitz 1999 · McFarlane 2002 · Iqbal & Bailey
2007/2008/2010 · Leviathan et al. 2023 · Zheng et al. (SGLang) 2024 · Xiao et al. (StreamingLLM)
2024 · LiveMind 2024 · Ink & Switch Patchwork 2024–26.

### The one-sentence honest positioning
> nib is not the first system in which a model writes beside a human in real time — Patchwork and
> the CHI '26 agent-in-the-document work got there, and Moshi solved the always-on half in speech.
> nib is the first system in which the model can **take words back off the page** and prove it did.

### The two claims most likely to be attacked, and where they are weakest
- **"Floor control is the unsolved research problem."** Half true, and reviewers will split. In
  *speech* it is being actively solved and benchmarked right now. In *text with an AI participant*
  it is genuinely open — but the CSCW answer has been available since 1989 and is "don't use a
  floor, use transformation plus visibility," which nib has not argued against. Argue against it
  explicitly (the argument is about *attention*, not correctness) or the framing will not survive
  review.
- **"No turn-based system can un-say."** As written this is false: any streaming UI can abort and
  delete tokens. The defensible version is narrower and stronger: *no turn-based system can un-say
  in response to evidence that arrived while it was speaking, because in a turn-based system no such
  evidence exists during the turn.* Say it that way.

---

## 12 · Verification ledger

**Verified against a primary source** (arXiv abstract page, ACL Anthology, publisher page, or the
author's own PDF/blog): Moshi (2410.00037) · SyncLLM (2409.15594 + ACL Anthology) · LSLM (2408.02622)
· Freeze-Omni (2411.00774) · VITA (2408.05211) · Mini-Omni (2408.16725) · Full-Duplex-Bench
(2503.04721) · Full-Duplex-Bench v1.5 (2507.23159, authors + overlap categories + "stop and response
latency" + responsive/floor-holding finding) · FD-Bench (2507.19040) · FLEXI (2509.22243) · HumDial
study (2604.21406, tracks + metric definitions + scoring weights) · EchoChain (2604.16456) ·
LiveMind (2406.14319) · duplex text models (EMNLP 2024 anthology) · StreamingLLM (2309.17453) ·
speculative decoding (2211.17192) · SGLang/RadixAttention (LMSYS blog) · Asynchronous Reasoning
(2512.10931) · Think-while-Listening (2510.07497) · AnchoredAI (2509.16128) · Proactive Thought
Partners (2609.01588, verbatim abstract) · Lehmann et al. CHI '26 (2509.11826, verbatim abstract) ·
Cursor Tab-RL (cursor.com/blog/tab-rl — reward structure, formula, and all four numbers) ·
Ink & Switch Patchwork notebook 07.

**Verified for citation metadata (venue, year, pagination, authors) but summarised from secondary
sources for mechanism detail:** SSJ 1974 (primary PDF exceeded the fetch size limit) · Duncan 1972
(six-cue list) · Schegloff 1982 · Heldner & Edlund 2010 (exact percentages not retrieved) ·
McFarlane 2002 (four-method definitions and the "negotiated wins" result; primary PDF would not
extract) · Gutwin & Greenberg 2002 (page range) · Posner & Baecker 1992 · Wang, Tan & Lu 2017 ·
Birnholtz 2012/2013 · Ellis & Gibbs 1989 · Greenberg & Marwood 1994 · Dommel & García-Luna-Aceves
1997 · Etherpad history · Chen et al. 2019 (the 60 ms p90 figure) · Ziegler et al. 2022 (acceptance
percentages, via search summary of the paper) · Wengelin 2006 / the 2000 ms burst threshold.

**Explicitly UNVERIFIED, cited from prior knowledge, flagged in place:** Chen et al. 2023
speculative sampling (2302.01318) · Kwon et al. SOSP 2023 PagedAttention · Schegloff, Jefferson &
Sacks 1977 on repair (invoked in §9 as a critique source) · the claim that "Designing Proactive
Thought Partners for Writing" covers retraction (a PDF-extraction artefact; the abstract does not
support it) · the Voxtral Realtime / vLLM resumable-request detail that appeared in a search summary
but not in the paper's abstract · DuplexMamba's state-duplication branch · Cotypist/Typeahead
performance claims (vendor marketing) · PIR / PRR / FTED as a single canonical definition set.

**Searched and found empty:** any system combining evidence-triggered emission from an ingest pass,
deletions as percepts, and visible tape-logged retraction in a text editor. The nearest three are
named in §7.
