/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <data_struct/cvector.h>
#include <data_struct/chash.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h> 

// An algorithm proposed by Donald E. Knuth in The Art Of Computer Programming Volume 3, under the
// topic of sorting and search chapter 6.4. 
// Src: http://www.partow.net/programming/hashfunctions/
static unsigned int
__DEKHash(char* str, unsigned int len)
{
   unsigned int hash = len;
   unsigned int i    = 0;

   for(i = 0; i < len; str++, i++)
   {
      hash = ((hash << 5) ^ (hash >> 27)) ^ (*str);
   }
   return hash;
}

static inline uint32_t
chash_hash(uint32_t key, uint32_t md)
{
    char *s = ((char*)(&key));
    size_t slen = sizeof(key);
    return __DEKHash(s, slen) % md;
}

void
chash_init(chash_t *t, size_t sz)
{
    assert(t);
    t->table = malloc(sizeof(cvector_t) * sz);
    assert(t->table);
    t->tableSize = sz;
    for (int i = 0; i < t->tableSize; i++) {
        cvector_init(&t->table[i]);
    }
}

void
chash_release(chash_t *t)
{
    if (!t) {
        return;
    }
    if (t->table) {
        for (int i = 0; i < t->tableSize; i++) {
            int c = cvector_count(&t->table[i]);
            for (int j = 0; j < c; j++) {
                chash_entry_t* entry = (chash_entry_t*) cvector_get(&t->table[i], j);
                if (entry) {
                    kfree(entry);
                }
            }
            cvector_free(&t->table[i]);
        }
        free(t->table);
    }
    t->table = NULL;
    t->tableSize = 0;
}

static chash_entry_t*
chash_get_entry(chash_t *t, uint32_t h, uint32_t key, int *index)
{
    size_t c = cvector_count(&t->table[h]);
    for (int i = 0; i < c; i++) {
        chash_entry_t* entry = (chash_entry_t*) cvector_get(&t->table[h], i);
        assert(entry);
        if (entry->key == key) {
            if (index != NULL) {
                *index = i;
            }
            return entry;
        }
    }
    return NULL;
}

chash_item_t
chash_get(chash_t *t, uint32_t key)
{
    // This function does _NOT_ give ownership over to caller.
    uint32_t h = chash_hash(key, t->tableSize);
    assert (h >= 0 && h < t->tableSize);
    chash_entry_t* entry = chash_get_entry(t, h, key, NULL);
    if (entry) {
        // Found existing entry.
        return entry->item;
    }
    return NULL;
}

int 
chash_set(chash_t *t, uint32_t key, chash_item_t obj)
{
    uint32_t h = chash_hash(key, t->tableSize);
    assert (h >= 0 && h < t->tableSize);

    chash_entry_t* entry = chash_get_entry(t, h, key, NULL);
    if (entry) {
        // Found existing entry. Set existing entry to new obj.
        entry->item = obj;
        return 0;
    }

    // No previous entry found. Create a new entry.
    entry = kmalloc(sizeof(chash_entry_t));
    if (!entry) {
        return -ENOMEM;
    }
    entry->key = key;
    entry->item = obj;
    cvector_add(&t->table[h], (cvector_item_t)entry);
    return 0;
}

void
chash_remove(chash_t *t, uint32_t key)
{
    uint32_t h = chash_hash(key, t->tableSize);
    assert (h >= 0 && h < t->tableSize);
    int index;
    chash_entry_t* entry = chash_get_entry(t, h, key, &index);
    if (entry) {
        kfree(entry);
        cvector_delete(&t->table[h], index);
    }
}

int
chash_find_free(chash_t *t, uint32_t rangeStart, uint32_t rangeEnd)
{
    for(int i = rangeStart; i < rangeEnd; i++) {
        uint32_t h = chash_hash(i, t->tableSize);
        chash_entry_t* entry = chash_get_entry(t, h, i, NULL);
        if (!entry) return i;
    }
    return -1;
}
