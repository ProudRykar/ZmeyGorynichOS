#ifndef KERNEL_ADDRESS_SPACE_H
#define KERNEL_ADDRESS_SPACE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint64_t pml4_physical;
    bool initialized;
} address_space_t;

bool address_space_create(address_space_t *address_space);

bool address_space_switch(const address_space_t *address_space);

bool address_space_destroy(address_space_t *address_space);

#endif