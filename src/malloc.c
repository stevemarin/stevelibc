#include "../include/stdlib.h"
#include "../include/syscall.h"

/* ==========================================================================
 * MALLOC.C : Heap Allocator
 * --------------------------------------------------------------------------
 * This small allocator gets memory by moving the process break with Linux's
 * brk system call. It keeps every block in address order in a doubly-linked
 * list; each block has a header immediately before the pointer returned to
 * the caller. Freed blocks can be reused, split to satisfy smaller requests,
 * and merged with physically adjacent free blocks.
 *
 * All list changes and break extensions happen under allocator_lock. This is
 * important for threads sharing a process heap: otherwise two allocations
 * could modify the list or request overlapping break ranges at the same time.
 * This allocator assumes it is the sole manager of brk for the process. It
 * does not return freed pages to the kernel, and is not intended to replace a
 * full production allocator.
 * ========================================================================== */

typedef struct block_header {
  unsigned long size;        // Usable payload capacity, excluding this header
  int free;                  // Nonzero when this block can satisfy an allocation
  struct block_header *next; // Next block in the heap's address-ordered list
  struct block_header *prev; // Previous block in the heap's address-ordered list
} __attribute__((aligned(16))) block_header;

/*
 * Keeping both the header size and payload size aligned means the payload
 * following any header is 16-byte aligned, and a split creates an aligned
 * header for the remainder. BLOCK_SIZE is rounded up by the type's alignment.
 */
#define ALIGNMENT 16UL
#define BLOCK_SIZE sizeof(block_header)
static block_header *free_list_head = 0;
static unsigned char allocator_lock;

/*
 * Acquire/release atomics protect both heap metadata and calls that extend
 * brk. This is a simple spinlock: a contending thread busy-waits rather than
 * sleeping, which keeps this runtime independent of a scheduler API.
 */
static void lock_allocator(void) {
  while (__atomic_test_and_set(&allocator_lock, __ATOMIC_ACQUIRE)) {
  }
}

static void unlock_allocator(void) { __atomic_clear(&allocator_lock, __ATOMIC_RELEASE); }

/*
 * sbrk-like wrapper for this allocator. Linux brk takes an absolute requested
 * break, not a delta, and returns the resulting break. On success this returns
 * the old break so the caller can use it as the start of its new block; on
 * failure it returns (void *)-1. Only nonnegative increments are used here.
 */
static void *sys_sbrk(long increment) {
  void *current = (void *)syscall1(SYS_brk, 0);
  if (increment == 0)
    return current;
  unsigned long current_address = (unsigned long)current;
  // Reject address wraparound before asking the kernel to extend the break.
  if ((unsigned long)increment > (~0UL - current_address))
    return (void *)-1;
  unsigned long requested_address = current_address + (unsigned long)increment;
  void *new_break = (void *)syscall1(SYS_brk, (long)requested_address);
  // Linux brk returns the resulting break, which may be below the request.
  if ((unsigned long)new_break != requested_address)
    return (void *)-1;
  return current;
}

