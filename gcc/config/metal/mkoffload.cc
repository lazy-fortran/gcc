/* Offload image generation tool for Apple Metal.

   Copyright (C) 2025 Free Software Foundation, Inc.

   Contributed by Christopher Albert.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 3, or (at your
   option) any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  */

/* Packages Metal Shading Language (MSL) code into a C source file defining
   the MSL code as a string, similar to how NVPTX mkoffload handles PTX.

   This is a skeleton implementation.  Full Metal offloading requires:
   1. A Metal compiler backend to generate MSL from GIMPLE/LTO
   2. Runtime MSL compilation using MTLDevice::newLibraryWithSource  */

#define IN_TARGET_CODE 1

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "obstack.h"
#include "diagnostic.h"
#include "intl.h"
#include <libgen.h>
#include "collect-utils.h"
#include "gomp-constants.h"

const char tool_name[] = "metal mkoffload";

#define COMMENT_PREFIX "//"

struct id_map
{
  id_map *next;
  char *msl_name;
  char *dim;
};

static id_map *func_ids, **funcs_tail = &func_ids;
static id_map *var_ids, **vars_tail = &var_ids;

/* Files to unlink.  */
static const char *msl_name;
static const char *msl_cfile_name;
static const char *metallib_name;
static const char *air_name;
static const char *omp_requires_file;
static const char *msl_dumpbase;

enum offload_abi offload_abi = OFFLOAD_ABI_UNSET;
const char *offload_abi_host_opts = NULL;

/* Delete tempfiles.  */

void
tool_cleanup (bool from_signal ATTRIBUTE_UNUSED)
{
  if (msl_cfile_name)
    maybe_unlink (msl_cfile_name);
  if (msl_name)
    maybe_unlink (msl_name);
  if (metallib_name)
    maybe_unlink (metallib_name);
  if (air_name)
    maybe_unlink (air_name);
  if (omp_requires_file)
    maybe_unlink (omp_requires_file);
}

static void
mkoffload_cleanup (void)
{
  tool_cleanup (false);
}

/* Unlink FILE unless requested otherwise.  */

void
maybe_unlink (const char *file)
{
  if (!save_temps)
    {
      if (unlink_if_ordinary (file)
	  && errno != ENOENT)
	fatal_error (input_location, "deleting file %s: %m", file);
    }
  else if (verbose)
    fprintf (stderr, "[Leaving %s]\n", file);
}

/* Add or change the value of an environment variable, outputting the
   change to standard error if in verbose mode.  */
static void
xputenv (const char *string)
{
  if (verbose)
    fprintf (stderr, "%s\n", string);
  putenv (CONST_CAST (char *, string));
}

static void
record_id (const char *p1, id_map ***where)
{
  gcc_assert (p1[0] == '"');
  p1++;
  const char *end = strchr (p1, '"');
  const char *end2 = strchr (p1, '\n');
  if (!end || !end2 || end >= end2)
    fatal_error (input_location, "malformed input file");

  id_map *v = XNEW (id_map);
  size_t len = end - p1;
  v->msl_name = XNEWVEC (char, len + 1);
  memcpy (v->msl_name, p1, len);
  v->msl_name[len] = '\0';
  p1 = end + 1;
  if (*end != '\n')
    {
      len = end2 - p1;
      v->dim = XNEWVEC (char, len + 1);
      memcpy (v->dim, p1, len);
      v->dim[len] = '\0';
    }
  else
    v->dim = NULL;
  v->next = NULL;
  id_map **tail = *where;
  *tail = v;
  *where = &v->next;
}

/* Read the whole input file.  It will be NUL terminated.  */

static const char *
read_file (FILE *stream, size_t *plen)
{
  size_t alloc = 16384;
  size_t base = 0;
  char *buffer;

  if (!fseek (stream, 0, SEEK_END))
    {
      /* Get the file size.  */
      long s = ftell (stream);
      if (s >= 0)
	alloc = s + 100;
      fseek (stream, 0, SEEK_SET);
    }
  buffer = XNEWVEC (char, alloc);

  for (;;)
    {
      size_t n = fread (buffer + base, 1, alloc - base - 1, stream);

      if (!n)
	break;
      base += n;
      if (base + 1 == alloc)
	{
	  alloc *= 2;
	  buffer = XRESIZEVEC (char, buffer, alloc);
	}
    }
  buffer[base] = 0;
  *plen = base;
  return buffer;
}

