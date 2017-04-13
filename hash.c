/*
   hash.c

   eJS Project
     Kochi University of Technology
     the University of Electro-communications

     Tomoharu Ugawa, 2016-17
     Hideya Iwasaki, 2016-17

   The eJS Project is the successor of the SSJS Project at the University of
   Electro-communications, which was contributed by the following members.

     Sho Takada, 2012-13
     Akira Tanimura, 2012-13
     Akihiro Urushihara, 2013-14
     Ryota Fujii, 2013-14
     Tomoharu Ugawa, 2012-14
     Hideya Iwasaki, 2012-14
*/


#include "prefix.h"
#define EXTERN extern
#include "header.h"

#define HASH_SKIP (27)

// #define REHASH_THRESHOLD (0.5)
#define REHASH_THRESHOLD (0.9)

/*
   allocates a hash table
 */
HashTable *malloc_hashtable(void) {
  return (HashTable*)malloc(sizeof(HashTable));
}

/*
   initializes a hash table with the specified size
 */
int hash_create(HashTable *table, unsigned int size) {
  int i;

  table->body = __hashMalloc(size);
  if (table->body == NULL) {
    LOG_EXIT("hash body malloc failed\n");
  }
  for (i = 0; i < size; i++) {
    table->body[i] = NULL;
  }
  table->size = size;
  table->filled = 0;
  table->entry_count = 0;
  return 0;
}

/*
   obtains the value and attribute associated with a given key
 */
int hash_get_with_attribute(HashTable *table, HashKey key, HashData *data,
                            Attribute *attr) {
  uint32_t hval;
  HashCell *cell;

  hval = string_hash(key) % table->size;
  for (cell = table->body[hval]; cell != NULL; cell = cell->next)
    if ((JSValue)(cell->entry.key) == key) {
      // found
      if (data != NULL) *data = cell->entry.data;
      if (attr != NULL) *attr = cell->entry.attr;
      // printf("hash_get_with_attr: success, *data = %d\n", *data);
      return HASH_GET_SUCCESS;
    }
  // not found
  // printf("hash_get_with_attr: fail\n");
  return HASH_GET_FAILED;
}

/*
   obtains the value associated with a given key
 */
int hash_get(HashTable *table, HashKey key, HashData *data) {
  int r;
  r = hash_get_with_attribute(table, key, data, NULL);
  /*
  if (r == HASH_GET_SUCCESS) {
    printf("hash_get: success, "); simple_print(*data); putchar('\n');
  } else
    printf("hash_get: fail\n");
  */
  return r;
#if 0
  uint32_t hval;
  HashCell *cell;

  hval = string_hash(key) % table->size;
  for (cell = table->body[hval]; cell != NULL; cell = cell->next)
    if ((JSValue)(cell->entry.key) == key) {
      // found
      if (data != NULL ) *data = cell->entry.data;
      return HASH_GET_SUCCESS;
    }
  // not found
  return HASH_GET_FAILED;
#endif
}

/*
   registers a value to a hash table under a given key with an attribute
 */
int hash_put_with_attribute(HashTable* table, HashKey key, HashData data,
                            Attribute attr) {
  HashCell* cell;
  uint32_t index;

  index = string_hash(key) % table->size;
  for (cell = table->body[index]; cell != NULL; cell = cell->next) {
    if (cell->entry.key == key) {
      // found
      if (!is_readonly(cell->entry.attr)) {
        cell->deleted = false;
        cell->entry.data = data;
        cell->entry.attr = attr;
        return HASH_PUT_SUCCESS;
      } else
        // return -1; // write error !!
        return HASH_PUT_FAILED;
    }
  }
  // not found
  cell = __hashCellMalloc();
  cell->next = table->body[index];
  table->body[index] = cell;
  cell->deleted = false;
  cell->entry.key = key;
  cell->entry.data = data;
  cell->entry.attr = attr;
  if (cell->next == NULL) {
    table->entry_count++;
    if (table->entry_count > REHASH_THRESHOLD * table->size)
      rehash(table);
  }
  return HASH_PUT_SUCCESS;
}

/*
   deletes the hash data
 */
