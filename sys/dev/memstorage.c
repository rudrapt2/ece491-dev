/*! @file memstorage.c
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
#include "uio.h"

#include <stddef.h>

// INTERNAL TYPE DEFINITIONS
//

/**
 * @brief Storage device backed by a block of memory. Allows modification of the backing memory block.
 */
struct memstorage
{
  struct storage storage; ///< Storage struct of memory storage
  void *buf;              ///< Block of memory
  size_t size;            ///< Size of memory block
};

// INTERNAL FUNCTION DECLARATIONS
//

static int memstorage_open(struct storage *sto);
static void memstorage_close(struct storage *sto);
static long memstorage_fetch(struct storage *sto, unsigned long long pos, void *buf, unsigned long bytecnt);
static int memstorage_cntl(struct storage *sto, int cmd, void *arg);

// INTERNAL GLOBAL CONSTANTS
//

static const struct storage_intf memstorage_intf = {
    .blksz = 1,
    .open = &memstorage_open,
    .close = &memstorage_close,
    .fetch = &memstorage_fetch,
    .store = NULL, // Read-only storage (blob data in .rodata)
    .cntl = &memstorage_cntl};

// EXPORTED FUNCTION DEFINITIONS
//

int attach_memory_storage(const char *name, void *buf, size_t size)
{
  struct memstorage *msto;
  int result;

  msto = kmalloc(sizeof(struct memstorage));
  if (!msto)
  {
    return -ENOMEM;
  }

  msto->buf = buf;
  msto->size = size;

  storage_init(&msto->storage, &memstorage_intf, size);

  result = register_device(name, DEV_STORAGE, msto);
  if (result != 0)
  {
    kfree(msto);
    return result;
  }

  return 0;
}

int attach_blob_storage(const char *name)
{
  // External symbols from linker script for embedded blob data
  extern char _kimg_blob_start[], _kimg_blob_end[];

  size_t blob_size = _kimg_blob_end - _kimg_blob_start;

  if (blob_size == 0)
  {
    return -ENOENT;
  }

  // Use the embedded blob data as the storage buffer
  // Note: This creates a read-only storage device since blob data is in .rodata
  return attach_memory_storage(name, _kimg_blob_start, blob_size);
}

struct storage *create_memory_storage(void *buf, size_t size)
{
  struct memstorage *msto;

  msto = kmalloc(sizeof(struct memstorage));
  if (!msto)
  {
    return NULL;
  }

  msto->buf = buf;
  msto->size = size;

  storage_init(&msto->storage, &memstorage_intf, size);
  return &msto->storage;
}

// INTERNAL FUNCTION DEFINITIONS
//

/**
 * @brief _open_ implementation for memory storage.
 * @param sto Storage struct pointer for memory storage
 * @return 0 on success
 */
static int memstorage_open(struct storage *sto)
{
  return 0; // Always successful for memory storage
}

/**
 * @brief _close_ implementation for memory storage.
 * @param sto Storage struct pointer for memory storage
 */
static void memstorage_close(struct storage *sto)
{
  kfree((void *)sto - offsetof(struct memstorage, storage));
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
static long memstorage_fetch(struct storage *sto, unsigned long long pos, void *buf, unsigned long bytecnt)
{
  struct memstorage *const msto = (void *)sto - offsetof(struct memstorage, storage);
  unsigned long len;

  // Check bounds
  if (pos >= msto->size)
    return 0;

  if (pos + bytecnt > msto->size)
    len = msto->size - pos;
  else
    len = bytecnt;

  memcpy(buf, (char *)msto->buf + pos, len);
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
static int memstorage_cntl(struct storage *sto, int cmd, void *arg)
{
  struct memstorage *const msto = (void *)sto - offsetof(struct memstorage, storage);
  (void)msto; // Mark as used to avoid warnings
  (void)arg;  // Mark as used to avoid warnings

  switch (cmd)
  {
  case FCNTL_GETEND:
    if (arg == NULL)
      return -EINVAL;
    *((unsigned long long *)arg) = msto->size;
    return 0;
  default:
    return -ENOTSUP;
  }
}