/* Process input file and generate MSL output.  */

static void
process (FILE *in, FILE *out, uint32_t omp_requires)
{
  size_t len = 0;
  const char *input = read_file (in, &len);
  const char *comma;
  id_map const *id;
  unsigned obj_count = 0;

  fprintf (out, "#include <stdint.h>\n\n");

  /* Generate MSL code arrays.  For now, we create a simple placeholder
     that demonstrates the structure.  A full implementation would parse
     GIMPLE/LTO and translate to MSL.  */
  fprintf (out, "static const char msl_code_%u[] =\n", obj_count++);
  fprintf (out, "\t\"#include <metal_stdlib>\\n\"\n");
  fprintf (out, "\t\"using namespace metal;\\n\\n\"\n");

  /* Parse input for function/variable mappings.  */
  for (size_t i = 0; i != len; i++)
    {
      /* Look for mapping directives similar to NVPTX format.  */
      if (startswith (input + i, "//VAR_MAP "))
	record_id (input + i + 10, &vars_tail);
      else if (startswith (input + i, "//FUNC_MAP "))
	record_id (input + i + 11, &funcs_tail);

      /* Skip to next line.  */
      while (i < len && input[i] != '\n')
	i++;
    }

  /* Generate example kernel for each function.  */
  for (id = func_ids; id; id = id->next)
    {
      fprintf (out, "\t\"kernel void %s(", id->msl_name);
      fprintf (out, "device float* data [[buffer(0)]], ");
      fprintf (out, "uint tid [[thread_position_in_grid]]) {\\n\"\n");
      fprintf (out, "\t\"  // Placeholder kernel body\\n\"\n");
      fprintf (out, "\t\"}\\n\\n\"");
    }

  fprintf (out, ";\n\n");

  /* Dump out array of MSL code objects.  */
  fprintf (out, "static const struct msl_obj {\n"
	   "  const char *code;\n"
	   "  __SIZE_TYPE__ size;\n"
	   "} msl_objs[] = {");
  unsigned ix;
  for (comma = "", ix = 0; ix != obj_count; comma = ",", ix++)
    fprintf (out, "%s\n\t{msl_code_%u, sizeof (msl_code_%u)}", comma, ix, ix);
  fprintf (out, "\n};\n\n");

  /* Dump out variable idents.  */
  fprintf (out, "static const char *const var_mappings[] = {");
  for (comma = "", id = var_ids; id; comma = ",", id = id->next)
    fprintf (out, "%s\n\t\"%s\"", comma, id->msl_name);
  fprintf (out, "\n};\n\n");

  /* Dump out function idents.  */
  fprintf (out, "static const struct metal_fn {\n"
	   "  const char *name;\n"
	   "  unsigned short dim[%d];\n"
	   "} func_mappings[] = {\n", GOMP_DIM_MAX);
  for (comma = "", id = func_ids; id; comma = ",", id = id->next)
    fprintf (out, "%s\n\t{\"%s\"%s}", comma, id->msl_name,
	     id->dim ? id->dim : "");
  fprintf (out, "\n};\n\n");

  fprintf (out,
	   "static const struct metal_data {\n"
	   "  uintptr_t omp_requires_mask;\n"
	   "  const struct msl_obj *msl_objs;\n"
	   "  unsigned msl_num;\n"
	   "  const char *const *var_names;\n"
	   "  unsigned var_num;\n"
	   "  const struct metal_fn *fn_names;\n"
	   "  unsigned fn_num;\n"
	   "} metal_data = {\n"
	   "  %d, msl_objs, sizeof (msl_objs) / sizeof (msl_objs[0]),\n"
	   "  var_mappings,"
	   "  sizeof (var_mappings) / sizeof (var_mappings[0]),\n"
	   "  func_mappings,"
	   "  sizeof (func_mappings) / sizeof (func_mappings[0])\n"
	   "};\n\n", omp_requires);

  fprintf (out, "#ifdef __cplusplus\n"
	   "extern \"C\" {\n"
	   "#endif\n");

  fprintf (out, "extern void GOMP_offload_register_ver"
	   " (unsigned, const void *, int, const void *);\n");
  fprintf (out, "extern void GOMP_offload_unregister_ver"
	   " (unsigned, const void *, int, const void *);\n");

  fprintf (out, "#ifdef __cplusplus\n"
	   "}\n"
	   "#endif\n");

  fprintf (out, "extern const void *const __OFFLOAD_TABLE__[];\n\n");

  /* TODO: Define proper GOMP_DEVICE_METAL constant.  */
  fprintf (out, "static __attribute__((constructor)) void init (void)\n"
	   "{\n"
	   "  GOMP_offload_register_ver (%#x, __OFFLOAD_TABLE__,"
	   " %d/*METAL*/, &metal_data);\n"
	   "};\n",
	   GOMP_VERSION_PACK (GOMP_VERSION, GOMP_VERSION_NVIDIA_PTX),
	   GOMP_DEVICE_NVIDIA_PTX);

  fprintf (out, "static __attribute__((destructor)) void fini (void)\n"
	   "{\n"
	   "  GOMP_offload_unregister_ver (%#x, __OFFLOAD_TABLE__,"
	   " %d/*METAL*/, &metal_data);\n"
	   "};\n",
	   GOMP_VERSION_PACK (GOMP_VERSION, GOMP_VERSION_NVIDIA_PTX),
	   GOMP_DEVICE_NVIDIA_PTX);
}

