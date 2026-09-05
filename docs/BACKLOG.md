# nib — BACKLOG

*Open items, each with the date it was raised and where it came from. An item leaves this file when
it lands (named with its version in the ROADMAP) or when it is deliberately not done (ROADMAP,
"Deliberately not doing"). Nothing here is a promise about order except the first section.*

## Stage 1d — the trunk as an asset (next, 2026-09-05)

- **Word wrap.** *Operator, 2026-09-05.* There is none, there is no toggle, and a long line runs off
  the right edge with no horizontal scroll, so the window can hide text with no sign that it has.
  Wrap on by default for prose, a toggle on the status line and the tape (every state that changes
  what is seen is a state), and a horizontal scroll when wrap is off. The column arithmetic of SPEC
  4.1.1 stays: a wrapped line is several rows of one line.
- **The checkpoint beside the document.** The trunk's KV and token list saved atomically next to the
  tape at switch-off and at quiet, with a sidecar carrying the model's SHA-256, the serve hash, the
  token count, the document revision, the tape head and the wall time of the last percept; restored
  at switch-on with every field checked, else the fold and the label *twin*. The idea is K5's
  (`C:\fusor1\converge\src\fusord.cpp`, `checkpoint` / `restore`, 2026-09-04), and the corpus's own
  amendment to the fold law: a resident rebuilt from its log is the twin. Measured there: about
  53 MB fixed plus 17 KB per token on this model, so about 340 MB at nib's 16k window; writes of
  60 MB took 56–92 ms. Replaces a fold that costs 39 s for 4.2 KB (ROADMAP, Stage 1c).
- **The resume tick.** On restore, and on a fold that resumes a tape, `[tick +Ns]` for the wall gap
  since the last percept, so the mind is told how long the world went on without it. K5's F5.
- **The hash cache.** SHA-256 of a 6.6 GB GGUF costs 17 s at 388 MB/s (2026-09-05). Cache it on the
  file's size and mtime under `runs/`, record `hash_cached` on the session row, and re-hash when
  either moves; the first switch-on of a model pays once.
- **Free VRAM on every judgment row.** The probe cost on this card ran 118–123 ms per boundary on a
  quiet card and 312–327 ms on the same binary an hour earlier with llama-server busy
  (2026-09-05). A slow probe with no VRAM number beside it reads as a slow resident. K5 puts
  `mib_free` on every boundary row from `ggml_backend_dev_memory`; nib should, and on the status
  line.
- **Torn-row recovery.** A crash inside a tape write leaves a torn last line; today `Tape::open`
  refuses the file and the session runs untaped behind a status message. Skip the fragment, chain
  from the last complete row, write a `warn` row naming the bytes skipped. K5's F6.
- **The driver's windows sit on the operator's screen** (no-activate, but shown; the screenshot of
  2026-09-05 is one of them). Place them off-screen rather than hide them: a hidden window gets no
  `WM_PAINT`, and the latency instrument measures keystroke to painted.

## Later

- **A string-bearing frame instead of auricle's fixed Delta** (SPEC 5.1.1, 5.1.7). K5 abandoned the
  496-byte payload and the 15-character lane; nib's chunking, lane check and lockstep meta ring
  exist only to work around them. At Stage 2, when the frame gains a `grain`.
- **The fold at prefill speed.** Decode each folded percept as one batch and judge only at the
  compiler's closed thoughts, or not at all; the trunk's bytes are identical either way and the
  cost falls by an order of magnitude. Second answer to the switch-on cost, after the checkpoint.
- **The T sweep** (SPEC 14.3). Needs a real typing tape; the synthetic cadence of `--ingest` and
  `--resident` never pauses, so T never fires.
- **The `Hand` table** (review §5.8): one table for authors, lanes, colours and the self-echo set.
  When the seats write, Stage 2.
- **A lane-contract v0.1 spool beside the document**, K5's intake format
  (`t_mono_ns \t lane \t grain \t text` under a `#lane-contract` header), projected from the
  tape's percept rows, so an external kernel can watch a pad. Act II.
- **`nib --verify` reading K5's chain** (`h = blake2b(prev_hex ‖ body)`) beside the family's, so
  one verifier walks every tape on this machine.
- **The consistency fence** (review §8): a selftest that pairs every count, version and date across
  README, SPEC, ROADMAP, HANDOFF and the devlog.

## For the estate, not for nib (read in K5 on 2026-09-05; the kernel is in flux, so noted, not filed)

- K5 loads backends with `ggml_backend_load_all_from_path` (fusord.cpp:1395), which pulls
  `ggml-rpc.dll` and with it ws2_32 into its process; its header says the kernel never opens a
  socket. nib's `load_backends` and `module_gate` in `src/resident.cpp` close that by name.
- Two chain formats exist now: K5's `prev`/`h` rows over the raw body, and the family's six-key
  REGISTRAR rows that `glance --verify` reads. Neither verifier reads the other's tape.
- K5's smoke drivers (`converge/smoke/drive.py`) crashed on a missing header row in T5 and T6, and
  T3 timed out at boot; none of the three completed as a test on 2026-09-05.
