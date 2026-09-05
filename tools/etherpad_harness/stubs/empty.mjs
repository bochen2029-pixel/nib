// An empty module, used to satisfy imports that exist only for their types.
const handler = { get: () => undefined };
export default new Proxy({}, handler);
