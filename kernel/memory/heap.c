#include "heap.h"
#include "pmm.h"
#include "vmm.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define HEAP_START 0xFFFF900000000000ULL

#define HEAP_INITIAL_SIZE (4ULL * PMM_PAGE_SIZE)
#define HEAP_MAX_SIZE (16ULL * 1024 * 1024)

#define HEAP_ALIGNMENT 16

#define HEAP_ALIGN_UP(value) \
    (((value) + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1))

typedef struct __attribute__((aligned(16))) heap_block
{
    size_t size;
    bool free;

    struct heap_block *previous;
    struct heap_block *next;
} heap_block_t;

static uint64_t heap_end = HEAP_START;
static uint64_t heap_mapped_size = 0;

static heap_block_t *heap_first_block = NULL;
static heap_block_t *heap_last_block = NULL;

static bool heap_initialized = false;

static void *heap_copy(void *destination, const void *source, size_t size)
{
    uint8_t *destination_bytes = (uint8_t *)destination;
    const uint8_t *source_bytes = (const uint8_t *)source;

    for (size_t i = 0; i < size; i++)
    {
        destination_bytes[i] = source_bytes[i];
    }

    return destination;
}

static heap_block_t *heap_find_block(const void *pointer)
{
    heap_block_t *block = heap_first_block;

    while (block != NULL)
    {
        if ((void *)(block + 1) == pointer)
        {
            return block;
        }

        block = block->next;
    }

    return NULL;
}

static void heap_merge_next(heap_block_t *block)
{
    heap_block_t *next = block->next;

    if (next == NULL || !next->free)
    {
        return;
    }

    block->size += sizeof(heap_block_t) + next->size;
    block->next = next->next;

    if (next->next != NULL)
    {
        next->next->previous = block;
    }
    else
    {
        heap_last_block = block;
    }
}

static void heap_split_block(heap_block_t *block, size_t size)
{
    if (block->size < size + sizeof(heap_block_t) + HEAP_ALIGNMENT)
    {
        return;
    }

    size_t remaining_size =
        block->size - size - sizeof(heap_block_t);

    heap_block_t *new_block =
        (heap_block_t *)((uint8_t *)(block + 1) + size);

    new_block->size = remaining_size;
    new_block->free = true;
    new_block->previous = block;
    new_block->next = block->next;

    if (new_block->next != NULL)
    {
        new_block->next->previous = new_block;
    }
    else
    {
        heap_last_block = new_block;
    }

    block->next = new_block;
    block->size = size;
}

static bool heap_expand(void)
{
    if (!heap_initialized ||
        heap_mapped_size >= HEAP_MAX_SIZE)
    {
        return false;
    }

    uint64_t physical_address = pmm_allocate_page();

    if (physical_address == 0)
    {
        return false;
    }

    if (!vmm_map_page(
        heap_end,
        physical_address,
        VMM_PAGE_WRITE
    ))
    {
        pmm_free_page(physical_address);
        return false;
    }

    heap_block_t *last = heap_last_block;

    if (last != NULL && last->free)
    {
        uint64_t last_end =
            (uint64_t)(uintptr_t)(last + 1) + last->size;

        if (last_end == heap_end)
        {
            last->size += PMM_PAGE_SIZE;
        }
        else
        {
            heap_block_t *new_block =
                (heap_block_t *)(uintptr_t)heap_end;

            new_block->size =
                PMM_PAGE_SIZE - sizeof(heap_block_t);

            new_block->free = true;
            new_block->previous = last;
            new_block->next = NULL;

            last->next = new_block;
            heap_last_block = new_block;
        }
    }
    else
    {
        heap_block_t *new_block =
            (heap_block_t *)(uintptr_t)heap_end;

        new_block->size =
            PMM_PAGE_SIZE - sizeof(heap_block_t);

        new_block->free = true;
        new_block->previous = last;
        new_block->next = NULL;

        if (last != NULL)
        {
            last->next = new_block;
        }
        else
        {
            heap_first_block = new_block;
        }

        heap_last_block = new_block;
    }

    heap_end += PMM_PAGE_SIZE;
    heap_mapped_size += PMM_PAGE_SIZE;

    return true;
}

bool heap_initialize(void)
{
    if (heap_initialized)
    {
        return false;
    }

    heap_end = HEAP_START;
    heap_mapped_size = 0;

    for (uint64_t offset = 0;
         offset < HEAP_INITIAL_SIZE;
         offset += PMM_PAGE_SIZE)
    {
        uint64_t physical_address = pmm_allocate_page();

        if (physical_address == 0)
        {
            return false;
        }

        if (!vmm_map_page(
            HEAP_START + offset,
            physical_address,
            VMM_PAGE_WRITE
        ))
        {
            pmm_free_page(physical_address);
            return false;
        }

        heap_mapped_size += PMM_PAGE_SIZE;
    }

    heap_end = HEAP_START + heap_mapped_size;

    heap_first_block = (heap_block_t *)(uintptr_t)HEAP_START;

    heap_first_block->size =
        heap_mapped_size - sizeof(heap_block_t);

    heap_first_block->free = true;
    heap_first_block->previous = NULL;
    heap_first_block->next = NULL;

    heap_last_block = heap_first_block;
    heap_initialized = true;

    return true;
}

void *kmalloc(size_t size)
{
    if (!heap_initialized || size == 0)
    {
        return NULL;
    }

    if (size > SIZE_MAX - (HEAP_ALIGNMENT - 1))
    {
        return NULL;
    }

    size = HEAP_ALIGN_UP(size);

    for (;;)
    {
        heap_block_t *block = heap_first_block;

        while (block != NULL)
        {
            if (block->free && block->size >= size)
            {
                heap_split_block(block, size);
                block->free = false;

                return (void *)(block + 1);
            }

            block = block->next;
        }

        if (!heap_expand())
        {
            return NULL;
        }
    }
}

void *kcalloc(size_t count, size_t size)
{
    if (count == 0 || size == 0)
    {
        return NULL;
    }

    if (count > SIZE_MAX / size)
    {
        return NULL;
    }

    size_t total_size = count * size;

    void *pointer = kmalloc(total_size);

    if (pointer == NULL)
    {
        return NULL;
    }

    uint8_t *bytes = (uint8_t *)pointer;

    for (size_t i = 0; i < total_size; i++)
    {
        bytes[i] = 0;
    }

    return pointer;
}

void kfree(void *pointer)
{
    if (!heap_initialized || pointer == NULL)
    {
        return;
    }

    heap_block_t *block = heap_find_block(pointer);

    if (block == NULL || block->free)
    {
        return;
    }

    block->free = true;

    heap_merge_next(block);

    if (block->previous != NULL && block->previous->free)
    {
        block = block->previous;
        heap_merge_next(block);
    }
}

void *krealloc(void *pointer, size_t new_size)
{
    if (pointer == NULL)
    {
        return kmalloc(new_size);
    }

    if (new_size == 0)
    {
        kfree(pointer);
        return NULL;
    }

    if (new_size > SIZE_MAX - (HEAP_ALIGNMENT - 1))
    {
        return NULL;
    }

    heap_block_t *block = heap_find_block(pointer);

    if (block == NULL || block->free)
    {
        return NULL;
    }

    size_t aligned_size = HEAP_ALIGN_UP(new_size);

    if (block->size >= aligned_size)
    {
        heap_split_block(block, aligned_size);
        return pointer;
    }

    void *new_pointer = kmalloc(new_size);

    if (new_pointer == NULL)
    {
        return NULL;
    }

    heap_copy(
        new_pointer,
        pointer,
        block->size
    );

    kfree(pointer);

    return new_pointer;
}