/* Compile MSL to metallib using Metal toolchain.  */

static void
compile_metal (const char *msl_file, const char *metallib_file)
{
  const char *air_file = air_name;
  if (!air_file)
    air_file = make_temp_file (".air");

  /* Compile .metal to .air.  */
  struct obstack argv_obstack;
  obstack_init (&argv_obstack);
  obstack_ptr_grow (&argv_obstack, "xcrun");
  obstack_ptr_grow (&argv_obstack, "-sdk");
  obstack_ptr_grow (&argv_obstack, "macosx");
  obstack_ptr_grow (&argv_obstack, "metal");
  obstack_ptr_grow (&argv_obstack, "-c");
  obstack_ptr_grow (&argv_obstack, msl_file);
  obstack_ptr_grow (&argv_obstack, "-o");
  obstack_ptr_grow (&argv_obstack, air_file);
  if (verbose)
    obstack_ptr_grow (&argv_obstack, "-v");
  obstack_ptr_grow (&argv_obstack, NULL);
  const char **metal_argv = XOBFINISH (&argv_obstack, const char **);

  if (verbose)
    {
      fprintf (stderr, "Compiling MSL to AIR:\n");
      for (const char **p = metal_argv; *p; p++)
	fprintf (stderr, " %s", *p);
      fprintf (stderr, "\n");
    }

  fork_execute (metal_argv[0], CONST_CAST (char **, metal_argv), false, NULL);
  obstack_free (&argv_obstack, NULL);

  /* Link .air to .metallib.  */
  obstack_init (&argv_obstack);
  obstack_ptr_grow (&argv_obstack, "xcrun");
  obstack_ptr_grow (&argv_obstack, "-sdk");
  obstack_ptr_grow (&argv_obstack, "macosx");
  obstack_ptr_grow (&argv_obstack, "metallib");
  obstack_ptr_grow (&argv_obstack, air_file);
  obstack_ptr_grow (&argv_obstack, "-o");
  obstack_ptr_grow (&argv_obstack, metallib_file);
  if (verbose)
    obstack_ptr_grow (&argv_obstack, "-v");
  obstack_ptr_grow (&argv_obstack, NULL);
  const char **metallib_argv = XOBFINISH (&argv_obstack, const char **);

  if (verbose)
    {
      fprintf (stderr, "Linking AIR to metallib:\n");
      for (const char **p = metallib_argv; *p; p++)
	fprintf (stderr, " %s", *p);
      fprintf (stderr, "\n");
    }

  fork_execute (metallib_argv[0], CONST_CAST (char **, metallib_argv),
		false, NULL);
  obstack_free (&argv_obstack, NULL);

  if (!save_temps)
    maybe_unlink (air_file);
}

