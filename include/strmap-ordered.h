#ifndef _STRMAP_ORDERED_H
#define _STRMAP_ORDERED_H

typedef struct strmap_ordered_node {
    const char *key;
    void *value;
    struct strmap_ordered_node *prev;
    struct strmap_ordered_node *next;
} StrMapOrderedNode;

typedef struct strmap_ordered {
    const char **keys;
    void **values;
    int size;
    int used_count;
    int element_count;
    StrMapOrderedNode **nodes;
    StrMapOrderedNode *head;        // Double linked list of the ordering
    StrMapOrderedNode *tail;
} StrMapOrdered;

typedef struct strmap_ordered_iterator {
    StrMapOrderedNode *current;
} StrMapOrderedIterator;

StrMapOrdered *new_strmap_ordered(void);
void free_strmap_ordered(StrMapOrdered *map);
void *strmap_ordered_get(StrMapOrdered *strmap, const char *key);
void strmap_ordered_put(StrMapOrdered *strmap, const char *key, void *value);
void strmap_ordered_delete(StrMapOrdered *strmap, const char *key);
StrMapOrderedIterator strmap_ordered_iterator(StrMapOrdered *map);
int strmap_ordered_iterator_finished(StrMapOrderedIterator *iterator);
void strmap_ordered_iterator_next(StrMapOrderedIterator *iterator);
const char *strmap_ordered_iterator_key(StrMapOrderedIterator *iterator);
#define strmap_ordered_foreach(map, it) for (StrMapOrderedIterator it = strmap_ordered_iterator(map); !strmap_ordered_iterator_finished(&it); strmap_ordered_iterator_next(&it))

#endif
