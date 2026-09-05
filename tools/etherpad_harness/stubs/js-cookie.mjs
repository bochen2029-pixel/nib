// Minimal stand-in for the js-cookie npm package. pad_utils imports it at module scope
// but the changeset algorithm never calls it.
const api = {
  get: () => undefined,
  set: () => undefined,
  remove: () => undefined,
  withAttributes: () => api,
  withConverter: () => api,
};
export const CookiesStatic = undefined; // type-only import in pad_utils.ts
export default api;
