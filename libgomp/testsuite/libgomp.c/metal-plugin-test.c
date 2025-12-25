/* Test the Metal plugin by directly calling plugin functions.
   This test validates the plugin infrastructure without requiring
   the compiler backend.

   { dg-do run { target *-*-darwin* } }  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <stdint.h>

/* Metal device type constant from libgomp-plugin.h.  */
#define OFFLOAD_TARGET_TYPE_METAL 9

/* Plugin function types - must match plugin signatures.  */
typedef const char *(*get_name_fn)(void);
typedef unsigned int (*get_caps_fn)(void);
typedef int (*get_type_fn)(void);
typedef int (*get_num_devices_fn)(unsigned int);
typedef bool (*init_device_fn)(int);
typedef bool (*fini_device_fn)(int);
typedef void *(*alloc_fn)(int, size_t);
typedef bool (*free_fn)(int, void *);
typedef bool (*host2dev_fn)(int, void *, const void *, size_t);
typedef bool (*dev2host_fn)(int, void *, const void *, size_t);
typedef bool (*memset_fn)(int, void *, int, size_t);
typedef unsigned (*version_fn)(void);

/* Plugin handle and function pointers.  */
static void *plugin_handle;
static get_name_fn plugin_get_name;
static get_caps_fn plugin_get_caps;
static get_type_fn plugin_get_type;
static get_num_devices_fn plugin_get_num_devices;
static init_device_fn plugin_init_device;
static fini_device_fn plugin_fini_device;
static alloc_fn plugin_alloc;
static free_fn plugin_free;
static host2dev_fn plugin_host2dev;
static dev2host_fn plugin_dev2host;
static memset_fn plugin_memset;
static version_fn plugin_version;

