#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "map-ordered.h"

typedef struct {
    char *string;
    uint16_t number;
} Key;

unsigned int key_hash(const void *ptr) {
    const Key *key = ptr;
    unsigned int hash = 2166136261u;
    for (const char *p = key->string; *p; p++)
        hash = (hash ^ (unsigned char)*p) * 16777619;
    return (hash ^ key->number) * 16777619;
}

int key_compare(const void *a, const void *b) {
    const Key *ka = a;
    const Key *kb = b;
    int cmp = strcmp(ka->string, kb->string);
    if (cmp) return cmp;
    return (ka->number - kb->number);
}

static void panic(const char *message) {
    printf("%s\n", message);
    exit(1);
}


int main() {
    MapOrdered *map = new_map_ordered(key_hash, key_compare);

    int COUNT = 10000;
    int SUB_COUNT = 3;
    for (int i = 0; i < COUNT; i++) {
        for (int j = 0; j < SUB_COUNT; j++) {
            Key *key = malloc(sizeof(Key));
            key->string = malloc(16);
            sprintf(key->string, "foo %d", i);
            key->number = j;

            char *value = malloc(16);
            sprintf(value, "bar %d %d", i, j);
            map_ordered_put(map, key, value);
        }
    }

    // Lookup the keys
    for (int i = 0; i < COUNT; i++) {
        for (int j = 0; j < SUB_COUNT; j++) {
            Key key;
            key.string = malloc(16);
            sprintf(key.string, "foo %d", i);
            key.number = j;
            char *value = malloc(16);
            sprintf(value, "bar %d %d", i, j);
            char *got_value = map_ordered_get(map, &key);
            if (!got_value) panic("Didn't get a match\n");
            if (strcmp(value, got_value)) panic("Got something horrible 1");

            key.string = "nuttin";
            got_value = map_ordered_get(map, &key);
            if (got_value) panic("Expected no match");
        }
    }

    // Test iteration
    int i = 0;
    for (MapOrderedIterator it = map_ordered_iterator(map); !map_ordered_iterator_finished(&it); map_ordered_iterator_next(&it), i++) {
        const Key *key = map_ordered_iterator_key(&it);
        int got_string_number = atoi(&(key->string[4]));
        uint16_t got_number = key->number;

        int expected_string_number = i / SUB_COUNT;
        int expected_number = i % SUB_COUNT;
        if (got_string_number != expected_string_number || got_number != expected_number)
            panic("Iteration returned wrong value");
    }

    // Reassign the keys with number=1
    for (int i = 0; i < COUNT; i++) {
        Key key;
        key.string = malloc(16);
        sprintf(key.string, "foo %d", i);
        key.number = 1;
        char *value = malloc(16);
        sprintf(value, "baz %d 1", i);
        map_ordered_put(map, &key, value);
    }

    // Recheck the changed keys
    for (int i = 0; i < COUNT; i++) {
        for (int j = 0; j < SUB_COUNT; j++) {
            Key key;
            key.string = malloc(16);
            sprintf(key.string, "foo %d", i);
            key.number = j;

            char *value = malloc(16);

            if (j == 1)
                sprintf(value, "baz %d 1", i);
            else
                sprintf(value, "bar %d %d", i, j);

            char *got_value = map_ordered_get(map, &key);
            if (!got_value) panic("Didn't get a match\n");
            if (strcmp(value, got_value)) panic("Got something horrible 2");
            key.string = "nuttin";
            got_value = map_ordered_get(map, &key);
            if (got_value) panic("Expected no match");
        }
    }

    // Delete the keys with number=2
    for (int i = 0; i < COUNT; i++) {
        Key key;
        key.string = malloc(16);
        sprintf(key.string, "foo %d", i);
        key.number = 2;
        map_ordered_delete(map, &key);
    }

    // Check the deleted keys are are gone
    for (int i = 0; i < COUNT; i++) {
        for (int j = 0; j < SUB_COUNT; j++) {
            Key key;
            key.string = malloc(16);
            sprintf(key.string, "foo %d", i);
            key.number = j;
            char *got_value = map_ordered_get(map, &key);
            if ((!!got_value) == (!!(j == 2))) panic("Mismatch in key deletion");
        }
    }

    // Test iteration again, now that the map has deleted values
    i = 0;
    int new_sub_count = 2;
    for (MapOrderedIterator it = map_ordered_iterator(map); !map_ordered_iterator_finished(&it); map_ordered_iterator_next(&it), i++) {
        const Key *key = map_ordered_iterator_key(&it);
        int got_string_number = atoi(&(key->string[4]));
        uint16_t got_number = key->number;

        int expected_string_number = i / new_sub_count;
        int expected_number = i % new_sub_count;
        if (got_string_number != expected_string_number || got_number != expected_number)
            panic("Iteration returned wrong value");
    }

    // Insert/deletion thrashing in a new map. This tests rehashing without
    // a resize.
    map = new_map_ordered(key_hash, key_compare);
    for (int i = 0; i < COUNT; i++) {
        for (int j = 0; j < SUB_COUNT; j++) {
            Key *key = malloc(sizeof(Key));
            key->string = malloc(16);
            sprintf(key->string, "foo %d", i);
            key->number = j;

            char value[] = "foo";
            map_ordered_put(map, key, value);
            char *got_value = map_ordered_get(map, key);
            if (!got_value) panic("Did not get a string");
            map_ordered_delete(map, key);
            got_value = map_ordered_get(map, key);
            if (got_value) panic("Got a string");
        }
    }
}
