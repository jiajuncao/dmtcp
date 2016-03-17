#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/mman.h>
#include <assert.h>

#include "ibmalloc.h"
#include "../lib/list.h"

#define LVL1_SIZE 32
#define LVL2_SIZE 64
#define LVL3_SIZE 128
#define LVL4_SIZE 256

#define NUM_SLOTS 1024

struct raw_buf {
  char *arena;
  // List holding the pointers to chunks
  char *addr_list;
  struct list_elem elem;
};

// A list of fixed-size pre-allocated buffers
struct fixed_size_buf {
  // Actual list holding free chunks
  struct list buf_list;
  // List of allocated buffers
  struct list raw_buf_list;
  // Size of the chunk
  size_t chunk_size;
  // Initial number of chunks
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

static void expand(struct fixed_size_buf *buf);

// Allocate size bytes of memory from buf
static void *alloc_from_buf(struct fixed_size_buf *buf, size_t size) {
  pthread_mutex_lock(&mutex);
  if (list_empty(&buf->buf_list)) {
    expand(buf);
  }
  struct list_elem *e;
  struct chunk_elem *chunk;

  e = list_pop_front(&buf->buf_list);
  chunk = list_entry(e, struct chunk_elem, elem);
  *(size_t *)chunk->addr = size;

  pthread_mutex_unlock(&mutex);

  return (char *)chunk->addr + sizeof(size_t);
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
  bool found = false;
  struct list_elem *e;

  pthread_mutex_lock(&mutex);
  for (e = list_begin(&buf->raw_buf_list);
       e != list_end(&buf->raw_buf_list);
       e = list_next(e)) {
    struct raw_buf *rb = list_entry(e, struct raw_buf, elem);

    if ((char *)ptr >= rb->arena &&
        (char *)ptr < rb->addr_list) {
      size_t offset = ((char *)ptr - rb->arena) / buf->chunk_size;
      struct chunk_elem *chunk = (struct chunk_elem *)
        (rb->addr_list) + offset;

      list_push_front(&buf->buf_list, &chunk->elem);
      found = true;
      break;
    }
  }
  pthread_mutex_unlock(&mutex);
  if (!found) {
    fprintf(stderr, "IBmalloc: unable to find the chunk during free\n");
    exit(1);
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

static size_t pow(size_t n) {
  size_t i = 0, rslt = 1;

  for (; i < n; i++)
    rslt *= 2;
  return rslt;
}

void expand(struct fixed_size_buf *buf) {
  size_t num_slots;
  struct raw_buf *new_buf;
  size_t i;

  assert(list_empty(&buf->buf_list));
  num_slots = buf->num_slots * pow(buf->num_expanded);
  buf->num_expanded++;
  new_buf = (struct raw_buf *)malloc(sizeof(struct raw_buf));

  if (new_buf == NULL) {
    fprintf(stderr, "IBmalloc: unable to allocate new_buf\n");
    exit(1);
  }

  size_t size = num_slots *
    (buf->chunk_size + sizeof(struct chunk_elem));

  new_buf->arena = (char *)mmap(NULL, size,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (new_buf->arena == NULL) {
    fprintf(stderr, "IBmalloc: unable to mmap new arena\n");
    exit(1);
  }

  new_buf->addr_list = new_buf->arena + num_slots * buf->chunk_size;

  for (i = 0; i < num_slots; i++) {
    struct chunk_elem *chunk = (struct chunk_elem *)
      (new_buf->addr_list + i * sizeof(struct chunk_elem));

    chunk->addr = new_buf->arena + i * buf->chunk_size;
    list_push_front(&buf->buf_list, &chunk->elem);
  }

  list_push_back(&buf->raw_buf_list, &new_buf->elem);
}


// Initialize a buffer with num_slots buffers of size chunk_size
static void buf_init(struct fixed_size_buf *buf,
                     size_t chunk_size, size_t num_slots) {
  list_init(&buf->buf_list);
  list_init(&buf->raw_buf_list);
  buf->chunk_size = chunk_size;
  buf->num_slots = num_slots;
  buf->num_expanded = 0;

  expand(buf);
}

static void buf_fini(struct fixed_size_buf *buf) {
  struct list_elem *e = list_begin(&buf->raw_buf_list);
  size_t total_num_slots = 0;
  size_t avail_num_slots = list_size(&buf->buf_list);
  size_t x = 0;

  while(e != list_end(&buf->raw_buf_list)) {
    struct list_elem *w = e;
    struct raw_buf *rb = list_entry(e, struct raw_buf, elem);
    size_t cur_num_slots = buf->num_slots * pow(x++);
    total_num_slots += cur_num_slots;
    e = list_next(e);
    list_remove(w);
    assert(munmap(rb->arena,
           cur_num_slots *
           (buf->chunk_size + sizeof (struct chunk_elem))) == 0);
    free(rb);
  }

  if (total_num_slots != avail_num_slots) {
    fprintf(stderr, "IBmalloc: memory leak detected: %zu %zu\n",
            total_num_slots, avail_num_slots);
  }
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
