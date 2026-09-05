# nib — BACKLOG

*Open items, each with the date it was raised and where it came from. An item leaves this file when
it lands (named with its version in the ROADMAP) or when it is deliberately not done (ROADMAP,
"Deliberately not doing"). Nothing here is a promise about order except the first section.*

## Landed in 0.8.0 (Stage 1d, 2026-09-05)

Word wrap with its toggle · the checkpoint beside the document with its sidecar, the restore and
the twin · the resume tick · the hash cache · free VRAM on every judgment row and the status line ·
torn-row recovery · a crash that names itself. The ROADMAP's Stage 1d entry has the numbers. What
follows is what is still open.

## Still open from Stage 1d

- **The driver's windows sit on the operator's screen** (no-activate, but shown; the screenshot of
  2026-09-05 is one of them). Place them off-screen rather than hide them: a hidden window gets no
  `WM_PAINT`, and the latency instrument measures keystroke to painted.
- **The fold at prefill speed**, for the first switch-on over a document and for the twin. The
  checkpoint means the fold is normally only the world since it, so this is no longer on the hot
  path, but a first fold over a long document still costs 39 s for 4.2 KB at word grain.
- **The `.prev` generation is written but never read.** `Resident::checkpoint` keeps it and
  `Resident::start` does not fall back to it when the current one fails to load (K5 does). One
  more `try_load` and a `boot_reason` of `restored_prev`; needs a fault-injection test to be worth
  anything, which is K5's T6.
- **The trunk is not bounded.** Nothing prunes `<doc>.trunk.bin` (58.8 MB a document) or the tape.
  A week of real work will say what that costs; Stage 5 is where it matters.

## Later

- **A string-bearing frame instead of auricle's fixed Delta** (SPEC 5.1.1, 5.1.7). K5 abandoned the
  496-byte payload and the 15-character lane; nib's chunking, lane check and lockstep meta ring
  exist only to work around them. At Stage 2, when the frame gains a `grain`.
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
