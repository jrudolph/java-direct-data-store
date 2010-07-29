/* Relocation hash table structures
 */

#include "dump.h"
#include "reloc.h"

struct RelocEntry {
  oop old;
  oop new;
};

typedef struct RelocEntry *Entry;

unsigned long RelocEntry_hash(const Entry tohash)
{
  return (unsigned long) tohash->old;
}

int RelocEntry_cmp(const Entry arg1, const Entry arg2)
{
    return ((unsigned long)arg1->old) - ((unsigned long)arg2->old);
}

IMPLEMENT_LHASH_HASH_FN(RelocEntry_hash, const Entry)
IMPLEMENT_LHASH_COMP_FN(RelocEntry_cmp, const Entry);

HashTable create_hash_table()
{
  return lh_new(LHASH_HASH_FN(RelocEntry_hash),
                LHASH_COMP_FN(RelocEntry_cmp));
}

void free_hash_table(HashTable t)
{
  // TODO: free all entries
  lh_free(t);
}



void put(HashTable table, oop old, oop new)
{
  Entry e = malloc(sizeof(struct RelocEntry));
  e->old = old;
  e->new = new;
  lh_insert(table, e);
}

oop find(HashTable table, oop old)
{
  struct RelocEntry e;
  e.old = old;
  Entry res = lh_retrieve(table, &e);
  if (res)
    return res->new;
  else
    return 0;
}
