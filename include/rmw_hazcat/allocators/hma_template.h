#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/mman.h>


#ifndef HMA_ALLOCATOR_H
#define HMA_ALLOCATOR_H


#define OFFSET_TO_PTR(a, o) (uint8_t *)a + o
#define PTR_TO_OFFSET(a, p) (uint8_t *)p - (uint8_t *)a

#define ALLOC_FROM_SHARED(s, type)    (type*)((uint8_t*)s - sizeof(struct local))

#define MAX_POOL_SIZE   0x100000000

// alloc_type is 32bit value.
// First 12 bits encode allocator strategy
// Next 12 bits encode device type
// Last 8 bits encode device number. Only used for multiple GPUs, etc. Must be zero for CPU
//
// First 24 bits collectively encode the needed allocator implementation.
// The last 20 bits collectively encode the memory "domain"
//
// Example definition:  alloc_type x = (alloc_type)(ALLOC_TLSF << 20 | CUDA << 8 | 3);

union alloc_type {
    struct {
        uint16_t strategy : 12;
        union {
            uint16_t device_type : 12;
            uint32_t domain : 20;
        };
    };
    struct {
        uint32_t alloc_impl : 24;
        uint8_t device_number;
    };
    uint32_t raw;
};

#define ALLOC_STRAT 0x000   // Not for use
#define ALLOC_RING  0x001
#define ALLOC_TLSF  0x002
#define ALLOC_HEAP  0x003

#define DEVICE      0x000   // Not for use
#define CPU         0x001
#define CUDA        0x002


/*
  // Copy paste at head of new allocators, so first 56 bytes can be cast as a hma_allocator
  union {
    struct {
      // Exist in local memory, pointing to static functions
      int   (*const allocate)   (void * self, size_t size);
      void  (*const deallocate) (void * self, int offset);
      void  (*const copy_from)  (void * here, void * there, size_t size);
      void  (*const copy_to)    (void * here, void * there, size_t size);
      void  (*const copy)       (void * here, void * there, size_t size, hma_allocator * dest_alloc);

      // Exist in shared memory
      const int shmem_id;
      const uint32_t alloc_type;
    };
    hma_allocator untyped;
  };

*/


struct local {
    int   (*allocate)   (void * self, size_t size);
    void  (*deallocate) (void * self, int offset);
    void  (*copy_from)  (void * there, void * here, size_t size);
    void  (*copy_to)    (void * there, void * here, size_t size);
    void  (*copy)       (void * there, void * here, size_t size, struct hma_allocator * dest_alloc);
};

struct shared {
    int shmem_id;
    struct {      // 32bit int indicating type of allocator and memory domain
        struct {
            uint16_t strategy : 12;
            union {
                uint16_t device_type : 12;
                uint32_t domain : 20;
            };
        };
        struct {
            uint32_t alloc_impl : 24;
            uint8_t device_number;
        };
    };
};

struct hma_allocator {
    // Exist in local memory, pointing to static functions
    union {
        struct local local;
        struct {
            int   (*allocate)   (void * self, size_t size);
            void  (*deallocate) (void * self, int offset);
            void  (*copy_from)  (void * there, void * here, size_t size);
            void  (*copy_to)    (void * there, void * here, size_t size);
            void  (*copy)       (void * there, void * here, size_t size, struct hma_allocator * dest_alloc);
        };
    };

    // Exist in shared memory
    union {
        struct shared shared;
        struct {
            int shmem_id;
            struct {      // 32bit int indicating type of allocator and memory domain
                struct {
                    uint16_t strategy : 12;
                    union {
                        uint16_t device_type : 12;
                        uint32_t domain : 20;
                    };
                };
                struct {
                    uint32_t alloc_impl : 24;
                    uint8_t device_number;
                };
            };
        };
    };
    
};

void * convert(void * ptr, size_t size, struct hma_allocator * alloc_src, struct hma_allocator * alloc_dest);

int cant_allocate_here(void * self, size_t size) {
    assert(0);
    return -1;
}

// copy_to, copy_from, and copy shouldn't get called on a CPU allocator, but they've been
// implemented here for completeness anyways
void cpu_copy_tofrom(void * there, void * here, size_t size) {
    memcpy(here, there, size);
}
void cpu_copy(void * there, void * here, size_t size, struct hma_allocator * dest_alloc) {
    dest_alloc->copy_from(there, here, size);
}

void populate_local_fn_pointers(struct local * alloc, uint32_t alloc_impl);

// TODO: Update documentation
// Don't call this outside this library
struct hma_allocator * create_shared_allocator(void * hint, size_t alloc_size, uint16_t strategy, uint16_t device_type, uint8_t device_number);

// TODO: Update documentation
// Do call this
struct hma_allocator * remap_shared_allocator(int shmem_id);


#endif // HMA_ALLOCATOR_H