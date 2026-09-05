// epcs.mjs — one place that loads the real Etherpad library, so every other script
// in this harness just does `import {CS, AttributePool} from './epcs.mjs'`.
import './hooks.mjs';
import { pathToFileURL } from 'node:url';

const EP = (f) => pathToFileURL('C:/etherpad-develop/src/static/js/' + f).href;
const HELPER = pathToFileURL('C:/etherpad-develop/src/tests/backend-new/easysync-helper.ts').href;

export const CS = await import(EP('Changeset.ts'));
export const { default: AttributePool } = await import(EP('AttributePool.ts'));
export const { default: AttributeMap } = await import(EP('AttributeMap.ts'));
export const { default: attributes } = await import(EP('attributes.ts'));
export const helper = await import(HELPER);

// ---- deterministic seeding -------------------------------------------------------------
// Etherpad's own generators (randomMultiline / randomTestChangeset) call Math.random()
// directly; the "randomSeed" in its test names is only a label. Swap in a seeded PRNG
// (mulberry32) around a call and the generators become reproducible without touching a
// single byte of the Etherpad tree.
export const seeded = (seed, fn) => {
  const real = Math.random;
  let a = (seed >>> 0) + 0x6d2b79f5;
  Math.random = () => {
    a |= 0; a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
  try { return fn(); } finally { Math.random = real; }
};
