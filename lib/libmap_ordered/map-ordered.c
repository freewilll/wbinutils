#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "map-ordered.h"

enum {
    TOMBSTONE_MARK   = -1,
    DEFAULT_SIZE     = 16,
    MAX_LOAD_FACTOR  = 666,  // 0.666 * 1000
    EXPANSION_FACTOR = 333,  // 0.333 * 1000
};

static void panic(const char *message) {
    printf("%s\n", message);
    exit(1);
}

static void maybe_rehash(MapOrdered *map) {
    if (map->used_count * 1000 < map->size * MAX_LOAD_FACTOR) return;

    int new_size;
    if (map->element_count * 1000 < map->size * EXPANSION_FACTOR)
        new_size = map->size;
    else
        new_size = map->size * 2;

    int mask = new_size - 1;

    void **keys = calloc(new_size, sizeof(void *));
    void **values = calloc(new_size, sizeof(void *));
    MapOrderedNode **nodes = calloc(new_size, sizeof(void *));

    for (int i = 0; i < map->size; i++) {
        void *key = map->keys[i];
        if (!key || key == (void *) TOMBSTONE_MARK) continue;

        unsigned int pos = map->hash_function(key) & mask;
        while (1) {
            if (!keys[pos]) {
                keys[pos] = key;
                values[pos] = map->values[i];
                nodes[pos] = map->nodes[i];
                break;
            }

            pos = (pos + 1) & mask;
        }
    }

    free(map->keys);
    free(map->values);
    free(map->nodes);

    map->keys = keys;
    map->values = values;
    map->nodes = nodes;
    map->size = new_size;
    map->used_count = map->element_count;
}

void map_ordered_put(MapOrdered *map, void *key, void *value) {
    maybe_rehash(map);
    unsigned int mask = map->size - 1;
    unsigned int pos = map->hash_function(key) & mask;

    while (1) {
        void *k = map->keys[pos];
        if (!k || k == (void *) TOMBSTONE_MARK) {
            map->keys[pos] = key;
            map->values[pos] = value;

            MapOrderedNode *node = malloc(sizeof(MapOrderedNode));
            node->key = key;
            node->value = value;
            node->prev = map->tail;
            node->next = NULL;

            if (map->tail)
                map->tail->next = node;
            else
                map->head = node;

            map->tail = node;
            map->nodes[pos] = node;

            map->element_count++;
            if (!k) map->used_count++;
            return;
        }

        if (map->compare_function(k, key) == 0) {
            map->values[pos] = value;
            map->nodes[pos]->value = value;
            return;
        }

        pos = (pos + 1) & mask;
    }
}

void *map_ordered_get(MapOrdered *map, const void *key) {
    unsigned int mask = map->size - 1;
    unsigned int pos = map->hash_function(key) & mask;

    while (map->keys[pos]) {
        if (map->keys[pos] != (void *) TOMBSTONE_MARK &&
            map->compare_function(map->keys[pos], key) == 0)
            return map->values[pos];

        pos = (pos + 1) & mask;
    }
    return NULL;
}

void map_ordered_delete(MapOrdered *map, const void *key) {
    unsigned int mask = map->size - 1;
    unsigned int pos = map->hash_function(key) & mask;

    while (map->keys[pos]) {
        if (map->keys[pos] != (void *) TOMBSTONE_MARK &&
            map->compare_function(map->keys[pos], key) == 0) {

            map->keys[pos] = (void *) TOMBSTONE_MARK;
            map->values[pos] = NULL;

            MapOrderedNode *node = map->nodes[pos];
            if (node->prev)
                node->prev->next = node->next;
            else
                map->head = node->next;

            if (node->next)
                node->next->prev = node->prev;
            else
                map->tail = node->prev;

            free(node);
            map->nodes[pos] = NULL;

            map->element_count--;
            return;
        }
        pos = (pos + 1) & mask;
    }
}

MapOrdered *new_map_ordered(MapOrderedHashFunc hash_function, MapOrderedCompareFunc compare_function) {
    MapOrdered *map = calloc(1, sizeof(MapOrdered));
    map->size = DEFAULT_SIZE;
    map->keys = calloc(DEFAULT_SIZE, sizeof(void *));
    map->values = calloc(DEFAULT_SIZE, sizeof(void *));
    map->nodes = calloc(DEFAULT_SIZE, sizeof(void *));
    map->hash_function = hash_function;
    map->compare_function = compare_function;
    return map;
}

void map_ordered_free(MapOrdered *map) {
    for (int i = 0; i < map->size; i++) {
        if (map->keys[i] && map->keys[i] != (void *) TOMBSTONE_MARK)
            free(map->keys[i]);
    }
    free(map->keys);
    free(map->values);
    free(map->nodes);
    free(map);
}

MapOrderedIterator map_ordered_iterator(MapOrdered *map) {
    return (MapOrderedIterator){ .current = map->head };
}

int map_ordered_iterator_finished(MapOrderedIterator *iterator) {
    return iterator->current == NULL;
}

void map_ordered_iterator_next(MapOrderedIterator *iterator) {
    if (iterator->current)
        iterator->current = iterator->current->next;
}

const void *map_ordered_iterator_key(MapOrderedIterator *iterator) {
    if (!iterator->current) panic("Iterator at end");
    return iterator->current->key;
}
