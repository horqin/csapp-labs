#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "cachelab.h"

#define SMALL_NUMBER     32
#define LARGE_NUMBER 524288

static struct {
    struct {
        int valid, tag, time;
    } table[SMALL_NUMBER][SMALL_NUMBER];
    int S, E;
} cache;

static struct {
    int hit, miss, eviction;
} event;

static void cache_init(int S, int E);
static void cache_access(char method, int set, int tag);
static void event_init();
static void trace_parser(FILE *file, int t, int s, int b);

int main(int argc, char *argv[]) {
    int s, E, b;
    FILE *t;
    for (;;) {
        char c;
        if ((c = getopt(argc, argv, "hvs:E:b:t:")) == -1)
            break;
        switch (c) {
        case 's': {
            s = atoi(optarg);
        } break;
        case 'E': {
            E = atoi(optarg);
        } break;
        case 'b': {
            b = atoi(optarg);
        } break;
        case 't': {
            t = fopen(optarg, "r");
        } break;
        }
    }
    event_init();
    cache_init(1 << s, E);
    trace_parser(t, 32 - s - b, s, b);
    fclose(t);
    printSummary(event.hit, event.miss, event.eviction);
    return 0;
}

void cache_access_once(int set, int tag);

void cache_init(int S, int E) {
    cache.S = S;
    cache.E = E;
    for (int i = 0; i < cache.S; ++i)
        for (int j = 0; j < cache.E; ++j)
            cache.table[i][j].valid = 0;
}

void cache_access(char method, int set, int tag) {
    switch (method) {
    case 'I': {
    } break;
    case 'L': case 'S': {
        cache_access_once(set, tag);
    } break;
    case 'M': {
        cache_access_once(set, tag);
        cache_access_once(set, tag);
    } break;
    }
}

void cache_access_once(int set, int tag) {
    int k = 0;
    for (int j = 0; j < cache.E; ++j)
        if (cache.table[set][j].valid && 
                cache.table[set][j].tag == tag) {
            k = j;
            event.hit++;
            goto aging;
        }
    for (int j = 0; j < cache.E; ++j)
        if (!cache.table[set][j].valid) {
            k = j;
            cache.table[set][k].valid = 1;
            cache.table[set][k].tag = tag;
            event.miss++;
            goto aging;
        }
    for (int j = 1; j < cache.E; ++j)
        if (cache.table[set][k].time < 
                cache.table[set][j].time)
            k = j;
    cache.table[set][k].tag = tag;
    event.miss++;
    event.eviction++;
aging:
    cache.table[set][k].time = 0;
    for (int j = 0; j < cache.E; ++j)
        if (j != k)
            cache.table[set][j].time++;
}

void event_init() {
    event.hit = 0;
    event.miss = 0;
    event.eviction = 0;
}

void trace_parser(FILE *file, int t, int s, int b) {
    char method;
    while (fscanf(file, "%c", &method) > 0)
        switch (method) {
        case 'I': case 'L': case 'S': case 'M': {
            long addr;
            fscanf(file, "%lx", &addr);
            int set = (addr >> b) % (1 << s);
            int tag = (addr >> (s + b)) % (1 << t);
            cache_access(method, set, tag);
        } break;
        }
}
