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
  bool initialized;

  /* Buffer allocation tracking.  */
  struct metal_buffer_entry buffers[MAX_METAL_BUFFERS];
  int num_buffers;
};

/* Device table.  */
static struct metal_device metal_devices[MAX_METAL_DEVICES];
static int num_metal_devices = -1;
static pthread_mutex_t metal_lock = PTHREAD_MUTEX_INITIALIZER;

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

      /* Store device name as UID.  */
      const char *name = device->name ()->utf8String ();
      snprintf (metal_devices[0].uid, sizeof (metal_devices[0].uid),
		"metal:%s", name ? name : "unknown");
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
			 unsigned version __attribute__((unused)),
			 const void *target_data __attribute__((unused)),
			 struct addr_pair **target_table __attribute__((unused)),
			 uint64_t **rev_fn_table __attribute__((unused)),
			 uint64_t *host_ind_fn_table __attribute__((unused)))
{
  /* TODO: Load Metal library and extract kernels.  */
  return -1;
}

bool
GOMP_OFFLOAD_unload_image (int ord __attribute__((unused)),
			   unsigned version __attribute__((unused)),
			   const void *target_data __attribute__((unused)))
{
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
  return nullptr;
}

bool
GOMP_OFFLOAD_managed_free (int ord __attribute__((unused)),
			   void *ptr __attribute__((unused)))
{
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

  if (n == 0)
    return true;

  if (!dst || !src)
    {
      GOMP_PLUGIN_error ("invalid %s address",
			 !dst ? "destination" : "source");
      return false;
    }

  pthread_mutex_lock (&metal_lock);
  if (!metal_devices[ord].initialized)
    {
      pthread_mutex_unlock (&metal_lock);
      GOMP_PLUGIN_error ("device %d not initialized", ord);
      return false;
    }
  pthread_mutex_unlock (&metal_lock);

  /* With shared storage mode on Apple Silicon, device memory is directly
     accessible from CPU.  A simple memcpy suffices.  */
  std::memcpy (dst, src, n);
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

  if (n == 0)
    return true;

  if (!dst || !src)
    {
      GOMP_PLUGIN_error ("invalid %s address",
			 !dst ? "destination" : "source");
      return false;
    }

  pthread_mutex_lock (&metal_lock);
  if (!metal_devices[ord].initialized)
    {
      pthread_mutex_unlock (&metal_lock);
      GOMP_PLUGIN_error ("device %d not initialized", ord);
      return false;
    }
  pthread_mutex_unlock (&metal_lock);

  /* With shared storage mode on Apple Silicon, device memory is directly
     accessible from CPU.  A simple memcpy suffices.  */
  std::memcpy (dst, src, n);
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

  if (n == 0)
    return true;

  if (!dst || !src)
    {
      GOMP_PLUGIN_error ("invalid %s address",
			 !dst ? "destination" : "source");
      return false;
    }

  pthread_mutex_lock (&metal_lock);
  if (!metal_devices[ord].initialized)
    {
      pthread_mutex_unlock (&metal_lock);
      GOMP_PLUGIN_error ("device %d not initialized", ord);
      return false;
    }
  pthread_mutex_unlock (&metal_lock);

  /* Use memmove for device-to-device copies to handle overlapping regions
     safely.  With shared storage mode, this is a CPU-side operation.  */
  std::memmove (dst, src, n);
  return true;
}

void
GOMP_OFFLOAD_run (int ord __attribute__((unused)),
		  void *tgt_fn __attribute__((unused)),
		  void *tgt_vars __attribute__((unused)),
		  void **args __attribute__((unused)))
{
}

void
GOMP_OFFLOAD_async_run (int ord __attribute__((unused)),
			void *tgt_fn __attribute__((unused)),
			void *tgt_vars __attribute__((unused)),
			void **args __attribute__((unused)),
			void *async_data __attribute__((unused)))
{
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

struct goacc_asyncqueue *
GOMP_OFFLOAD_openacc_async_construct (int device __attribute__((unused)))
{
  return nullptr;
}

bool
GOMP_OFFLOAD_openacc_async_destruct (struct goacc_asyncqueue *aq __attribute__((unused)))
{
  return false;
}

int
GOMP_OFFLOAD_openacc_async_test (struct goacc_asyncqueue *aq __attribute__((unused)))
{
  return 0;
}

bool
GOMP_OFFLOAD_openacc_async_synchronize (struct goacc_asyncqueue *aq __attribute__((unused)))
{
  return false;
}

bool
GOMP_OFFLOAD_openacc_async_serialize (struct goacc_asyncqueue *aq1 __attribute__((unused)),
				      struct goacc_asyncqueue *aq2 __attribute__((unused)))
{
  return false;
}

void
GOMP_OFFLOAD_openacc_async_queue_callback (struct goacc_asyncqueue *aq __attribute__((unused)),
					   void (*fn) (void *) __attribute__((unused)),
					   void *data __attribute__((unused)))
{
}

bool
GOMP_OFFLOAD_openacc_async_host2dev (int ord __attribute__((unused)),
				     void *dst __attribute__((unused)),
				     const void *src __attribute__((unused)),
				     size_t n __attribute__((unused)),
				     struct goacc_asyncqueue *aq __attribute__((unused)))
{
  return false;
}

bool
GOMP_OFFLOAD_openacc_async_dev2host (int ord __attribute__((unused)),
				     void *dst __attribute__((unused)),
				     const void *src __attribute__((unused)),
				     size_t n __attribute__((unused)),
				     struct goacc_asyncqueue *aq __attribute__((unused)))
{
  return false;
}

bool
GOMP_OFFLOAD_openacc_async_dev2dev (int ord __attribute__((unused)),
				    void *dst __attribute__((unused)),
				    const void *src __attribute__((unused)),
				    size_t n __attribute__((unused)),
				    struct goacc_asyncqueue *aq __attribute__((unused)))
{
  return false;
}

union goacc_property_value
GOMP_OFFLOAD_openacc_get_property (int ord __attribute__((unused)),
				   enum goacc_property prop __attribute__((unused)))
{
  union goacc_property_value val = { .val = 0 };

  return val;
}

} /* extern "C" */
