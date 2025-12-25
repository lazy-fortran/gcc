/* Plugin for Apple Metal execution.

   Copyright (C) 2025 Free Software Foundation, Inc.

   Contributed by Christopher Albert.

   This file is part of the GNU Offloading and Multi Processing Library
   (libgomp).

   Libgomp is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   Libgomp is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.  */

/* Apple Metal plugin for OpenACC offloading on macOS.  */

#define _GNU_SOURCE
#include "config.h"

/* metal-cpp requires these to be defined before including headers.  */
#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Metal/Metal.hpp>
#include <Foundation/Foundation.hpp>

extern "C" {
#include "libgomp-plugin.h"
#include "gomp-constants.h"
#include "oacc-plugin.h"
}

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <pthread.h>

/* Maximum number of Metal devices supported.  */
#define MAX_METAL_DEVICES 8

/* Maximum number of tracked buffer allocations per device.  */
#define MAX_METAL_BUFFERS 4096

/* Buffer allocation tracking entry.  */
struct metal_buffer_entry
{
  void *ptr;		/* Pointer returned by buffer->contents().  */
  MTL::Buffer *buffer;	/* The Metal buffer object.  */
};

/* Per-device state.  */
struct metal_device
{
  MTL::Device *device;
  MTL::CommandQueue *queue;
  char uid[64];
  char name[128];  /* Persistent copy of device name for property queries.  */
  bool initialized;

  /* Buffer allocation tracking.  */
  struct metal_buffer_entry buffers[MAX_METAL_BUFFERS];
  int num_buffers;
};

/* Device table.  */
static struct metal_device metal_devices[MAX_METAL_DEVICES];
static int num_metal_devices = -1;
static pthread_mutex_t metal_lock = PTHREAD_MUTEX_INITIALIZER;

/* OpenACC async queue structure.  Each queue wraps a Metal command queue
   for asynchronous command submission.  */
struct goacc_asyncqueue
{
  int device_ord;		/* Device ordinal this queue belongs to.  */
  MTL::CommandQueue *queue;	/* Metal command queue for async ops.  */
  MTL::CommandBuffer *last_cmd;	/* Last submitted command buffer (for sync).  */
  MTL::SharedEvent *event;	/* Shared event for GPU-side synchronization.  */
  uint64_t event_value;		/* Current event signal value.  */
  pthread_mutex_t mutex;	/* Protects last_cmd, event, event_value.  */
};

/* Initialize device enumeration (called once).  */
static void
metal_enumerate_devices (void)
{
  if (num_metal_devices >= 0)
    return;

  pthread_mutex_lock (&metal_lock);
  if (num_metal_devices >= 0)
    {
      pthread_mutex_unlock (&metal_lock);
      return;
    }

  /* Get system default device.  On Apple Silicon this is the integrated GPU.
     MTL::CopyAllDevices() could be used to get all devices on Intel Macs
     with discrete GPUs, but for now we only support the default device.  */
  MTL::Device *device = MTL::CreateSystemDefaultDevice ();
  if (device)
    {
      num_metal_devices = 1;
      metal_devices[0].device = device;
      metal_devices[0].queue = nullptr;
      metal_devices[0].initialized = false;
      metal_devices[0].num_buffers = 0;

      /* Store device name persistently (Metal string may be autoreleased).  */
      const char *name = device->name ()->utf8String ();
      snprintf (metal_devices[0].name, sizeof (metal_devices[0].name),
		"%s", name ? name : "unknown");
      snprintf (metal_devices[0].uid, sizeof (metal_devices[0].uid),
		"metal:%s", metal_devices[0].name);
    }
  else
    num_metal_devices = 0;

  pthread_mutex_unlock (&metal_lock);
}

/* Add a buffer to the device's tracking table.  Returns true on success.
   Caller must hold metal_lock.  */
static bool
metal_add_buffer (int ord, void *ptr, MTL::Buffer *buffer)
{
  struct metal_device *dev = &metal_devices[ord];
  if (dev->num_buffers >= MAX_METAL_BUFFERS)
    return false;

  dev->buffers[dev->num_buffers].ptr = ptr;
  dev->buffers[dev->num_buffers].buffer = buffer;
  dev->num_buffers++;
  return true;
}

/* Find and remove a buffer from the device's tracking table.
   Returns the MTL::Buffer if found, nullptr otherwise.
   Caller must hold metal_lock.  */
static MTL::Buffer *
metal_remove_buffer (int ord, void *ptr)
{
  struct metal_device *dev = &metal_devices[ord];
  for (int i = 0; i < dev->num_buffers; i++)
    {
      if (dev->buffers[i].ptr == ptr)
	{
	  MTL::Buffer *buffer = dev->buffers[i].buffer;
	  /* Move last entry to fill the gap.  */
	  dev->buffers[i] = dev->buffers[dev->num_buffers - 1];
	  dev->num_buffers--;
	  return buffer;
	}
    }
  return nullptr;
}

/* Find a buffer that contains the given pointer within its bounds.
   Returns the MTL::Buffer if found, nullptr otherwise.
   Caller must hold metal_lock.  */
static MTL::Buffer *
metal_find_buffer (int ord, const void *ptr)
{
  struct metal_device *dev = &metal_devices[ord];
  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);

  for (int i = 0; i < dev->num_buffers; i++)
    {
      uintptr_t buf_start = reinterpret_cast<uintptr_t>(dev->buffers[i].ptr);
      uintptr_t buf_end = buf_start + dev->buffers[i].buffer->length ();
      if (addr >= buf_start && addr < buf_end)
	return dev->buffers[i].buffer;
    }
  return nullptr;
}

