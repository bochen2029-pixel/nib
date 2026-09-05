// gen.mjs — stands in for `nib.exe --diff <seed> <n>` while nib's C++ side does not exist.
// It generates the SAME case corpus nib should generate, and fills `got` from the real
// library so that `check.mjs` reports 100% (proving the plumbing). Pass `--corrupt N` to
// damage case N and prove the checker actually catches a byte-level disagreement.
//
//   node gen.mjs 7 > cases.jsonl
//   node gen.mjs 7 --corrupt 3 > bad.jsonl
import { CS, AttributePool, helper, seeded } from './epcs.mjs';

const seed = Number(process.argv[2] ?? 1);
const corruptIdx = process.argv.includes('--corrupt')
  ? Number(process.argv[process.argv.indexOf('--corrupt') + 1]) : -1;

// The four-attribute pool Etherpad's own randomised tests use.
const POOL = () => {
  const p = new AttributePool();
  for (const kv of ['apple,', 'apple,true', 'banana,', 'banana,true']) p.putAttrib(kv.split(','));
  return p;
};

const cases = [];
const push = (c) => { c.id = cases.length + 1; cases.push(c); };

for (let s = seed; s < seed + 6; s++) {
  seeded(s, () => {
    const start = `${helper.randomMultiline(10, 20)}\n`;
    const [c1, t1] = helper.randomTestChangeset(start, true);
    const [c2, t2] = helper.randomTestChangeset(t1, true);
    const [c3] = helper.randomTestChangeset(t2, true);

    const pool = POOL().toJsonable();
    push({ op: 'checkRep', cs: c1, got: true });
    push({ op: 'applyToText', cs: c1, orig: start, got: t1 });
    push({ op: 'compose', cs1: c1, cs2: c2, pool, got: CS.compose(c1, c2, POOL()) });
    push({ op: 'compose', cs1: c2, cs2: c3, pool, got: CS.compose(c2, c3, POOL()) });
    push({ op: 'follow', cs1: c1, cs2: helper.randomTestChangeset(start, true)[0], rev: false, pool,
      got: null });
    // splices, the editor's hot path
    for (const [st, nd, ins] of [[0, 0, 'x'], [1, 1, ''], [2, 0, 'a\nb'], [start.length, 0, 'tail']]) {
      push({ op: 'splice', orig: start, start: st, ndel: nd, ins, got: CS.makeSplice(start, st, nd, ins) });
    }
    // an authored insert, with the pool the changeset needs
    const p = new AttributePool();
    const authored = CS.makeSplice(start, 0, 0, 'hi', [['author', `a.seed${s}`]], p);
    push({ op: 'splice', orig: start, start: 0, ndel: 0, ins: 'hi',
      attribs: [['author', `a.seed${s}`]], got: authored });
    // inverse, against line/aline views of the same text
    const lines = start.match(/[^\n]*\n/g) ?? [start];
    const alines = CS.splitAttributionLines(CS.makeAttribution(start), start);
    push({ op: 'inverse', cs: c1, lines, alines, pool,
      got: CS.inverse(c1, lines, alines, POOL()) });
  });
}
// fill in the follow cases now that both sides exist
for (const c of cases) {
  if (c.op === 'follow' && c.got === null) c.got = CS.follow(c.cs1, c.cs2, c.rev, POOL());
}
if (corruptIdx > 0 && cases[corruptIdx - 1]) {
  const c = cases[corruptIdx - 1];
  c.got = typeof c.got === 'string' ? `${c.got}X` : !c.got;
}
for (const c of cases) process.stdout.write(JSON.stringify(c) + '\n');
