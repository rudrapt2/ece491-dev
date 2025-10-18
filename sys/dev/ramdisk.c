/*! @file ramdisk.c
    @brief Memory-backed I/O implementation
    @copyright Copyright (c) 2024-2025 University of Illinois

*/

#ifdef MEMIO_DEBUG
#define DEBUG
#endif

#ifdef MEMIO_TRACE
#define TRACE
#endif

#include "devimpl.h"
#include "error.h"
#include "string.h"
#include "heap.h"
#include "misc.h"
#include "console.h"
#include "uio.h"

#include <stddef.h>

#ifndef RAMDISK_NAME
#define RAMDISK_NAME "ramdisk"
#endif

// INTERNAL TYPE DEFINITIONS
//

/**
 * @brief Storage device backed by a block of memory. Allows modification of the backing memory block.
 */
struct ramdisk
{
  struct storage storage; ///< Storage struct of memory storage
  void *buf;              ///< Block of memory
  size_t size;            ///< Size of memory block
};

// INTERNAL FUNCTION DECLARATIONS
//

static int ramdisk_open(struct storage *sto);
static void ramdisk_close(struct storage *sto);
static long ramdisk_fetch(struct storage *sto, unsigned long long pos, void *buf, unsigned long bytecnt);
static int ramdisk_cntl(struct storage *sto, int cmd, void *arg);

// INTERNAL GLOBAL CONSTANTS
//

static const struct storage_intf ramdisk_intf = {
    .blksz = 1,
    .open = &ramdisk_open,
    .close = &ramdisk_close,
    .fetch = &ramdisk_fetch,
    .store = NULL, // Read-only storage (blob data in .rodata)
    .cntl = &ramdisk_cntl};

// EXPORTED FUNCTION DEFINITIONS
//

void ramdisk_attach()
{
  // External symbols from linker script for embedded blob data
  extern char _kimg_blob_start[], _kimg_blob_end[];

  struct ramdisk *rdisk;
  int result;

  size_t blob_size = _kimg_blob_end - _kimg_blob_start;

  if (blob_size == 0) {
    kprintf("Not enough available RAM for ramdisk\n");
    return;
  }
  
  rdisk = kmalloc(sizeof(struct ramdisk));

  // Use the embedded blob data as the storage buffer
  // Note: This creates a read-only storage device since blob data is in .rodata
  rdisk->buf = _kimg_blob_start;
  rdisk->size = blob_size;

  storage_init(&rdisk->storage, &ramdisk_intf, blob_size);

  result = register_device(RAMDISK_NAME, DEV_STORAGE, rdisk);

  if (result != 0)
    kfree(rdisk);

  return;
}

// INTERNAL FUNCTION DEFINITIONS
//

/**
 * @brief _open_ implementation for memory storage.
 * @param sto Storage struct pointer for memory storage
 * @return 0 on success
 */
static int ramdisk_open(struct storage *sto)
{
  return 0; // Always successful for memory storage
}

/**
 * @brief _close_ implementation for memory storage.
 * @param sto Storage struct pointer for memory storage
 */
static void ramdisk_close(struct storage *sto)
{
  kfree((void *)sto - offsetof(struct ramdisk, storage));
}

/**
 * @brief _fetch_ implementation for memory storage.
 * @details Performs proper bounds checks, then copies data from memory block to passed buffer
 * @param sto Storage struct pointer for memory storage
 * @param pos Position in storage to read from
 * @param buf Buffer to copy data from memory to
 * @param bytecnt Number of bytes to read from memory
 * @return Number of bytes successfully read
 */
static long ramdisk_fetch(struct storage *sto, unsigned long long pos, void *buf, unsigned long bytecnt)
{
  struct ramdisk *const rdisk = (void *)sto - offsetof(struct ramdisk, storage);
  unsigned long len;

  // Check bounds
  if (pos >= rdisk->size)
    return 0;

  if (pos + bytecnt > rdisk->size)
    len = rdisk->size - pos;
  else
    len = bytecnt;

  memcpy(buf, (char *)rdisk->buf + pos, len);
  return len;
}

/**
 * @brief _cntl_ functions for memory storage.
 * @details Memory storage supports basic control operations
 * @param sto Storage struct pointer for memory storage
 * @param cmd command to run
 * @param arg Argument for commands
 * @return 0 on success, error on failure or unsupported command
 */
static int ramdisk_cntl(struct storage *sto, int cmd, void *arg)
{
  struct ramdisk *const rdisk = (void *)sto - offsetof(struct ramdisk, storage);
  (void)rdisk; // Mark as used to avoid warnings
  (void)arg;  // Mark as used to avoid warnings

  switch (cmd)
  {
  case FCNTL_GETEND:
    if (arg == NULL)
      return -EINVAL;
    *((unsigned long long *)arg) = rdisk->size;
    return 0;
  default:
    return -ENOTSUP;
  }
}