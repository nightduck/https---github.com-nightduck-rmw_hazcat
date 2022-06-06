#include "rmw_hazcat/allocators/hma_template.h"
#include "rmw_hazcat/allocators/cpu_ringbuf_allocator.h"
#include "rmw_hazcat/allocators/cuda_ringbuf_allocator.h"

void * convert(void * ptr, size_t size, struct hma_allocator * alloc_src, struct hma_allocator * alloc_dest) {
    if (alloc_src->domain == alloc_dest->domain) {
        // Zero copy condition
        return ptr;
    } else {
        // Allocate space on the destination allocator
        void * here = OFFSET_TO_PTR(alloc_dest, alloc_dest->allocate(alloc_dest, size));
        assert(here > alloc_dest);

        if (alloc_src->domain == CPU) {
            alloc_dest->copy_from(here, ptr, size);
        } else if (alloc_dest->domain == CPU) {
            alloc_src->copy_to(here, ptr, size);
        } else {
            alloc_src->copy(here, ptr, size, alloc_dest);
        }
        return here;
    }
}

void populate_local_fn_pointers(struct local * alloc, uint32_t alloc_impl) {
    switch(alloc_impl) {
        case CPU_RINGBUF_IMPL:
            alloc->allocate = cpu_ringbuf_allocate;
            alloc->deallocate = cpu_ringbuf_deallocate;
            alloc->copy_from = cpu_copy_tofrom;
            alloc->copy_to = cpu_copy_tofrom;
            alloc->copy = cpu_copy;
            break;
        case CUDA_RINGBUF_IMPL:
            alloc->allocate = cuda_ringbuf_allocate;
            alloc->deallocate = cuda_ringbuf_deallocate;
            alloc->copy_from = cuda_ringbuf_copy_from;
            alloc->copy_to = cuda_ringbuf_copy_to;
            alloc->copy = cuda_ringbuf_copy;
            break;
        default:
            // TODO: Cleaner error handling
            assert(0);
            break;
    }
}

// TODO: Update documentation
// Don't call this outside this library
struct hma_allocator * create_shared_allocator(void * hint, size_t alloc_size, uint16_t strategy, uint16_t device_type, uint8_t device_number) {
    uint32_t alloc_impl = strategy << 12 | device_type;

    // Construct local portion of allocator
    struct local * fps = (struct local*)mmap(hint,
        sizeof(struct local), PROT_READ | PROT_WRITE, MAP_FIXED | MAP_ANONYMOUS, 0, 0);
    populate_local_fn_pointers(fps, alloc_impl);

    printf("Mounted local portion of alloc at: %xp\n", fps);
    // Create shared memory block (requested size )
    int id = shmget(IPC_PRIVATE, alloc_size - sizeof(struct local), 0640);
    if (id == -1) {
      // TODO: More robust error checking
      return NULL;
    }

    printf("Allocator id: %d\n", id);

    // Construct shared portion of allocator
    struct shared* alloc = (struct shared*)shmat(id, hint + sizeof(struct local), 0);
    alloc->shmem_id = id;
    alloc->alloc_impl = alloc_impl;
    alloc->device_number = device_number;

    printf("Mounted shared portion of alloc at: %xp\n", alloc);

    // TODO: shmctl to set permissions and such

    // Give back base allocator, straddling local and shared memory mappings
    return (struct hma_allocator *)fps;
}

// TODO: Update documentation
// Do call this
struct hma_allocator * remap_shared_allocator(int shmem_id) {
    // Temporarily map in shared allocator to read it's alloc_type
    struct shared * shared = (struct shared*)shmat(shmem_id, NULL, 0);

    struct hma_allocator * alloc;
    switch (shared->alloc_impl)
    {
    case CPU_RINGBUF_IMPL:
        alloc = cpu_ringbuf_remap(shared);
        break;
    
    default:
        break;
    }

    // TODO: Switch case calling remap function for each alloc type, which map in pool, remap
    //       allocator, populate function pointers, and return pointer to new location

    // Unmap temp mapping, and return pointer from switch case block
    shmdt(shared);

    return alloc;
}