int hash_delete(HashTable *table, HashKey key) {
  HashCell *cell, *prev;
  uint32_t index;

  index = string_hash(key) % table->size;
  for (prev = NULL, cell = table->body[index]; cell != NULL;
       prev = cell, cell = cell->next) {
    if (cell->entry.key == key) {
      // found
      if (!is_dont_delete(cell->entry.attr)) return HASH_GET_FAILED;
      if (prev == NULL) {
        table->body[index] = cell->next;
      } else {
        prev->next = cell->next;
      }
      hashCellFree(cell);
      return HASH_GET_SUCCESS;
    }
  }
  return HASH_GET_FAILED;
}

#ifdef HIDDEN_CLASS
/*
   copies a hash table
   This function is used only for copying a hash table in a hidden class.
   This function returns the number of copied properties.
 */
int hash_copy(Context *ctx, HashTable *from, HashTable *to) {
  int i, fromsize, tosize;
  HashCell *cell, *new;
  uint32_t index;
  int n, ec;


  fromsize = from->size;
  tosize = to->size;
  n = 0;
  ec = 0;
  for (i = 0; i < fromsize; i++) {
    for (cell = from->body[i]; cell != NULL; cell = cell->next) {
      // we do not copy the transition entry.
      if (is_transition(cell->entry.attr)) continue;
      index = string_hash(cell->entry.key) % tosize;
      new = __hashCellMalloc();
      new->deleted = false;
      new->entry = cell->entry;
      if (to->body[index] == NULL) ec++;   // increments entry count
      new->next = to->body[index];
      to->body[index] = new;
      n++;
    }
  }
  to->entry_count = ec;
  to->filled = from->filled;
  return n;
}
#endif

/*
   calculates the hash value
 */
uint32_t calc_hash_len(const char* s, uint32_t len) {
  uint32_t value = 0;
  int i;

  for (i = 0; i < len; i++) {
    value += s[i];
    value += (value << 10);
    value ^= (value >> 6);
  }
  value += (value << 3);
  value ^= (value >> 11);
  value += (value << 15);
  return value;
}

uint32_t calc_hash(const char* s) {
  return calc_hash_len(s, strlen(s));
}

/*
   calculates the hash value (for two strings)
 */
uint32_t calc_hash_len2(const char* s1, uint32_t len1, const char* s2, uint32_t len2) {
  uint32_t value = 0;
  int i;

  for (i = 0; i < len1; i++) {
    value += s1[i];
    value += (value << 10);
    value ^= (value >> 6);
  }
  for (i = 0; i < len2; i++) {
    value += s2[i];
    value += (value << 10);
    value ^= (value >> 6);
  }
  value += (value << 3);
  value ^= (value >> 11);
  value += (value << 15);
  return value;
}

uint32_t calc_hash2(const char* s1, const char* s2) {
  return calc_hash_len2(s1, strlen(s1), s2, strlen(s2));
}


//
HashCell** __hashMalloc(int size) {
  HashCell** ret = (HashCell**)gc_malloc_critical(sizeof(HashCell*) * size,
						  HTAG_HASH_BODY);
  memset(ret, 0, sizeof(HashCell*) * size);
  return ret;
}

//
HashCell* __hashCellMalloc() {
  HashCell* cell = (HashCell*)malloc(sizeof(HashCell));
  cell->next = NULL;
  return cell;
}

//
int rehash(HashTable *table) {
  int size = table->size;
  int newsize = size * 2;
  HashIterator iter;
  HashCell *p;
  HashCell** newhash = __hashMalloc(newsize);
  HashCell** oldhash = table->body;

  iter = createHashIterator(table);
  while (___hashNext(table, &iter, &p) != FAIL) {
    uint32_t index = string_hash(p->entry.key) % newsize;
    p->next = newhash[index];
    newhash[index] = p;
  }
  table->body = newhash;
  table->size = newsize;
  hashBodyFree(oldhash);
  return 0;
}

int init_hash_iterator(HashTable *t, HashIterator *h) {
  int i, size;

  size = t->size;
  for (i = 0; i < size; i++) {
    if (t->body[i] != NULL) {
      h->p = t->body[i];
      h->index = i;
      // printf("init_hash_iterator: h->index = %d, h->p = %p\n", h->index, h->p);
      return TRUE;
    }
  }
  h->p = NULL;
  return FALSE;
}

