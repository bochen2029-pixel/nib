# LIFT MAP — auricle/FUSOR → nib, Stages 2–4

**Read-only survey, 2026-09-04.** Nothing was modified anywhere. No GPU binary was run.
All line numbers verified by `cat -n` / `grep -n` at the time of writing.

## 0 · Provenance receipts

**`fusord.cpp` is BYTE-IDENTICAL between the snapshot and the working tree.**

```
sha256  3ae1d69041fc6ac23e7bc9826840b4b551081c70d648f0a8016dce83da5aca28
        C:/auricle_snapshots/20260813-131533/src/fusor/fusord.cpp
        C:/auricle/src/fusor/fusord.cpp
        45,952 bytes · 791 lines · mtime Aug 12 21:59 (both)
```

`cmp -s` returns 0. The snapshot adds nothing and the working tree has not drifted. Every
`fusord.cpp:N` citation below is valid against either path.

Sizes of everything else read in full:

| file | lines |
|---|---|
| `C:/auricle/src/fusor/source.h` | 177 |
| `C:/auricle/src/fabric/fabric.h` | 336 |
| `C:/auricle/src/fabric/durable.h` | 66 |
| `C:/auricle/src/core/ring.h` | 79 |
| `C:/auricle/src/app/soak.cpp` | 695 |
| `C:/auricle/src/app/syncytium.cpp` | 391 |
| `C:/auricle/src/app/starship.cpp` | 1001 |
| `C:/auricle/src/app/blackbox.cpp` | 1770 |
| `C:/auricle/src/cortex/cortex.h` | 135 |
| `C:/auricle/src/cortex/cortex_llama.cpp` | 683 |
| `C:/auricle/src/m0/m0_molt.cpp` | 256 |
| `C:/auricle/src/m0/m0_demo.cpp` | 211 |
| `C:/auricle/src/tape/tape.h` | 27 (**not a tape — see §5**) |

---

# 1 · EMISSION (Stage 2) — `fusord.cpp`

## 1.1 The whole emission block, in one range

**`fusord.cpp:472–562`** is the entire "a seat cleared zero, so it speaks" path. Stage 1b stops
at nib `resident.cpp:301–303` where this block would begin. Everything in §1 below is inside it
unless said otherwise.

## 1.2 The speak-cue frame (BYTE-FROZEN)

**`fusord.cpp:477–480`**

```cpp
const std::string cue = std::string("<|im_end|>\n<|im_start|>user\nYou are the ") +
    MINDS[m].name + ". " + MINDS[m].mandate + ". You chose to speak about what you "
    "just perceived in the stream. Give your one-sentence line now — no preamble."
    "<|im_end|>\n<|im_start|>assistant\n<think>\n\n</think>\n\n";
```

nib **already carries this verbatim** — `C:/nib/src/resident.cpp:62–66` hashes exactly these
literals inside `serve_hash()`, and nib computes fusord's pin `0xe7ffa5704ba31076`
(`fusord.cpp:157`, `nib/src/resident.h:49`). Stage 2 therefore decodes a constant that is
**already pinned and already checked in `--selftest`**. Do not retype it; reconstruct it from
the same string pieces the hash covers.

**The gate that fires the cue.** `fusord.cpp:472–473`:

```cpp
for (int m = 0; m < 3; ++m) {
    if (margins[m] <= 0.0f) continue;
```

Dial is literally zero, in the code, not a config: `margin > 0` speaks. `margins[m]` is
`l[emit_tok] - l[hold_tok]` computed at `fusord.cpp:468` on the ephemeral `DECIDE` branch.
nib computes the identical number at `resident.cpp:290`.

**Branch mechanics for the emission**, `fusord.cpp:475–483`:

```cpp
llama_memory_seq_rm(mem, GEN, -1, -1);
llama_memory_seq_cp(mem, TRUNK, GEN, -1, -1);   // fork at 0 MiB
... dec(ctx, ct, GEN, npast, true);
llama_pos gpos = npast + (llama_pos)ct.size();
```

Sequence ids, `fusord.cpp:298`: `TRUNK = 0, DECIDE = 7, GEN = 6, SCRIBE = 5`. Seats 1/2/3 are
declared in `MINDS` (`fusord.cpp:123–130`) but **unused in fusord** — one shared `GEN` seq is
reused per seat, serially. The per-seat seqs are used by `syncytium.cpp` / `starship.cpp` /
`blackbox.cpp`. nib currently declares only `TRUNK = 0, DECIDE = 7` (`resident.cpp:81`).

## 1.3 The sampler — every parameter

**`fusord.cpp:285–292`** — two chains, both trivially short:

```cpp
llama_sampler* smp = llama_sampler_chain_init(llama_sampler_chain_default_params());
llama_sampler_chain_add(smp, llama_sampler_init_min_p(0.05f, 1));
llama_sampler_chain_add(smp, llama_sampler_init_temp(0.7f));
llama_sampler_chain_add(smp, llama_sampler_init_dist(11));
llama_sampler* smp_scribe = llama_sampler_chain_init(llama_sampler_chain_default_params());
llama_sampler_chain_add(smp_scribe, llama_sampler_init_min_p(0.05f, 1));
llama_sampler_chain_add(smp_scribe, llama_sampler_init_temp(0.3f));
llama_sampler_chain_add(smp_scribe, llama_sampler_init_dist(11));
```

| parameter | emission (`smp`) | molt scribe (`smp_scribe`) |
|---|---|---|
| min-p | **0.05**, `min_keep = 1` | 0.05, min_keep 1 |
| temperature | **0.7** | **0.3** |
| RNG seed (`dist`) | **11** | 11 |
| top-k | **absent** | absent |
| top-p | **absent** | absent |
| repeat penalty | **absent** | absent |
| grammar / logit bias | **absent** | absent |
| chain order | min-p → temp → dist | same |

**This exact chain is reproduced identically in three other binaries** — `syncytium.cpp:163–166`,
`starship.cpp:406–409` (seed = `argv` seed, default from `--seed`), `blackbox.cpp:890–893`
(seed = `--seed`, default 11). The only thing anyone ever varies is the `dist` seed. `min_p(0.05)`
and `temp(0.7)` never move. **Treat min-p 0.05 / temp 0.7 / order as frozen; treat the seed as
configuration** (it must be *recorded*, §3.4).

`cortex_llama.cpp` is a different, older sampler and is **not** the reference — do not lift from it.

## 1.4 The generation loop — cap, stops, streaming, and why

**`fusord.cpp:484–503`.** The comment at 484–488 is the law, quoted here in full because it is
the single most important paragraph for nib Stage 2's pacing:

> `// ONE SENTENCE, HARD CAP. Measured 2026-08-12: latency-to-notice was COUPLED to`
> `// emission length — the longer the mind had to say, the blinder it went (500 ms →`
> `// 3.5 s inside one line). That is the back door in miniature and the same law as`
> `// yesterday's: pacing may delay the mind's next token, never a percept. Generation`
> `// is the blind window, so the blind window is bounded by construction.`

```cpp
std::string say;
for (int t = 0; t < 28; ++t) {                                   // 490  HARD CAP = 28 tokens
    const llama_token tok = llama_sampler_sample(smp, ctx, -1);  // 491
    if (llama_vocab_is_eog(vocab, tok)) break;                   // 492  stop 1: EOG
    char pc[256]; const int pn = llama_token_to_piece(vocab, tok, pc, sizeof(pc), 0, true);
    std::string piece(pc, pn > 0 ? (size_t)pn : 0);
    if (piece.find('\n') != std::string::npos) break;            // 495  stop 2: any newline
    say += piece;                                                // 496
    std::vector<llama_token> one{tok};
    dec(ctx, one, GEN, gpos, true); ++gpos;                      // 498  one token per decode
    if (t >= 6) {                                                // 499  stop 3: sentence-final
        const char lc = say.empty() ? 0 : say[say.size() - 1];   //      but only after 7 tokens
        if (lc == '.' || lc == '!' || lc == '?') break;          // 501
    }
}
llama_memory_seq_rm(mem, GEN, -1, -1);                           // 504  branch dropped, always
```

Stops, in evaluation order: **EOG** (492) · **newline inside the piece** (495) · **28-token hard
cap** (490) · **`.`/`!`/`?` at the end of `say`, only once `t >= 6`** (499–502). Note the piece
containing the newline is *discarded*, not appended.

**Partial-word streaming.** fusord does **not** stream. `say` accumulates and is printed once, at
`fusord.cpp:546–547`, after the loop finishes. The token-at-a-time surfaces exist elsewhere:
`syncytium.cpp:277–283` and `starship.cpp:602–608` print and trace each `piece` as it lands
(`"k":"tok"` rows), and `blackbox.cpp:1524–1530` does the same. **nib must lift the per-token
surface from `starship.cpp:602–635`, not from fusord** — fusord's is a console `printf` of a
finished line, which is precisely the shape nib is trying not to be.

**There is no abort inside fusord's generation loop.** The 28-token cap *is* fusord's entire
answer to "the world may arrive while I am talking": bound the blind window rather than interrupt
it. The interruptible loops are `syncytium.cpp:243–297`, `starship.cpp:572–786`,
`blackbox.cpp:1451–1543`. See §2.

**No "13 µs abort" exists in fusord.** See §2.6 for where that number actually comes from.

## 1.5 The self-echo law — how speech re-enters the world

There are **two distinct rules both called self-echo**, and nib currently implements only one.

**(a) The emission IS a percept, committed directly to the trunk.** `fusord.cpp:548–561`:

```cpp
// …and because it REALLY said it (the operator just read it), the emission is part
// of the world: it commits to the trunk on the seat's own lane. Measured 2026-08-12,
// first live smoke: without this the mind has no memory of having spoken, so while a
// contradiction stands unaddressed EVERY later boundary re-fires it — the SKEPTIC
// repeated one catch across 10 consecutive boundaries. A resident that cannot
// remember speaking is not present; it is stuck. (Soak keeps `pure` because there
// the emissions were never surfaced to anyone — a measurement, not a room.)
if (!pure) {
    const std::string commit = std::string("\n[") + MINDS[m].name + "] " + say;   // 556
    auto cmt = tk(vocab, commit, false);
    dec(ctx, cmt, TRUNK, npast, true); npast += (llama_pos)cmt.size();            // 558
    read_frontier();                       // keep the nerve's denominator valid  // 559
    tail_push_line(std::string("[") + MINDS[m].name + "] " + say);                // 560
}
```

**The lane is the seat's own name**, uppercase, in the same `\n[lane] text` frame the world uses
(`fusord.cpp:724`). No self-exception, no separate channel, no marker. `read_frontier()` at 559 is
mandatory — skipping it leaves `logZ` stale and the next word's surprisal is garbage.
`tail_push_line` at 560 puts the speech in the molt's verbatim tail so it survives a fold (§4).

The `--pure` flag (`fusord.cpp:203–205`, `213`) turns this OFF and is documented as the
*measurement* semantics, not the room semantics. **nib is a room. `pure` must be false.**

**(b) A delta arriving on a seat's lane must NOT be re-ingested.** `fusord.cpp:704–707`:

```cpp
// SELF-ECHO SUPPRESSION: never ingest our own seats' output as world input.
bool self = false;
for (auto& m : MINDS) if (!_stricmp(d.lane, m.name)) self = true;
if (self || !_stricmp(d.lane, "fusor")) { ++echo_skipped; continue; }
```

The doctrine is `FUSOR_HARNESS_SPEC.md:124` (§5 item 8):

> **Self-echo suppression** *(new, tiny — critical)* — the decision gate must SKIP self-authored
> deltas (lane stamp), or each nucleus deliberates about interrupting itself.

`source.h:16–17` cites this as "spec §5.8" and the citation **is valid** — it is item 8 of the
numbered list under `## 5` (line 115). Not a dangling reference.

**(c) The law's current, corrected form supersedes fusord's implementation.**
`C:/NEW/FUSOR_MASTER-ONEPAGER_2026-08-23_FABLE5.md:28`:

> *Percepts are never dropped — judgment may be delayed; the world is never edited — and, the
> newest correction, **with no self-exception**: the mind's own speech is a percept too, **or
> say-it-once is structurally unlearnable.***

`C:/NEW/MERIDIAN_SIGHT_SPEC_v0.1_FABLE5_2026-08-23.md:142` names the defect fusord left behind:

> ***Own acts are percepts.** … the firsthour daemon ran self-echo-filtered and paid for it in
> perseveration — **the one percept class the gate dropped was the mind's own voice**, making
> say-it-once structurally unlearnable … In MERIDIAN the law is written the other way:
> **L-GATE has no self exception.***

And the reconciliation, `C:/NEW/FUSOR_X_AURICLE_THE-STARSHIP-FIT_v0.1_FABLE5_2026-09-01.md:151`:

> ***Own speech is a percept.** Words that reached the air re-enter as lane `self`
> (`ADDENDUM-F §3.1:79–80`); **the gate skips seat lanes** (`fusord.cpp:704-707`) **so the mind
> hears itself without judging itself.***

**Why nib must have both.** In a pad the resident writes into the buffer it reads, so its own
emission will come back around as a document changeset. Rule (b) — already built at
`nib/src/ingest.h:152–156`, `PadSource::add_seat`/`is_seat` — stops the round trip. Rule (a) is
**structurally absent from nib** and is what Stage 2 must add: the emitted text is decoded onto
`TRUNK` directly at `npast_`, prefixed `\n[SEAT] `, followed by `read_frontier()`. If Stage 2
implements (b) only, nib reproduces the exact bug fusord's comment describes — the SKEPTIC
repeating one catch ten boundaries running — and, per MERIDIAN, makes say-it-once *structurally
unlearnable* rather than merely unlearned. See §7.12.

## 1.6 The 1500 ms flush law, the token cap, and the gate rule

**`fusord.cpp:669–688`**, the whole cadence, verbatim structure:

```cpp
// The pre-registered flush law: boundary OR 24 tok OR 1500 ms — first wins.
const bool backlog = backlog_now > 8;                                    // 677
if (backlog) {
    if (clause_toks >= 24) judge_and_maybe_emit("c", bscore);            // 679  coarsened
}
else if (bscore >= 0.5f) judge_and_maybe_emit("b", bscore);              // 681  boundary
else if (clause_toks >= 24) judge_and_maybe_emit("n", bscore);           // 682  token cap
else if ((long)(wall_ms() - last_flush_ms) >= 1500 && clause_toks >= 6)
    judge_and_maybe_emit("t", bscore);                                   // 683–684  timeout
if (molt_wm > 0 && npast >= molt_wm) {                                   // 685
    if (!clause.empty()) judge_and_maybe_emit("m", bscore);              // 686  pre-molt final
    do_molt();                                                           // 687
}
```

Constants: **`bscore >= 0.5`** boundary mass, **24 tokens**, **1500 ms with a 6-token floor**,
**backlog > 8** ring entries. `backlog_now = src.pending()` is read once per loop pass at
`fusord.cpp:701`. The `"f"` reason (line-final) fires at `fusord.cpp:746` after a whole delta's
words are ingested.

nib has all of this already, in `resident.cpp:320–331` and `resident.cpp:357` — verbatim
including the 6-token floor, with the constants hoisted to `Config` (`resident.h:63–65`).
**Nothing in §1.6 needs lifting; it is done.**

The doctrine, `FUSOR_HARNESS_SPEC.md:118`: gates judgment at boundaries
`` `{boundary | 24 tok | 1500 ms}` ``. The back-pressure corollary, `fusord.cpp:21–24`:

> *Back-pressure corollary (doctrine, 2026-08-12): pacing may delay the mind's next TOKEN; it
> must never delay a PERCEPT. Ingest is unconditional; only judgment cadence is modulated.
> A ring that overflows COUNTS the loss loudly — a silently dropped percept is the turn
> reborn inside the loop.*

## 1.7 The idle tick

**`fusord.cpp:709–719`**:

```cpp
const uint64_t gap = d.wall_ms - last_delta_ms;
if (idle_tick_s > 0 && gap > (uint64_t)idle_tick_s * 1000ull) {
    char tb[64]; std::snprintf(tb, sizeof(tb), "\n[tick +%llus]",     // 712  THE FORMAT
                               (unsigned long long)(gap / 1000));
    auto tt = tk(vocab, tb, false);
    dec(ctx, tt, TRUNK, npast, true); npast += (llama_pos)tt.size();  // 715
    read_frontier(); ++ticks; tail_push_line(tb + 1);                 // 716
    fprintf(lf, "{\"k\":\"tick\",\"ms\":%.0f,\"gap_s\":%llu}\n", …);  // 717–718
}
```

Default `idle_tick_s = 30` (`fusord.cpp:198`). The tick is decoded **before** the delta that broke
the silence. There is a **second, different tick format** at `fusord.cpp:636–638` — the molt outage
tick, `"\n[tick +%llus — I paused the watch to consolidate my memory]"`. nib replicates only the
first (`nib/src/ingest.h:63–65`, correctly cited to `fusord.cpp:712`) and per nib SPEC 5.1.9.1 must
run the lifted resident with `idle_tick_s = 0` so silence is not counted twice.

**Stage 2 must add the second tick** when it adds the molt (§4).

## 1.8 The ledger JSONL rows — every field

