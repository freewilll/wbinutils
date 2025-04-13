#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "strmap-ordered.h"

enum {
    TOMBSTONE        = -1,
    DEFAULT_SIZE     = 16,
    MAX_LOAD_FACTOR  = 666,  // 0.666 * 1000
    EXPANSION_FACTOR = 333,  // 0.333 * 1000
};

static void panic(const char *message) {
    printf("%s\n", message);
    exit(1);
}

static unsigned int hash(const char *str) {
    // FNV hash function
    unsigned int result = 2166136261;
    const char *p = str;
    while (*p) {
        result = (result ^ (*p)) * 16777619;
        p++;
    }
    return result;
}

static void maybe_rehash(StrMapOrdered *map) {
    if (map->used_count * 1000 < map->size * MAX_LOAD_FACTOR) return;

    int new_size;
    if (map->element_count * 1000 < map->size * EXPANSION_FACTOR)
        new_size = map->size;
    else
        new_size = map->size * 2;

    int mask = new_size - 1;
    const char **keys = calloc(new_size,  sizeof(char *));
    void **values = calloc(new_size,  sizeof(void *));
    StrMapOrderedNode **nodes = calloc(new_size, sizeof(void *));

    for (int i = 0; i < map->size; i++) {
        const char *key;
        if (!(key = map->keys[i]) || key == (char *) TOMBSTONE) continue;
        unsigned int pos = hash(key) & mask;
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

void strmap_ordered_put(StrMapOrdered *map, const char *key, void *value) {
    maybe_rehash(map);

    unsigned int mask = map->size - 1;
    unsigned int pos = hash(key) & mask;

    const char *k;
    while (1) {
        k = map->keys[pos];
        if (!k || k == (char *) TOMBSTONE) {
            map->keys[pos] = key;
            map->values[pos] = value;

            // Update double linked list
            StrMapOrderedNode *node = malloc(sizeof(StrMapOrderedNode));
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

        if (!strcmp(k, key)) {
            map->values[pos] = value;
            map->nodes[pos]->value = value;
            return;
        }
        pos = (pos + 1) & mask;
    }
}

void *strmap_ordered_get(StrMapOrdered *map, const char *key) {
    unsigned int mask = map->size - 1;
    unsigned int pos = hash(key) & mask;

    const char *k;
    while ((k = map->keys[pos])) {
        if (k != (char *) TOMBSTONE && !strcmp(k, key)) return map->values[pos];
        pos = (pos + 1) & mask;
    }
    return 0;
}

void strmap_ordered_delete(StrMapOrdered *map, const char *key) {
    unsigned int mask = map->size - 1;
    unsigned int pos = hash(key) & mask;

    const char *k;
    while ((k = map->keys[pos])) {
        if (k != (char *) TOMBSTONE && !strcmp(k, key)) {
            map->keys[pos] = (char *) TOMBSTONE;
            map->values[pos] = 0;

            // Delete it from the double linked list
            StrMapOrderedNode *node = map->nodes[pos];
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

int strmap_ordered_iterator_finished(StrMapOrderedIterator *iterator) {
    return iterator->current == NULL;
}

void strmap_ordered_iterator_next(StrMapOrderedIterator *iterator) {
    if (iterator->current) iterator->current = iterator->current->next;
}

const char *strmap_ordered_iterator_key(StrMapOrderedIterator *iterator) {
    if (!iterator->current) panic("Iterator reached the end");
    return iterator->current->key;
}

StrMapOrderedIterator strmap_ordered_iterator(StrMapOrdered *map) {
    return (StrMapOrderedIterator) { .current = map->head };
}

StrMapOrdered *new_strmap_ordered(void) {
    StrMapOrdered *map = calloc(1, sizeof(StrMapOrdered));
    map->size = DEFAULT_SIZE;
    map->keys = calloc(DEFAULT_SIZE, sizeof(void *));
    map->values = calloc(DEFAULT_SIZE, sizeof(char *));
    map->nodes = calloc(DEFAULT_SIZE, sizeof(void *));
    return map;
}

void free_strmap_ordered(StrMapOrdered *map) {
    free(map->keys);
    free(map->values);
    free(map->nodes);
    free(map);
}