HashIterator createHashIterator(HashTable *table) {
  int i, size = table->size;
  HashIterator iter;

  iter.p = NULL;
  for (i = 0; i < size; i++) {
    if (table->body[i] != NULL) {
      iter.p = table->body[i];
      iter.index = i;
      break;
    }
  }
  return iter;
}

//
int hash_next(HashTable *table, HashIterator *iter, HashData *data) {
  HashEntry e;
  int r;

  if ((r = __hashNext(table, iter, &e)) == SUCCESS) {
    // printf("hash next: %016lx ", e.data); simple_print(e.data); printf("\n");
    *data = e.data;
  }
  return r;
}

//
int hash_next_key(HashTable *table, HashIterator *Iter, HashKey *key) {
  HashEntry e;
  int r;

  if ((r = __hashNext(table, Iter, &e)) == SUCCESS)
    *key = e.key;
  return r;
}

//
int ___hashNext(HashTable *table, HashIterator *iter, HashCell **p) {
  int i;

  if (iter->p == NULL) return FAIL;
  *p = iter->p;
  if (iter->p->next != NULL) {
    iter->p = iter->p->next;
    return SUCCESS;
  }
  for (i = iter->index + 1; i < table->size; i++) {
    if (table->body[i] != NULL) {
      iter->index = i;
      iter->p = table->body[i];
      return SUCCESS;
    }
  }
  iter->p = NULL;
  return SUCCESS;
}

//
int __hashNext(HashTable *table, HashIterator *iter, HashEntry *ep) {
  int i;

  // printf("__hashNext: iter->index = %d, iter->p = %p\n", iter->index, iter->p);
  if (iter->p == NULL) return FAIL;
  *ep = iter->p->entry;
  /*
  printf("            ep-key = %p, ep->data = %p\n", ep->key, ep->data);
  printf("            iter->p->next = %p\n", iter->p->next);
  */
  if (iter->p->next != NULL) {
    iter->p = iter->p->next;
    return SUCCESS;
  }
  // printf("            iter->index = %d, table->size = %d\n", iter->index, table->size);
  for(i = iter->index + 1; i < table->size; i++) {
    // printf("            i = %d, table->body[i] %p\n", i, table->body[i]);
    if(table->body[i] != NULL) {
      /*
      printf("__hashNext: i = %d\n", i);
      printf("  body[i]->key = %p, body[i]->data = %p\n",
             table->body[i]->entry.key, table->body[i]->entry.data);
      */
      iter->index = i;
      iter->p = table->body[i];
      return SUCCESS;
    }
  }
  iter->p = NULL;
  return SUCCESS;
}

//
void hashBodyFree(HashCell** body) {
#if !defined(USE_BOEHMGC) && !defined(USE_NATIVEGC)
  free(body);
#endif
}

//
void hashCellFree(HashCell* cell) {
#if !defined(USE_BOEHMGC) && !defined(USE_NATIVEGC)
  free(cell);
#endif
}

/*
   prints a hash table (for debugging)
 */
void print_hash_table(HashTable *tab) {
  HashCell *p;
  unsigned int i, ec;

  printf("HashTable %p: size = %d, entry_count = %d\n", tab, tab->size, tab->entry_count);
  ec = 0;
  for (i = 0; i < tab->size; i++) {
    if ((p = tab->body[i]) == NULL) continue;
    ec++;
    do {
      printf(" (%d: (", i);
      printf("%016lx = ", p->entry.key); simple_print(p->entry.key);
      printf(", ");
      printf("%016lx = %d\n", (int)p->entry.data);
      printf("))\n");
    } while ((p = p->next) != NULL);
    // if (ec >= tab->entry_count) break;
  }
  printf("end HashTable\n");

}

/*
   FIXME: This function is not completed yet for the adaptation of
   HIDDEN_CLASS.
 */
