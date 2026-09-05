// hooks.mjs — pure-stdlib module hooks so Node 24 can import Etherpad's TypeScript
// sources straight off disk, with NO npm install anywhere and NO write to the Etherpad tree.
//
// Three problems to solve:
//   1. Etherpad writes extensionless relative specifiers ("./AttributeMap"). Node's ESM
//      resolver requires an extension. RESOLVE hook appends ".ts" (then ".js", then "/index.ts").
//   2. Changeset.ts -> pad_utils.ts -> js-cookie (an npm package that is not installed and
//      that the changeset algorithm never touches). RESOLVE hook redirects bare specifiers
//      to local stubs.
//   3. Node's type STRIPPING is erasure-only: it cannot tell `import {Attribute} from
//      './types/Attribute'` (a type) from a value import, so it leaves the import in and the
//      module fails with "does not provide an export named 'Attribute'". LOAD hook appends a
//      dummy `export const <Name> = undefined;` for every `export type` / `export interface`
//      the file declares. The type alias itself is erased, so there is no collision.
import { registerHooks } from 'node:module';
import { existsSync, readFileSync } from 'node:fs';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { dirname, resolve as presolve } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));
const stub = (name) => pathToFileURL(presolve(here, 'stubs', name)).href;

const BARE = new Map([
  ['js-cookie', stub('js-cookie.mjs')],
  ['jquery', stub('empty.mjs')],
  ['underscore', stub('empty.mjs')],
]);

const tryExts = (base) => {
  for (const ext of ['.ts', '.mts', '.js', '.mjs', '/index.ts', '/index.js']) {
    if (existsSync(base + ext)) return base + ext;
  }
  return null;
};

registerHooks({
  resolve(specifier, context, nextResolve) {
    if (BARE.has(specifier)) return { url: BARE.get(specifier), shortCircuit: true };
    if (specifier.startsWith('./') || specifier.startsWith('../')) {
      const parentPath = context.parentURL && context.parentURL.startsWith('file:')
        ? fileURLToPath(context.parentURL) : presolve(here, 'x');
      const abs = presolve(dirname(parentPath), specifier);
      if (!existsSync(abs)) {
        const hit = tryExts(abs);
        if (hit) return { url: pathToFileURL(hit).href, shortCircuit: true };
        return { url: stub('empty.mjs'), shortCircuit: true };
      }
    }
    try {
      return nextResolve(specifier, context);
    } catch (e) {
      if (!specifier.startsWith('.') && !specifier.startsWith('node:')) {
        return { url: stub('empty.mjs'), shortCircuit: true };
      }
      throw e;
    }
  },

  load(url, context, nextLoad) {
    if (!url.startsWith('file:') || !url.endsWith('.ts')) return nextLoad(url, context);
    const src = readFileSync(fileURLToPath(url), 'utf8');
    const names = new Set();
    for (const m of src.matchAll(/^\s*export\s+(?:type|interface)\s+([A-Za-z_$][\w$]*)/gm)) {
      names.add(m[1]);
    }
    let extra = '';
    for (const n of names) extra += `\nexport const ${n} = undefined;`;
    return {
      format: 'module-typescript',
      source: src + extra,
      shortCircuit: true,
    };
  },
});
