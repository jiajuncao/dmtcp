#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>

#include "ibmalloc.h"
#include "../lib/list.h"

#define LVL1_SIZE 32
#define LVL2_SIZE 64
#define LVL3_SIZE 128
#define LVL4_SIZE 256

#define NUM_SLOTS 1024

// A list of fixed-size pre-allocated buffers
struct fixed_size_buf {
  struct list buf_list;
  // Actual memory
  char *arena;
  // Size of per buffer
  size_t chunk_size;
  // Addr to hold the list of pointers to each buffer
  char *chunk_array_addr;
  // Original number of elements
  size_t num_slots;
  // Number of expanded elements
  size_t num_expanded;
};

struct chunk_elem {
  void *addr;
  struct list_elem elem;
};

static struct fixed_size_buf lvl1;
static struct fixed_size_buf lvl2;
static struct fixed_size_buf lvl3;
static struct fixed_size_buf lvl4;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Allocate size bytes of memory from buf
static void *alloc_from_buf(struct fixed_size_buf *buf, size_t size) {
  pthread_mutex_lock(&mutex);
  // If there're remaining slots from the buffer, fetch one
  if (list_size(&buf->buf_list) > 0) {
    struct list_elem *e;
    struct chunk_elem *chunk;

    e = list_pop_front(&buf->buf_list);
    chunk = list_entry(e, struct chunk_elem, elem);
    *(size_t *)chunk->addr = size;

    pthread_mutex_unlock(&mutex);

    return (char *)chunk->addr + sizeof(size_t);
  }
  // Otherwise, allocate new memory
  else {
    void *ret = malloc(size);
    if (!ret) {
      fprintf(stderr, "Cannot allocate memory\n");
      return NULL;
    }

    buf->num_expanded++;
    *(size_t *)ret = size;
    pthread_mutex_unlock(&mutex);

    return (char *)ret + sizeof(size_t);
  }
}

void *ibmalloc(size_t size) {
  size_t real_size = size + sizeof(size_t);

  if (real_size <= LVL1_SIZE)
    return alloc_from_buf(&lvl1, real_size);
  else if (real_size <= LVL2_SIZE)
    return alloc_from_buf(&lvl2, real_size);
  else if (real_size <= LVL3_SIZE)
    return alloc_from_buf(&lvl3, real_size);
  else if (real_size <= LVL4_SIZE)
    return alloc_from_buf(&lvl4, real_size);
  else {
    fprintf(stderr, "Currently don't support size bigger than %d\n",
            LVL4_SIZE);
    return NULL;
  }
}

static void free_from_buf(struct fixed_size_buf *buf, void *ptr) {
  pthread_mutex_lock(&mutex);
  // Memory is allocated from the static buffer, return to the list
  if ((char *)ptr >= buf->arena &&
      (char *)ptr < buf->arena + buf->chunk_size * buf->num_slots) {
    size_t offset = ((char *)ptr - buf->arena) / buf->chunk_size;
    struct chunk_elem *chunk =
      (struct chunk_elem *)(buf->chunk_array_addr) + offset;

    list_push_front(&buf->buf_list, &chunk->elem);
    pthread_mutex_unlock(&mutex);
    return;
  }
  // Othersise, dynamically allcated, do a normal free()
  else {
    free(ptr);
    buf->num_expanded--;
    pthread_mutex_unlock(&mutex);
    return;
  }
}

void ibfree(void *ptr) {
  void *real_ptr = (char *)ptr - sizeof(size_t);
  size_t size = *(size_t *)real_ptr;

  if (size <= LVL1_SIZE)
    free_from_buf(&lvl1, real_ptr);
  else if (size <= LVL2_SIZE)
    free_from_buf(&lvl2, real_ptr);
  else if (size <= LVL3_SIZE)
    free_from_buf(&lvl3, real_ptr);
  else if (size <= LVL4_SIZE)
    free_from_buf(&lvl4, real_ptr);
  else {
    fprintf(stderr, "IBmalloc: Shouldn't free a buffer larger than %d bytes\n",
            LVL4_SIZE);
  }
}

// Initialize a buffer with num_slots buffers of size chunk_size
static void buf_init(struct fixed_size_buf *buf,
                     size_t chunk_size, size_t num_slots) {
  size_t i;

  list_init(&buf->buf_list);
  buf->chunk_size = chunk_size;
  buf->arena = (char *)malloc(buf->chunk_size * num_slots);
  buf->chunk_array_addr =
    (char *)malloc(num_slots * sizeof(struct chunk_elem));
  buf->num_slots = num_slots;
  buf->num_expanded = 0;

  if (!buf->arena || !buf->chunk_array_addr) {
    fprintf(stderr, "Unable to allocate memory\n");
    exit(1);
  }

  for (i = 0; i < num_slots; i++) {
    struct chunk_elem *chunk;

    chunk = (struct chunk_elem *)
            (buf->chunk_array_addr + i * sizeof(struct chunk_elem));
    chunk->addr = buf->arena + buf->chunk_size * i;
    list_push_front(&buf->buf_list, &chunk->elem);
  }
}

static void buf_fini(struct fixed_size_buf *buf) {
  if (buf->num_expanded > 0 ||
      list_size(&buf->buf_list) < buf->num_slots) {
    fprintf(stderr, "IBmalloc: memory leak detected\n");
  }

  free(buf->chunk_array_addr);
  free(buf->arena);
}

void ibmalloc_init() {
  pthread_mutex_lock(&mutex);
  buf_init(&lvl1, LVL1_SIZE, NUM_SLOTS);
  buf_init(&lvl2, LVL2_SIZE, NUM_SLOTS);
  buf_init(&lvl3, LVL3_SIZE, NUM_SLOTS);
  buf_init(&lvl4, LVL4_SIZE, NUM_SLOTS);
  pthread_mutex_unlock(&mutex);
}

void ibmalloc_fini() {
  pthread_mutex_lock(&mutex);
  buf_fini(&lvl1);
  buf_fini(&lvl2);
  buf_fini(&lvl3);
  buf_fini(&lvl4);
  pthread_mutex_unlock(&mutex);
}