void print_object_properties(JSValue o) {
  HashCell *p;
  HashTable *tab;
  JSValue v;
  unsigned int i, ec;

#ifdef HIDDEN_CLASS
  tab = obj_hidden_class_map(o);
#else
  tab = obj_map(o);
#endif
  printf("Object %016lx: (type = %ld, n_props = %ld, map = %p)\n",
         o, obj_header_tag(o), obj_n_props(o), tab);
  ec = 0;
  for (i = 0; i < tab->size; i++) {
    if ((p = tab->body[i]) == NULL) continue;
    do {
      ec++;
      printf(" (%d: (", i);
      printf("%016lx = ", p->entry.key); simple_print(p->entry.key);
      printf(", ");
      printf("%016lx = ", p->entry.data); simple_print(p->entry.data);
      printf(", ");
      v = obj_prop_index(o, (int)p->entry.data);
      printf("%016lx = ", v); simple_print(v);
      printf("))");
    } while ((p = p->next) != NULL);
    if (ec >= tab->entry_count) break;
  }
  printf("\n");
}

//
char* ststrdup(const char* str) {
  uint64_t len = strlen(str)+1;
  char *dst = (char*)malloc(sizeof(char) * len);

  strcpy(dst, str);
  return dst;
}

//----------------------------------------------------------------------------
// string table

/*
   initializes the string table
 */
void init_string_table(unsigned int size) {
  StrCons **a;

  a = (StrCons **)malloc(sizeof(StrCons*) * size);
  memset(a, 0, sizeof(StrCons*) * size);
  string_table.obvector = a;
  string_table.size = size;
  string_table.count = 0;
}

JSValue str_intern(const char* s, int len, uint32_t hash, int mode) {
  int index;
  StrCons *c;
  StringCell *p;
  JSValue v;

  index = hash % string_table.size;
  // printf("strIntern: s = %s, len = %d, hash = %ud, index = %d\n",
  //       s, len, hash, index);
  for (c = string_table.obvector[index]; c != NULL; c = c->next) {
    p = remove_string_tag(c->str);
#ifdef STROBJ_HAS_HASH
    if (len != p->length) continue;
#endif
    // printf(" now testing %s and %s\n", s, p->value);
    if (memcmp(s, p->value, len + 1) == 0) {
      //    printf("  strIntern: %s found, address = %lld\n", s, c->str);
      return c->str;
    }
  }
  // not found in the string table
  // if intern_soft, then returns null
  if (mode == INTERN_SOFT) return JS_NULL;
  // allocate a string and interns it
  p = allocate_string(len);
#ifdef STROBJ_HAS_HASH
  p->hash = hash;
#endif
  memcpy(p->value, s, len + 1);
  v = put_string_tag(p);
  // printf("  strIntern: %s not found, allocated addresss = %lld\n", s, v);
  c = (StrCons*)malloc(sizeof(StrCons));
  c->str = v;
  if ((c->next = string_table.obvector[index]) == NULL)
    string_table.count++;
  string_table.obvector[index] = c;
  return v;
}

JSValue str_intern2(Context *ctx,
		    const char* s1, int len1, const char* s2, int len2,
                    uint32_t hash, int mode) {
  int index;
  StrCons *c;
  StringCell *p;
  JSValue v;

  index = hash % string_table.size;
  for (c = string_table.obvector[index]; c != NULL; c = c->next) {
    p = remove_string_tag(c->str);
#ifdef STROBJ_HAS_HASH
    if (len1 + len2 != p->length) continue;
#endif
    if (memcmp(s1, p->value, len1) == 0 &&
        memcmp(s2, p->value + len1, len2 + 1) == 0)
      return c->str;
  }
  if (mode == INTERN_SOFT) return JS_NULL;
  disable_gc();
  p = allocate_string(len1 + len2);
#ifdef STROBJ_HAS_HASH
  p->hash = hash;
#endif
  memcpy(p->value, s1, len1);
  memcpy(p->value + len1, s2, len2 + 1);
  v = put_string_tag(p);
  c = (StrCons*)gc_malloc_critical(sizeof(StrCons), HTAG_STR_CONS);
  c->str = v;
  if ((c->next = string_table.obvector[index]) == NULL)
    string_table.count++;
  string_table.obvector[index] = c;
  gc_push_tmp_root(&v);
  enable_gc(ctx);
  gc_pop_tmp_root(1);
  return v;
}
