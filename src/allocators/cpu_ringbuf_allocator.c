#ifdef __cplusplus
extern "C"
{
#endif

#include "rmw_hazcat/allocators/cpu_ringbuf_allocator.h"

struct cpu_ringbuf_allocator * create_cpu_ringbuf_allocator(size_t item_size, size_t ring_size)
{
  struct cpu_ringbuf_allocator * alloc = (struct cpu_ringbuf_allocator *)create_shared_allocator(
    NULL, sizeof(struct cpu_ringbuf_allocator) + item_size * ring_size, CPU, ALLOC_RING, 0);

  alloc->count = 0;
  alloc->rear_it = 0;
  alloc->item_size = item_size;
  alloc->ring_size = ring_size;

  return alloc;
}

int cpu_ringbuf_allocate(void * self, size_t size)
{
  struct cpu_ringbuf_allocator * s = (struct cpu_ringbuf_allocator *)self;
  if (s->count == s->ring_size) {
    // Allocator full
    return -1;
  }
  int forward_it = (s->rear_it + s->count) % s->ring_size;

  // Give address relative to shared object
  int ret = sizeof(struct cpu_ringbuf_allocator) + s->item_size * forward_it;

  // Update count of how many elements in pool
  s->count++;

  return ret;
}

void cpu_ringbuf_deallocate(void * self, int offset)
{
  struct cpu_ringbuf_allocator * s = (struct cpu_ringbuf_allocator *)self;
  if (s->count == 0) {
    return;       // Allocator empty, nothing to deallocate
  }
  int entry = (offset - sizeof(struct cpu_ringbuf_allocator)) / s->item_size;

  // Do math with imaginary overflow indices so forward_it >= entry >= rear_it
  int forward_it = s->rear_it + s->count;
  if (__glibc_unlikely(entry < s->rear_it)) {
    entry += s->ring_size;
  }

  // Most likely scenario: entry == rear_it as allocations are deallocated in order
  s->rear_it = entry + 1;
  s->count = forward_it - s->rear_it;
}

struct hma_allocator * cpu_ringbuf_remap(struct shared * temp)
{
  // Create a local mapping, and populate function pointers so they resolve in this process
  struct local * fps = (struct local *)mmap(
    NULL,
    sizeof(struct local), PROT_READ | PROT_WRITE, MAP_FIXED | MAP_ANONYMOUS, 0, 0);
  populate_local_fn_pointers(fps, CPU_RINGBUF_IMPL);

  // Map in shared portion of allocator
  shmat(temp->shmem_id, fps + sizeof(struct local), 0);

  // fps can now be typecast to cpu_ringbuf_allocator* and work correctly. Updates to any member
  // besides top 40 bytes will be visible across processes
  return (struct hma_allocator *)fps;
}

#ifdef __cplusplus
}
#endif