/* Validate that a device pointer with the given size fits within a registered
   Metal buffer.  Returns true if valid, false with error message if not.
   Caller must hold metal_lock.  */
static bool
metal_validate_device_ptr (int ord, const void *ptr, size_t n,
			   const char *direction)
{
  MTL::Buffer *buffer = metal_find_buffer (ord, ptr);
  if (!buffer)
    {
      GOMP_PLUGIN_error ("%s: device pointer %p is not a registered Metal buffer",
			 direction, ptr);
      return false;
    }

  uintptr_t buf_start = reinterpret_cast<uintptr_t>(buffer->contents ());
  size_t buf_len = buffer->length ();
  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);

  /* Calculate offset within buffer and check bounds without overflow.  */
  size_t offset = addr - buf_start;
  if (n > buf_len - offset)
    {
      GOMP_PLUGIN_error ("%s: transfer of %zu bytes at %p exceeds buffer bounds "
			 "(buffer size %zu, offset %zu)", direction, n, ptr,
			 buf_len, offset);
      return false;
    }

  return true;
}

extern "C" {

const char *
GOMP_OFFLOAD_get_name (void)
{
  return "metal";
}

const char *
GOMP_OFFLOAD_get_uid (int ord)
{
  metal_enumerate_devices ();
  if (ord < 0 || ord >= num_metal_devices)
    return nullptr;
  return metal_devices[ord].uid;
}

unsigned int
GOMP_OFFLOAD_get_caps (void)
{
  return GOMP_OFFLOAD_CAP_OPENACC_200;
}

int
GOMP_OFFLOAD_get_type (void)
{
  return OFFLOAD_TARGET_TYPE_METAL;
}

int
GOMP_OFFLOAD_get_num_devices (unsigned int omp_requires_mask __attribute__((unused)))
{
  metal_enumerate_devices ();
  return num_metal_devices;
}

bool
GOMP_OFFLOAD_init_device (int ord)
{
  metal_enumerate_devices ();
  if (ord < 0 || ord >= num_metal_devices)
    return false;

  pthread_mutex_lock (&metal_lock);
  if (metal_devices[ord].initialized)
    {
      pthread_mutex_unlock (&metal_lock);
      return true;
    }

  /* Create command queue for this device.  */
  MTL::Device *device = metal_devices[ord].device;
  MTL::CommandQueue *queue = device->newCommandQueue ();
  if (!queue)
    {
      pthread_mutex_unlock (&metal_lock);
      return false;
    }

  metal_devices[ord].queue = queue;
  metal_devices[ord].initialized = true;
  pthread_mutex_unlock (&metal_lock);

  return true;
}

bool
GOMP_OFFLOAD_fini_device (int ord)
{
  metal_enumerate_devices ();
  if (ord < 0 || ord >= num_metal_devices)
    return false;

  pthread_mutex_lock (&metal_lock);
  if (!metal_devices[ord].initialized)
    {
      pthread_mutex_unlock (&metal_lock);
      return true;
    }

  /* Release all remaining buffers for this device.  */
  for (int i = 0; i < metal_devices[ord].num_buffers; i++)
    {
      if (metal_devices[ord].buffers[i].buffer)
	{
	  metal_devices[ord].buffers[i].buffer->release ();
	  metal_devices[ord].buffers[i].buffer = nullptr;
	  metal_devices[ord].buffers[i].ptr = nullptr;
	}
    }
  metal_devices[ord].num_buffers = 0;

  if (metal_devices[ord].queue)
    {
      metal_devices[ord].queue->release ();
      metal_devices[ord].queue = nullptr;
    }

  if (metal_devices[ord].device)
    {
      metal_devices[ord].device->release ();
      metal_devices[ord].device = nullptr;
    }

  metal_devices[ord].initialized = false;
  pthread_mutex_unlock (&metal_lock);

  return true;
}

unsigned
GOMP_OFFLOAD_version (void)
{
  return GOMP_VERSION;
}

int
GOMP_OFFLOAD_load_image (int ord __attribute__((unused)),
			 unsigned version,
			 const void *target_data __attribute__((unused)),
			 struct addr_pair **target_table __attribute__((unused)),
			 uint64_t **rev_fn_table __attribute__((unused)),
			 uint64_t *host_ind_fn_table __attribute__((unused)))
{
  if (GOMP_VERSION_DEV (version) != GOMP_VERSION_METAL)
    {
      GOMP_PLUGIN_error ("Offload data incompatible with Metal plugin"
			 " (expected %u, received %u)",
			 GOMP_VERSION_METAL, GOMP_VERSION_DEV (version));
      return -1;
    }

  /* Metal backend not yet implemented.  When implemented, this function
     will extract MSL code from target_data, compile it using
     MTLDevice::newLibraryWithSource, create compute pipelines for each
     kernel, and return kernel addresses in target_table.  */
  GOMP_PLUGIN_error ("Metal image loading not yet implemented");
  return -1;
}

bool
GOMP_OFFLOAD_unload_image (int ord __attribute__((unused)),
			   unsigned version __attribute__((unused)),
			   const void *target_data __attribute__((unused)))
{
  GOMP_PLUGIN_error ("Metal image unloading not yet implemented");
  return false;
}

void *
GOMP_OFFLOAD_alloc (int ord, size_t size)
{
  /* Zero-size allocations return nullptr per OpenMP specification.  */
  if (size == 0)
    return nullptr;

  metal_enumerate_devices ();
  if (ord < 0 || ord >= num_metal_devices)
    return nullptr;

  pthread_mutex_lock (&metal_lock);

  if (!metal_devices[ord].initialized)
    {
      pthread_mutex_unlock (&metal_lock);
      return nullptr;
    }

  MTL::Device *device = metal_devices[ord].device;
  if (!device)
    {
      pthread_mutex_unlock (&metal_lock);
      return nullptr;
    }

  /* Use shared storage mode for unified memory on Apple Silicon.
     This means both CPU and GPU can access the same memory directly.  */
  MTL::Buffer *buffer = device->newBuffer (size,
					   MTL::ResourceStorageModeShared);
  if (!buffer)
    {
      pthread_mutex_unlock (&metal_lock);
      return nullptr;
    }

  void *ptr = buffer->contents ();
  if (!ptr)
    {
      pthread_mutex_unlock (&metal_lock);
      buffer->release ();
      return nullptr;
    }

  if (!metal_add_buffer (ord, ptr, buffer))
    {
      pthread_mutex_unlock (&metal_lock);
      buffer->release ();
      return nullptr;
    }

  pthread_mutex_unlock (&metal_lock);
  return ptr;
}

bool
GOMP_OFFLOAD_free (int ord, void *ptr)
{
  metal_enumerate_devices ();
  if (ord < 0 || ord >= num_metal_devices)
    return false;

  if (!ptr)
    return true;  /* Freeing nullptr is a no-op.  */

  pthread_mutex_lock (&metal_lock);
  MTL::Buffer *buffer = metal_remove_buffer (ord, ptr);
  pthread_mutex_unlock (&metal_lock);

  if (!buffer)
    return false;  /* Buffer not found in tracking table.  */

  buffer->release ();
  return true;
}

void *
GOMP_OFFLOAD_managed_alloc (int ord __attribute__((unused)),
			    size_t size __attribute__((unused)))
{
  GOMP_PLUGIN_error ("Metal managed allocation not yet implemented");
  return nullptr;
}

bool
GOMP_OFFLOAD_managed_free (int ord __attribute__((unused)),
			   void *ptr __attribute__((unused)))
{
  GOMP_PLUGIN_error ("Metal managed free not yet implemented");
  return false;
}

bool
GOMP_OFFLOAD_dev2host (int ord, void *dst, const void *src, size_t n)
{
  metal_enumerate_devices ();
  if (ord < 0 || ord >= num_metal_devices)
    {
      GOMP_PLUGIN_error ("invalid device ordinal %d", ord);
      return false;
    }

  if (!dst || !src)
    {
      GOMP_PLUGIN_error ("invalid %s address",
			 !dst ? "destination" : "source");
      return false;
    }

  if (n == 0)
    return true;

  pthread_mutex_lock (&metal_lock);
  if (!metal_devices[ord].initialized)
    {
      pthread_mutex_unlock (&metal_lock);
      GOMP_PLUGIN_error ("device %d not initialized", ord);
      return false;
    }

  /* Validate source is within a registered Metal buffer.  */
  if (!metal_validate_device_ptr (ord, src, n, "dev2host"))
    {
      pthread_mutex_unlock (&metal_lock);
      return false;
    }

  /* With shared storage mode on Apple Silicon, device memory is directly
     accessible from CPU.  Hold lock during copy to prevent concurrent free.  */
  std::memcpy (dst, src, n);
  pthread_mutex_unlock (&metal_lock);
  return true;
}

bool
GOMP_OFFLOAD_host2dev (int ord, void *dst, const void *src, size_t n)
{
  metal_enumerate_devices ();
  if (ord < 0 || ord >= num_metal_devices)
    {
      GOMP_PLUGIN_error ("invalid device ordinal %d", ord);
      return false;
    }

  if (!dst || !src)
    {
      GOMP_PLUGIN_error ("invalid %s address",
			 !dst ? "destination" : "source");
      return false;
    }

  if (n == 0)
    return true;

  pthread_mutex_lock (&metal_lock);
  if (!metal_devices[ord].initialized)
    {
      pthread_mutex_unlock (&metal_lock);
      GOMP_PLUGIN_error ("device %d not initialized", ord);
      return false;
    }

  /* Validate destination is within a registered Metal buffer.  */
  if (!metal_validate_device_ptr (ord, dst, n, "host2dev"))
    {
      pthread_mutex_unlock (&metal_lock);
      return false;
    }

  /* With shared storage mode on Apple Silicon, device memory is directly
     accessible from CPU.  Hold lock during copy to prevent concurrent free.  */
  std::memcpy (dst, src, n);
  pthread_mutex_unlock (&metal_lock);
  return true;
}

bool
GOMP_OFFLOAD_dev2dev (int ord, void *dst, const void *src, size_t n)
{
  metal_enumerate_devices ();
  if (ord < 0 || ord >= num_metal_devices)
    {
      GOMP_PLUGIN_error ("invalid device ordinal %d", ord);
      return false;
    }

  if (!dst || !src)
    {
      GOMP_PLUGIN_error ("invalid %s address",
			 !dst ? "destination" : "source");
      return false;
    }

  if (n == 0)
    return true;

  pthread_mutex_lock (&metal_lock);
  if (!metal_devices[ord].initialized)
    {
      pthread_mutex_unlock (&metal_lock);
      GOMP_PLUGIN_error ("device %d not initialized", ord);
      return false;
    }

  /* Validate both source and destination are within registered Metal buffers.  */
  if (!metal_validate_device_ptr (ord, src, n, "dev2dev source"))
    {
      pthread_mutex_unlock (&metal_lock);
      return false;
    }
  if (!metal_validate_device_ptr (ord, dst, n, "dev2dev destination"))
    {
      pthread_mutex_unlock (&metal_lock);
      return false;
    }

  /* Use memmove for device-to-device copies to handle overlapping regions
     safely.  Hold lock during copy to prevent concurrent free.  */
  std::memmove (dst, src, n);
  pthread_mutex_unlock (&metal_lock);
  return true;
}

bool
GOMP_OFFLOAD_memset (int ord, void *ptr, int val, size_t count)
{
  metal_enumerate_devices ();
  if (ord < 0 || ord >= num_metal_devices)
    {
      GOMP_PLUGIN_error ("invalid device ordinal %d", ord);
      return false;
    }

  if (!ptr)
    {
      GOMP_PLUGIN_error ("memset: invalid device pointer");
      return false;
    }

  if (count == 0)
    return true;

  pthread_mutex_lock (&metal_lock);
  if (!metal_devices[ord].initialized)
    {
      pthread_mutex_unlock (&metal_lock);
      GOMP_PLUGIN_error ("device %d not initialized", ord);
      return false;
    }

  if (!metal_validate_device_ptr (ord, ptr, count, "memset"))
    {
      pthread_mutex_unlock (&metal_lock);
      return false;
    }

  /* With shared storage mode, device memory is directly accessible.
     Hold lock during memset to prevent concurrent free.  */
  std::memset (ptr, val, count);
  pthread_mutex_unlock (&metal_lock);
  return true;
}

/* Strided 2D memory copy.  With Metal's shared storage mode, both host and
   device memory are directly accessible, so we implement this as a simple
   row-by-row memcpy.

   Parameters:
   - dim1_size: width in bytes of the region to copy
   - dim0_len: height (number of rows) to copy
   - dst_offset1_size: X offset in bytes in destination
   - dst_offset0_len: Y offset (row count) in destination
   - dst_dim1_size: destination pitch (full row width in bytes)
   - src_offset1_size: X offset in bytes in source
   - src_offset0_len: Y offset (row count) in source
   - src_dim1_size: source pitch (full row width in bytes)

   Returns: 1 on success, 0 on failure, -1 to use fallback.  */

int
GOMP_OFFLOAD_memcpy2d (int dst_ord, int src_ord, size_t dim1_size,
		       size_t dim0_len, void *dst, size_t dst_offset1_size,
		       size_t dst_offset0_len, size_t dst_dim1_size,
		       const void *src, size_t src_offset1_size,
		       size_t src_offset0_len, size_t src_dim1_size)
{
  /* Determine which device to use for validation.  */
  int ord = (dst_ord != -1) ? dst_ord : src_ord;

  /* Both host-to-host is not our concern.  */
  if (dst_ord == -1 && src_ord == -1)
    return -1;

  metal_enumerate_devices ();
  if (ord < 0 || ord >= num_metal_devices)
    {
      GOMP_PLUGIN_error ("memcpy2d: invalid device ordinal %d", ord);
      return 0;
    }

  if (!dst || !src)
    {
      GOMP_PLUGIN_error ("memcpy2d: invalid %s pointer",
			 !dst ? "destination" : "source");
      return 0;
    }

  if (dim0_len == 0 || dim1_size == 0)
    return 1;

  pthread_mutex_lock (&metal_lock);
  if (!metal_devices[ord].initialized)
    {
      pthread_mutex_unlock (&metal_lock);
      GOMP_PLUGIN_error ("memcpy2d: device %d not initialized", ord);
      return 0;
    }

  /* Validate that the last row offset does not overflow.  */
  size_t dst_last_row, src_last_row, dst_row_off, src_row_off;
  if (__builtin_add_overflow (dst_offset0_len, dim0_len - 1, &dst_last_row)
      || __builtin_mul_overflow (dst_last_row, dst_dim1_size, &dst_row_off)
      || __builtin_add_overflow (dst_row_off, dst_offset1_size, &dst_row_off)
      || __builtin_add_overflow (dst_row_off, dim1_size, &dst_row_off))
    {
      pthread_mutex_unlock (&metal_lock);
      GOMP_PLUGIN_error ("memcpy2d: destination address overflow");
      return 0;
    }
  if (__builtin_add_overflow (src_offset0_len, dim0_len - 1, &src_last_row)
      || __builtin_mul_overflow (src_last_row, src_dim1_size, &src_row_off)
      || __builtin_add_overflow (src_row_off, src_offset1_size, &src_row_off)
      || __builtin_add_overflow (src_row_off, dim1_size, &src_row_off))
    {
      pthread_mutex_unlock (&metal_lock);
      GOMP_PLUGIN_error ("memcpy2d: source address overflow");
      return 0;
    }

  /* With shared storage mode, both host and device memory are directly
     accessible.  Perform row-by-row copy with appropriate strides.  */
  char *d = static_cast<char *>(dst);
  const char *s = static_cast<const char *>(src);

  for (size_t row = 0; row < dim0_len; row++)
    {
      char *dst_row = d + (dst_offset0_len + row) * dst_dim1_size
		      + dst_offset1_size;
      const char *src_row = s + (src_offset0_len + row) * src_dim1_size
			    + src_offset1_size;
      std::memcpy (dst_row, src_row, dim1_size);
    }

  pthread_mutex_unlock (&metal_lock);
  return 1;
}

/* Strided 3D memory copy.  With Metal's shared storage mode, both host and
   device memory are directly accessible, so we implement this as a simple
   slice-by-slice, row-by-row memcpy.

   Parameters:
   - dim2_size: width in bytes of the region to copy (innermost)
   - dim1_len: height (number of rows per slice) to copy
   - dim0_len: depth (number of slices) to copy
   - dst_offset2_size: X offset in bytes in destination
   - dst_offset1_len: Y offset (row count) in destination
   - dst_offset0_len: Z offset (slice count) in destination
   - dst_dim2_size: destination row pitch (full row width in bytes)
   - dst_dim1_len: destination slice height (rows per slice)
   - src_offset2_size: X offset in bytes in source
   - src_offset1_len: Y offset (row count) in source
   - src_offset0_len: Z offset (slice count) in source
   - src_dim2_size: source row pitch (full row width in bytes)
   - src_dim1_len: source slice height (rows per slice)

   Returns: 1 on success, 0 on failure, -1 to use fallback.  */

int
GOMP_OFFLOAD_memcpy3d (int dst_ord, int src_ord, size_t dim2_size,
		       size_t dim1_len, size_t dim0_len, void *dst,
		       size_t dst_offset2_size, size_t dst_offset1_len,
		       size_t dst_offset0_len, size_t dst_dim2_size,
		       size_t dst_dim1_len, const void *src,
		       size_t src_offset2_size, size_t src_offset1_len,
		       size_t src_offset0_len, size_t src_dim2_size,
		       size_t src_dim1_len)
{
  /* Determine which device to use for validation.  */
  int ord = (dst_ord != -1) ? dst_ord : src_ord;

  /* Both host-to-host is not our concern.  */
  if (dst_ord == -1 && src_ord == -1)
    return -1;

  metal_enumerate_devices ();
  if (ord < 0 || ord >= num_metal_devices)
    {
      GOMP_PLUGIN_error ("memcpy3d: invalid device ordinal %d", ord);
      return 0;
    }

  if (!dst || !src)
    {
      GOMP_PLUGIN_error ("memcpy3d: invalid %s pointer",
			 !dst ? "destination" : "source");
      return 0;
    }

  if (dim0_len == 0 || dim1_len == 0 || dim2_size == 0)
    return 1;

  pthread_mutex_lock (&metal_lock);
  if (!metal_devices[ord].initialized)
    {
      pthread_mutex_unlock (&metal_lock);
      GOMP_PLUGIN_error ("memcpy3d: device %d not initialized", ord);
      return 0;
    }

  /* Calculate slice sizes with overflow check.  */
  size_t dst_slice_size, src_slice_size;
  if (__builtin_mul_overflow (dst_dim2_size, dst_dim1_len, &dst_slice_size))
    {
      pthread_mutex_unlock (&metal_lock);
      GOMP_PLUGIN_error ("memcpy3d: destination slice size overflow");
      return 0;
    }
  if (__builtin_mul_overflow (src_dim2_size, src_dim1_len, &src_slice_size))
    {
      pthread_mutex_unlock (&metal_lock);
      GOMP_PLUGIN_error ("memcpy3d: source slice size overflow");
      return 0;
    }

  /* Validate that the last element offset does not overflow.  */
  size_t dst_last_slice, dst_last_row, dst_off, tmp;
  if (__builtin_add_overflow (dst_offset0_len, dim0_len - 1, &dst_last_slice)
      || __builtin_mul_overflow (dst_last_slice, dst_slice_size, &dst_off)
      || __builtin_add_overflow (dst_offset1_len, dim1_len - 1, &dst_last_row)
      || __builtin_mul_overflow (dst_last_row, dst_dim2_size, &tmp)
      || __builtin_add_overflow (dst_off, tmp, &dst_off)
      || __builtin_add_overflow (dst_off, dst_offset2_size, &dst_off)
      || __builtin_add_overflow (dst_off, dim2_size, &dst_off))
    {
      pthread_mutex_unlock (&metal_lock);
      GOMP_PLUGIN_error ("memcpy3d: destination address overflow");
      return 0;
    }

  size_t src_last_slice, src_last_row, src_off;
  if (__builtin_add_overflow (src_offset0_len, dim0_len - 1, &src_last_slice)
      || __builtin_mul_overflow (src_last_slice, src_slice_size, &src_off)
      || __builtin_add_overflow (src_offset1_len, dim1_len - 1, &src_last_row)
      || __builtin_mul_overflow (src_last_row, src_dim2_size, &tmp)
      || __builtin_add_overflow (src_off, tmp, &src_off)
      || __builtin_add_overflow (src_off, src_offset2_size, &src_off)
      || __builtin_add_overflow (src_off, dim2_size, &src_off))
    {
      pthread_mutex_unlock (&metal_lock);
      GOMP_PLUGIN_error ("memcpy3d: source address overflow");
      return 0;
    }

  /* With shared storage mode, both host and device memory are directly
     accessible.  Perform slice-by-slice, row-by-row copy.  */
  char *d = static_cast<char *>(dst);
  const char *s = static_cast<const char *>(src);

  for (size_t slice = 0; slice < dim0_len; slice++)
    {
      for (size_t row = 0; row < dim1_len; row++)
	{
	  char *dst_row = d
			  + (dst_offset0_len + slice) * dst_slice_size
			  + (dst_offset1_len + row) * dst_dim2_size
			  + dst_offset2_size;
	  const char *src_row = s
				+ (src_offset0_len + slice) * src_slice_size
				+ (src_offset1_len + row) * src_dim2_size
				+ src_offset2_size;
	  std::memcpy (dst_row, src_row, dim2_size);
	}
    }

  pthread_mutex_unlock (&metal_lock);
  return 1;
}

void
GOMP_OFFLOAD_run (int ord __attribute__((unused)),
		  void *tgt_fn __attribute__((unused)),
		  void *tgt_vars __attribute__((unused)),
		  void **args __attribute__((unused)))
{
  GOMP_PLUGIN_fatal ("Metal kernel execution not yet implemented");
}

void
GOMP_OFFLOAD_async_run (int ord __attribute__((unused)),
			void *tgt_fn __attribute__((unused)),
			void *tgt_vars __attribute__((unused)),
			void **args __attribute__((unused)),
			void *async_data __attribute__((unused)))
{
  GOMP_PLUGIN_fatal ("Metal async kernel execution not yet implemented");
}

/* OpenACC functions.  */

void
GOMP_OFFLOAD_openacc_exec (void (*fn) (void *) __attribute__((unused)),
			   size_t mapnum __attribute__((unused)),
			   void **hostaddrs __attribute__((unused)),
			   void **devaddrs __attribute__((unused)),
			   unsigned *dims __attribute__((unused)),
			   void *targ_mem_desc __attribute__((unused)))
{
  GOMP_PLUGIN_fatal ("Metal OpenACC kernel execution not yet implemented");
}

void
GOMP_OFFLOAD_openacc_async_exec (void (*fn) (void *) __attribute__((unused)),
				 size_t mapnum __attribute__((unused)),
				 void **hostaddrs __attribute__((unused)),
				 void **devaddrs __attribute__((unused)),
				 unsigned *dims __attribute__((unused)),
				 void *targ_mem_desc __attribute__((unused)),
				 struct goacc_asyncqueue *aq __attribute__((unused)))
{
  GOMP_PLUGIN_fatal ("Metal OpenACC async kernel execution not yet implemented");
}

void *
GOMP_OFFLOAD_openacc_create_thread_data (int ord __attribute__((unused)))
{
  return nullptr;
}

void
GOMP_OFFLOAD_openacc_destroy_thread_data (void *data __attribute__((unused)))
{
}

/* Helper function to signal the queue's event after GPU work.
   Infrastructure for kernel execution - will be called after
   committing command buffers that contain GPU work.

   Usage (when kernel execution is implemented):
     MTL::CommandBuffer *cmd = queue->commandBuffer();
     // ... encode GPU work ...
     metal_async_queue_signal(aq, cmd);
     cmd->commit();

   This enables GPU-side synchronization via async_serialize.  */
static void
metal_async_queue_signal (struct goacc_asyncqueue *aq,
			  MTL::CommandBuffer *cmd)
{
  if (!aq || !cmd || !aq->event)
    return;

  /* Increment event value and encode signal.  The command buffer
     will signal this value upon completion.  */
  pthread_mutex_lock (&aq->mutex);
  aq->event_value++;
  cmd->encodeSignalEvent (aq->event, aq->event_value);
  pthread_mutex_unlock (&aq->mutex);
}

/* Helper function to encode a wait for another queue's event.
   Infrastructure for kernel execution - will be called when
   creating command buffers that must wait for another queue.

   Usage (when kernel execution is implemented):
     MTL::CommandBuffer *cmd = queue->commandBuffer();
     metal_async_queue_encode_wait(my_aq, other_aq, cmd);
     // ... encode GPU work that depends on other_aq ...
     cmd->commit();

   Returns the event value that was waited for, or 0 if no wait
   was encoded.  */
static uint64_t
metal_async_queue_encode_wait (struct goacc_asyncqueue *waiter_aq,
			       struct goacc_asyncqueue *target_aq,
			       MTL::CommandBuffer *cmd)
{
  if (!waiter_aq || !target_aq || !cmd || !target_aq->event)
    return 0;

  /* Get the current event value to wait for.  */
  pthread_mutex_lock (&target_aq->mutex);
  uint64_t wait_value = target_aq->event_value;
  pthread_mutex_unlock (&target_aq->mutex);

  if (wait_value > 0)
    cmd->encodeWaitForEvent (target_aq->event, wait_value);

  return wait_value;
}

struct goacc_asyncqueue *
GOMP_OFFLOAD_openacc_async_construct (int device)
{
  metal_enumerate_devices ();
  if (device < 0 || device >= num_metal_devices)
    {
      GOMP_PLUGIN_error ("invalid device ordinal %d", device);
      return nullptr;
    }

  pthread_mutex_lock (&metal_lock);
  if (!metal_devices[device].initialized)
    {
      pthread_mutex_unlock (&metal_lock);
      GOMP_PLUGIN_error ("device %d not initialized", device);
      return nullptr;
    }

  MTL::Device *mtl_device = metal_devices[device].device;
  pthread_mutex_unlock (&metal_lock);

  if (!mtl_device)
    {
      GOMP_PLUGIN_error ("device %d has no Metal device", device);
      return nullptr;
    }

  /* Create a new command queue for this async queue.  */
  MTL::CommandQueue *queue = mtl_device->newCommandQueue ();
  if (!queue)
    {
      GOMP_PLUGIN_error ("failed to create command queue for device %d", device);
      return nullptr;
    }

  /* Create shared event for GPU-side synchronization between queues.
     SharedEvents allow one queue to signal completion and another queue
     to wait without blocking the CPU thread.  */
  MTL::SharedEvent *event = mtl_device->newSharedEvent ();
  if (!event)
    {
      queue->release ();
      GOMP_PLUGIN_error ("failed to create shared event for device %d", device);
      return nullptr;
    }

  struct goacc_asyncqueue *aq = new struct goacc_asyncqueue;
  aq->device_ord = device;
  aq->queue = queue;
  aq->last_cmd = nullptr;
  aq->event = event;
  aq->event_value = 0;
  pthread_mutex_init (&aq->mutex, nullptr);

  return aq;
}

bool
GOMP_OFFLOAD_openacc_async_destruct (struct goacc_asyncqueue *aq)
{
  if (!aq)
    return true;

  pthread_mutex_lock (&aq->mutex);

  /* Wait for any pending commands to complete before destroying.  */
  if (aq->last_cmd)
    {
      aq->last_cmd->waitUntilCompleted ();
      aq->last_cmd->release ();
      aq->last_cmd = nullptr;
    }

  if (aq->event)
    {
      aq->event->release ();
      aq->event = nullptr;
    }

  if (aq->queue)
    {
      aq->queue->release ();
      aq->queue = nullptr;
    }

  pthread_mutex_unlock (&aq->mutex);
  pthread_mutex_destroy (&aq->mutex);

  delete aq;
  return true;
}

int
GOMP_OFFLOAD_openacc_async_test (struct goacc_asyncqueue *aq)
{
  if (!aq)
    return 1;  /* Null queue is always complete.  */

  pthread_mutex_lock (&aq->mutex);

  if (!aq->last_cmd)
    {
      pthread_mutex_unlock (&aq->mutex);
      return 1;  /* No commands submitted yet.  */
    }

  /* Check if the last command buffer has completed.
     Metal command buffer status:
     - MTL::CommandBufferStatusNotEnqueued = 0
     - MTL::CommandBufferStatusEnqueued = 1
     - MTL::CommandBufferStatusCommitted = 2
     - MTL::CommandBufferStatusScheduled = 3
     - MTL::CommandBufferStatusCompleted = 4
     - MTL::CommandBufferStatusError = 5  */
  MTL::CommandBufferStatus status = aq->last_cmd->status ();
  pthread_mutex_unlock (&aq->mutex);

  return (status == MTL::CommandBufferStatusCompleted
	  || status == MTL::CommandBufferStatusError) ? 1 : 0;
}

bool
GOMP_OFFLOAD_openacc_async_synchronize (struct goacc_asyncqueue *aq)
{
  if (!aq)
    return true;

  pthread_mutex_lock (&aq->mutex);

  if (!aq->last_cmd)
    {
      pthread_mutex_unlock (&aq->mutex);
      return true;  /* No commands to wait for.  */
    }

  /* Wait for the last command buffer to complete.  */
  aq->last_cmd->waitUntilCompleted ();

  /* Check for errors.  */
  bool success = true;
  if (aq->last_cmd->status () == MTL::CommandBufferStatusError)
    {
      GOMP_PLUGIN_error ("Metal command buffer error");
      success = false;
    }

  pthread_mutex_unlock (&aq->mutex);
  return success;
}

bool
GOMP_OFFLOAD_openacc_async_serialize (struct goacc_asyncqueue *aq1,
				      struct goacc_asyncqueue *aq2)
{
  /* Serialize aq2 to wait for aq1's completion.  After this call,
     any subsequent operations on aq2 are guaranteed to execute after
     aq1's current operations complete.

     SharedEvent-based synchronization is available via:
     - metal_async_queue_signal: call after each command buffer commit on aq1
     - metal_async_queue_encode_wait: call before command buffer commit on aq2

     When kernel execution is implemented, each command buffer will call
     metal_async_queue_signal after encoding GPU work, allowing aq2 to
     use metal_async_queue_encode_wait to create GPU-side dependencies
     without blocking the CPU thread.

     Current implementation: Block on aq1's completion via synchronize.  */
  if (!aq1 || !aq2)
    return true;  /* Nothing to serialize.  */

  return GOMP_OFFLOAD_openacc_async_synchronize (aq1);
}

void
GOMP_OFFLOAD_openacc_async_queue_callback (struct goacc_asyncqueue *aq,
					   void (*fn) (void *),
					   void *data)
{
  if (!aq || !fn)
    return;

  /* For now, execute callback synchronously after waiting for completion.
     A proper implementation would use Metal's addCompletedHandler.

     Note: We still execute the callback even if synchronization reports
     an error, as the callback may need to perform cleanup.  The callback
     should check device state if error handling is needed.  */
  GOMP_OFFLOAD_openacc_async_synchronize (aq);
  fn (data);
}

bool
GOMP_OFFLOAD_openacc_async_host2dev (int ord, void *dst, const void *src,
				     size_t n,
				     struct goacc_asyncqueue *aq __attribute__((unused)))
{
  /* With shared storage mode on Apple Silicon, the memory is unified.
     Async transfers are effectively synchronous memcpy operations since
     both CPU and GPU access the same physical memory.  The async queue
     is not used for the actual copy - it would only be relevant if we
     needed to schedule the copy as a GPU operation.  */
  metal_enumerate_devices ();
  if (ord < 0 || ord >= num_metal_devices)
    {
      GOMP_PLUGIN_error ("invalid device ordinal %d", ord);
      return false;
    }

  if (!dst || !src)
    {
      GOMP_PLUGIN_error ("invalid %s address",
			 !dst ? "destination" : "source");
      return false;
    }

  if (n == 0)
    return true;

  /* Validate destination and perform copy under lock to prevent concurrent free.  */
  pthread_mutex_lock (&metal_lock);
  if (!metal_validate_device_ptr (ord, dst, n, "async_host2dev"))
    {
      pthread_mutex_unlock (&metal_lock);
      return false;
    }

  std::memcpy (dst, src, n);
  pthread_mutex_unlock (&metal_lock);
  return true;
}

bool
GOMP_OFFLOAD_openacc_async_dev2host (int ord, void *dst, const void *src,
				     size_t n,
				     struct goacc_asyncqueue *aq __attribute__((unused)))
{
  /* With shared storage mode, async dev2host is a synchronous memcpy.  */
  metal_enumerate_devices ();
  if (ord < 0 || ord >= num_metal_devices)
    {
      GOMP_PLUGIN_error ("invalid device ordinal %d", ord);
      return false;
    }

  if (!dst || !src)
    {
      GOMP_PLUGIN_error ("invalid %s address",
			 !dst ? "destination" : "source");
      return false;
    }

  if (n == 0)
    return true;

  /* Validate source and perform copy under lock to prevent concurrent free.  */
  pthread_mutex_lock (&metal_lock);
  if (!metal_validate_device_ptr (ord, src, n, "async_dev2host"))
    {
      pthread_mutex_unlock (&metal_lock);
      return false;
    }

  std::memcpy (dst, src, n);
  pthread_mutex_unlock (&metal_lock);
  return true;
}

bool
GOMP_OFFLOAD_openacc_async_dev2dev (int ord, void *dst, const void *src,
				    size_t n,
				    struct goacc_asyncqueue *aq __attribute__((unused)))
{
  /* With shared storage mode, async dev2dev is a synchronous memmove.  */
  metal_enumerate_devices ();
  if (ord < 0 || ord >= num_metal_devices)
    {
      GOMP_PLUGIN_error ("invalid device ordinal %d", ord);
      return false;
    }

  if (!dst || !src)
    {
      GOMP_PLUGIN_error ("invalid %s address",
			 !dst ? "destination" : "source");
      return false;
    }

  if (n == 0)
    return true;

  /* Validate both pointers and perform copy under lock to prevent concurrent free.  */
  pthread_mutex_lock (&metal_lock);
  if (!metal_validate_device_ptr (ord, src, n, "async_dev2dev source"))
    {
      pthread_mutex_unlock (&metal_lock);
      return false;
    }
  if (!metal_validate_device_ptr (ord, dst, n, "async_dev2dev destination"))
    {
      pthread_mutex_unlock (&metal_lock);
      return false;
    }

  std::memmove (dst, src, n);
  pthread_mutex_unlock (&metal_lock);
  return true;
}

union goacc_property_value
GOMP_OFFLOAD_openacc_get_property (int ord, enum goacc_property prop)
{
  union goacc_property_value propval = { .val = 0 };

  metal_enumerate_devices ();
  if (ord < 0 || ord >= num_metal_devices)
    {
      GOMP_PLUGIN_error ("invalid device ordinal %d for property query", ord);
      return propval;
    }

  pthread_mutex_lock (&metal_lock);

  if (!metal_devices[ord].device)
    {
      pthread_mutex_unlock (&metal_lock);
      GOMP_PLUGIN_error ("device %d has no Metal device", ord);
      return propval;
    }

  MTL::Device *device = metal_devices[ord].device;

  switch (prop)
    {
    case GOACC_PROPERTY_MEMORY:
      /* Metal API doesn't provide total device memory.  Return
	 recommendedMaxWorkingSetSize() as approximation.  This is Apple's
	 recommendation for optimal working set size, NOT total memory.
	 On unified memory systems, actual available memory may be larger.  */
      propval.val = device->recommendedMaxWorkingSetSize ();
      break;

    case GOACC_PROPERTY_FREE_MEMORY:
      /* Metal doesn't provide a direct API for free memory.
	 Return 0 to indicate "unknown" per OpenACC spec.  */
      propval.val = 0;
      break;

    case GOACC_PROPERTY_NAME:
      /* Return the persistent device name stored during enumeration.  */
      propval.ptr = const_cast<char *>(metal_devices[ord].name);
      break;

    case GOACC_PROPERTY_VENDOR:
      propval.ptr = const_cast<char *>("Apple");
      break;

    case GOACC_PROPERTY_DRIVER:
      propval.ptr = const_cast<char *>("Metal");
      break;

    default:
      break;
    }

  pthread_mutex_unlock (&metal_lock);
  return propval;
}

} /* extern "C" */