static void
compile_native (const char *infile, const char *outfile, const char *compiler,
		bool fPIC, bool fpic)
{
  const char *collect_gcc_options = getenv ("COLLECT_GCC_OPTIONS");
  if (!collect_gcc_options)
    fatal_error (input_location,
		 "environment variable COLLECT_GCC_OPTIONS must be set");

  struct obstack argv_obstack;
  obstack_init (&argv_obstack);
  obstack_ptr_grow (&argv_obstack, compiler);
  if (fPIC)
    obstack_ptr_grow (&argv_obstack, "-fPIC");
  if (fpic)
    obstack_ptr_grow (&argv_obstack, "-fpic");
  if (save_temps)
    obstack_ptr_grow (&argv_obstack, "-save-temps");
  if (verbose)
    obstack_ptr_grow (&argv_obstack, "-v");
  obstack_ptr_grow (&argv_obstack, "-c");
  obstack_ptr_grow (&argv_obstack, infile);
  obstack_ptr_grow (&argv_obstack, "-o");
  obstack_ptr_grow (&argv_obstack, outfile);
  obstack_ptr_grow (&argv_obstack, NULL);
  const char **new_argv = XOBFINISH (&argv_obstack, const char **);

  fork_execute (new_argv[0], CONST_CAST (char **, new_argv), false, NULL);
  obstack_free (&argv_obstack, NULL);
}

