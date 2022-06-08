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

struct hma_allocator * cpu_ringbuf_remap(struct hma_allocator * temp)
{
  // Map in shared portion of allocator
  struct hma_allocator * fps = shmat(temp->shmem_id, NULL, 0);

  // fps can now be typecast to cpu_ringbuf_allocator* and work correctly. Updates to any member
  // besides top 40 bytes will be visible across processes
  return fps;
}

void cpu_ringbuf_unmap(struct hma_allocator * alloc)
{
  struct shmid_ds buf;
  if(shmctl(alloc->shmem_id, IPC_STAT, &buf) == -1) {
    printf("Destruction failed on fetching segment info\n");
    //RMW_SET_ERROR_MSG("Error reading info about shared StaticPoolAllocator");
    //return RMW_RET_ERROR;
    return;
  }
  if(buf.shm_cpid == getpid()) {
    printf("Marking segment fo removal\n");
    if(shmctl(alloc->shmem_id, IPC_RMID, NULL) == -1) {
        printf("Destruction failed on marking segment for removal\n");
        //RMW_SET_ERROR_MSG("can't mark shared StaticPoolAllocator for deletion");
        //return RMW_RET_ERROR;
        return;
    }
  }
  int ret = shmdt(alloc);
  if(ret) {
    printf("cpu_ringbuf_unmap, failed to detach\n");
    handle_error("shmdt");
  }
}

#ifdef __cplusplus
}
#endif