fusord's ledger is `runs/fusor_ledger.jsonl`, opened **append** (`fusord.cpp:313`, `"a"` — "a
resident's ledger outlives one process"; soak uses `"w"` at `soak.cpp:398`), and `fflush`ed at
every boundary (`fusord.cpp:594`: *"a resident's ledger is durable as it goes, not at exit"*).
Escaping is `jesc()` at `fusord.cpp:83–91` — handles `"` `\` `\n`, **drops `\r`**, drops control
bytes below 0x20, passes UTF-8 through raw. It is **not** a canonical JSON writer; see §5.

### `k=hdr` — `fusord.cpp:315–324` (once, at start)
`src` · `live`(true) · `dial`(0) · `mode`("pure"|"room") · `molt_wm` · `model` · `seed_toks` ·
`t0_wall` · `kv`("q8_0"|"f16") · `n_ctx` · `serve_hash`("0x%016llx") · `label_enum`
(`["useful","wrong_content","too_late","wrong_to_speak"]`) · `e_def` · `self_ref_detector`.

`serve_hash` on the header is the model-identity row: **it is what lets a reader of the tape say
which machine was on the other end** (nib SPEC 8.1.3). Note it hashes the *serve bytes*, not the
GGUF — the GGUF path is in `model` as a string only.

### `k=b` — `fusord.cpp:570–578` (one per boundary, always)
`i` (boundary index) · `ms` (relative) · `lane` · `reason` (`b`|`n`|`t`|`c`|`f`|`m`) · `score`
(boundary mass) · `lat_ms` · `probe_ms` · `surp_mean` · `surp_max` · `self_ref` (bool) · `clause` ·
**`m_spk` · `m_skp` · `m_sen`** (all three margins, 2 dp).

**There is no `k=hold` row anywhere.** Holds live only as (i) the three margins on the `b` row and
(ii) the `holds` aggregate on `k=end`, incremented at `fusord.cpp:566`. nib SPEC 8.1.2 requires
"every hold" on the tape; nib's `Judgment` struct (`resident.h:31–38`) already carries per-seat
margin + bscore + reason + clause, so **nib can and should emit an explicit hold row where fusord
only has an aggregate**. That is a deliberate improvement, not a lift.

### `k=e` — `fusord.cpp:588–593` (surfaced emissions ONLY)
`i` · `ms` · `mind` · `m` (margin) · `gen_ms` · `reason` · `self_ref` · `say` · `clause`.

Guarded at `fusord.cpp:579–580` by `margins[m] > 0 && !says[m].empty()` — and `says[m]` is
`.clear()`ed on suppression (`fusord.cpp:527`, `536`). The comment at 581–587 records the bug this
fixed: the first live hour gated on margin alone, so every suppressed event *also* ledgered as `e`
and the post-hoc read was **12× high**. The stated law: **`events = e + e_suppressed`, by addition,
never by conflation** (`fusord.cpp:584`, and `e_def` on the header).

### `k=e_suppressed` — two shapes
- **`fusord.cpp:523–526`**, `reason:"resolved"`: `i` · `ms` · `mind` · `m` · `reason` · `say`.
- **`fusord.cpp:532–535`**, `reason:"repeat"`: same **plus `clause`**.

### `k=e_rearm` — `fusord.cpp:540–543`
`i` · `ms` · `mind` · `why` (`"evidence"` | `"expired"`) · `say`.

### `k=e_resolved` — `fusord.cpp:455–457`
`i` · `ms` · `mind` · `by` (the accepting clause).

### `k=tick` — `fusord.cpp:717–718`
`ms` · `gap_s`.

### `k=molt` — `fusord.cpp:645–648`
`ms` · `before` · `rung_toks` · `after` · `dur_ms` · `rung` (the full authored rung text).

### `k=end` — `fusord.cpp:757–769` (once, at stop)
`words` · `toks` · `boundaries` · `holds` · `emits` · `ticks` · `molts` · `molt_outage_ms` ·
`deltas` · `echo_skipped` · `self_ref_boundaries` · `suppressed_repeats` · `coarse_boundaries` ·
`ring_dropped` · `surp_mean_all` · `mean_lat_ms` · `max_lat_ms` · `wall_ms` ·
`stopped` (`"trunk_full"`|`"stopped"`).

## 1.9 The manners layer — say-it-once, and why it is NOT in the weights

**`fusord.cpp:364–378`** is the most directly transferable doctrine in the file for Stage 2:

> *Measured on the first live smoke (2026-08-12): a PERSISTENT condition produces a PERSISTENT
> emission. While the MySQL contradiction stood unaddressed the SKEPTIC re-fired it at every
> subsequent boundary — 10 consecutive times. Committing the seat's own words to the trunk (so it
> knows it spoke) DAMPENS the margin (4.33 → 3.58 → 3.15) but does not cross zero: self-knowledge
> is not sufficient at dial 0 on the 9B. The disposition to say a thing ONCE is not in the weights
> — it is the "manners" layer, and it is a fine-tune target, not something to fake in the harness.
> So the harness does the honest minimum: it SURFACES once and LOGS the rest (`k=e_suppressed`),
> exactly as the falter is measured-but-not-acted-on. Nothing is deleted; the corpus keeps every
> would-be emission.*

The mechanism, all deterministic and model-free:

| piece | lines | what |
|---|---|---|
| `near_dup(a,b)` | 428–442 | ≥ **60 %** of `a`'s words appear in `b` (lowercased, space-split) |
| `content_overlap(a,b)` | 405–426 | stopword-filtered shared-word count; `STOP[]` at 401–404 |
| `looks_like_acceptance(s)` | 391–397 | 8 phrases; **bare "thanks" deliberately excluded** |
| `SUPP_TTL_MS` | 427 | **600000** (10 min) |
| in-window test | 510–511 | `(boundaries - last_say_i) <= 40 && (now - last_say_ms) <= TTL` |
| evidence re-arm | 515 | `dup && content_overlap(clause, last_say) >= 2 && !resolved` |
| targeted acceptance | 450–458 | acceptance must overlap the seat's own last line by ≥ 1 content word |
| **resolved is permanent** | 516–529 | `dup && resolved` ⇒ suppress forever, log `reason:"resolved"` |

The asymmetry law, `fusord.cpp:516–520`:

> *ASYMMETRY: RESOLVED and UNADDRESSED are different states, and only one of them should ever fire
> again. If the world ACCEPTS the correction ("you're right, it's Postgres"), the condition is
> closed — suppression becomes permanent for that line, and no amount of later reference re-arms
> it. An unaddressed thing may be raised again; a settled thing may not.*

Two measured near-misses are already fixed in this code and must not be re-introduced:
`fusord.cpp:385–390` (bare "thanks" resolved **all three seats permanently** off one closing
pleasantry — a false acceptance is the unsafe direction) and `fusord.cpp:398–400` (raw word overlap
let `"missing before we"` re-arm SPEAKER off the stopwords "before"/"we").

`blackbox.cpp` replaces all of this with a simpler **near-dup damper**: normalized word-set
**Jaccard ≥ 0.60** against a per-mind window of the last **8** emissions
(`blackbox.cpp:635–636`, `640–662`, applied `1483–1510`). It is output-side, deterministic,
traced as `"k":"dampened"`, and it **yields the floor and re-runs the same occasion excluding the
yielder** (`blackbox.cpp:1506`). `norm_words` keeps *every* token — `blackbox.cpp:641–643` records
that a `>2`-char filter made `"On it."` normalize to the empty set, so a short line could repeat
forever.

**For nib:** the fusord suppressor is 130 lines of measured heuristic tuned to a meeting stream;
the blackbox damper is 25 lines and strictly better-behaved. **Lift the blackbox damper; port only
`resolved`-permanence from fusord**, because "the human accepted it" is exactly the signal a pad
has in abundance (the human edited the block the resident wrote about).

## 1.10 What is byte-frozen vs. free configuration

**BYTE-FROZEN (train ≡ serve; a drifted byte must refuse to start):**

| thing | fusord | nib status |
|---|---|---|
| `SEED_SYS` | 103–111 | ✔ `resident.cpp:19–27` |
| `SEED_EXAMPLES` (6 examples) | 112–119 | ✔ `resident.cpp:28–35` |
| `SEED_OPEN` | 120 | ✔ `resident.cpp:36` |
| seat names + mandates ×3 | 122–130 | ✔ `resident.cpp:38–45` |
| probe frame `"\n["` `" — "` `"]\nwatcher:"` | 149, built 464–465 | ✔ hashed 61, built 283–284 |
| speak-cue frame | 150–154, built 477–480 | ✔ hashed 62–66, **not yet built** |
| `serve_hash()` FNV-1a construction | 139–156 | ✔ `resident.cpp:50–68` |
| `SERVE_HASH_PIN = 0xe7ffa5704ba31076` | 157 | ✔ `resident.h:49` |
| the lane frame `"\n[" + lane + "] "` | 724 | ✔ `resident.cpp:338` |
| the tick text `"\n[tick +%llus]"` | 712 | ✔ `ingest.h:63–65` |
| `" hold"` / `" emit"` tokenized with leading space | 239–240 | ✔ `resident.cpp:208–209` |
| dial = **0** (`margin > 0`) | 473 | ✔ `resident.cpp:294` |
| segmenter boundary set `. ! ? \n EOG` | 255–265 | ✔ `resident.cpp:218–229` |
| refuse-to-start on hash mismatch | 177–193 (exit 2) | ✔ `resident.cpp:130–139` |

**FREE CONFIGURATION (measure it, record it on the tape, do not pretend it is frozen):**

| thing | fusord default | nib |
|---|---|---|
| `n_ctx` | 65536 q8_0 / 32768 f16 (271) | **8192** (`resident.h:56`) — deliberate, SPEC 6.2.6 |
| KV type | q8_0 + flash-attn (273–276) | same (`resident.cpp:236–239`) |
| `n_batch`/`n_ubatch`/`n_seq_max`/`kv_unified` | 512/512/8/true (272) | identical (`resident.cpp:235`) |
| sampler seed (`dist`) | 11 (288) | — Stage 2 |
| min-p / temp | 0.05 / 0.7 (286–287) | — Stage 2 |
| generation cap | 28 tokens (490) | — Stage 2 |
| `molt_wm` | 24576 (198) | — Stage 2/4 |
| `idle_tick_s` | 30 (198) | 30 in the **compiler** (`ingest.h:65`); resident must run 0 |
| `max_toks` | 30000 (198) | `n_ctx - 512` guard (`resident.cpp:313`) |
| backlog threshold | > 8 (677) | ✔ (`resident.cpp:323`) |
| `SUPP_TTL_MS` | 600000 (427) | — Stage 2 |
| suppression window | 40 boundaries (510) | — Stage 2 |

---

# 2 · UN-SAYING (Stage 3)

## 2.1 There are FOUR distinct kill mechanisms in the estate. Only one is right for nib.

| # | mechanism | trigger | where | verdict for nib |
|---|---|---|---|---|
| 1 | **`contradicts ∩ dep` hard-kill** | a *committed* rev whose `contra[]` intersects a live branch's `dep[]` | `fabric.h:274–285`, `emit_abort` 290–302 | **THE ONE TO LIFT** |
| 2 | **reflex soft-pause** | a *forming* partial tagged `about_rev` = a dep | `fabric.h:203–212` | lift as the pre-stage |
| 3 | **self-falter (surprisal)** | peak content-surprisal at the incoming percept > bar | `starship.cpp:758–783` | **DO NOT LIFT** — §2.5 |
| 4 | **floor_taken** | another seat out-ranks the speaker mid-utterance | `blackbox.cpp:1127–1139` | lift the *shape*, §6 |

## 2.2 Mechanism 1 + 2: the fabric spine, with line ranges

`fabric.h:274–285` — the serializer, single-writer, on every commit:

```cpp
// HARD cross-abort: contra ∩ each live branch's dep -> KILL (only commits
// kill), a mechanical set-test on the control plane (never parses payload).
if (r.n_contra) {
    for (auto& b : branches_) {
        if (b.state == BranchState::Killed) continue;
        bool hit = false;
        for (int i = 0; i < r.n_contra && !hit; ++i)
            if (dep_has(b, r.contra[i])) hit = true;
        if (hit) { b.state = BranchState::Killed; ++kills_;
                   emit_abort(b, r); }
    }
}
```

`fabric.h:203–212` — the soft pause, off a forming partial:

```cpp
bool publish(const Partial& p) {
    if (reflex_.size() >= kReflexCap) { ++reflex_drops_; return false; }   // rate cap = 64 (241)
    reflex_.push_back(p);
    if (p.about_rev) {
        for (auto& b : branches_)
            if (b.state == BranchState::Running && dep_has(b, p.about_rev))
                b.state = BranchState::Paused;   // soft-abort (pause)
    }
    return true;
}
```

`BranchState` (`fabric.h:90`): `Running | Paused | Killed | Committed`.
`open_branch(lane, dep[], n)` at `fabric.h:184–191`, `kMaxDep = 8` (`fabric.h:56`).

**The law is `Paused ≠ Killed`.** Quoted in `C:/NEW/BLACKBOX_QC-TOTALITY_FABLE5_2026-08-17_ADDENDUM-B.md:50`:
> **L-COMMIT: "forming evidence may pause a thought; only commits kill it"**

`fabric.h:8–9` states the serializer's whole job in one sentence: it *"orders, stamps author,
canonical(), chains, op_id-dedups, commits, runs the contradicts∩dep hard-abort, pushes the reflex
head-advance."* `fabric.h:274–275` adds the crucial constraint: **"a mechanical set-test on the
control plane (never parses payload)."** No content inspection, ever. That is what makes it a
serializer rule and not manners — nib CLAUDE.md rule 4.

## 2.3 The KV handling on abort — exactly what is called

**Every single abort in the estate is one line.** There is no partial rollback, no rewind, no
position bookkeeping:

| site | line | call |
|---|---|---|
| syncytium, killed branch | `syncytium.cpp:246` | `llama_memory_seq_rm(mem, m.seq, -1, -1);` |
| starship, spine kill | `starship.cpp:577` | `llama_memory_seq_rm(mem, m.seq, -1, -1);` |
| starship, self-falter | `starship.cpp:772` | `llama_memory_seq_rm(mem, m.seq, -1, -1);` |
| blackbox, floor taken | `blackbox.cpp:1129` | `llama_memory_seq_rm(mem, seat_seq(cur), -1, -1);` |
| blackbox, empty gen | `blackbox.cpp:1478` | same |
| blackbox, dampened | `blackbox.cpp:1504` | same |
| blackbox, normal commit | `blackbox.cpp:1518` | same |
| fusord, after every emission | `fusord.cpp:504` | `llama_memory_seq_rm(mem, GEN, -1, -1);` |
| cortex | `cortex_llama.cpp:607` | `llama_memory_seq_rm(mem_, kSeqSpeak, -1, -1);` |
| m0_demo, the measured one | `m0_demo.cpp:179` | `llama_memory_seq_rm(mem, SPK, -1, -1);` |

**`(-1, -1)` = the whole sequence.** The fork side is always the mirror pair,
`seq_rm(branch)` then `seq_cp(TRUNK → branch)` — `syncytium.cpp:210–211`, `starship.cpp:467–468`,
`blackbox.cpp:983–984`, `fusord.cpp:475–476`, `fusord.cpp:462–463` (probe).

`seq_cp` is the **0 MiB fork**: measured at `SESSION_HANDOFF.md:71` — *"`seq_cp` fork of 3 branches
over an 8k trunk = **0 MiB** (pointer-share, not copy)"* — and `SYNCYTIUM_M0_LOCAL.md:35`, gated on
`kv_unified = true` ⇒ `llama_n_ctx_seq == llama_n_ctx`, asserted at `fusord.cpp:279–283` and
already at nib `resident.cpp:242–245`.

**The invariant that makes it safe: the trunk was never dirtied.** `m0_demo.cpp:182–183` says it
outright: *"[trunk] seq-0 frontier still %d tokens — the wrong sentence NEVER entered the living
state."* And `cortex.h:26–28`: *"on abort the branch is discarded (seq 0 was never dirtied) and the
caller may immediately re-speak from the updated state."*

**This is the entire mechanical basis for nib SPEC 6.4.1's "never committed, never saved."** The
forming text lives on a KV branch and in a render buffer. Neither is the document. Dropping the
branch and clearing the render buffer is the whole un-say. There is nothing to roll back because
nothing was ever written.

## 2.4 The abort record — what is written down

`fabric.h:290–302`, `emit_abort` — **the serializer authors the abort rev itself**, on the killed
branch's own lane, citing the trigger:

```cpp
Rev a{}; a.type = RevType::Abort; a.op_id = 0;
std::strncpy(a.lane, lanes_[b.lane].name, kLane - 1);
a.n_dep = b.n_dep; for (int i = 0; i < b.n_dep; ++i) a.dep[i] = b.dep[i];   // what it stood on
a.n_contra = 1; a.contra[0] = trigger.rev_id;                              // what killed it
const char* msg = "BRANCH_KILLED by contradicts-intersect";                // why it died
… a.prev_hash = head_; a.hash = rev_hash(a); a.sig = rev_sign(a);
tape_.push_back(a); head_ = a.hash;
```

**Note what `emit_abort` does NOT carry: the formed text.** The abort rev names the branch, the
deps and the trigger — never the words. nib SPEC 6.4.2 requires *"what was formed, what reached the
air, and why it died."* The estate carries those on the **trace**, not the fabric tape:

- **`starship.cpp:580`** — `{"k":"abort","t":N,"mind":"NAME"}` (spine)
- **`starship.cpp:777–778`** — `{"k":"abort","t":N,"mind":"HELM","reason":"selffalter"}`
- **`blackbox.cpp:1134–1135`** — the closest to what nib needs:
  ```
  {"k":"abort","mind":"NAME","reason":"floor_taken","heard":"<the prefix that reached the air>"}
  ```
  followed by `commit_to_tape(lane, "BRANCH_KILLED floor_taken")` at `blackbox.cpp:1136` —
  **two records, one on the trace with the words, one on the hash-chained tape without them.**
- **`starship.cpp:524–553`**, `cut_audio` — the fullest "what reached the air" record in the
  estate, and the model for nib's: `ms_to_silence` · `played_ms` · `word` (the word being spoken
  at the cut) · `heard` (the spoken prefix) · **`mid_word`** (bool) · **`live_mind`** ·
  **`live_mouth`** · `queued_ms`. Lines 526–535 compute `mid_word` honestly by comparing the
  spoken prefix's tail against the word being synthesized, and 533–535 will *say so* when the cut
  landed on a word boundary instead: *"on a WORD BOUNDARY — the mid-word claim does NOT hold this
  run"*. Lines 539–543 assert the branch was genuinely mid-decode and the mouth genuinely
  mid-utterance — *"(not a queued artifact)"*.

**nib's Stage 3 record should be `cut_audio`'s shape with the mouth replaced by the pad:**
`formed` (the whole forming string) · `reached_air` (the prefix actually rendered to the screen —
in a pad these differ only if rendering lags decoding) · `mid_word` · `live` (was the branch
mid-decode) · `reason` · `trigger` (which changeset/rev killed it) · `dep` (which block the forming
text stood on) · `ms_to_withdraw`.

## 2.5 THE TRIGGER: what may kill a forming sentence — and what may NOT

**`FUSOR_HARNESS_SPEC.md:123` (§5 item 7) is a standing prohibition and it is directly binding on
nib Stage 3.** Quoted in full:

> **The nerve** *(exists — tapped, and DELIBERATELY NOT WIRED TO ANY GATE)* — per-token −logP off
> the frontier. **v1: log it; gate on nothing.** ⚠ **Measured 2026-08-12 (VAL control battery):
> unconditional surprisal ranks NOVELTY, not relevance, and is INVERTED — the irrelevant control
> (7.54) outscored the real contradiction (6.91), and the most inert percept scored highest.** No
> threshold separates them, so **recalibration is dead as a fix — do not retry it.** Consequently:
> **no salience gating on surprisal, no surprisal triage, and the falter is not the catch** where a
> deterministic mechanism exists (the spine's `contradicts∩dep`, or at a fence the floor-severity +
> lane-arrival delta). The falter stays **measured, logged and disclosed**
> (`falter_suppressed` / `falter_state`) — a negative result kept visible, not deleted.

The code obeys it. `starship.cpp:758–768` — when the falter clears the bar on a scene where it is
not validated, it is logged as `{"k":"falter_suppressed"}` and the branch **survives**:

```cpp
if (peak > SURP_THRESH && !S.falter_primary) {
    // MEASURED, DISCLOSED, NOT ACTED ON. On this scene the falter does not
    // discriminate relevance (rung D, the irrelevant control, outscored the
    // real contradiction), so the catch belongs to the mechanical spine,
    // which scored 5/5 blinded. Logging keeps the negative result visible.
```

`blackbox.cpp:1176–1180` does the same thing unconditionally — *"measured, logged, NOT the catch
(falter_primary=false; the probes decide)"*. The surprisal read itself
(`sm_stats`, `starship.cpp:108–113`; `blackbox.cpp:598–604`) with the **header/content split**
(`starship.cpp:706–720`: the `"\n[world] "` lane header scored 18.4 nats identically on every rung
and would have made the ladder look like it fired when it had not discriminated at all) is worth
lifting **as an instrument**. Never as a gate.

**So: what is nib's `contradicts ∩ dep`?**

The pad hands it to you for free. The resident's forming emission is a branch whose **`dep` is the
block (or blocks) it is about** — the block the human most recently completed, per nib
BLUEPRINT §5. A human keystroke landing in a block in that `dep` set is a `contra` on that rev.
`fabric.h`'s test is then literal, mechanical, payload-blind, and already written:

```
new changeset touches block B  →  rev with contra = [rev_id(B)]
resident's forming branch has  dep = [rev_id(B)]
                               ⇒  contra ∩ dep ≠ ∅  ⇒  KILL
```

This is *the same test* as the floor rule in §6 — the floor rule refuses an emission **before it
is composed**; `contradicts ∩ dep` kills one **after it started forming**. Same set, two moments.
Building them from one `dep`/`contra` set is how nib gets Stage 2 and Stage 3 out of one mechanism.

**Late percepts (Act II) attach to the same path** — nib BLUEPRINT §7: *"on a healed partition,
edits arrive from the past. A resident that already emitted on an incomplete view must withdraw —
so partition-heal is wired into the abort path deliberately rather than discovered there."*

## 2.6 The "13 microseconds" — exact provenance, and what it is NOT

nib BLUEPRINT §6 says: *"a sentence killed mid-word when the world contradicts it, in 13
microseconds, with the killed words on the tape — while the turn-based twin on identical weights
missed the same moment by 65.7 seconds."*

**The 13 µs is `m0_demo.cpp:177–181`, and it is the wall time of one `llama_memory_seq_rm` call:**

```cpp
auto t_ab = clk::now();
llama_memory_seq_rm(mem, SPK, -1, -1);   // KILL — the only cost is this
const double ab_ms = ms_since(t_ab);
std::printf("[ABORT] the Skeptic's contradiction killed the Speaker at token %d "
            "(seq_rm = %.3f ms).\n", spn, ab_ms);
```

Doc citations: `FUSOR-1_ORGAN_RECOVERY_KERNEL_SPEC.md:50` (*"`seq_rm` = 13 µs to un-say a branch"*),
`STARSHIP_DEMO_SCRIPT.md:263` (*"the cost of un-saying a forming branch (measured,
`m0-4-crossabort`)"*), `IT_WAS_ALWAYS_NEXT_TOKEN_2026-08-13.md:78`.

**What it is not.** It is not the latency from "the world arrived" to "the characters disappeared."
It is not the decision latency. `m0_demo.cpp:171–173` discloses that the kill *instant* in that
demo is illustrative, not derived:

> *In the live engine the kill lands the instant the Skeptic commits its contradicts-rev; here that
> instant is illustrated at a few tokens in, at the same ≤1-micro-batch between-burst granularity.*

The honest end-to-end numbers in the estate are **milliseconds**, not microseconds:
`starship.cpp:924–926` reports `tel_commit_ms - helm_falter_ms`; `C:/NEW/FUSOR_COMPETITIVE-THESIS_FABLE5_2026-08-20.md:12`
quotes *"1 ms to silence"* for the un-said sentence. **nib must quote its own measured
keystroke→withdrawn number and must not inherit 13 µs**, which for nib would additionally have to
include a Win32 repaint. Report `seq_rm` separately if you want it, labelled as the branch-drop
cost only.

## 2.7 The abort's second beat: the re-form, and the BEAT ASSERTION

**`starship.cpp:893–915`** — after a stop, the speaker **re-forms real prose on the corrected
world**, on a fresh branch `REFORM = 6`, 48-token cap, same sampler:

```cpp
const llama_seq_id REFORM = 6;
llama_memory_seq_rm(mem, REFORM, -1, -1);
llama_memory_seq_cp(mem, TRUNK, REFORM, -1, -1);
std::string rc = std::string("\n") + S.fault_line +
    "\n<|im_end|>\n<|im_start|>user\n" + S.reform_cue + "<|im_end|>\n"
    "<|im_start|>assistant\n<think>\n\n</think>\n\n";
… for (int g = 0; g < 48 && !llama_vocab_is_eog(vocab, rtok); ++g) { … }
llama_memory_seq_rm(mem, REFORM, -1, -1);
```

The comment at 874–876 records the bug that made this necessary: the re-form used to be gated on
`helm_faltered` alone, so when a scene moved to `falter_primary = false` the speaker was cut and
then **said nothing**, silently deleting the demo's second beat. **The re-form follows the STOP,
not the mechanism.**

**`starship.cpp:929–981` — the BEAT ASSERTION. Lift this pattern wholesale into nib's `--selftest`.**
The run asserts its own narrative against its own trace, and fails the exit code if the story and
the record disagree:

```cpp
if (stopped_any) {
    if (!trace_has("abort"))   { …"a stop occurred but the trace carries NO abort event."… }
    if (!trace_has("reform"))  { …"the demo's second beat is dead."… }
    if (voice_on && !trace_has("audiocut")) { …"the mind stopped and the mouth did not."… }
} else {
    // THE CONTROL'S ASSERTION IS THE MIRROR IMAGE …
    if (spk_abort)             { …"the control is not a control."… }
    if (trace_has("reform"))   { …"nothing stopped, yet the trace carries a reform event."… }
    if (trace_has("audiocut")) { …"the mouth was cut with no stop behind it."… }
}
…
if (beat_fail) return 2;   // fail loudly AND in the exit code (999)
```

And the structural law that generated it, `starship.cpp:828–832`:

> *§5.8 STRUCTURAL FIX · ONE source of truth; the prose is DERIVED from it. Both narration bugs were
> hand-written sentences duplicating state that already lived in a variable — so the sentence could
> (and did) contradict the measurement it described. The class disappears when the trace field and
> the prose are computed from the SAME expression. **Never write a summary line alongside a field;
> derive it from the field.***

This is exactly nib's Stage 3 falsifier ("*a killed sentence that is not on the tape, or that
leaves residue in the buffer*") made mechanical. nib's status line and `--about` output must be
derived from the tape rows, never written beside them.

## 2.8 Three seats on one trunk: the batch layout, and whether nib can start with one

**`syncytium.cpp:299–330` is the co-decode.** The layout, verbatim:

```cpp
llama_batch b = llama_batch_init(nact, 0, 1);
b.n_tokens = nact;
for (int k = 0; k < nact; ++k) {
    Mind& mm = minds[act[k]];
    b.token[k] = mm.pending; b.pos[k] = mm.pos;               // each seat its OWN position
    b.n_seq_id[k] = 1; b.seq_id[k][0] = (llama_seq_id)mm.seq; // each its OWN sequence
    b.logits[k] = 1;                                          // logits for ALL of them
}
llama_decode(ctx, b);
for (int k = 0; k < nact; ++k) {
    Mind& mm = minds[act[k]];
    ++mm.pos; ++mm.toks;
    mm.pending = llama_sampler_sample(smp, ctx, k);            // sample by BATCH INDEX k
}
llama_batch_free(b);
```

The three points that matter: **one token per active seat per batch**, each carrying its own
`pos` and `seq_id`; **`logits[k] = 1` for every row**; and **`llama_sampler_sample(smp, ctx, k)`
indexed by batch position** — not `-1`. `syncytium.cpp:304–311` keeps the sequential path
(`--seq` / `force_seq`) as the A/B baseline, and it uses `sample(smp, ctx, -1)` because there is
only one row.

Measured cost: **1.208×** for three seats vs one, worst case with all three *speaking*
(`SESSION_HANDOFF.md:71`, `SYNCYTIUM_M0_LOCAL.md:37`, `521–522`). `SESSION_HANDOFF.md:90` is the
honesty note nib must carry if it ever quotes the number:

> *The 1.2× is real but **bimodal**: ~1.05–1.15× for the product shape (1 Speaker + 2 probers),
> ~1.3–1.5× for 3 sustained concurrent speaks… Measured worst-case (3 speaks) = **1.208×**.*

`THE_RESIDUAL_2026-08-15.md:48` records that quoting it flat was *ordered retired* and is
*"printed flat on eight public pages."* Do not add a ninth.

**Can nib start with one seat and keep the layout? Yes — and the estate's own code shows how.**
`fusord.cpp` already probes three seats but generates on **one shared `GEN` sequence, serially**
(`fusord.cpp:472–504`), because at most one line is being composed at a time. nib's Act I has
exactly that shape: three probes, at most one composition. So:

- keep `seat_seq(i) = i + 1` (`blackbox.cpp:934–935`, the "starship layout") as the *addressing*
  scheme from the start, even with one live composer;
- write `dec()` so it takes `seq_id` and `pos` per token from the outset — nib's
  `resident.cpp:95–111` already does;
- when a second composer arrives (Act III), the only change is batching N pending tokens into one
  `llama_batch` instead of N calls, i.e. `syncytium.cpp:312–327` dropped in. Nothing above it moves.

`kv_unified = true` and the `n_ctx_seq == n_ctx` assertion are the precondition, and nib already
has both (`resident.cpp:235`, `242–245`).

---

# 3 · THE TWIN RACE (Stage 4)

## 3.1 The twin is `blackbox.cpp --twin`. `starship.cpp`'s twin is a *canned string*.

This distinction matters and nib's docs blur it.

**`starship.cpp`'s twin is not a model run.** It is a scenario field, `S.twin_line`
(`starship.cpp:137`, used at `497–498` and `921–923`). The comment at `starship.cpp:14–16` is
honest about it — *"a turn-based ship answers on the query using the PRE-FAULT snapshot and COMMITS
the false all-clear"* — but the string is authored, not generated. **It is a narration device, not
a control arm.** Do not lift it.

**`blackbox.cpp --twin` is the real thing** (`blackbox.cpp:754`, `777`, `1242–1354`).

## 3.2 What `twin_turn` actually does — `blackbox.cpp:1243–1354`

```cpp
auto twin_turn = [&]() {
    ++twin_turns; tw_turn_sims.push_back(sim_now());
    auto& m = w.minds[w.speaker_ix];
    std::string prompt = w.seed_sys;                       // 1247  SAME system seed
    for (auto& l : twin_ts) prompt += l + "\n";            // 1248  the FULL visible transcript
    prompt += "<|im_end|>\n<|im_start|>user\nYou are " + m.name + ". " + m.role +
              ". Reply to the visitor - one sentence, no preamble.<|im_end|>\n"
              "<|im_start|>assistant\n<think>\n\n</think>\n\n";   // 1249–1251
    auto pt = tk(vocab, prompt, true);
    llama_memory_seq_rm(mem, 1, -1, -1);                   // 1253  drop everything
    dec(ctx, pt, 1, 0, true);                              // 1254  FULL RE-PREFILL from pos 0
    …
    while (!llama_vocab_is_eog(vocab, pend) && toks < 64) {        // 1262  64-token cap
        … dec(ctx, one, 1, tp, true); ++tp; ++toks;
        sim_charge_tok(earliest_due());                            // 1270
        pend = sample1();
        // world events land on the TAPE at true time — and CANNOT inform this reply.  1272
        while (score_ix < score.size() && score[score_ix].at_ms <= s2) { … ++twin_blind_ct; … }
    }
    …
    twin_ts.push_back("[" + m.name + "] " + utt);          // 1348
    for (auto& p2 : twin_pending) twin_ts.push_back(p2);   // 1349  stale events visible only NOW
```

**Held IDENTICAL across the arms** (this is the list nib must copy):

| held identical | evidence |
|---|---|
| weights / model path | one binary, `--twin` flag only; `blackbox.cpp:858` records `model` on both |
| **the system seed** | `w.seed_sys`, same object, `blackbox.cpp:894` (resident) vs `1247` (twin) |
| the deck + score + their sha256 | `blackbox.cpp:345`, `458`, printed **before any model bytes load** (`814–821`) |
| the sampler seed | `--seed`, `blackbox.cpp:893`, one chain built once |
| the sampler chain | min-p 0.05 → temp 0.7 → dist(seed), `blackbox.cpp:890–893` |
| **the seat's name and role** | `w.minds[w.speaker_ix]`, same struct |
| the token cap | 64 both arms (`1452` resident, `1262` twin) |
| **the world's schedule** | virtual sim clock; both arms see events at the *identical* `sim_ms` |
| **the tick** (silence-as-world) | `blackbox.cpp:1603` — `if (twin) twin_ts.push_back(line); else commit_to_trunk(line);` |
| the empty-generation guard | `1335–1342` (twin) mirrors `1471–1482` (resident) |
| the lane-tag strip | `1325–1330` (twin) mirrors `1459–1468` (resident) |
| the trigger-fire pass | `1292` `fire_triggers_after_commit(...)` mirrors the resident's |

**Differs BY CONSTRUCTION — the single independent variable:**

| differs | resident | twin |
|---|---|---|
| **the trigger** | a boundary round on any commit → probe → `margin > dial` | a visitor say-card, and nothing else |
| context maintenance | incremental decode onto `TRUNK`, never re-read | **full re-prefill from position 0 every wake** (`1253–1254`) |
| probes / dials / damper | present | **absent — recorded as `null`, never a number it never used** |
| spontaneous output | possible | **0 by construction** (`blackbox.cpp:1657`) |
| mid-reply world | interrupts (§6) | **invisible until the next wake** (`twin_pending`, `1285`, `1349`) |

`blackbox.cpp:836–838`, the banner:

> `ARM: TURN-BASED TWIN (QC-003) - same weights/score/deck/seed; exists only at visitor turns;`
> `full re-prefill per wake; no seats, no probes, no damper.`

## 3.3 The paired record — the exact row

`blackbox.cpp:840–861`, `"k":"run"`, one per arm, and **QC-005 per-arm config honesty**
(`848–850`) is the discipline nib must copy verbatim:

> *QC-005: per-arm config honesty. The twin has no probes, so dials and the damper do not exist on
> that arm — it records `null`, never a number it never used. **An A/B pair's two headers must never
> appear to disagree about a knob only one arm has.***

Fields: `engine_rev` · `spec` · `family` · `score` · `seed` · (`dial` **or** `dial_scalar_used:false`) ·
`bar` · `sim_ms_per_tok` · `probe_time:"uncharged"` · **`arm`** (`"twin"`|`"resident"`) ·
**`dials`** (array or `null`) · **`damper`** (bool or `null`) · `twin` (bool) · `dry` · `fast` ·
`sim_rate` · `model`. Plus `"k":"world"`/`"k":"deck"` with sha256 (`859–861`) and `"k":"mind"` per
seat (`862–863`), and at the end `"k":"sampler","consulted":N` (`1638–1639`) — *"the tape
self-discloses whether randomness was consumed — a sweep over an unconsulted run is vacuous by
record."*

The output path itself carries the config, `blackbox.cpp:1726–1739`:
> *§9 law: a sweep never overwrites its evidence — the config is part of the path.*
`runs/blackbox_<family>_<score>_<deck>_s<seed>[_twin][_K±R±N±][_nd][_dry]_<runtag>.json`, and
`1734–1737` stamps a UTC run tag so *"a run may never overwrite a run."*

## 3.4 The 65.7 seconds — exact provenance

**`C:/auricle/runs/cabv2_twin_console.txt:114`:**

```
   DOOR EVENT at sim 219.3s -> earliest possible twin reaction: next visitor turn, +65.7s
```

Produced by **`blackbox.cpp:1659–1675`** (the F-ZOMBIE-text gate, QC-003). The computation, exactly:

```cpp
for (auto& wv : tw_world) {
    if (wv.second != "handle_zone_enter") continue;      // only the door event
    long nxt = -1;
    for (long t2 : tw_turn_sims) if (t2 >= wv.first) { nxt = t2; break; }   // next twin wake
    if (nxt < 0) …"twin reaction: NEVER"…
    else …"earliest possible twin reaction: next visitor turn, +%.1fs", (nxt - wv.first)/1000.0
```

**Read that carefully. It is:**
- an **"earliest possible"** number — the gap to the twin's next scheduled wake, **not an observed
  reaction**. The twin never reacted to the door at all;
- in **sim time under a virtual clock** (`blackbox.cpp:77–107`; `--fast`/`--dry` set
  `g_virtual_clock`), with generation charged at `kSimMsPerTok = 17` (`blackbox.cpp:93`, *"lock:
  clock.sim_ms_per_tok [PICKED from 59 tok/s MEASURED]"*). `blackbox.cpp:76` is explicit: *"Latency
  claims were already void under `--fast`"*;
- for **one deck, one score, one seed, one event** (n = 1);
- **between two separate runs**, not within one — you run the binary twice, once with `--twin`;
- and `blackbox.cpp:1667` and `1672–1673` carry a self-disclosure: *"a +0.0s here is deck
  AUTHORING, not the access pattern"* — the twin's wake schedule is set by the deck's visitor
  cards, so the number is a property of the deck as much as of the architecture.

`C:/auricle/UNPROMPTED_SHARPENING_v1-DIRECTION_FABLE5_2026-08-17.md:42` names the policy this
measures — **`on-address`**: *"invoke only when a CHAT delta addresses the SUT (the
VAD-endpoint/turn-based twin, benchmark-shaped)… the twin's earliest possible reaction to the
flagship event was +65.7 seconds, on the identical stream."*

## 3.5 The exact discipline nib must copy so a flip is a valid paired sample

nib's Stage 4 is **stronger in one way and weaker in another** than blackbox's.

**Stronger:** nib's toggle produces a *within-session* pair on the *same real stream*, one per
flip, during ordinary work. blackbox needs two runs of a scripted deck.

**Weaker — and this is the trap:** blackbox's pairing rests on **matched input**. The deck and
score are byte-identical across arms, sha256'd before any model byte loads (`blackbox.cpp:814–821`),
and every world line is forked to both arms at the identical sim ms. nib has **no matched input at
all** — the human types different things in the two modes, and *the mode changes what the human
types*. A nib pair is therefore an observational pair, not a controlled one.

### The ratified doctrine — FUSOR_DECISIONS D1 · Twin parity (status: DOCTRINE)

`C:/auricle/FUSOR_DECISIONS_2026-08-12.md:7–14` is the authoritative list, and it is shorter and
sharper than blackbox's implementation:

> *The twin inherits **every non-residency guard in identical code** and differs from the resident
> in **exactly four treatments, named so they cannot drift**:
> **1. decode-on-delta · 2. persistent KV · 3. mid-decode abort · 4. boundary-grain judgment.**
> Everything else is common ground and **byte-identical — explicitly including the seed** (SEED_SYS,
> the six worked examples, the three mandates). **The seed is the largest scaffold in the live path
> and will *feel* like "the resident's prompt" — that is exactly how it would silently diverge.**
> Guard-firing rates reported per arm per hour; **a guard that never fires in the twin's arm is a
> RESULT, never a reason to remove it.***

The trap it exists to prevent, `C:/auricle/OPEN_PROBLEMS_2026-08-12.md:92–94`:
> ***Do you give the twin the same repeat-suppressor, the same generation cap, the same
> coarsening?** If not, the comparison is a polished resident against a naive fake, and the win
> means nothing.*

And `C:/auricle/BLACKBOX_SPEC_v0.1_QC-001.md:36`:
> ***A control arm may never be quietly weakened; that is worse than an unfair advantage.***

**nib's Stage 4 is a two-treatment toggle, not a four-treatment one.** nib's ROADMAP says *"only
the trigger may differ"* — treatment 4 (boundary-grain judgment) — and SPEC 6.1.2 holds seat, seed,
sampler and mandate identical. That is a *narrower* experiment than D1's, which also varies
decode-on-delta, persistent KV and mid-decode abort. **Both are valid; they answer different
questions, and nib must say which it is running.** The D1 twin answers "is residency worth it";
nib's narrow twin answers "is the *trigger* worth it, holding context maintenance constant".
Running the narrow one and citing D1's framing would be the drift D1 was written to stop.

Two more rules that bind either version —
`C:/auricle/UNPROMPTED_BENCH_SPEC_v0_2026-08-14.md:348–350`, `264–272`:
> ***Matched compute requires matched weights.*** *The polling arms and the resident arm must run
> **the same model weights**, or the comparison silently becomes model-vs-model and the
> architectural claim evaporates.*
> ***Charge everything. No exemptions.*** *`cost/hr` is total tokens, total joules, and total
> wall-seconds measured **from process start to process end**, including idle residency, KV
> maintenance, every ingest token, and every probe — whether or not the system emitted anything.
> … **Any exemption is where the paper dies.***

And the thesis-level rule, `C:/auricle/IT_WAS_ALWAYS_NEXT_TOKEN_2026-08-13.md:244`:
> **F-PRESENCE** *(the resident vs the maximally-good turn-based twin, blind-graded, **the twin
> never retired**)* … ***No organ outlives its null; neither does the thesis.***

**The discipline, as a checklist:**

1. **One code path, one set of constants, forked at the trigger only.** Build the seat, seed,
   sampler chain, cue, cap and stop conditions once; branch on `mode` at exactly one place —
   "does a boundary probe fire the composer, or does a keystroke?" If any second `if (turn_based)`
   appears anywhere, the toggle has become a preference (nib ROADMAP Stage 4 falsifier).
2. **Record `null`, never a stale number** (QC-005). In TURN-BASED there is no probe, so
   `margin`, `bscore` and `reason` are `null` on that arm's rows — not `0`, not the last
   resident value.
3. **Both modes must receive the tick.** `blackbox.cpp:1599–1603` records that this was the *one*
   world-line site that missed the arm fork, *"quietly handicapping the control arm and making the
   'reassembles the FULL context' claim false."* In nib the tick is produced in the compiler
   (`ingest.h:63–65`), so this is nearly free — but the turn-based arm must still *see* it.
4. **The twin must re-prefill.** If nib's TURN-BASED arm keeps the incremental trunk, it is not a
   turn-based twin; it is the resident with a different trigger, and the interesting variable
   (context maintenance) has silently been held constant. `blackbox.cpp:1253–1254` is the
   reference. State plainly which of "trigger only" and "trigger + prefill discipline" nib is
   testing — they are different experiments and the estate tests the second.
5. **Mid-composition world must be recorded as blind on the turn arm.** `blackbox.cpp:1272–1293`
   lands the events on the tape at true time and defers them to `twin_pending`; the counter
   `twin_blind_ct` is the product claim in one integer.
6. **Charge generation time on both arms.** `sim_charge_tok` (`blackbox.cpp:102–107`, called at
   `1270` and `1534`). nib runs on a real clock so this is automatic — but nib must then **never**
   quote a latency taken under any accelerated mode.
7. **Every flip goes on the tape with its timestamp** (nib SPEC 6.1.3), and the arm identity rides
   **every subsequent row**, not only the switch row — otherwise a partial tape cannot be attributed.
8. **Count the sampler.** `blackbox.cpp:899–902`, `1638–1639`. If the turn arm's
   `sampler_consulted` is 0 for a stretch, that stretch is vacuous.
9. **Path-tag the record by config** (`blackbox.cpp:1726–1739`) so a session never overwrites its
   own evidence.
10. **Do not quote 65.7 s as nib's number.** It is blackbox's, on a deck, at n = 1, in sim time,
    as an "earliest possible". nib's Stage 4 exists to produce a *real* distribution; say so.

---

# 4 · THE MOLT / FOLD

## 4.1 The mechanism — `fusord.cpp:599–650`

```cpp
auto do_molt = [&]() {
    const uint64_t m0 = wall_ms();
    const llama_pos before = npast;
    llama_memory_seq_rm(mem, SCRIBE, -1, -1);                        // 603
    llama_memory_seq_cp(mem, TRUNK, SCRIBE, -1, -1);                 // 604   0 MiB fork
    const std::string cue =
        "\n<|im_end|>\n<|im_start|>user\n[The stream has been going a long while and "
        "the log is huge. As the room's keeper, write your running memory now: every "
        "decision, hard constraint, owner, deadline, established fact, and open thread "
        "that still matters for the work ahead. Drop the chatter. End with one line "
        "noting what you dropped.]\n<|im_end|>\n<|im_start|>assistant\n<think>\n\n"
        "</think>\n\n";                                              // 605–611
    … dec(ctx, ct, SCRIBE, npast, true);
    for (int t = 0; t < 600; ++t) {                                  // 616   600-token rung cap
        const llama_token tok = llama_sampler_sample(smp_scribe, ctx, -1);   // temp 0.3
        if (llama_vocab_is_eog(vocab, tok)) break;
        … dec(ctx, one, SCRIBE, spos, true); ++spos; ++rtoks;
    }
    llama_memory_seq_rm(mem, SCRIBE, -1, -1);                        // 624
    llama_memory_seq_rm(mem, TRUNK, -1, -1);                         // 625   THE TRUNK IS DROPPED
    std::string tail_text;
    for (auto& p : tailq) tail_text += p.first + "\n";               // 627
    if (!cur_tail_line.empty()) tail_text += cur_tail_line + "\n";   // 628
    const std::string reseed = std::string(SEED_SYS) + SEED_EXAMPLES + SEED_OPEN +
        "[memory] " + rung + "\n[recent, verbatim]\n" + tail_text;   // 629–630
    auto rt = tk(vocab, reseed, true);
    dec(ctx, rt, TRUNK, 0, true);                                    // 632   rebase at pos 0
    npast = (llama_pos)rt.size();
    read_frontier();                                                 // 634
    … "\n[tick +%llus — I paused the watch to consolidate my memory]"  // 636–638
    dec(ctx, tt, TRUNK, npast, true); npast += …; read_frontier();   // 640–641
```

**What survives a fold, in order:**
1. `SEED_SYS` + `SEED_EXAMPLES` + `SEED_OPEN` — **the frozen serve bytes, re-decoded verbatim.**
   The molt is the one place where `train ≡ serve` is re-asserted at runtime.
2. `"[memory] "` + the rung — up to **600 tokens** authored by the model itself at temp **0.3**,
   on a 0-MiB fork of its own trunk.
3. `"\n[recent, verbatim]\n"` + the tail — the last **600 words** (`TAIL_MAX_WORDS`,
   `fusord.cpp:341`), maintained by `tail_push_line` (`fusord.cpp:355–361`), which **includes the
   resident's own emissions** (pushed at `fusord.cpp:560`) and the idle ticks (`fusord.cpp:716`).
4. Then a **`[tick +Ns — I paused the watch to consolidate my memory]`** so the outage is itself
   perceived — *"the outage is a perceivable drop-event"* (`fusord.cpp:599`).

**Everything else is gone.** No KV is retained; `seq_rm(TRUNK, -1, -1)` at 625 drops the whole
thing and 632 re-prefills from position 0.

**Is the fold on the tape? Yes** — `k=molt`, `fusord.cpp:645–648`, carrying `before` · `rung_toks` ·
`after` · `dur_ms` · **the full `rung` text**. That is the strongest tape row in the estate: a
reader can reconstruct exactly what the mind chose to remember.

**Trigger:** `fusord.cpp:685–688` — `npast >= molt_wm` checked after every ingested word, with the
open clause force-judged first (`reason "m"`) so no thought is lost across the fold. Default
`molt_wm = 24576` against `n_ctx = 65536` (`fusord.cpp:198`, `271`) — i.e. **the watermark is 37 %
of the window**, not 90 %. `fusord.cpp:200–202` explains the q8_0 KV choice as *"so no molt fires
in the maiden hour."*

## 4.2 The provenance — `m0_molt.cpp`

`m0_molt.cpp:1–18` is the original: *"compaction is NOT naive summarization. The PERSONA ITSELF,
with its identity/convention in context, reads its own grown trunk and decides what matters —
compression AS comprehension, persona-shaped."* And `m0_molt.cpp:16`: *"Isomorphic / no hardcode:
the model does the compression on its own forward pass (no external summarizer), triggered by a
context watermark + its own EOG boundary (no schedule)."*

Its rebase (`m0_molt.cpp:190–201`) keeps **K = 3** recent lines verbatim (`m0_molt.cpp:191`) where
fusord keeps 600 words. Its needle test (`m0_molt.cpp:222–244`) is the falsifier shape: plant a
decision + a hard constraint + an owner early, bury them under the watermark, add a late update,
then query all three **and** a piece of chatter, and assert the needles survive while the chatter
is dropped.

`FUSOR_HARNESS_SPEC.md:122` (§5 item 6): *"**Molt** — watermark-at-boundary → scribe rung →
single-seq rebase. Runs longer than the window; outage = Deadline-Law drop-event."*
Measured fold ratio across the estate: **14.8×** (5,036 → 341 tokens, load-bearing facts surviving)
— `C:/NEW/COLD_SESSION_PROMPT_SYNTHESIS_2026-08-23.md:82`.

## 4.3 What nib must do at `n_ctx = 8192`

nib currently has **no molt at all**. `resident.cpp:313` and `339` simply *refuse to ingest* once
`npast_ + tokens >= n_ctx - 512`:

```cpp
if (npast_ + (long long)wt.size() >= cfg_.n_ctx - 512) return;   // Stage 1b does not molt yet
```

**That is a silent percept drop and it violates nib SPEC 5.1.4 / CLAUDE.md rule 7.** It is
acceptable in Stage 1b as a stated stub; it is not acceptable once the thing runs for a working
session. At 8192 with a real writing session it will be hit fast.

Concretely, at `n_ctx = 8192`:

1. **Scale the watermark, keep the ratio.** fusord's is 24576/65536 = 0.375. At 8192 that is
   **`molt_wm ≈ 3072`**. The reason the ratio is low, not 0.9: the rebase re-decodes
   seed + rung + tail, and the *scribe fork* runs on the grown trunk before the drop — you need
   headroom for a 600-token rung generated at `npast` ≈ watermark, plus the reseed.
2. **Scale the rung and tail together.** A 600-token rung + 600-word tail reseeds to roughly
   1100–1600 tokens on top of the ~430-token seed — a large fraction of 8192. Start at
   **rung ≤ 256 tokens, tail ≤ 200 words**, and *measure the reseed size*, don't assume it.
3. **The reseed must re-decode `SEED_SYS + SEED_EXAMPLES + SEED_OPEN` verbatim** (`fusord.cpp:629`).
   nib's `serve_hash` covers those literals, so the molt reuses the same constants; do not
   reconstruct the seed a second way.
4. **Force-judge the open clause before folding** (`fusord.cpp:686`), reason `"m"`.
5. **The resident's own emissions must be in the tail** (`fusord.cpp:560`) or the mind forgets it
   spoke across a fold and §1.9's ten-times-repeated catch comes back at every molt boundary.
6. **Emit the outage tick** (`fusord.cpp:636–641`) — nib's compiler owns ticks, so the resident
   must be able to inject this one directly, or the compiler must be told a fold happened.
7. **Put the fold on the tape** with the full rung text (`k=molt`), and **count the outage**. In a
   pad the human is *typing during the fold*; percepts must queue on the ring and be ingested after
   (ingest is unconditional), so the molt's real cost in nib is a **latency spike, not a gap** —
   measure it as such.
8. **Nib has a resource the estate did not: the document itself.** The rung need not be the only
   thing that survives, because the committed buffer is on disk and losslessly re-readable. A nib
   molt could reseed with `seed + [memory] rung + [document, verbatim tail]`. That is a genuine
   departure and should be marked as one, not smuggled in as a lift.

---

# 5 · THE TAPE

*(§5.1–5.4 below are filled from the dedicated family-format survey; §5.5 is the schema proposal.)*

## 5.1 What auricle actually has — three different things, none of them nib's tape

**`C:/auricle/src/tape/tape.h` is not a tape.** All 27 lines of it are a per-project memory seam
(`load_standing_context` / `append_final_state`), explicitly *"stub until P4"* (`tape.h:25`).
It has no hash, no chain, no rows. **Do not lift it. Do not cite it as the family tape.**

**`fabric.h` is the hash-chained tape, and its hash is a labelled stub.**
`fabric.h:11–14` says so in the file header:

> *Stub-grade primitives, honestly labelled: the hash is a **128-bit non-crypto mix** (blake2b is
> the production swap, §5.2); the "signature" is a keyed hash (Ed25519 in production); the store is
> in-memory (SQLite/WAL is the persistence swap behind the same append/get/head/verify interface).*

The hash, `fabric.h:38–50`, is **FNV-1a twice with different seeds, each finalized through a
Murmur3 64-bit mix**:

```cpp
inline uint64_t mix64(uint64_t x) {
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33; return x;
}
inline uint64_t fnv(const uint8_t* p, size_t n, uint64_t seed) {
    uint64_t h = seed;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return mix64(h);
}
inline Hash128 hash128(const uint8_t* p, size_t n) {
    return {fnv(p, n, 0xcbf29ce484222325ULL), fnv(p, n, 0x9e3779b97f4a7c15ULL)};
}
```

Two 64-bit FNV-1a passes over the same bytes with FNV-offset-basis and the golden-ratio constant as
seeds. Non-cryptographic, 128 bits, second-preimage-trivial. Fine for tamper *localization* in a
test lab; **not** fine for a record nib intends to publish.

**The canonical form is binary, not JSON.** `fabric.h:93–116`:

```cpp
struct ByteSink {                       // little-endian fixed-width, length-prefixed
    void u8 / u16 / u32 / u64 …
    void bytes(const void* p, size_t n) { u32((uint32_t)n); … }   // length prefix = injective
};
inline std::vector<uint8_t> canonical(const Rev& r) {
    s.u64(r.rev_id); s.u32(r.epoch); s.u16((uint16_t)r.type);
    s.bytes(r.lane, kLane); s.u64(r.op_id);
    s.u8(r.author_trust); s.u8(r.flush_reason);
    s.u8(r.n_dep);    for (…) s.u64(r.dep[i]);
    s.u8(r.n_contra); for (…) s.u64(r.contra[i]);
    s.u64((uint64_t)r.ts);
    s.u64(r.prev_hash.a); s.u64(r.prev_hash.b);
    s.bytes(r.payload, r.payload_len);
    return s.v;  // NOTE: excludes hash + sig (both derived)
}
```

**The chain rule**, `fabric.h:117–120` and `fabric.h:265–272`:
- `prev_hash` is a **field inside** the canonical bytes (line 113), so the chain is bound by the
  hash rather than concatenated outside it;
- `hash = hash128(canonical(r))` — canonical **excludes** `hash` and `sig` (line 115);
- genesis `prev` is a **zero-initialized `Hash128{}`**, i.e. `a = b = 0`
  (`fabric.h:174`, `durable.h:53`);
- `head_ = r.hash` after each commit (`fabric.h:272`).

**Verify**, `fabric.h:173–181` (and the identical `durable.h:51–59`):

```cpp
bool verify_chain(uint64_t* bad_rev_id) const {
    Hash128 prev{};
    for (const auto& r : tape_) {
        if (r.prev_hash != prev)   { *bad_rev_id = r.rev_id; return false; }
        if (rev_hash(r) != r.hash) { *bad_rev_id = r.rev_id; return false; }
        prev = r.hash;
    }
    return true;
}
```

Two independent checks per row — link continuity and self-consistency — and it **localizes** the
first bad row rather than returning a bare bool. `fabric.h:230–232` provides `corrupt_row()` as a
deliberate self-attack hook; `m0_fabric.cpp` exercises it.

**Durability** (`durable.h`): committed `Rev`s are appended as **raw fixed-size binary structs**
(`durable.h:29–38`, `sizeof(Rev)` per record), restored by `read()`ing them back
(`durable.h:43–49`), and re-verified. `durable.h:5–6` states the two-plane law: *"the reflex plane's
PARTIALS never touch it (S-PLANES: only committed finals persist; partials are ephemeral)."*
`m0_tape.cpp:97` asserts it: *"partials in the reloaded tape: none — the reflex plane never persisted."*

**`blackbox.cpp` is the only place in the estate with a real cryptographic hash** —
`sha256_hex` over BCrypt, `blackbox.cpp:152–177`. It hashes **input artifacts** (the world file and
the deck file, `blackbox.cpp:345`, `458`), printed **before any model byte loads**
(`blackbox.cpp:814–821`, F-FRESH). It does **not** hash rows and it does **not** chain. And
`blackbox.cpp:155–156` / `174` is the discipline nib should copy:

> *QC-008 §34: hashing is F-FRESH's substrate — if it is unavailable the run REFUSES (exit 5)
> rather than printing a placeholder string where a hash belongs.*

**The traces (`runs/*.json`) are not hash-chained at all.** `blackbox.cpp:1740–1757`,
`starship.cpp:988–995`, `syncytium.cpp:378–386` all write a plain JSON array of events. The
`"k":"tape"` rows inside them (`blackbox.cpp:1619–1622`) are a *rendering* of the fabric tape, with
`rev` and `lane` and `text` but **no `hash` and no `prev`** — the chain does not survive into the
published artifact.

## 5.2 The gap nib SPEC 8.1.1 is asserting into

nib SPEC 8.1.1 requires: *append-only and hash-chained in the family's format (BLAKE2b-256 over the
previous digest, a NUL, and the canonical JSON payload), so that `glance --verify` reads it.*

**Nothing in auricle produces that.** fabric's chain is 128-bit non-crypto over a length-prefixed
binary encoding with `prev` *inside* the hashed bytes; nib's spec calls for BLAKE2b-256 over
`prev_digest ‖ 0x00 ‖ canonical_json` with `prev` *outside*. These are different constructions and
neither can verify the other. The authority for the family format is therefore **glance/caseclock/
fray, not auricle** — see §5.3.

## 5.3 The real family format — verified, and nib SPEC 8.1.1 is CORRECT

**The normative source is not any C++ repo. It is `C:/REGISTRAR/core/tape.py`.** glance, caseclock
and fray are all ports of it and cite it by path. `C:/glance`, `C:/caseclock`, `C:/fray` and
`C:/facet` exist; `C:/vramtop` and `C:/everywho` do not exist at the root of `C:` (`everywho` lives
under `C:/Intellect_AI_tools/`), and neither `facet` nor `everywho` contains tape code.

### The hash

**BLAKE2b-256 (RFC 7693), unkeyed, unsalted, unpersonalized, 32-byte digest, lowercase hex.**
Hand-transcribed in every repo — **nothing is vendored** (no `blake2.c`, no `blake2b.h` anywhere).

`C:/REGISTRAR/core/tape.py:60–66` — the reference:

```python
def _digest(prev: str, payload: str) -> str:
    """blake2b over (previous digest, canonical payload). Canonical => reproducible."""
    h = hashlib.blake2b(digest_size=32)
    h.update(prev.encode("utf-8"))
    h.update(b"\x00")
    h.update(payload.encode("utf-8"))
    return h.hexdigest()
```

The parameter block is the plain one, `0x01010020` — `C:/glance/src/tape.cpp:54` and
`C:/fray/src/tape.cpp:54`: `h_[0] ^= 0x01010000ull ^ (uint64_t)outlen_;   // depth 1, fanout 1, no
key`; `C:/caseclock/src/hash.cpp:83`: `h[0] ^= 0x01010000ull ^ (0ull << 8) ^ 32ull;`. **No `h[4..7]`
XOR anywhere ⇒ no salt, no personalization.** Lowercase hex tables at
`C:/glance/src/tape.cpp:109–115`, `C:/fray/src/tape.cpp:109–115`,
`C:/caseclock/src/app_util.h:122–128`.

`C:/caseclock/src/hash.h:1–4` states the provenance discipline:
> *No vendored code: the BLAKE2b below is this repository's own transcription of the RFC, checked
> against hashlib's vectors in `tests/expected/hash-vectors.json` by `--selftest`.*

Those vectors are on disk (`C:/caseclock/tests/expected/hash-vectors.json`) and include the
127/128/129-byte cases, because the "keep the last block in the buffer, never compress it early"
rule is where transcriptions go wrong (`C:/caseclock/docs/devlog.md:57–63`;
`C:/glance/src/tape.cpp:84–88`, `99–107`).

SHA-256 exists in the family but **is never the chain** — caseclock uses BCrypt SHA-256 for file,
rule-set and sign-out hashes only (`C:/caseclock/src/hash.cpp:123–154`), exactly as
`blackbox.cpp:152–177` does for its inputs.

### The chain rule

```
digest = BLAKE2b-256( prev_digest_hex_ASCII ‖ 0x00 ‖ canonical({"at","body","kind","seq"}) )
```

- `prev` is the **previous row's 64-char lowercase hex string as ASCII text**, not raw bytes;
- the separator is a **single 0x00 byte**;
- **`prev` and `digest` are both excluded from the hashed payload** — `prev` enters as the leading
  chunk, `digest` is the output;
- **genesis `prev` = 64 ASCII `'0'`** — `C:/REGISTRAR/core/tape.py:56` (`GENESIS = "0" * 64`),
  `C:/caseclock/src/tape.h:23`, `C:/glance/src/tape.h:62`, `C:/fray/src/tape.h:71`.

`C:/glance/src/tape.cpp:189–203` (byte-identical at `C:/fray/src/tape.cpp:189–203`):

```cpp
std::string Tape::payload(uint64_t seq, const std::string& kind, int64_t at,
                          const std::string& body_canonical) {
    return canon::obj({ {"seq", canon::num((int64_t)seq)}, {"kind", canon::str(kind)},
                        {"at", canon::num(at)}, {"body", body_canonical} });
}
std::string Tape::digest(const std::string& prev, const std::string& pay) {
    Blake2b b; b.init(32);
    b.update(prev.data(), prev.size());
    const uint8_t z = 0; b.update(&z, 1);
    b.update(pay.data(), pay.size());
    uint8_t out[32]; b.final(out);
    return hex(out, 32);
}
```

Pinned cross-implementation vectors, `C:/glance/src/selftest.cpp:445–450`:
```cpp
check(p0 == "{\"at\":-20,\"body\":{\"c\":0.9,\"n\":3,\"ok\":true,\"text\":\"SYNTHETIC é — no PHI\"},\"kind\":\"note\",\"seq\":0}", "payload: the reference's bytes");
check(d0 == "404e7301fbe61b8cb50b671e3ebf2dad98635c8e78efe41f912f3ce6c7233194", "digest 0 equals REGISTRAR tape.py's");
```

### Canonical JSON

The rule, stated in `C:/glance/src/tape.h:6` and `C:/fray/src/tape.h:8`:
```
canonical = json.dumps(sort_keys=True, separators=(",",":"), ensure_ascii=False)
```
(`C:/REGISTRAR/core/tape.py:69–71`.) The precise consequences — `C:/caseclock/src/json.h:3–8` is
the tightest written statement:

> *Keys sort by code point (UTF-8 byte order is the same order); strings escape only `"` `\` and
> the C0 controls (`\b \f \n \r \t` named, the rest `\u00xx`, **lowercase**); everything else,
> including every non-ASCII code point and **`0x7f`**, passes through raw.*

- **key order:** lexicographic by code point; insertion order discarded
  (`C:/glance/src/tape.cpp:168` sorts on `a.first < b.first`;
  `C:/caseclock/src/json.cpp:110` `std::stable_sort` on the same predicate);
- **whitespace:** none; separators exactly `,` and `:`;
- **escaping:** `C:/glance/src/tape.cpp:129–146` — `"` `\` `\n` `\r` `\t` `\b` `\f` named, other
  `c < 0x20` as `\u%04x` lowercase, everything else raw. **`/` is not escaped. Non-ASCII passes
  through as raw UTF-8. 0x7F (DEL) passes through raw** — verified on disk: a bare octal 177 byte
  appears in `reference-tape.jsonl` line 11;
- **integers:** `std::to_string(int64_t)` (`C:/glance/src/tape.cpp:148`);
- **floats: Python `repr` semantics — shortest round-trip, NOT `%.17g`**, with a `.0` suffix on
  whole values (`C:/glance/src/tape.cpp:150–163`, loop `p = 1..17` until `strtod` round-trips).
  Pinned at `C:/glance/src/selftest.cpp:441`: `flt(1.0)=="1.0"`, `flt(1e-05)=="1e-05"`,
  `flt(123456789.0)=="123456789.0"`, `flt(0.1+0.2)=="0.30000000000000004"`.

### The row schema

**Exactly six top-level keys**, written canonically so the on-disk order is always
`at, body, digest, kind, prev, seq` (`C:/glance/src/tape.cpp:205–207`):

| key | type | meaning |
|---|---|---|
| `seq` | int, **0-based, contiguous** | row index |
| `kind` | string, **free-form, tool-defined** | row type |
| `at` | int | logical time |
| `body` | object | the payload |
| `prev` | 64 lowercase hex | previous digest; genesis 64 zeros |
| `digest` | 64 lowercase hex | this row's digest |

**There is no `ts`, no `v`, no version field and no `tool` field on a row.** Tool and version
metadata live on the **unchained header line only** — line 1, whose only family-required key is
`case_id` (`C:/glance/src/tape.cpp:338–344`; `C:/REGISTRAR/core/tape.py:195–199`). glance adds
`tool`, `version`, `window`, `t0`, `synthetic` (`C:/glance/src/session.cpp:141`); fray adds `tool`,
`version`, `case` (`C:/fray/src/fray.cpp:165`).

A real tape — `C:/caseclock/tests/expected/reference-tape.jsonl`, 12 lines, generated by
REGISTRAR's Python:

```
{"case_id":"TR-4118"}
{"at":-20,"body":{"text":"SYNTHETIC. No real donor data; every duration is illustrative."},"digest":"e2cc97b029ac8e8baa3bad8a71d34bc17af897d07f1a05e872c1ec5162f64578","kind":"note","prev":"0000000000000000000000000000000000000000000000000000000000000000","seq":0}
{"at":-20,"body":{"files":{"a.json":"ab12","z.json":"00ff"},"layers":["L0","L1"],"lead_minutes":[60,15,0],"verified_by":null,"verified_on":null},"digest":"ea4142a2fbf030bad4d1cff7b7bafd086150b7cfc91f71f6239808033b0d2230","kind":"rules","prev":"e2cc97b029ac8e8baa3bad8a71d34bc17af897d07f1a05e872c1ec5162f64578","seq":1}
```

### The `--verify` loop, and exactly what it rejects on

Entry `C:/glance/src/glance.cpp:248–262` (exit **0** intact, exit **3** broken). The loop,
`C:/glance/src/tape.cpp:279–311` (byte-identical at `C:/fray/src/tape.cpp:279–311`):

1. **the first non-empty line is skipped unconditionally** — `if (first) { first = false;
   continue; }` — it is the header, and **its content is never validated**;
2. a row missing any of the six keys ⇒ *"not a tape row"*;
3. `seq != expect` (expect starts at **0**, increments) ⇒ *"rows are missing or reordered"* —
   **seq contiguity IS checked and must start at 0**;
4. `prev != running head` (head starts at 64 zeros) ⇒ chain break;
5. recomputed `digest != stored digest` ⇒ *"the body has been altered since it was written"*.

**What it does NOT check:** no monotonic-time check at all (`at` is never parsed as a number by
glance — it is spliced back in verbatim); no terminal/footer row; no row count; no header
validation; no `kind` vocabulary; no UTF-8 validation; no check of the row's own key order.
`Tape::open` (`C:/glance/src/tape.cpp:319–334`) runs the same verify and **refuses to append to a
broken chain** (asserted at `C:/glance/src/selftest.cpp:479–481`).

caseclock's verify is the same law over a parsed model (`C:/caseclock/src/tape.cpp:73–84`) but
**stricter on parse**: it requires `at` to be an int and `body` to be an object
(`C:/caseclock/src/tape.cpp:120–124`).

### `glance --verify` is generic — proven empirically

```
$ C:/glance/glance.exe --verify C:/caseclock/tests/expected/reference-tape.jsonl
C:/caseclock/tests/expected/reference-tape.jsonl: INTACT, 11 rows, head 3f84af177fcb0a26
exit=0
```

That file was written by **REGISTRAR's Python**, its header is a bare `{"case_id":"TR-4118"}`, and
**none of its eleven `kind` values** (`note`, `rules`, `fact`, `derived`, `said`, `held`,
`silence`, `withdrawn`, `infeasible`, `signout`) **is a glance kind** — glance's own are `target`,
`note`, `change`, `fault` (`C:/glance/src/session.cpp:147`, `309`, `316`). The verifier never looks
at `kind` except to name it in an error. Standing acceptance check at
`C:/glance/docs/devlog.md:50`; fray's statement of the property at `C:/fray/docs/devlog.md:401–410`
(*"Three tools, three repositories, one format, one verifier"*); the promise in the usage line,
`C:/glance/src/glance.cpp:63`.

The normative paragraph, `C:/caseclock/docs/BLUEPRINT.md:96–112`:
> *`digest = BLAKE2b-256(prev · "\0" · canonical({seq, kind, at, body}))`, hex; `prev` is the
> previous digest and **stays outside the payload**; the first row's `prev` is 64 zeros. This is
> what the reference `core/tape.py` does (`hashlib.blake2b(digest_size=32)`); **an earlier draft of
> this section said SHA-256, which does not produce the reference's bytes.***

**Verdict: nib SPEC 8.1.1 is accurate, and the goal is achievable today.**

### What nib must satisfy

**Required for `glance --verify` to accept nib's tape:**
1. clean UTF-8 JSONL, one object per line, no BOM. **Never** caseclock's DPAPI-at-rest variant
   (`caseclock-tape 1 dpapi` magic + base64 rows, `C:/caseclock/src/tape.h:12`, `24`) — glance
   fails at row 0 on it;
2. **exactly one header line first** — content unchecked but it must be there. Follow the family:
   `{"case_id":"nib:<slug>", "tool":"nib", "version":"...", ...}` canonicalized;
3. all six keys on every row;
4. `seq` from 0, contiguous;
5. `prev` chain from 64 zeros;
6. `digest` per the rule above, lowercase hex.

**No magic line, no `tool` field and no version field on rows is required or checked.** `kind` is
free-form, so nib may use `changeset`, `judgment`, `hold`, `emit`, `abort`, `switch`, `molt`, …

**Strongly recommended** (needed by the *other* verifiers, not glance's): `at` an int64 and `body`
a JSON object (`C:/caseclock/src/tape.cpp:120–124`); and **genuinely canonical body bytes** —
glance re-hashes the body's raw substring from the line, so a non-canonical body still passes
glance, while REGISTRAR's `tape.py` re-canonicalizes after a full parse
(`C:/REGISTRAR/core/tape.py:174`) and would reject it. For "any fold in the family reads it," the
body must be true `json.dumps(sort_keys=True, separators=(",",":"), ensure_ascii=False)` bytes,
DEL-raw and Python-float-repr included.

**nib has no tape implementation yet** — `C:/nib/src/` has `changeset`, `doc`, `edit`, `ingest`,
`nib`, `resident`, `selftest`, and no `tape.h/.cpp`. **The cheapest correct path is to port
`C:/fray/src/tape.{h,cpp}` verbatim** (the narrow-string variant): the diff from glance's is 8
mechanical hunks — namespace, `util.h` include, `wstring`→`string`, `CreateFileW`→`CreateFileA`.

**And auricle contributes none of this.** Its `fabric.h` shares the chain's *shape* (prev-link +
recompute + first-bad localization) and **none of its bytes**: 128-bit non-crypto vs BLAKE2b-256,
binary length-prefixed vs canonical JSON text, `prev_hash` *inside* the hashed payload vs outside,
`Hash128{}` genesis vs 64 ASCII zeros. What auricle contributes is the **semantics**, §5.4.

## 5.4 The semantics worth lifting from `fabric.h` regardless of the hash

| law | line | why nib needs it |
|---|---|---|
| author is **stamped from the authed lane, never writer-declared** | 257–259 | nib's whole authorship claim (SPEC §7) |
| `op_id` idempotency — a retried op returns, never doubles | 252–256 | a pad retries on reconnect (Act II) |
| typed refusals, counted, never silent | 250, 261–263, 332–333 | CLAUDE.md rule 7 |
| `canonical()` **excludes** the derived `hash`/`sig` | 115 | or verify is circular |
| length-prefixed encoding = injective | 100–103 | prevents field-boundary collisions |
| `verify_chain` **localizes** the first bad row | 173–181 | "the tape is broken" is not a receipt |
| a deliberate `corrupt_row()` self-attack hook | 230–232 | the falsifier must be runnable |
| the two-plane **type fence**: partials never persist | `durable.h:5–6`, `m0_tape.cpp:9` | **this IS nib SPEC 6.4.3** |
| the fold is on the tape with the full rung | `fusord.cpp:645–648` | nib SPEC 8.1.2 |
| refuse to run if the hash primitive is unavailable | `blackbox.cpp:155–156`, `174` | no placeholder where a hash belongs |
| the config is part of the output path | `blackbox.cpp:1726–1739` | a run never overwrites a run |

`IT_WAS_ALWAYS_NEXT_TOKEN_2026-08-13.md:238` states the fence as a compile-time property, which is
exactly nib SPEC 6.4.3's "*enforced by the file format and not by a code path*":

> *the two planes are a **type fence** — writing a partial into the tape is a compile error, not a
> policy.*

**In nib that means: the forming region must be a different C++ type from a committed changeset,
and the tape writer must not have an overload that accepts it.**

## 5.5 The row types nib needs, in the family's actual shape

**The envelope is fixed by the family and nib does not get to choose it** (§5.3). Every row is
exactly six keys; everything nib-specific lives in `body`; `kind` is free-form.

```
{"at":A,"body":{…},"digest":D,"kind":K,"prev":P,"seq":S}
```

- `seq` from 0, contiguous. `prev` chains from 64 `'0'`.
- `digest = BLAKE2b-256(prev ‖ 0x00 ‖ canonical({"at","body","kind","seq"}))`, lowercase hex.
- **`at`** — the family uses whole logical units (caseclock: minutes from a case reference). nib's
  natural unit is **milliseconds since the session's `t0`**, and `t0` (wall) goes in the header.
  `at` must be an `int64` for caseclock/REGISTRAR to parse it. Nothing enforces monotonicity, but
  nib should keep it monotonic anyway.
- Header line 1, canonicalized:
  `{"case_id":"nib:<doc-slug>","synthetic":false,"t0":<epoch_ms>,"tool":"nib","version":"0.7.0"}`

| `kind` | `body` fields |
|---|---|
| **`session`** | `model_path` · `model_desc` · **`model_sha256`** · `serve_hash` (`"0xe7ff…"`) · `n_ctx` · `kv` · `devices` · `have_gpu` · `sampler` `{"chain":["min_p","temp","dist"],"min_p":0.05,"temp":0.7,"seed":11}` · `gen_cap` · `floor_ms` · `flush_ms` · `clause_tok_cap` · `bscore_gate` · `molt_wm` · `arm` · `egress_bytes` (0) — **the row that answers SPEC 8.1.3** |
| **`mandate`** | `seat` · `name` · `mandate` · `seq_id` — one per seat, immediately after `session`, verbatim from the frozen literals |
| **`coefficient`** | `name` · `value` · `was` · `source` (`"default"`/`"cli"`/`"runtime"`) — every dial that ever moves (SPEC 8.1.2; *"coefficients on the tape"*, one-pager `:62`) |
| **`switch`** | `which` (`"ai"`/`"mode"`) · `from` · `to` · `by` (`"operator"`) · `gates` — SPEC 6.1.3. The estate's shape: `{t, from, to, actor, reason, gates}`, `WHOLE-MACHINE:40` |
| **`changeset`** | `author` · `rev` · `base_rev` · `cs` (Easysync string) · `blocks` (touched ids) · `ins` · `del` |
| **`judgment`** | `i` · `lane` · `reason` (`b`/`n`/`t`/`c`/`f`/`m`) · `bscore` · `probe_ms` · `lat_ms` · `clause` · `margins` `{"SPEAKER":…,"SKEPTIC":…,"SENTINEL":…}` · `arm` · **`coarse`** (bool) — fusord's `k=b` plus arm and the explicit coarse flag (`fusord.cpp:585–587`: *"a corpus row must carry the judgment grain that produced it"*) |
| **`hold`** | `i` · `seat` · `margin` · `reason` · `suppressed_by` (`null`/`"floor"`/`"repeat"`/`"resolved"`/`"dampened"`/`"empty_gen"`) · `last_margin` (for the escalation test, §6.13) — **fusord has no such row; nib should** (§1.8) |
| **`emit`** | `i` · `seat` · `margin` · `gen_ms` · `toks` · `stop` (`"eog"`/`"newline"`/`"cap"`/`"sentence"`) · `say` · `clause` · `block` · `after_block` · `dep` (rev ids) · `rev` |
| **`abort`** | `i` · `seat` · **`formed`** · **`reached_air`** · **`mid_word`** · **`live`** · `reason` (`"contradicts_dep"`/`"floor_taken"`/`"late_percept"`/`"operator"`) · `trigger_rev` · `dep` · `ms_to_withdraw` · `toks` — `cut_audio`'s shape (`starship.cpp:544–552`) with the pad in place of the mouth |
| **`molt`** | `before` · `rung_toks` · `after` · `dur_ms` · `rung` (full text) · `tail_words` — `fusord.cpp:645–648` |
| **`tick`** | `gap_s` — `fusord.cpp:717–718` |
| **`end`** | fusord's `k=end` aggregate (`fusord.cpp:757–769`) with nib's counters, plus `ring_dropped`, `echoes`, `truncated_lanes` (`PadSource`), `coarse_frac`, `sampler_consulted` |

Three invariants to assert in `--selftest`, each from a measured estate bug:

- **`events = emit + hold`, by addition, never by conflation** (`fusord.cpp:584`) — the bug that
  read 12× high;
- **`emits(starts) = commits + dampened + empty_gen + aborts`, per seat and total. Any violation =
  verifier red** (`C:/NEW/BLACKBOX_SPEC_v0.2_DRAFT_FABLE5_2026-08-17.md:144`);
- **the narrative is derived from the fields, never written beside them**
  (`starship.cpp:828–832`), checked by the beat assertion (`starship.cpp:929–981`).

And the reciprocal rendering law, `C:/NEW/BLACKBOX_SPEC_v0.2_DRAFT_FABLE5_2026-08-17.md:162`:
> **if the verifier counts it, the viewer shows it.**

Two invariants worth asserting in `--selftest`, both taken from measured estate bugs:

- **`events = emit + hold`, by addition, never by conflation** (`fusord.cpp:584`) — the bug that
  read 12× high.
- **the narrative is derived from the fields, never written beside them**
  (`starship.cpp:828–832`), and the beat assertion (`starship.cpp:929–981`) checks it.

---

# 6 · DOCTRINE THAT CONSTRAINS FLOOR CONTROL

Every law here is quoted with its source. Code-level laws are cited to the file that enforces them;
doc-level laws to the document that states them.

## 6.1 The placement law — where the mind's contribution goes

`C:/NEW/FUSOR_MASTER-ONEPAGER_2026-08-23_FABLE5.md:40`:

> *a glance is a priced act (196 tokens per 448² crop), **filed after the line it illuminates (the
> placement law, ~19 nats, serving-lane-scoped, serializer-enforced)***

This is the estate's answer to "where do the words go," derived on the sight lane, and it is
**exactly nib BLUEPRINT §5's rule**: *"The resident's block is inserted after the block the human
most recently completed, and it never moves text a human wrote."* Two things carry over verbatim:
**"after the line it illuminates"** (not before, not inline, not interleaved) and
**"serializer-enforced"** (not manners).

The full normative form, with the reason it is enforced even where the hazard may not exist —
`C:/NEW/MERIDIAN_SIGHT_SPEC_v0.1_FABLE5_2026-08-23.md:128`:

> ***Placement, serialized:** every committed glance files **after the line it illuminates** —
> behind its referent, ahead of nothing load-bearing. On the reference pipeline this is a measured
> law (an image immediately before a judged line collapses a live catch ~19 nats; matched text
> harmless; ≥3 lines full protection; the strong catch dies too) … **The hazard class stays
> unreachable by construction either way — the serializer is three lines of code and costs nothing
> even where the cliff turns out not to exist.***

**That last clause is the argument for nib's Stage 2 floor rule in one sentence.** It is cheap, it
is structural, and it does not require the hazard to be proven first.

The same sentence carries **see-it-once**, the sight-lane form of say-it-once:
> *the sighting ledger makes seeing **condition-grain (see-it-once)***

And the master law that generates all of it,
`C:/NEW/FUSOR_MASTER-ONEPAGER_2026-08-23_FABLE5.md:28`:
> *And hazards are made **unreachable by construction, never forbidden by policy** — the serializer,
> not the rulebook.*
> ***The seam law:** compress the cognition into the forward pass, never the fence — the pass
> proposes everything (sentences, glances, matches, dispositions); **deterministic seams dispose**.*

Its ancestor in the blackbox family, `C:/auricle/BLACKBOX_SPEC_v0.1_QC-001.md:5`:
> ***Floor serialization.** The cabin serializes the floor: one mind generates at a time; a boundary
> round that selects a winner while another mind is mid-utterance IS the interruption path (abort
> reason `floor_taken`, then the winner generates).*

## 6.2 The floor is taken — judgment interrupts, surprise only testifies

`blackbox.cpp:1127–1139`, and its comment at 1128 is the law in one line:

> `// THE FLOOR IS TAKEN (QC-001 §3): judgment interrupts; surprise only testifies.`

```cpp
if (mid_utterance && cur >= 0) {
    llama_memory_seq_rm(mem, seat_seq(cur), -1, -1);
    ++C.aborts;
    …"ABORT (floor_taken): %s's forming line killed mid-word - the floor passes to %s."
    …"the air heard: \"%s\""
    tr("\"k\":\"abort\",\"mind\":…,\"reason\":\"floor_taken\",\"heard\":…");
    commit_to_tape(w.minds[cur].lane, "BRANCH_KILLED floor_taken");
    cur = -1;
```

And the guard above it, `blackbox.cpp:1096`:

```cpp
if (mid_utterance && i == cur) continue;   // the speaker holds the floor unless out-ranked
```

**A speaker holds the floor by default. It is displaced only by a *judgment* that out-ranks it —
never by a surprisal spike.** Nib's Act I has one composer, so the analogous rule is: *the human
always out-ranks the resident*, and the displacement is unconditional.

**Yielding the floor** (QC-006 / QC-008 §30), `blackbox.cpp:1088–1090` and `1471–1507`: an empty
generation, a punctuation-only generation, or a dampened near-duplicate **yields the floor and
re-runs the same occasion excluding the yielder** —
> *a yielded floor re-runs the SAME occasion immediately, excluding the yielder; the exclusion dies
> with that occasion and can never leak to an unrelated round.*

Traced as `{"k":"floor_yield","mind":…,"reason":…,"reround":true}` (`blackbox.cpp:1479`, `1492`,
`1505`).

## 6.3 Only commits kill; forming evidence may only pause

`C:/NEW/BLACKBOX_QC-TOTALITY_FABLE5_2026-08-17_ADDENDUM-B.md:50`, naming it **L-COMMIT**:

> *constitutionally the **only** thing allowed to dispose (L-COMMIT: "forming evidence may pause a
> thought; only commits kill it")*

Enforced at `fabric.h:203–212` (pause, `Running → Paused`) and `fabric.h:274–285` (kill, on commit
only, `"only commits kill"`). Demonstrated as two beats in `syncytium.cpp:252–262` then `245–251`,
and in `starship.cpp:583–592` then `574–582`.

**For nib:** a keystroke *in flight* (a partial word, an IME composition, a selection) pauses the
resident's forming region; a **committed changeset** kills it. That mapping is free — nib's document
model already distinguishes them.

## 6.4 The world never dilates for the mind

`fusord.cpp:21–24` (doctrine, dated 2026-08-12):

> *Back-pressure corollary: **pacing may delay the mind's next TOKEN; it must never delay a
> PERCEPT.** Ingest is unconditional; only judgment cadence is modulated. A ring that overflows
> COUNTS the loss loudly — a silently dropped percept is the turn reborn inside the loop.*

Restated where it bites, `fusord.cpp:670–676`:

> *So when the intake is backed up, judgment COARSENS to line-final + the token cap; ingest stays
> unconditional and every percept still lands. **Delay a judgment, never drop a percept.** The
> coarsening is logged (reason "c") so the corpus shows it.*

And again in the audio pacing loop, `starship.cpp:622–628`:

> *Pace: stay near the listener — **BUT the world does not wait for our pacing.** … Pacing may
> delay the mind's next TOKEN; it must never delay a PERCEPT. Break the moment the delta is due.*

nib BLUEPRINT §5 states the same law from the human's side: *"The mind must not dilate the world
any more than the world dilates for the mind."*

## 6.5 The blind window is bounded by construction

`fusord.cpp:484–488`:

> *ONE SENTENCE, HARD CAP. Measured 2026-08-12: latency-to-notice was COUPLED to emission length —
> the longer the mind had to say, the blinder it went (500 ms → 3.5 s inside one line). That is the
> back door in miniature… **Generation is the blind window, so the blind window is bounded by
> construction.***

**This is a floor-control law disguised as a token cap.** The reason the resident may hold the
floor at all is that it can only hold it for 28 tokens. nib must keep a hard cap for the same
reason, and must **measure** its own latency-to-notice *during* composition — the coupling is the
thing that breaks, not the cap.

## 6.6 Say-it-once is a fine-tune target, not a harness trick

`fusord.cpp:370–373`:

> *The disposition to say a thing ONCE is not in the weights — it is the "manners" layer, and it is
> **a fine-tune target, not something to fake in the harness**. So the harness does the honest
> minimum: it SURFACES once and LOGS the rest (`k=e_suppressed`)… **Nothing is deleted; the corpus
> keeps every would-be emission.***

`fusord.cpp:374–378` — suppression **expires**, and permanent silence is its own failure:

> *A permanent silence is its own failure: if the condition stands unaddressed for forty minutes and
> someone then acts on it, the seat SHOULD fire again. Two re-arm paths, both logged: TIME (a
> decaying window) and EVIDENCE — new material that touches the same words the seat spoke about,
> i.e. the topic came back up.*

`fusord.cpp:516–520` — the asymmetry (quoted in full in §1.9): **resolved is permanent; unaddressed
may fire again.**

## 6.7 Condition grain

`C:/NEW/BRAIN_RECONTEXT_FUSOR-AT-CENTER_2026-08-21.md:22` and the one-pager
(`C:/NEW/FUSOR_MASTER-ONEPAGER_2026-08-23_FABLE5.md:62`):

> *matched-grain 63.4 % → 6.7 % (≈9.5×); **perseveration = memory-of-own-acts**; event-plasticity*

The mechanism behind "perseveration = memory-of-own-acts" is `fusord.cpp:548–561` (§1.5a): the mind
committing its own speech to the trunk is what makes repetition *visible to it*, and
`fusord.cpp:367–369` records the measured effect and its limit — the margin damps 4.33 → 3.58 →
3.15 **but does not cross zero**. Self-knowledge dampens; it does not stop. The harness must.

## 6.8 The vise — no runtime dial buys both catch-rate and livability

`C:/NEW/BRAIN_RECONTEXT_FUSOR-AT-CENTER_2026-08-21.md:22`:

> ***the vise** [M] — no runtime dial separates catch-rate from livability (36/39 ceiling deaf where
> it matters; **921.3 fires/stream-hour** unlivable at dial-zero; the gap lives in the weights, not
> the memory)*

And `C:/NEW/CUTSCENE_SPEC_INTERACTIVE-PRESENCE_v0.1_FABLE5_2026-08-21.md:9`:

> *dial-zero fires 921.3/stream-hour (unlivable — the wheelspin); the best fixed dial catches 36/39
> and is deaf where it matters (the single fixed gear ratio, a compromise everywhere). That is THE
> VISE… **the strategy cannot live in the driver's thumb or a runtime knob.***

**This is the single most important constraint on nib Stage 2's expectations.** nib runs v11 at
dial 0, and v11 is the tune that cut the fire rate **63.4 % → 6.7 % per decision-boundary (≈9.5×)**
at matched grain (`C:/NEW/FUSOR_MASTER-ONEPAGER_2026-08-23_FABLE5.md:32`; see §7.13 on the
grain-poisoned older pair). At 6.7 % of boundaries on a real writing stream, nib will still be
**noticeably talkative**, and the answer is *not* to add a dial — the estate has already measured
that dials do not work. The answers available to nib are: the floor rule (structural), the damper
(deterministic), resolve-permanence, and a retune. Anything else re-runs an experiment whose
negative result is already banked.

**The rate numbers nib should expect, and the budget it should hold itself to:**

| measurement | value | source |
|---|---|---|
| dial-0 would-be interruptions | **593 / stream-hour** | `IT_WAS_ALWAYS_NEXT_TOKEN_2026-08-13.md:88` |
| dial-0, live, distinct *conditions* | **~59 / hour** (the flood is breadth, not repetition) | `MASTER-ONEPAGER:32` |
| dial-0 fires, another regime | **921.3 / stream-hour** | `BRAIN_RECONTEXT_FUSOR-AT-CENTER_2026-08-21.md:22` |
| first live hour, judged events | **169** in 25 clean minutes | `runs/fusor-firsthour-2026-08-12.md:163` |
| first live hour, **surfaced** after suppression | **13 = 31.2 / hour** (SEN 10 / SKP 1 / SPK 2) | same |
| the pre-registered livability **budget** | **≤ 6 surfaced / hour** | `runs/fusor-firsthour-2026-08-12.md:62–63` |
| boundary rate per active human stream | **0.2–0.5 Hz** | `IT_WAS_ALWAYS_NEXT_TOKEN_2026-08-13.md:240` |

So: fusord's *suppressed* rate was 31.2/hr against a budget of 6/hr — **five times over, after all
the manners**. nib's Stage 5 falsifier ("does the operator leave it on for a week") will be decided
in that gap.

Two arithmetic warnings that come with the numbers,
`C:/auricle/UNPROMPTED_BENCH_SPEC_v0_2026-08-14.md:208`, `212–215`:
> ***≥95 % of tape duration must be null**, or the restraint metric is meaningless*
> *a respectable **5 % per-sample FPR** on a balanced set, dropped onto a workday with a boundary
> every few seconds, **fires continuously**. FPR-per-sample and fires-per-hour are different
> quantities and **do not convert without the base rate**.*

And the reporting rule (`:294`): ***Report C against F, swept over T. Never a single number.***

## 6.9 Hazards unreachable by construction

nib's own `CLAUDE.md` rule 4 is the estate's rule:

> *The resident never writes into a block a human is touching. **Not a policy, a serializer rule**:
> an emit targeting a block with a human keystroke inside the floor window is refused **before it is
> composed**. Hazards unreachable by construction, never forbidden by manners.*

The estate's matching statements: `fabric.h:274–275` (*"a mechanical set-test on the control plane
(never parses payload)"*), `fabric.h:8–9` (one serializer disposes), `fabric.h:257–259` (author
stamped, never declared), `blackbox.cpp:1709` (*"structural: no dispatch-on-content path exists
(S-PARSER)"*), `blackbox.cpp:1703` (*"structural: no door-open code path exists"* — a state
mutation proven impossible by absence rather than by a check). nib's own Stage 1b already applies
it: `resident.h:7–12` — *"Emission is not disabled here, it is ABSENT… no generation code in this
file to switch off."*

## 6.10 Never simulate a tell that is not an event

nib `CLAUDE.md` rule 5 and BLUEPRINT §5:

> *Emission is paced — but pacing must express a real internal state and never perform one. Holds
> are silence. Aborts are visible retraction. **A thin margin may legitimately slow emission,
> because the mind is closer to changing its own.** An invented stutter is a lie, and it is the
> exact place a demo would cheat.*

The estate's enforcement of the same instinct: `starship.cpp:526–543` refuses to *claim* a mid-word
cut it did not achieve (*"on a WORD BOUNDARY — the mid-word claim does NOT hold this run"*) and
asserts the branch was genuinely live (*"NOT LIVE — disclose this run"*). `starship.cpp:758–768`
and `blackbox.cpp:1176–1180` log a measurement that would have looked good and refuse to act on it.
`cortex.h:49–51`: *"`conf` is MEASURED confidence… never a model self-report."*

**The one honest lever nib has for pacing is the margin itself** — it is a real number produced by
a real forward pass, and BLUEPRINT §5 already licenses using it. Note the *direction*: a **thin**
margin slows emission. Do not invert it into "confident = fast, uncertain = hesitant-looking";
that is a tell, not a state.

## 6.11 Resolve semantics

`fusord.cpp:381–390` and `450–458`. Acceptance is **targeted** — it must touch the seat's own topic
by at least one content word (`fusord.cpp:453`), because the un-targeted version let a closing
pleasantry resolve all three seats permanently (`fusord.cpp:385–388`):

> *A false acceptance is the UNSAFE direction (permanent silence on a standing contradiction), so:
> bare "thanks" is out, and acceptance now also requires the accepting clause to TOUCH the seat's
> own topic.*

In a pad, "acceptance" has a far better signal than a word list: **the human edited, kept, or
deleted the block the resident wrote.** That is a changeset, not a heuristic — and it makes
resolve a serializer fact rather than a lexical guess. This is the clearest place where nib's
surface is *better evidence* than auricle's stream.

## 6.12 Barge-in is a percept, not a cancel

`C:/NEW/FUSOR_IMPLEMENTATION-SPEC_THE-WHOLE-MACHINE_DRAFT-1_2026-08-25_FABLE5.md:52`:

> *Interruption … **barge-in is a percept, not a cancel: the kernel hears it mid-emission and
> disposes** (the re-say, ADDENDUM C register)*

And `C:/NEW/FUSOR_X_AURICLE_THE-STARSHIP-FIT_v0.1_FABLE5_2026-09-01.md:151`:

> ***Barge-in is a percept, not a cancel** (WHOLE-MACHINE:52): the operator speaking over the
> computer is world the kernel disposes — usually an `abort` of the forming sentence, then the
> **re-say***

**Directly binding on nib Stage 3.** A keystroke during formation is not an API cancel with a
`stop()` semantic — it is world, it enters the trunk like any other percept, and the abort is
what the mind *does* with it. The consequence: the killed sentence's context still contains the
keystroke, so the re-form (§2.7) is genuinely informed rather than merely retried.

## 6.13 Repeat is a measured threshold, not a policy vote

`C:/auricle/UNPROMPTED_AMENDMENTS_2026-08-15.md:44–46` corrects fusord's binary suppressor:

> ***all four persistent conditions in the live hour were rise-then-fall*** — surfaced at their
> weakest margin, peaking *while suppressed* (0.19 → 3.27 on the loudest), decaying as the world
> moved on. **A re-raise at a higher margin is not noise; it is escalation on strengthening
> evidence. Fix — make `repeat` a measured threshold rather than a policy vote.** A repeat is
> **free** iff its margin exceeds the same condition's last-surfaced margin by δ; otherwise full
> weight.*

`C:/auricle/IT_WAS_ALWAYS_NEXT_TOKEN_2026-08-13.md:90`:
> *Conviction accrues while suppressed. … First-crossing-wins under-reads the mind; suppression is
> not absence of conviction.*

**For nib:** the `hold` row must carry `last_margin` so the escalation test is computable after the
fact, and a re-raise above `last_margin + δ` is permitted rather than damped. This is a *refinement*
of §1.9's damper, not a replacement.

And the semantic-dedup rule, `C:/auricle/UNPROMPTED_BENCH_SPEC_v0_2026-08-14.md:321–322`:
> *`repeat` is detected by **mark resolution, not text similarity** — two paraphrases citing the
> same evidence are one condition surfaced twice, which is exactly the semantic dedup a verbatim
> guard misses.*

## 6.14 Ticks never trigger a probe round

`C:/auricle/BLACKBOX_SPEC_v0.1_QC-001.md:7`:

> ***Ticks.** Idle gaps ≥30 s insert a tick line that decodes onto the trunk (silence-as-world) but
> **never triggers a probe round** (the S-QUIET compromise; **anti-turn exemption 4**).*

`C:/NEW/HOTWASH_SPEC_v0.1_2026-08-16.md:145`:
> *Silence enters as world to be perceived. **Ticks never trigger probes by themselves**; they
> extend the trunk.*

Enforced in code at `fusord.cpp:709–719` (decode, `read_frontier`, ledger row — **no
`judge_and_maybe_emit` call**) and `blackbox.cpp:1605` (*"NOTE: ticks never trigger a probe round
(QC-001 §4); they do open queue seams"*).

**nib violates this today. See §7.11.**

The honest open problem alongside it, `C:/auricle/OPEN_PROBLEMS_2026-08-12.md:157–163`:
> ***Nothing has ever measured whether ticks change behaviour.*** They are ingested; no experiment
> has shown an emission that depended on one. … If ticks are inert, then "surprise without
> pre-naming" is only half-implemented — the mind reacts to *arrivals*, never to *absences*.

**nib can settle this** — it produces its own ticks (`ingest.h:63–65`) and can A/B them.

## 6.15 The probe-cost law: a silent mind can go blind

`C:/auricle/IT_WAS_ALWAYS_NEXT_TOKEN_2026-08-13.md:81`:

> ***The probe-cost law** (measured live). A resident is real-time only while its total forward-pass
> demand outruns the world — and **judgment demand scales with boundary density, not output
> length.** 186 boundaries × 3 seats × ~60 ms ≈ 33 s of GPU against ~8 s of arrivals, with zero
> emissions: ***a silent mind can go blind.*** Capacity model:
> **Σ(lane rate × boundary density × seats × probe cost) < 1**, per lane, measured before wiring
> any lane.*

**nib must compute this for typing.** Prose typing at ~40 wpm with a sentence every ~15 words gives
a boundary roughly every 20 s — comfortable. A **paste** gives fusord's measured 3.3 boundaries per
line and blows the budget instantly, which is exactly what the gate rule exists for.

## 6.16 Coarse mode is Fake 1, and must be bounded, per-lane, and disclosed

`C:/auricle/OPEN_PROBLEMS_2026-08-12.md:56–59`:
> *The gate rule coarsens judgment to line-final + token cap under backlog. That is mechanically
> **cadence-based judgment — Fake 1** — and it is correct only as *disclosed* graceful degradation.
> … **if a real workday spends a large fraction of its boundaries in coarse mode, then the
> production behavior of the "resident" is partly a fake.***

The ratified thresholds, `C:/auricle/FUSOR_DECISIONS_2026-08-12.md:39–42`:
> ***>40% coarse = not a valid disposition sample** (doctrine, was prereg).*
> ***>10% coarse = the hour reports the fraction beside the fire rate and splits the analysis.***
> ***Always per lane** — paste events and conversational lanes diverge wildly; a pooled number
> hides which one degraded.*

**nib already counts `coarsened()` (`resident.h:89`) and does nothing with it.** Stage 2 must put
`coarse_frac` beside every fire rate it ever prints, per lane, and refuse to call a >40 % session a
disposition sample.

## 6.17 The anti-turn construction — R1 through R6

`C:/auricle/UNPROMPTED_BENCH_SPEC_v0_2026-08-14.md:92–130` and
`C:/auricle/UNPROMPTED_AMENDMENTS_2026-08-15.md:54–56`. These are the structural rules that decide
whether nib's Stage 4 toggle is measuring anything:

> **R1 · The clock pushes. There is no `next()`.** *The single most common way an anti-turn harness
> silently becomes turn-based is exposing `next()`. **A pull interface *is* a turn**: the SUT
> decides when the world happens.* (`:92–99`)

> **R2 · Emission is asynchronous, never a return value.** *`on_delta(d) -> Optional[Emission]`
> looks anti-turn and is not: it can only speak *in response to a delta*, so the turn has merely
> been renamed from "message" to "delta." Under that interface **the stalled case is
> unrepresentable**.* (`:101–109`)

> **R3 · No end-of-tape signal exists until the tape actually ends.** (`:113`)
> **R4 · Time is a lane.** *This is what makes "nothing has happened for 2h48m" an *observable*
> rather than an absence.* (`:119–122`)
> **R5 · Late is a miss.** *…no partial credit after `c` … in the product a warning after the send
> key is an autopsy.* (`:126–130`)

> **R6: the SUT's state persists across deltas, and `staleness` is the enforcement.** *a submission
> may internally **reconstruct** its state per delta — re-prefill, re-read, restart — and still
> satisfy R1–R5. **That is turn-based on the inside.***
> (`UNPROMPTED_AMENDMENTS_2026-08-15.md:54–56`)

**R2 is the one nib is closest to violating.** `nib/src/resident.cpp:334–358` — `feed()` takes a
percept and *returns* judgments through `out`. That is `on_delta(d) -> Optional[Emission]` in all
but name, and under it a stalled pad is unrepresentable. The saving grace is nib's tick
(`ingest.h:63–65`), which is R4 done right: silence arrives as a percept and therefore has a
representation. **Keep the tick, and make sure the emission path can fire on one** — otherwise R2
is violated in fact even though R4 is satisfied on paper.

**R6 is the axis Stage 4's twin is defined on** — the twin re-prefills, and that is the whole
point (§3.5 item 4).

## 6.18 The floor-yield distance bug — scope the exclusion to the occasion

The estate hit and closed this exact class. `C:/auricle/BLACKBOX_SPEC_v0.1_QC-001.md:54`:
> ***Floor-yield distance (QC-006 note, now closed):** the exclusion could be consumed by an
> unrelated later round.*

Closed at `C:/NEW/BLACKBOX_SPEC_v0.2_DRAFT_FABLE5_2026-08-17.md:51`:
> ***Yield re-runs the same occasion.** A damper or empty-gen disposal triggers an **immediate**
> re-round excluding the yielder; **the exclusion dies with that occasion and can never be consumed
> by an unrelated later round.***

**This is not the same thing as nib's 2 s floor window** — it is about the *yielder* exclusion. But
it names the failure mode nib's floor window shares: an exclusion whose lifetime is measured in
something other than the occasion it belongs to will eventually apply to a round it has nothing to
do with. When nib implements the floor rule, the refusal must be recomputed at the moment of the
attempted emission from the block's current state, never cached as "this seat is excluded until
`t + 2 s`".

## 6.19 Output-side dampers are legal; percept-side ones are not

`C:/auricle/BLACKBOX_SPEC_v0.1_QC-001.md:18`:
> *Legality: **the gate law protects *percepts*, never the system's own output**; this is
> "deterministic disposes" on emissions only.*

This is the licence for everything in §1.9 and lift item #9, and the fence around it: nib may
deterministically dispose of *its own would-be emissions* all day, and may never do the same to a
keystroke.

The two measured escapes to avoid: `C:/auricle/BLACKBOX_SPEC_v0.1_QC-001.md:21` —
> ***Damper window 4 → 8:** measured escape — Katherine's r14 line repeated near-verbatim at r30
> because exactly 4 intervening emissions evicted it from the window.*
> ***Empty-generation guard:** a mind can emit an empty utterance … v0.1 committed it to the tape …
> and its `utterance_end` advanced the card chain — **an empty line steered the scene.***

## 6.20 The number fence, and dual derivation

`C:/NEW/FUSOR_IMPLEMENTATION-SPEC_THE-WHOLE-MACHINE_DRAFT-1_2026-08-25_FABLE5.md:3`:
> ***A number without its grain, denominator, and source is not a number.***

Enforced as CI, `:100`:
> *the **honesty lint** — the number fence as CI in the room repo: any rendered metric must bind
> {value, grain, denominator, source}; **a bare number fails the build***

And `C:/auricle/UNPROMPTED_AMENDMENTS_2026-08-15.md:64`:
> ***`grain` and `denominator` are mandatory columns on every metric row**, enforced by the scorer,
> not by discipline. **A row without them does not serialize.***

The reason, `C:/auricle/UNPROMPTED_AMENDMENTS_2026-08-15.md:70–72`:
> ***sixteen instrument lies in eight days, and in every single case the lie was caught because a
> second, independently-derived surface disagreed** … **Not once by inspecting the instrument.** …
> **Fix — dual derivation for every headline number.***

And `C:/auricle/IT_WAS_ALWAYS_NEXT_TOKEN_2026-08-13.md:92`:
> ***The instrument lies before the mind does.** Of the first hour's systems findings, most were
> failures of the *measuring apparatus* (a file opened exclusively; a double-logged event stream
> that read 12× high), each found by running, each fixed, each disclosed.*

**For nib:** every number on the status line and in `--about` needs its denominator visible, and
nib's Stage 5 headline ("emissions per hour at somebody's elbow") must be derivable two ways — from
the tape's `emit` rows and from the `end` aggregate — with a selftest that they agree.

## 6.21 No accelerated run may be cited as evidence about interruption

`C:/auricle/BLACKBOX_SPEC_v0.1_QC-001.md:26` — the estate's own funeral, and the cleanest warning
for nib's Stage 3 demo:

> *the clock's only advance site sits in the loop's lowest-priority branch, so sim time cannot move
> while a mind is generating. Under `--fast` no world event can land mid-utterance, which makes the
> interrupt, abort, falter and twin-blindness surfaces ***unreachable rather than rare***. **Every
> `aborts=0` and `blind=0` in the archive is therefore structural, not measured.** … until it
> lands, **no accelerated run may be cited as evidence about interruption.**

**nib's exposure is the mirror image:** a `--selftest` that drives the pad from a synthetic clock
(`nib.cpp:163–169`, `clock += 40`) can make an abort **unreachable** just as easily. nib's Stage 3
falsifier must run on the real clock, through the real window, and the run must assert that the
abort surface was *reachable* — not merely that no abort fired.

The companion, `STARSHIP_DEMO_SCRIPT.md:170`:
> *generation outruns speech ~20×, so **without pacing the mind finishes seconds before the mouth
> reaches the claim and "aborted mid-utterance" is theatre.***

In a pad the "mouth" is the render, and rendering is effectively instant — so nib does **not**
inherit this pacing problem. But it inherits the question it answers: *was the human's keystroke
genuinely inside the forming region's lifetime?* `starship.cpp:539–543`'s live-assertion is the
shape of the answer.

## 6.22 Scaffolding ratio — which guards can be switched off

`C:/auricle/OPEN_PROBLEMS_2026-08-12.md:40–50`:

> *Each is honest, disclosed, and correct. **The sum is a mind whose good behavior increasingly
> lives in the harness rather than the weights.*** … *Fix: Enumerate every scaffold in the live
> path, and pre-register a **scaffolding-ratio receipt** per tune generation: **which guards can be
> switched off without the behavior collapsing.***

And `:229–232`: *the flywheel harvests real-world negatives **from a scaffolded system** … the next
model learns the distribution as shaped by the harness — **including the harness's mistakes***.

**nib's Stage 2 will add four scaffolds at once** (floor rule, damper, resolve-permanence,
generation cap). Each must be individually switchable and the tape must record which were on, so
Stage 5's week of real work is a measurement of something and not of the harness.

---

# 7 · DISCREPANCIES FOUND

Checked every citation nib's docs make against auricle, and every claim auricle makes about itself
that Stages 2–4 depend on.

## 7.1 nib's citations of auricle — all three are CORRECT

| nib claim | verdict |
|---|---|
| `fusord.cpp:723-746` = the trunk sees `\n[lane] text` and splits it into words itself (`SPEC.md:211`, `ingest.h:37`) | ✅ **correct.** 724 builds `"\n[" + d.lane + "] "`; 733–745 is the word-split loop; 746 is the line-final judge. |
| `fusord.cpp:242-254` = the segmenter boundary-set tightening (`SPEC.md:212`, `ingest.cpp:29`, `ingest.h:59`) | ✅ **correct.** 242 is the section banner, 243–254 the measured rationale, ending exactly at *"train ≡ serve holds."* |
| `fusord.cpp:712` = the tick text, byte-identical (`SPEC.md:261`, `ingest.h:63`) | ✅ **correct.** 712 holds `"\n[tick +%llus]"`. |
| `sizeof(Delta)` is **528**, not the 512 `source.h:32` claims (`SPEC.md:243`, `ROADMAP.md:133`, `ingest.h:13`, `selftest.cpp:527`) | ✅ **nib is right, auricle's comment is wrong.** 8 + 16 + 2 + 496 = 522, padded to 528 by `uint64_t` alignment. `source.h:32` says *"sizeof(Delta) stays a tidy 512B"*. |
| "spec §5.8" for self-echo (`SPEC.md:317`, via `source.h:17`) | ✅ **valid**, though non-obvious: it is item **8** of the numbered list under `## 5 · The resident engine`, at `FUSOR_HARNESS_SPEC.md:124`. |
| `SERVE_HASH_PIN = 0xe7ffa5704ba31076` (`SPEC.md:6.2.4`, `resident.h:49`) | ✅ matches `fusord.cpp:157` exactly. |

**nib's citation hygiene is good.** The discrepancies below are auricle's, or are nib docs
compressing two auricle facts into one.

## 7.2 `fusord.cpp:26–27` overstates ledger compatibility with `soak --brief`

`fusord.cpp:26–27` claims:
> *Ledger: runs/fusor_ledger.jsonl — **the SAME schema soak writes**, so `soak --brief` and
> `soak --review` work on it unchanged.*

It is not the same schema, in three ways, and two of them fail silently:

1. **`b` row: `lag_ms` → `lat_ms`.** soak writes `lag_ms` (`soak.cpp:504`); fusord writes `lat_ms`
   (`fusord.cpp:571`).
2. **`end` row: soak's brief reads fields fusord never writes.** `do_brief` pulls `stream_ms`,
   `mean_lag_ms`, `max_lag_ms`, `capacity_wps` (`soak.cpp:209–213`). fusord's `end` row
   (`fusord.cpp:757–762`) has `wall_ms`, `mean_lat_ms`, `max_lat_ms` and no `stream_ms` or
   `capacity_wps`. `jget_num` simply does not fire, so **the brief reports 0 for every latency and
   capacity figure with no warning.**
3. **`b` row: the new reason `"c"` is miscounted.** `do_brief`'s reason histogram
   (`soak.cpp:185–188`) buckets `b`/`n`/`t`/`m` and sends **everything else to `by_reason_f`** —
   so fusord's coarsened judgments are silently reported as line-finals. `fusord.cpp:447` was
   deliberate about counting them (*"degradation must be COUNTED, not inferred"*); the reader
   un-counts them.

**Bearing on nib:** do not inherit the assumption that a downstream reader tolerates added fields.
nib's tape needs a `v` (schema version) on every row and a reader that **refuses unknown `k`
loudly** — `blackbox.cpp:1713` does exactly that (`UNCHECKED (unknown key)`) and it is the right
shape.

## 7.3 BLUEPRINT §6 fuses two unrelated measurements into one sentence

nib BLUEPRINT §6: *"a sentence killed mid-word when the world contradicts it, **in 13
microseconds**, with the killed words on the tape — while the turn-based twin on identical weights
**missed the same moment by 65.7 seconds**."*

These are **two different experiments, two different binaries, two different clocks, and not the
same moment**:

| | 13 µs | 65.7 s |
|---|---|---|
| binary | `m0_demo.cpp` (M0-4) | `blackbox.cpp --twin` (QC-003) |
| what | wall time of one `llama_memory_seq_rm` (`m0_demo.cpp:177–181`) | gap to the twin's next scheduled wake (`blackbox.cpp:1659–1675`) |
| scenario | Eiffel-Tower whodunit toy | the cabin deck, `handle_zone_enter` |
| clock | real, `steady_clock` | **virtual sim clock** (`blackbox.cpp:82–107`) |
| status | measured cost of a primitive | **"earliest possible"** — the twin never reacted |
| n | 1 | 1 |
| receipt | `STARSHIP_DEMO_SCRIPT.md:263` | `C:/auricle/runs/cabv2_twin_console.txt:114` |

The estate itself flags this class of error. `THE_RESIDUAL_2026-08-15.md:186`:
> *Adversary **ordered** the 1.2× scalar retired for a bimodal (08-07) … **the presentation quotes a
> worst-case ceiling as typical.***

**Recommendation:** BLUEPRINT §6 should keep the *shape* of the claim (a forming sentence killed by
the world; a turn-based twin structurally unable to see the moment) and drop both borrowed numbers
until nib measures its own. nib is going to produce a *better* twin number than 65.7 s anyway
(§3.5), and one it can defend.

## 7.4 BLUEPRINT §7's "fork at 0 MiB, co-decode at 1.208×" — correct but must carry its caveat

Both numbers verify: `SESSION_HANDOFF.md:71`, `SYNCYTIUM_M0_LOCAL.md:35`, `37`, `521–522`. But
`SESSION_HANDOFF.md:90` and `THE_COMPOUNDING_DEPLOYMENT_2026-08-15.md:144` both require the
bimodal caveat to travel with it:
> *Quote the bimodal, never the single scalar: 1.208× is a ceiling taken with all three branches
> decoding simultaneously. Reporting it flat understates your own economics by up to 15 %.*

nib BLUEPRINT §7 currently prints it flat. `THE_RESIDUAL_2026-08-15.md:48` counts *"eight public
pages"* already doing so.

## 7.5 `C:/auricle/src/tape/tape.h` is not the tape

Anything reading "auricle's tape module" and finding `src/tape/` will find a 27-line
project-memory stub marked *"stub until P4"* (`tape.h:25`) with no hash and no chain. The
hash-chained tape is `src/fabric/fabric.h` + `src/fabric/durable.h`. nib's `HANDOFF.md:99`
correctly points at `fabric.h`; anyone following the directory name will not.

## 7.6 nib SPEC 5.1.6 names only half the self-echo law

SPEC 5.1.6 states the *filter* (a seat-lane delta must not be fed back). It does not state the
*commit* (`fusord.cpp:548–561`) — that the emission enters the trunk directly on the seat's lane,
followed by `read_frontier()`. Both are required and they are not the same rule. §1.5 above.
Without the commit, nib reproduces the measured ten-repeats bug.

## 7.7 `fusord.cpp` declares three per-seat sequences and uses none of them

`fusord.cpp:122–130` gives each `Mind` a `seq` (1, 2, 3), and nothing in the file reads it —
generation uses the single shared `GEN = 6` (`fusord.cpp:298`, `475`). The per-seat seqs are live
only in `syncytium.cpp:190–192`, `starship.cpp:446–450`, `blackbox.cpp:934–935`. Harmless, but a
Stage 2 lift that copies `MINDS` wholesale inherits a dead field. nib's `Seat`
(`resident.h:41`) already drops it — keep it dropped until there is a second live composer, and
add `seat_seq(i) = i + 1` as a *function* (`blackbox.cpp:935`) rather than a struct field.

## 7.8 `k=hdr`'s `serve_hash` is not a model hash

nib SPEC 8.1.2 requires *"the model's hash"* on the tape. fusord records `serve_hash`
(`fusord.cpp:317`, `324`) — a 64-bit FNV over the **prompt literals** — and the model as a **path
string** (`fusord.cpp:316`). Nothing hashes the GGUF. `blackbox.cpp:152–177` shows the estate does
own a real SHA-256 (BCrypt) and uses it on *input artifacts*. **nib should hash the GGUF with it**
(and record both `model_sha256` and `serve_hash`, which answer different questions: "which weights"
and "which prompt distribution").

## 7.9 nib's Stage 1b silently drops percepts at the context wall

`resident.cpp:313` and `resident.cpp:339` `return` without ingesting once
`npast_ + n >= n_ctx - 512`, with no counter and no row. That is a silently dropped percept —
nib SPEC 5.1.4, CLAUDE.md rule 7, and `fusord.cpp:23–24` all forbid it. It is a stated Stage 1b
stub (the comment says *"Stage 1b does not molt yet"*), but the correct stub is **counted and
loud**, not silent. Either count it now, or land §4.3's molt in Stage 2.

## 7.10 `sizeof(Delta)` — the auricle side is still wrong

`source.h:32` still reads *"`kPayloadMax = 496; // sizeof(Delta) stays a tidy 512B`"*. nib includes
this header unmodified and correctly asserts 528 in its own selftest
(`nib/src/selftest.cpp:527–528`). Read-only task, so nothing was changed — noting it so the
estate-side comment can be fixed deliberately if desired.

## 7.11 **nib's idle ticks fire a full three-seat probe round. The estate's do not.**

This is the most consequential defect found, because it becomes a *hazard* the moment Stage 2 adds
an emit path.

- The estate: `fusord.cpp:709–719` decodes the tick onto the trunk, calls `read_frontier()`, writes
  the `k=tick` ledger row — and **never calls `judge_and_maybe_emit`**. `blackbox.cpp:1605` says
  why: *"ticks never trigger a probe round (QC-001 §4)"*. Doctrine at
  `BLACKBOX_SPEC_v0.1_QC-001.md:7` — the **S-QUIET compromise, anti-turn exemption 4**.
- nib: the tick is produced by the *compiler* as an ordinary `Percept` with `kind == 't'`
  (`ingest.cpp:77–90`), and `nib.cpp:175` feeds every percept identically —
  `res.feed(p.lane, p.text, p.wall_ms, 0, js);` — **with no branch on `p.kind`**.
  `Resident::feed` (`resident.cpp:334–358`) ends unconditionally with
  `if (!clause_.empty()) judge("f", 0.0f, out);`, so **a tick fires three probes**.

Harmless in Stage 1b, where a probe only produces a number. In Stage 2 it means **silence itself
can trigger an emission**, which is precisely the thing exemption 4 exists to forbid — and it is
the shape of Fake 1 (a clock waking the mind) reappearing inside an anti-turn loop.

**Fix:** `Resident::feed` must take the percept `kind` and, for `'t'`, decode the tick onto the
trunk + `read_frontier()` + record the row, then return without judging.

Note that nib's tick is otherwise *better* than the estate's: because the pad produces it, silence
has a first-class representation (bench rule **R4 · Time is a lane**,
`UNPROMPTED_BENCH_SPEC_v0_2026-08-14.md:119–122`), which is exactly what the estate wanted and only
approximated.

## 7.12 The self-echo law's current form is the OPPOSITE of what fusord implements

fusord (2026-08-12) *filters* its own speech at the ring (`fusord.cpp:704–707`). The corrected
law, dated later, is that there is **no self-exception at all**:

`C:/NEW/FUSOR_MASTER-ONEPAGER_2026-08-23_FABLE5.md:28`:
> *Percepts are never dropped … and, the newest correction, **with no self-exception**: the mind's
> own speech is a percept too, **or say-it-once is structurally unlearnable.***

`C:/NEW/MERIDIAN_SIGHT_SPEC_v0.1_FABLE5_2026-08-23.md:142`:
> ***Own acts are percepts.** … the firsthour daemon ran self-echo-filtered and paid for it in
> perseveration — **the one percept class the gate dropped was the mind's own voice, making
> say-it-once structurally unlearnable** … In MERIDIAN the law is written the other way:
> **L-GATE has no self exception.***

`C:/NEW/FUSOR_MASTER-ONEPAGER_ADDENDUM-F_THE-BUILD-SPEC_2026-08-24_FABLE5.md:79–80`:
> *`self` is the resident's own committed speech re-entering (**the no-self-exception law, wired**)*

**The implementable resolution** (`FUSOR_X_AURICLE_THE-STARSHIP-FIT_v0.1_FABLE5_2026-09-01.md:151`):
> ***Own speech is a percept.** Words that reached the air re-enter as lane `self` … **the gate
> skips seat lanes** (`fusord.cpp:704-707`) **so the mind hears itself without judging itself.***

So the two rules of §1.5 are the *trunk* half and the *gate* half, and the estate is explicit that
getting only one is a failure in either direction — filter it from the trunk and say-it-once cannot
be learned; feed it to the gate and each seat deliberates about interrupting itself.

**nib SPEC 5.1.6 currently states only the gate half.** It should be amended to state both, and
`PadSource`'s `is_seat` filter should be documented as the *gate* half rather than as the whole law.

The estate also records that this is still formally unresolved between two of its own binaries —
`C:/auricle/OPEN_PROBLEMS_2026-08-12.md:173–175`:
> *`soak` keeps emissions out of the trunk (pure). `fusord` puts them in (room). Both defensible;
> **decided by argument, not measurement, and now divergent in the same codebase.***

nib is a room. `pure` is false. Say so on the tape.

## 7.13 "21.1 % → 6.7 %" is grain-poisoned; the matched-grain pair is 63.4 % → 6.7 %

`C:/NEW/FUSOR_MASTER-ONEPAGER_2026-08-23_FABLE5.md:32`:
> *at matched grain the tune cut the fire rate **63.4 % → 6.7 % per decision-boundary (≈9.5×)** with
> the catches kept.*

The older pair mixes denominators (per-decision vs per-boundary). It appears in
`C:/NEW/BRAIN_RECONTEXT_FUSOR-AT-CENTER_2026-08-21.md:22` as *"21.1 % → 6.7 %"*. If nib's docs ever
quote the tune's effect, quote the matched-grain pair, per the number fence (§6.20).

## 7.14 nib's serve-hash pin is correct *because* nib's seats are byte-identical

Worth stating explicitly, because a natural reading of the estate's re-pinning doctrine
(`fusord.cpp:132–138`: *"Re-pinning is a DELIBERATE act that belongs in the same commit as a
retune"*) would suggest nib needs its own pin. It does not — nib's `SEED_SYS`, `SEED_EXAMPLES`,
`SEED_OPEN`, seat names and mandates, probe frame and cue frame are **byte-identical** to fusord's
(diffed field by field: `resident.cpp:19–45`, `61–66` vs `fusord.cpp:103–130`, `149–154`), which is
why `serve_hash()` returns the same `0xe7ffa5704ba31076`. nib is deliberately running v11 in v11's
own distribution. **The pin must change only if nib retunes**, and the `0 = unpinned → print and
refuse` bootstrap (`fusord.cpp:180–185`) is the right behaviour for that day — not a placeholder to
delete.

## 7.15 Mandate width is the dominant uncontrolled variable, and nib inherits all three seats

`C:/auricle/UNPROMPTED_AMENDMENTS_2026-08-15.md:24`:
> *The estate has measured it in five independent regimes: soak split **SENTINEL 1,630 / SKEPTIC
> 178 / SPEAKER 9** (90 % of fires from the vaguest mandate) · full-day Run C **2,316/2,823**
> (82 %) … live hour **160/169**. **Mandate width buys volume, not conviction.***

nib carries all three mandates verbatim, so **expect SENTINEL to dominate nib's Stage 2 emissions
by an order of magnitude** — and expect that to be a property of the mandate's vagueness, not of
the pad. The estate's answer is per-seat dials (`fusord`'s successor work; measured vindication at
`C:/auricle/runs/fusor-firsthour-2026-08-12.md:89–94`, where
`(spk ∞ / skp 1.96 / sen 3.00)` retained both catch classes where a global dial could not), and
`blackbox.cpp:826–827`, `1098` implements per-seat dials as `dial_of[i]`.

**But note §6.8: a dial is not the fix, and the estate has already measured that.** For nib Act I
with one composer, the more useful move is to say plainly which seat spoke on every `emit` row and
let Stage 5's week decide whether SENTINEL earns its mandate.

## 7.16 Margins are only meaningful near contention

`C:/NEW/FUSOR_MASTER-ONEPAGER_2026-08-23_FABLE5.md:44`:
> *Margins are read only near contention (**in the deep tail they measure phrasing, not judgment**
> — surface variance 5.94× meaning).*

nib's Stage 1b prints every margin (`nib.cpp:177–181`) and its Stage 2 plan pacing on the margin
(§6.10). Both are fine — but a margin of −11 and a margin of −4 are not a meaningful ordering, and
nib must not build an interface (or a pacing curve) that implies they are.

---

# 8 · WHAT TO LIFT, ADAPT, OR WRITE FRESH — ordered

Ordered by dependency. **V** = verbatim, **A** = adapt, **F** = fresh.

## Stage 2 — emission with floor control

| # | | what | source | notes |
|---|---|---|---|---|
| 1 | **V** | the speak-cue construction | `fusord.cpp:477–480` | already hashed in nib; rebuild from the same pieces `resident.cpp:62–66` covers |
| 2 | **V** | the sampler chain | `fusord.cpp:285–288` | min-p 0.05 → temp 0.7 → dist(11); no top-k/top-p/penalty/grammar. Record the seed on the tape |
| 3 | **V** | fork/drop around composition | `fusord.cpp:475–476`, `504` | `seq_rm(GEN)` → `seq_cp(TRUNK→GEN)` → decode cue → … → `seq_rm(GEN)` |
| 4 | **V** | the generation loop and its four stops | `fusord.cpp:489–503` | 28-token cap; EOG; newline-in-piece; sentence-final after `t >= 6` |
| 5 | **A** | per-token surface (the pad renders as it forms) | `starship.cpp:602–635` | fusord prints a finished line; nib needs the streaming shape. Drop the audio pacing; the pad has no mouth |
| 6 | **V** | **the self-echo commit** | `fusord.cpp:555–561` | `\n[SEAT] ` + say → `TRUNK` at `npast_`, then `read_frontier()`, then push to the molt tail. **The one structurally-absent piece in nib.** |
| 7 | **F** | **the floor rule** | — | `dep`/`contra` sets over block ids; a block touched within 2 s is a `contra`; refuse **before** composing (nib CLAUDE.md rule 4). Shape it exactly like `fabric.h:274–285` so Stage 3 reuses it |
| 8 | **A** | placement: "after the block the human most recently completed" | one-pager `:40` (placement law) + BLUEPRINT §5 | serializer-enforced, never manners |
| 9 | **A** | the near-dup damper | `blackbox.cpp:635–636`, `640–662`, `1483–1510` | Jaccard ≥ 0.60 over a per-mind window of 8; **prefer this over `fusord.cpp:364–442`** (25 lines vs 130) |
| 10 | **A** | resolve-permanence | `fusord.cpp:381`, `450–458`, `516–529` | in a pad the acceptance signal is a *changeset on the resident's block*, not a word list — strictly better evidence |
| 11 | **A** | floor-yield-and-re-round | `blackbox.cpp:1471–1507` | empty / punctuation-only / dampened ⇒ yield, re-run the same occasion excluding the yielder |
| 12 | **V** | the emit/hold ledger arithmetic | `fusord.cpp:579–593` | `events = emit + hold`, by addition (the 12×-high bug) |
| 13 | **A** | the emit rate expressing internal state | BLUEPRINT §5 | pace on the **margin** (a real forward-pass number); thin margin ⇒ slower. Nothing else |

## Stage 3 — un-saying made visible

| # | | what | source | notes |
|---|---|---|---|---|
| 14 | **V** | branch fork/drop as the whole un-say | `syncytium.cpp:210–211` / `246`; `m0_demo.cpp:179` | `seq_rm(branch, -1, -1)`. The trunk was never dirtied — `m0_demo.cpp:182–183` |
| 15 | **A** | `contradicts ∩ dep` hard-kill | `fabric.h:274–285` + `emit_abort` `290–302` | payload-blind set test. In nib: a committed changeset touching a block in the forming region's `dep` |
| 16 | **A** | reflex soft-pause | `fabric.h:203–212` | a keystroke in flight pauses; a committed changeset kills. **L-COMMIT** |
| 17 | **A** | the abort record | `starship.cpp:544–552` (`cut_audio`) | `formed` · `reached_air` · `mid_word` · `live` · `reason` · `trigger_rev` · `dep` · `ms_to_withdraw` |
| 18 | **V** | the honest mid-word/live disclosure | `starship.cpp:526–543` | say so when the cut landed on a word boundary; assert the branch was genuinely mid-decode |
| 19 | **A** | the re-form after the stop | `starship.cpp:893–915` | fresh branch, corrected world, real generation. **Follows the STOP, not the mechanism** (`874–876`) |
| 20 | **V** | the BEAT ASSERTION | `starship.cpp:929–981`, exit 2 at `999` | into `nib --selftest`: stop ⇒ abort row + reform row must exist; no stop ⇒ neither may |
| 21 | **V** | narration derived from fields | `starship.cpp:828–832` | status line and `--about` computed from tape rows, never written beside them |
| 22 | **F** | the provisional render band | `cortex.h:52–67`, `hud.h:31–44` (band A committed / band B dream) | conceptual model only; nib's is a Win32 text surface, so the *type fence* is nib's own |
| 23 | **V** | **partials never persist, as a type** | `durable.h:5–6`; `m0_tape.cpp:9`, `97`; `IT_WAS_ALWAYS_NEXT_TOKEN:238` | *"writing a partial into the tape is a compile error, not a policy"* — nib SPEC 6.4.3 |
| 24 | **DO NOT LIFT** | surprisal as an abort gate | `starship.cpp:758–783` | `FUSOR_HARNESS_SPEC.md:123`: inverted, no threshold separates, *"recalibration is dead as a fix — do not retry it"* |
| 25 | **A** | surprisal as an *instrument*, with header/content split | `starship.cpp:706–727`; `sm_stats` `108–113` | log it, disclose it (`falter_suppressed`), gate on nothing |
| 26 | **F** | nib's own keystroke→withdrawn latency | — | do **not** inherit 13 µs (§2.6, §7.3). Report `seq_rm` separately if at all |
| 26a | **V** | **barge-in is a percept, not a cancel** | `WHOLE-MACHINE:52`; `STARSHIP-FIT:151` | the killing keystroke enters the trunk *first*, so the re-form is informed rather than retried |
| 26b | **V** | the abort surface must be *reachable*, and the run must assert it | `BLACKBOX_SPEC_v0.1_QC-001.md:26` | *"Every `aborts=0` … is therefore structural, not measured"* — Stage 3's falsifier runs on the real clock, through the real window |

## Stage 4 — the two switches and the paired record

| # | | what | source | notes |
|---|---|---|---|---|
| 27 | **A** | the turn-based twin | `blackbox.cpp:1243–1354` | full re-prefill from pos 0 per wake; `seq 1`; same seed_sys, same sampler, same cap. **Not** `starship.cpp`'s canned `twin_line` |
| 28 | **V** | per-arm config honesty (QC-005) | `blackbox.cpp:840–858`, esp. `848–850` | `dials`/`damper` are `null` on the turn arm. *"never a number it never used"* |
| 29 | **V** | both arms receive the tick | `blackbox.cpp:1599–1603` | the one site that missed the fork and *"quietly handicapp[ed] the control arm"* |
| 30 | **V** | mid-composition world is blind on the turn arm, and counted | `blackbox.cpp:1272–1293`, `1349`; `twin_blind_ct` | the product claim in one integer |
| 31 | **V** | sampler-consultation count on the tape | `blackbox.cpp:899–902`, `1638–1639` | *"a sweep over an unconsulted run is vacuous by record"* |
| 32 | **V** | probe/cue parity gate as a build gate | `blackbox.cpp:613–629`, `702–734` (`--print-probe`) | one source of truth for the serve bytes + a diff against a labelled frozen literal. nib's `serve_hash` is the same idea; add the *printing* gate too |
| 33 | **V** | config in the output path; a run never overwrites a run | `blackbox.cpp:1726–1739` | |
| 34 | **A** | the F-ZOMBIE summary | `blackbox.cpp:1654–1679` | for nib: per switch-flip, the gap between a world event and the turn arm's next trigger |
| 35 | **F** | the pairing caveat | — | nib pairs are **observational, not matched-input** (§3.5). Say so on the tape and in the write-up |

## The molt (needed by Stage 2's first long session)

| # | | what | source | notes |
|---|---|---|---|---|
| 36 | **A** | the molt | `fusord.cpp:599–650`; provenance `m0_molt.cpp:173–204` | at `n_ctx 8192`: `molt_wm ≈ 3072`, rung ≤ 256 tok, tail ≤ 200 words — **measure the reseed size** |
| 37 | **V** | reseed re-decodes the frozen seed verbatim | `fusord.cpp:629` | the one runtime re-assertion of train ≡ serve |
| 38 | **V** | force-judge the open clause before folding | `fusord.cpp:686` (reason `"m"`) | |
| 39 | **V** | the outage tick | `fusord.cpp:636–641` | the second tick format, which nib does not yet have |
| 40 | **V** | `k=molt` on the tape with the full rung | `fusord.cpp:645–648` | |
| 41 | **F** | fix the silent context-wall drop | `resident.cpp:313`, `339` | count it loudly now; molt properly in Stage 2 |

## The tape (cross-cutting)

| # | | what | source | notes |
|---|---|---|---|---|
| 42 | **V** | **the whole tape module** | **port `C:/fray/src/tape.{h,cpp}`** | 8 mechanical hunks off glance's: namespace, include, `wstring`→`string`, `CreateFileW`→`CreateFileA`. Gives BLAKE2b-256, the canonicalizer, the chain, `verify_file`, and refuse-to-append-to-a-broken-chain, all already cross-checked against REGISTRAR's Python |
| 43 | **V** | the BLAKE2b RFC vectors as a selftest gate | `C:/caseclock/tests/expected/hash-vectors.json`; `C:/caseclock/src/hash.h:1–4` | including the 127/128/129 cases — that is where transcriptions break |
| 44 | **V** | the pinned cross-implementation payload+digest vector | `C:/glance/src/selftest.cpp:445–450` | proves nib's bytes equal REGISTRAR's |
| 45 | **V** | the float-repr and escape rules | `C:/glance/src/tape.cpp:129–163`; `C:/caseclock/src/json.h:3–8` | DEL raw, non-ASCII raw, Python shortest-round-trip floats with a `.0` suffix |
| 46 | **F** | the header line and the `kind` vocabulary | §5.3, §5.5 | `{"case_id":"nib:<slug>","tool":"nib","version":…,"t0":…}`; kinds are free-form |
| 47 | **V** | run `glance --verify` on nib's tape in `--selftest` | `C:/glance/docs/devlog.md:50` (the standing acceptance check) | the falsifier for SPEC 8.1.1, and it is one shell line |
| 48 | **A** | author stamped from the lane, never declared | `fabric.h:257–259` | nib SPEC §7 — semantics only; the record is the family's |
| 49 | **A** | `op_id` idempotency; typed, counted refusals | `fabric.h:250–263`, `332–333` | a pad retries on reconnect (Act II) |
| 50 | **V** | a deliberate corruption hook for the falsifier | `fabric.h:230–232` | flip a byte, assert verify localizes the row |
| 51 | **V** | refuse to run without the hash primitive | `blackbox.cpp:155–156`, `174` (exit 5) | no placeholder where a hash belongs. nib's BLAKE2b is in-tree, so the analogue is: **a failed tape write is fatal**, `BLACKBOX_SPEC_v0.1_QC-001.md:70` |
| 52 | **V** | SHA-256 over input artifacts before the model loads | `blackbox.cpp:152–177`, `814–821` | hash the **GGUF** — §7.8 |
| 53 | **V** | reject unknown row kinds loudly on read | `blackbox.cpp:1713` (`UNCHECKED (unknown key)`) | glance's verifier ignores `kind`; nib's own reader must not |
| 54 | **F** | the row bodies | §5.5 | `session` · `mandate` · `coefficient` · `switch` · `changeset` · `judgment` · `hold` · `emit` · `abort` · `molt` · `tick` · `end` |
| 55 | **V** | the number fence on every rendered metric | `WHOLE-MACHINE:3`, `:100`; `AMENDMENTS:64` | {value, grain, denominator, source}; a bare number fails the build |
| 56 | **V** | dual derivation for every headline number | `AMENDMENTS:70–72` | 16 instrument lies in 8 days, every one caught by a second surface disagreeing |

## Corrections to land alongside (found in this survey)

| # | | what | where | notes |
|---|---|---|---|---|
| 57 | **fix** | **ticks must not fire a probe round** | `nib/src/resident.cpp:334–358`, `nib/src/nib.cpp:175` | §7.11 — `feed()` must branch on `Percept::kind`; `'t'` decodes and returns. Anti-turn exemption 4, `BLACKBOX_SPEC_v0.1_QC-001.md:7` |
| 58 | **fix** | **count the context-wall drop, or molt** | `nib/src/resident.cpp:313`, `339` | §7.9 — a silent `return` is a silently dropped percept |
| 59 | **doc** | SPEC 5.1.6 should state **both** halves of self-echo | `nib/docs/SPEC.md:316–319` | §7.12 — trunk half + gate half; `MERIDIAN_SIGHT_SPEC:142` (*"L-GATE has no self exception"*) |
| 60 | **doc** | BLUEPRINT §6 should drop the borrowed 13 µs / 65.7 s pairing | `nib/docs/BLUEPRINT.md` §6 | §7.3 — two experiments, two clocks, not the same moment |
| 61 | **doc** | BLUEPRINT §7's 1.208× needs its bimodal caveat | `nib/docs/BLUEPRINT.md` §7 | §7.4 — `SESSION_HANDOFF.md:90` |
| 62 | **build** | print `coarse_frac` beside every fire rate, per lane | Stage 2 | §6.16 — >40 % coarse = not a valid disposition sample (`FUSOR_DECISIONS:39–42`) |
| 63 | **build** | `hold` rows carry `last_margin` (the escalation test) | Stage 2 | §6.13 — a re-raise above `last_margin + δ` is escalation, not noise |
| 64 | **build** | every Stage 2 scaffold individually switchable, recorded on the tape | Stage 2 | §6.22 — the scaffolding-ratio receipt |