static int aligned_size(unsigned long size, unsigned long *result) {
  /*
   * Round upward to the next alignment boundary using the power-of-two mask.
   * The guard is needed because adding the padding near ULONG_MAX would wrap.
   * Return false for an unrepresentable size; otherwise store the rounded size.
   */
  if (size > (~0UL - (ALIGNMENT - 1)))
    return 0;
  *result = (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
  return 1;
}

static block_header *find_block(void *ptr) {
  /*
   * Convert a user pointer back to its header only after finding an exact
   * payload address in the allocator's list. This avoids blindly subtracting
   * BLOCK_SIZE from an arbitrary pointer and treating unrelated memory as a
   * header. It also lets free/realloc reject pointers that were not allocated
   * here (and lets free ignore an already-freed block).
   */
  for (block_header *block = free_list_head; block; block = block->next) {
    if ((void *)((char *)block + BLOCK_SIZE) == ptr)
      return block;
  }
  return 0;
}

static void merge_next(block_header *block) {
  /*
   * Merge only the physically following block, not merely the next list entry.
   * The address check matters because heap memory could have been obtained or
   * reserved elsewhere between two blocks. Once merged, remove the absorbed
   * header from the list and repair the successor's back-link.
   */
  if (!block->next || !block->next->free || (char *)block + BLOCK_SIZE + block->size != (char *)block->next)
    return;

  block->size += BLOCK_SIZE + block->next->size;
  block->next = block->next->next;
  if (block->next)
    block->next->prev = block;
}

static void split_block(block_header *block, unsigned long size) {
  /*
   * Trim an oversized block to 'size' and turn its tail into a free block.
   * The tail must fit both its own header and at least one aligned payload
   * unit; otherwise the tiny remainder would be unusable and stays with the
   * allocated block.
   */
  if (block->size < size || block->size - size < BLOCK_SIZE + ALIGNMENT)
    return;

  block_header *next_block = (block_header *)((char *)block + BLOCK_SIZE + size);
  next_block->size = block->size - size - BLOCK_SIZE;
  next_block->free = 1;
  next_block->next = block->next;
  next_block->prev = block;
  if (block->next)
    block->next->prev = next_block;
  block->next = next_block;
  block->size = size;
  // The old tail may have been adjacent to another free block; keep it merged.
  merge_next(next_block);
}

/*
 * Allocate while the caller holds allocator_lock. Keeping this internal
 * operation lock-free lets realloc allocate/copy/free atomically with respect
 * to other allocator calls without trying to acquire the same lock twice.
 */
static void *malloc_locked(unsigned long size) {
  if (size == 0)
    return 0;
  /*
   * The syscall wrapper takes a signed long increment. Check that both the
   * aligned payload plus its header fit in that positive range before adding.
   */
  if (!aligned_size(size, &size) || size > (unsigned long)(~0UL >> 1) - BLOCK_SIZE)
    return 0;

  // First-fit search: reuse the first free block large enough for this request.
  block_header *current = free_list_head;
  while (current) {
    if (current->free && current->size >= size) {
      split_block(current, size);
      current->free = 0;
      return (char *)current + BLOCK_SIZE;
    }
    current = current->next;
  }

  /*
   * No reusable block fits, so extend the break by header + aligned payload.
   * sys_sbrk returns the previous break, which becomes this block's header.
   */
  unsigned long total_size = size + BLOCK_SIZE;
  block_header *block = (block_header *)sys_sbrk((long)total_size);
  if (block == (void *)-1)
    return 0;

  block->size = size;
  block->free = 0;
  block->next = 0;
  block->prev = 0;

  if (!free_list_head) {
    free_list_head = block;
  } else {
    block_header *last = free_list_head;
    while (last->next)
      last = last->next;
    last->next = block;
    block->prev = last;
  }

  return (char *)block + BLOCK_SIZE;
}

static void free_locked(block_header *block) {
  // Mark available, merge toward higher addresses, then merge into a free predecessor.
  block->free = 1;
  merge_next(block);
  if (block->prev && block->prev->free)
    merge_next(block->prev);
}

/*
 * malloc's public entry point. The lock covers the search, any list changes,
 * and any brk growth, then is released before returning the user payload.
 */
void *malloc(unsigned long size) {
  lock_allocator();
  void *ptr = malloc_locked(size);
  unlock_allocator();
  return ptr;
}

void *calloc(unsigned long nmemb, unsigned long size) {
  /*
   * Multiplication can wrap before malloc sees the request, potentially
   * allocating less space than the caller asked for. Reject that case first.
   */
  if (size && nmemb > ~0UL / size)
    return 0;
  unsigned long total = nmemb * size;
  void *ptr = malloc(total);
  if (!ptr)
    return 0;

  /*
   * Clear exactly the requested byte count. Volatile stores prevent the
   * freestanding compiler from replacing this loop with a memset call, which
   * this minimal library does not implement.
   */
  volatile unsigned char *bytes = (volatile unsigned char *)ptr;
  for (unsigned long i = 0; i < total; i++)
    bytes[i] = 0;
  return ptr;
}

void *realloc(void *ptr, unsigned long size) {
  // The null-pointer case is equivalent to a fresh allocation.
  if (!ptr)
    return malloc(size);
  if (size == 0) {
    // This implementation treats a zero-size request as release-and-return-null.
    free(ptr);
    return 0;
  }

  unsigned long aligned;
  if (!aligned_size(size, &aligned))
    return 0;

  lock_allocator();
  /*
   * Keep the list stable while locating and inspecting the old allocation.
   * A bad pointer or a block already marked free is rejected without changing
   * allocator state.
   */
  block_header *block = find_block(ptr);
  if (!block || block->free) {
    unlock_allocator();
    return 0;
  }

  /*
   * If the existing payload is large enough, keep the same address. For a
   * shrink, split off a useful tail block; if the tail is too small,
   * split_block leaves the allocation's capacity unchanged.
   */
  if (aligned <= block->size) {
    split_block(block, aligned);
    unlock_allocator();
    return ptr;
  }

  /*
   * Grow by allocating another block before releasing the original. If the
   * allocation fails, the old pointer and its contents are still valid.
   */
  void *new_ptr = malloc_locked(size);
  if (!new_ptr) {
    unlock_allocator();
    return 0;
  }

  /*
   * Preserve the old payload up to its capacity (or the new requested size,
   * whichever is smaller). Volatile accesses keep the compiler from emitting
   * a memcpy dependency that this minimal runtime does not provide.
   */
  volatile unsigned char *destination = (volatile unsigned char *)new_ptr;
  volatile const unsigned char *source = (volatile const unsigned char *)ptr;
  unsigned long copy_size = block->size < size ? block->size : size;
  for (unsigned long i = 0; i < copy_size; i++)
    destination[i] = source[i];
  free_locked(block);
  unlock_allocator();
  return new_ptr;
}

/*
 * Release a block allocated by this allocator. Null is a no-op. The list
 * lookup rejects foreign pointers; the free flag prevents a second free from
 * inserting the same block into coalescing logic again.
 */
void free(void *ptr) {
  if (!ptr)
    return;
  lock_allocator();
  block_header *block = find_block(ptr);
  if (block && !block->free)
    free_locked(block);
  unlock_allocator();
}
