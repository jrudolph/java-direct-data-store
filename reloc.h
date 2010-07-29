#include <openssl/lhash.h>

typedef LHASH *HashTable;

HashTable create_hash_table();
void free_hash_table(HashTable t);
void put(HashTable table, oop old, oop new);
oop find(HashTable table, oop old);
