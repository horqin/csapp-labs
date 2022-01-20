#include "csapp.h"

#define SMALL_NUMBER    64
#define LARGE_NUMBER 65536

static int  cache_access(char *info, void *data);
static void cache_modify(char *info, void *data, int size);

void parse(char *head, char *addr, char *port, char *file) {
    head = strchr(head, ':') + 1;
    head = strchr(head, '/') + 1;
    head = strchr(head, '/') + 1;
    strncpy(addr, head, strchr(head, ':') - head);
    addr[strchr(head, ':') - head] = 0;
    head = strchr(head, ':') + 1;
    strncpy(port, head, strchr(head, '/') - head);
    port[strchr(head, '/') - head] = 0;
    head = strchr(head, '/') + 1;
    strncpy(file, head, strchr(head, ' ') - head);
    file[strchr(head, ' ') - head] = 0;
}

void *thread(void *arg) {
    int connectfd = (int)(unsigned long)arg;
    pthread_detach(pthread_self());

    char buf[LARGE_NUMBER], *ptr;
    int nbuf, n;

    nbuf = 0;
    do {
        if ((n = Read(connectfd, buf + nbuf, sizeof(buf) - nbuf)) == 0)
            app_error("[E] connection between client and proxy is closed");
        nbuf += n;
    } while (!memmem(buf, nbuf, "\r\n\r\n", 4));

    char addr[SMALL_NUMBER/4];
    char port[SMALL_NUMBER/4];
    char file[SMALL_NUMBER/4];
    parse(buf, addr, port, file);
    char info[SMALL_NUMBER];
    sprintf(info, "%s-%s-%s", addr, port, file);

    if (!(nbuf = cache_access(info, buf))) {
        int clientfd = Open_clientfd(addr, port);
        nbuf = sprintf(buf, "%s /%s %s\r\nHost: %s\r\nConnection: %s\r\nProxy-Connection: %s\r\n\r\n", 
                "GET", file, "HTTP/1.0", "www.cmu.edu", "close", "close");
        ptr = buf;
        while (ptr < buf + nbuf)
            ptr += Write(clientfd, ptr, nbuf - (ptr - buf));
        nbuf = 0;
        while ((n = Read(clientfd, buf + nbuf, sizeof(buf) - nbuf)) != 0)
            nbuf += n;
        Close(clientfd);
        cache_modify(info, buf, nbuf);
    }
    ptr = buf;
    while (ptr < buf + nbuf)
        ptr += Write(connectfd, ptr, nbuf - (ptr - buf));
    Close(connectfd);

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(-1);
    }
    char *port = argv[1];

    int listenfd = Open_listenfd(port);
    for (;;) {
        int connectfd = Accept(listenfd, NULL, NULL);
        pthread_t tid;
        pthread_create(&tid, NULL, &thread, (void *)(unsigned long)connectfd);
    }
}

static struct {
    struct {
        char info[SMALL_NUMBER];
        char data[LARGE_NUMBER];
        int  size, time;
    } arr[SMALL_NUMBER];
    int num, max;
} cache = { .max = SMALL_NUMBER };

static pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;

int cache_access(char *info, void *data) {
    int size = 0;
    pthread_mutex_lock(&cache_lock);
    for (int i = 0; i < cache.num; ++i)
    if (!strcmp(cache.arr[i].info, info)) {
        memcpy(data, cache.arr[i].data, cache.arr[i].size);
        size = cache.arr[i].size;
        cache.arr[i].time = 0;
        for (int j = 0; j < cache.num; ++j)
        if (j != i) ++cache.arr[j].time;
        break;
    }
    pthread_mutex_unlock(&cache_lock);
    return size;
}

void cache_modify(char *info, void *data, int size) {
    pthread_mutex_lock(&cache_lock);
    int i;
    if (cache.num < cache.max) i = cache.num++;
    else {
        i = 0;
        for (int j = 1; j < cache.num; ++j)
        if (cache.arr[j].time > cache.arr[i].time) i = j;
    }
    strcpy(cache.arr[i].info, info);
    memcpy(cache.arr[i].data, data, size);
    cache.arr[i].size = size;
    cache.arr[i].time = 0;
    for (int j = 0; j < cache.num; ++j) 
    if (j != i) ++cache.arr[j].time;
    pthread_mutex_unlock(&cache_lock);
}
