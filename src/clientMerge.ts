// Saving a client portal without undoing someone else's work.
//
// The portal keeps a client (settings, media list, bug tickets, sheet data) in one database record, and the
// screens save the whole record. When two people had the same portal open, whoever saved last put back their
// older copy of everything they had not touched. These functions work out what the person saving actually
// changed, compared with the version they started from, so only that is written on top of the current record:
//   - a field they did not touch is left as it is now in the database;
//   - in a list of items with ids (media, bug tickets), only the items they added, changed or removed are applied.

type Item = { id: string; [key: string]: unknown };

const same = (a: unknown, b: unknown): boolean => JSON.stringify(a) === JSON.stringify(b);

const isItemList = (value: unknown): value is Item[] =>
  Array.isArray(value) && value.every((item) => !!item && typeof item === "object" && typeof (item as Item).id === "string");

// base: the list the editor started from; mine: the list they are saving; current: the list in the database now
export function mergeItemLists(base: Item[], mine: Item[], current: Item[]): Item[] {
  const baseById = new Map(base.map((item) => [item.id, item]));
  const mineById = new Map(mine.map((item) => [item.id, item]));
  const editedByMe = (id: string): boolean => mineById.has(id) && !same(mineById.get(id), baseById.get(id));

  const merged = current
    .filter((item) => !(baseById.has(item.id) && !mineById.has(item.id))) // I removed it
    .map((item) => (editedByMe(item.id) ? (mineById.get(item.id) as Item) : item)); // I changed it
  const present = new Set(merged.map((item) => item.id));
  // What I added, and what I changed that someone else removed meanwhile (my edit is kept)
  for (const item of mine) if (!present.has(item.id) && editedByMe(item.id)) merged.push(item);
  return merged;
}

export const REMOVED = Symbol("removed");

// The fields to write: only those the editor changed. A field they removed maps to REMOVED.
export function changedFields(
  base: Record<string, unknown>,
  mine: Record<string, unknown>,
  current: Record<string, unknown>,
  skip: string[] = [],
): Record<string, unknown | typeof REMOVED> {
  const changes: Record<string, unknown | typeof REMOVED> = {};
  for (const key of new Set([...Object.keys(base), ...Object.keys(mine)])) {
    if (skip.includes(key) || same(mine[key], base[key])) continue;
    if (!(key in mine) || mine[key] === undefined) {
      changes[key] = REMOVED;
    } else if (isItemList(mine[key]) && isItemList(base[key] ?? []) && isItemList(current[key] ?? [])) {
      changes[key] = mergeItemLists((base[key] as Item[]) ?? [], mine[key] as Item[], (current[key] as Item[]) ?? []);
    } else {
      changes[key] = mine[key];
    }
  }
  return changes;
}