#define DLSYM_FN(name)							\
  plugin_##name = (name##_fn)dlsym (plugin_handle,			\
				    "GOMP_OFFLOAD_" #name);		\
  if (!plugin_##name)							\
    {									\
      fprintf (stderr, "Could not find GOMP_OFFLOAD_" #name ": %s\n",	\
	       dlerror ());						\
      dlclose (plugin_handle);						\
      plugin_handle = NULL;						\
      return 0;								\
    }

static int
load_plugin (void)
{
  const char *err;

  /* Try to load the Metal plugin.  macOS uses .dylib extension.  */
  plugin_handle = dlopen ("libgomp-plugin-metal.dylib", RTLD_LAZY);
  if (!plugin_handle)
    {
      /* Try alternate path for build tree.  */
      plugin_handle = dlopen ("./.libs/libgomp-plugin-metal.dylib", RTLD_LAZY);
    }
  if (!plugin_handle)
    {
      /* Plugin not available - cache dlerror before printf.  */
      err = dlerror ();
      printf ("UNSUPPORTED: Metal plugin not available (%s)\n",
	      err ? err : "unknown error");
      return 0;
    }

  /* Load all required functions.  */
  DLSYM_FN (get_name);
  DLSYM_FN (get_caps);
  DLSYM_FN (get_type);
  DLSYM_FN (get_num_devices);
  DLSYM_FN (init_device);
  DLSYM_FN (fini_device);
  DLSYM_FN (alloc);
  DLSYM_FN (free);
  DLSYM_FN (host2dev);
  DLSYM_FN (dev2host);
  DLSYM_FN (memset);
  DLSYM_FN (version);

  return 1;
}

static void
unload_plugin (void)
{
  if (plugin_handle)
    dlclose (plugin_handle);
}

static int
test_device_info (void)
{
  const char *name;
  unsigned int caps;
  int type, num_devices;

  printf ("Testing device info...\n");

  name = plugin_get_name ();
  if (!name || strlen (name) == 0)
    {
      fprintf (stderr, "FAIL: get_name returned null or empty\n");
      return 0;
    }
  printf ("  Plugin name: %s\n", name);

  caps = plugin_get_caps ();
  printf ("  Capabilities: 0x%x\n", caps);

  type = plugin_get_type ();
  printf ("  Device type: %d\n", type);
  if (type != OFFLOAD_TARGET_TYPE_METAL)
    {
      fprintf (stderr, "FAIL: expected device type %d (Metal), got %d\n",
	       OFFLOAD_TARGET_TYPE_METAL, type);
      return 0;
    }

  num_devices = plugin_get_num_devices (0);
  printf ("  Number of devices: %d\n", num_devices);
  if (num_devices < 0)
    {
      fprintf (stderr, "FAIL: get_num_devices returned negative\n");
      return 0;
    }

  printf ("  Plugin version: %u\n", plugin_version ());

  printf ("PASS: device info\n");
  return 1;
}

static int
test_device_lifecycle (int dev)
{
  printf ("Testing device lifecycle for device %d...\n", dev);

  if (!plugin_init_device (dev))
    {
      fprintf (stderr, "FAIL: init_device failed\n");
      return 0;
    }
  printf ("  Device initialized\n");

  if (!plugin_fini_device (dev))
    {
      fprintf (stderr, "FAIL: fini_device failed\n");
      return 0;
    }
  printf ("  Device finalized\n");

  printf ("PASS: device lifecycle\n");
  return 1;
}

static int
test_memory_operations (int dev)
{
  void *dev_ptr;
  const size_t size = 1024;
  char *host_src, *host_dst;

  printf ("Testing memory operations on device %d...\n", dev);

  /* Initialize device.  */
  if (!plugin_init_device (dev))
    {
      fprintf (stderr, "FAIL: init_device failed\n");
      return 0;
    }

  /* Allocate device memory.  */
  dev_ptr = plugin_alloc (dev, size);
  if (!dev_ptr)
    {
      fprintf (stderr, "FAIL: alloc failed\n");
      plugin_fini_device (dev);
      return 0;
    }
  printf ("  Allocated %zu bytes at %p\n", size, dev_ptr);

  /* Prepare host buffers.  */
  host_src = (char *)malloc (size);
  host_dst = (char *)malloc (size);
  if (!host_src || !host_dst)
    {
      fprintf (stderr, "FAIL: host malloc failed\n");
      free (host_src);  /* Safe even if NULL.  */
      free (host_dst);
      plugin_free (dev, dev_ptr);
      plugin_fini_device (dev);
      return 0;
    }

  /* Fill source with pattern.  */
  for (size_t i = 0; i < size; i++)
    host_src[i] = (char)(i & 0xFF);
  memset (host_dst, 0, size);

  /* Test host2dev.  */
  if (!plugin_host2dev (dev, dev_ptr, host_src, size))
    {
      fprintf (stderr, "FAIL: host2dev failed\n");
      free (host_src);
      free (host_dst);
      plugin_free (dev, dev_ptr);
      plugin_fini_device (dev);
      return 0;
    }
  printf ("  host2dev: OK\n");

  /* Test dev2host.  */
  if (!plugin_dev2host (dev, host_dst, dev_ptr, size))
    {
      fprintf (stderr, "FAIL: dev2host failed\n");
      free (host_src);
      free (host_dst);
      plugin_free (dev, dev_ptr);
      plugin_fini_device (dev);
      return 0;
    }
  printf ("  dev2host: OK\n");

  /* Verify data.  */
  if (memcmp (host_src, host_dst, size) != 0)
    {
      fprintf (stderr, "FAIL: data mismatch after transfer\n");
      free (host_src);
      free (host_dst);
      plugin_free (dev, dev_ptr);
      plugin_fini_device (dev);
      return 0;
    }
  printf ("  Data verification: OK\n");

  /* Test memset.  */
  if (!plugin_memset (dev, dev_ptr, 0xAB, size))
    {
      fprintf (stderr, "FAIL: memset failed\n");
      free (host_src);
      free (host_dst);
      plugin_free (dev, dev_ptr);
      plugin_fini_device (dev);
      return 0;
    }
  printf ("  memset: OK\n");

  /* Verify memset.  */
  if (!plugin_dev2host (dev, host_dst, dev_ptr, size))
    {
      fprintf (stderr, "FAIL: dev2host after memset failed\n");
      free (host_src);
      free (host_dst);
      plugin_free (dev, dev_ptr);
      plugin_fini_device (dev);
      return 0;
    }

  for (size_t i = 0; i < size; i++)
    {
      if ((unsigned char)host_dst[i] != 0xAB)
	{
	  fprintf (stderr, "FAIL: memset verification failed at byte %zu\n", i);
	  free (host_src);
	  free (host_dst);
	  plugin_free (dev, dev_ptr);
	  plugin_fini_device (dev);
	  return 0;
	}
    }
  printf ("  memset verification: OK\n");

  /* Free device memory.  */
  if (!plugin_free (dev, dev_ptr))
    {
      fprintf (stderr, "FAIL: free failed\n");
      free (host_src);
      free (host_dst);
      plugin_fini_device (dev);
      return 0;
    }
  printf ("  free: OK\n");

  /* Cleanup.  */
  free (host_src);
  free (host_dst);
  plugin_fini_device (dev);

  printf ("PASS: memory operations\n");
  return 1;
}

int
main (void)
{
  int num_devices;

  printf ("Metal Plugin Unit Test\n");
  printf ("======================\n\n");

  if (!load_plugin ())
    {
      /* Plugin not available - this is not an error on non-macOS.  */
      return 0;
    }

  if (!test_device_info ())
    {
      unload_plugin ();
      return 1;
    }

  num_devices = plugin_get_num_devices (0);
  if (num_devices == 0)
    {
      printf ("\nNo Metal devices available - skipping device tests\n");
      unload_plugin ();
      return 0;
    }

  printf ("\n");
  if (!test_device_lifecycle (0))
    {
      unload_plugin ();
      return 1;
    }

  printf ("\n");
  if (!test_memory_operations (0))
    {
      unload_plugin ();
      return 1;
    }

  printf ("\n======================\n");
  printf ("All tests PASSED\n");

  unload_plugin ();
  return 0;
}