int
main (int argc, char **argv)
{
  FILE *in = NULL;
  FILE *out = NULL;
  const char *outname = NULL;
  const char *collect_gcc = getenv ("COLLECT_GCC");
  if (!collect_gcc)
    fatal_error (input_location,
		 "environment variable COLLECT_GCC must be set");

  progname = tool_name;
  gcc_init_libintl ();
  diagnostic_initialize (global_dc, 0);
  diagnostic_color_init (global_dc);

  if (atexit (mkoffload_cleanup) != 0)
    fatal_error (input_location, "atexit failed");

  const char *driver = getenv ("OFFLOAD_TARGET_DEFAULT");
  if (!driver)
    driver = "metal-apple-darwin";

  /* We may be called with all the arguments stored in some file and
     passed with @file.  Expand them into argv before processing.  */
  expandargv (&argc, &argv);

  /* Scan the argument vector.  */
  bool fopenmp = false;
  bool fopenacc = false;
  bool fPIC = false;
  bool fpic = false;
  const char *dumppfx = NULL;
  for (int i = 1; i < argc; i++)
    {
#define STR "-foffload-abi="
      if (startswith (argv[i], STR))
	{
	  if (strcmp (argv[i] + strlen (STR), "lp64") == 0)
	    offload_abi = OFFLOAD_ABI_LP64;
	  else if (strcmp (argv[i] + strlen (STR), "ilp32") == 0)
	    offload_abi = OFFLOAD_ABI_ILP32;
	  else
	    fatal_error (input_location,
			 "unrecognizable argument of option " STR);
	}
#undef STR
      else if (startswith (argv[i], "-foffload-abi-host-opts="))
	{
	  if (offload_abi_host_opts)
	    fatal_error (input_location,
			 "%<-foffload-abi-host-opts%> specified "
			 "multiple times");
	  offload_abi_host_opts
	    = argv[i] + strlen ("-foffload-abi-host-opts=");
	}
      else if (strcmp (argv[i], "-fopenmp") == 0)
	fopenmp = true;
      else if (strcmp (argv[i], "-fopenacc") == 0)
	fopenacc = true;
      else if (strcmp (argv[i], "-fPIC") == 0)
	fPIC = true;
      else if (strcmp (argv[i], "-fpic") == 0)
	fpic = true;
      else if (strcmp (argv[i], "-save-temps") == 0)
	save_temps = true;
      else if (strcmp (argv[i], "-v") == 0)
	verbose = true;
      else if (strcmp (argv[i], "-dumpbase") == 0
	       && i + 1 < argc)
	dumppfx = argv[++i];
    }

  if (!(fopenacc ^ fopenmp))
    fatal_error (input_location, "either %<-fopenacc%> or %<-fopenmp%> "
		 "must be set");

  struct obstack argv_obstack;
  obstack_init (&argv_obstack);
  obstack_ptr_grow (&argv_obstack, driver);
  if (save_temps)
    obstack_ptr_grow (&argv_obstack, "-save-temps");
  if (verbose)
    obstack_ptr_grow (&argv_obstack, "-v");
  obstack_ptr_grow (&argv_obstack, "-xlto");
  switch (offload_abi)
    {
    case OFFLOAD_ABI_LP64:
      obstack_ptr_grow (&argv_obstack, "-m64");
      break;
    case OFFLOAD_ABI_ILP32:
      obstack_ptr_grow (&argv_obstack, "-m32");
      break;
    default:
      gcc_unreachable ();
    }
  if (fopenmp)
    obstack_ptr_grow (&argv_obstack, "-mgomp");

  for (int ix = 1; ix != argc; ix++)
    {
      if (!strcmp (argv[ix], "-o") && ix + 1 != argc)
	outname = argv[++ix];
      else
	obstack_ptr_grow (&argv_obstack, argv[ix]);
    }

  if (!dumppfx)
    dumppfx = outname;

  msl_dumpbase = concat (dumppfx, ".c", NULL);
  if (save_temps)
    msl_cfile_name = msl_dumpbase;
  else
    msl_cfile_name = make_temp_file (".c");

  out = fopen (msl_cfile_name, "w");
  if (!out)
    fatal_error (input_location, "cannot open '%s'", msl_cfile_name);

  /* PR libgomp/65099: Currently, we only support offloading in 64-bit
     configurations.  */
  if (offload_abi == OFFLOAD_ABI_LP64)
    {
      char *mko_dumpbase = concat (dumppfx, ".mkoffload", NULL);
      if (save_temps)
	msl_name = mko_dumpbase;
      else
	msl_name = make_temp_file (".mkoffload");
      obstack_ptr_grow (&argv_obstack, "-dumpdir");
      obstack_ptr_grow (&argv_obstack, "");
      obstack_ptr_grow (&argv_obstack, "-dumpbase");
      obstack_ptr_grow (&argv_obstack, mko_dumpbase);
      obstack_ptr_grow (&argv_obstack, "-dumpbase-ext");
      obstack_ptr_grow (&argv_obstack, "");
      obstack_ptr_grow (&argv_obstack, "-o");
      obstack_ptr_grow (&argv_obstack, msl_name);
      obstack_ptr_grow (&argv_obstack, NULL);
      const char **new_argv = XOBFINISH (&argv_obstack, const char **);

      char *execpath = getenv ("GCC_EXEC_PREFIX");
      char *cpath = getenv ("COMPILER_PATH");
      char *lpath = getenv ("LIBRARY_PATH");
      unsetenv ("GCC_EXEC_PREFIX");
      unsetenv ("COMPILER_PATH");
      unsetenv ("LIBRARY_PATH");

      if (save_temps)
	omp_requires_file = concat (dumppfx, ".mkoffload.omp_requires", NULL);
      else
	omp_requires_file = make_temp_file (".mkoffload.omp_requires");

      xputenv (concat ("GCC_OFFLOAD_OMP_REQUIRES_FILE=", omp_requires_file, NULL));
      fork_execute (new_argv[0], CONST_CAST (char **, new_argv), true,
		    ".gcc_args");
      obstack_free (&argv_obstack, NULL);
      unsetenv ("GCC_OFFLOAD_OMP_REQUIRES_FILE");

      xputenv (concat ("GCC_EXEC_PREFIX=", execpath, NULL));
      xputenv (concat ("COMPILER_PATH=", cpath, NULL));
      xputenv (concat ("LIBRARY_PATH=", lpath, NULL));

      in = fopen (omp_requires_file, "rb");
      if (!in)
	fatal_error (input_location, "cannot open omp_requires file %qs",
		     omp_requires_file);
      uint32_t omp_requires;
      if (fread (&omp_requires, sizeof (omp_requires), 1, in) != 1)
	fatal_error (input_location, "cannot read omp_requires file %qs",
		     omp_requires_file);
      fclose (in);

      in = fopen (msl_name, "r");
      if (!in)
	fatal_error (input_location, "cannot open intermediate file");

      process (in, out, omp_requires);
      fclose (in);
    }

  fclose (out);

  compile_native (msl_cfile_name, outname, collect_gcc, fPIC, fpic);

  return 0;
}
