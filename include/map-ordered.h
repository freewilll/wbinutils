#ifndef _MAP_ORDERED_H
#define _MAP_ORDERED_H

#include <stdint.h>

typedef unsigned int (*MapOrderedHashFunc)(const void *key);
typedef int (*MapOrderedCompareFunc)(const void *a, const void *b);

typedef struct map_ordered_node {
    void *key;
    void *value;
    struct map_ordered_node *prev;
    struct map_ordered_node *next;
} MapOrderedNode;

typedef struct map_ordered {
    void **keys;
    void **values;
    MapOrderedNode **nodes;
    int size;
    int used_count;
    int element_count;
    MapOrderedNode *head;    // Double linked list of the ordering
    MapOrderedNode *tail;
    MapOrderedHashFunc hash_function;
    MapOrderedCompareFunc compare_function;
} MapOrdered;

typedef struct map_ordered_iterator {
    MapOrderedNode *current;
} MapOrderedIterator;

MapOrdered *new_map_ordered(MapOrderedHashFunc hash_function, MapOrderedCompareFunc compare_function);
void map_ordered_free(MapOrdered *map);
void *map_ordered_get(MapOrdered *map, const void *key);
void map_ordered_put(MapOrdered *map, void *key, void *value);
void map_ordered_delete(MapOrdered *map, const void *key);

MapOrderedIterator map_ordered_iterator(MapOrdered *map);
int map_ordered_iterator_finished(MapOrderedIterator *iterator);
void map_ordered_iterator_next(MapOrderedIterator *iterator);
const void *map_ordered_iterator_key(MapOrderedIterator *iterator);

#define map_ordered_foreach(map, it) \
    for (MapOrderedIterator it = map_ordered_iterator(map); \
         !map_ordered_iterator_finished(&it); \
         map_ordered_iterator_next(&it))

#endif
