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

   This tool generates MSL kernel skeletons and packages them for runtime
   compilation using MTLDevice::newLibraryWithSource.  */

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

static const char *msl_name;
static const char *msl_cfile_name;
static const char *omp_requires_file;
static const char *msl_dumpbase;
static const char *msl_version = "3.0";

enum offload_abi offload_abi = OFFLOAD_ABI_UNSET;
const char *offload_abi_host_opts = NULL;

void
tool_cleanup (bool from_signal ATTRIBUTE_UNUSED)
{
  if (msl_cfile_name)
    maybe_unlink (msl_cfile_name);
  if (msl_name)
    maybe_unlink (msl_name);
  if (omp_requires_file)
    maybe_unlink (omp_requires_file);
}

static void
mkoffload_cleanup (void)
{
  tool_cleanup (false);
}

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

static const char *
read_file (FILE *stream, size_t *plen)
{
  size_t alloc = 16384;
  size_t base = 0;
  char *buffer;

  if (!fseek (stream, 0, SEEK_END))
    {
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

static const char *
msl_type_mapping (const char *ctype)
{
  if (strcmp (ctype, "float") == 0)
    return "float";
  if (strcmp (ctype, "double") == 0)
    return "double";
  if (strcmp (ctype, "int") == 0)
    return "int";
  if (strcmp (ctype, "unsigned int") == 0)
    return "uint";
  if (strcmp (ctype, "long") == 0)
    return "long";
  if (strcmp (ctype, "unsigned long") == 0)
    return "ulong";
  if (strcmp (ctype, "char") == 0)
    return "char";
  if (strcmp (ctype, "unsigned char") == 0)
    return "uchar";
  if (strcmp (ctype, "short") == 0)
    return "short";
  if (strcmp (ctype, "unsigned short") == 0)
    return "ushort";

  return ctype;
}

static void
emit_msl_header (FILE *out)
{
  fprintf (out, "#include <metal_stdlib>\n");
  fprintf (out, "using namespace metal;\n\n");
}

static void
emit_kernel_signature (FILE *out, const char *kernel_name,
		       const char *dim_spec)
{
  fprintf (out, "kernel void %s(", kernel_name);

  fprintf (out, "device float* data [[buffer(0)]],\n");
  fprintf (out, "                      ");
  fprintf (out, "uint tid [[thread_position_in_grid]])");

  fprintf (out, ") {\n");
  fprintf (out, "    // Kernel implementation placeholder\n");
  fprintf (out, "}\n\n");
}

static void
process (FILE *in, FILE *out, uint32_t omp_requires)
{
  size_t len = 0;
  const char *input = read_file (in, &len);
  const char *comma;
  id_map const *id;
  unsigned obj_count = 0;

  fprintf (out, "#include <stdint.h>\n\n");

  fprintf (out, "static const char msl_code_%u[] =\n", obj_count++);
  fprintf (out, "\t\"#include <metal_stdlib>\\n\"\n");
  fprintf (out, "\t\"using namespace metal;\\n\"\n");
  fprintf (out, "\t\"\\n\"\n");

  fprintf (out, "\t\"kernel void example_kernel(\"\n");
  fprintf (out, "\t\"    device float* data [[buffer(0)]],\\n\"\n");
  fprintf (out, "\t\"    uint tid [[thread_position_in_grid]]) {\\n\"\n");
  fprintf (out, "\t\"    data[tid] = data[tid] * 2.0f;\\n\"\n");
  fprintf (out, "\t\"}\\n\";\n\n");

  for (size_t i = 0; i != len;)
    {
      char c;
      while ((c = input[i++]))
	{
	  if (c == '\n')
	    {
	      if (UNLIKELY (startswith (input + i, "//: MSL_VERSION ")))
		{
		  msl_version = input + i + strlen ("//: MSL_VERSION ");
		  continue;
		}
	      while (startswith (input + i, "//:"))
		{
		  i += 3;

		  if (startswith (input + i, "VAR_MAP "))
		    record_id (input + i + 8, &vars_tail);
		  else if (startswith (input + i, "FUNC_MAP "))
		    record_id (input + i + 9, &funcs_tail);
		  else
		    abort ();
		  while (input[i++] != '\n')
		    continue;
		}
	    }
	}
      break;
    }

  fprintf (out, "static const struct msl_obj {\n"
	   "  const char *code;\n"
	   "  __SIZE_TYPE__ size;\n"
	   "} msl_objs[] = {");
  for (comma = "", unsigned ix = 0; ix != obj_count; comma = ",", ix++)
    fprintf (out, "%s\n\t{msl_code_%u, sizeof (msl_code_%u)}", comma, ix, ix);
  fprintf (out, "\n};\n\n");

  fprintf (out, "static const char *const var_mappings[] = {");
  for (comma = "", id = var_ids; id; comma = ",", id = id->next)
    fprintf (out, "%s\n\t\"%s\"", comma, id->msl_name);
  fprintf (out, "\n};\n\n");

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

  fprintf (out, "static __attribute__((constructor)) void init (void)\n"
	   "{\n"
	   "  GOMP_offload_register_ver (%#x, __OFFLOAD_TABLE__,"
	   " %d/*METAL*/, &metal_data);\n"
	   "};\n",
	   GOMP_VERSION_PACK (GOMP_VERSION, GOMP_VERSION_METAL),
	   GOMP_DEVICE_METAL);

  fprintf (out, "static __attribute__((destructor)) void fini (void)\n"
	   "{\n"
	   "  GOMP_offload_unregister_ver (%#x, __OFFLOAD_TABLE__,"
	   " %d/*METAL*/, &metal_data);\n"
	   "};\n",
	   GOMP_VERSION_PACK (GOMP_VERSION, GOMP_VERSION_METAL),
	   GOMP_DEVICE_METAL);
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
  obstack_ptr_grow (&argv_obstack, "-dumpdir");
  obstack_ptr_grow (&argv_obstack, "");
  obstack_ptr_grow (&argv_obstack, "-dumpbase");
  obstack_ptr_grow (&argv_obstack, msl_dumpbase);
  obstack_ptr_grow (&argv_obstack, "-dumpbase-ext");
  obstack_ptr_grow (&argv_obstack, ".c");
  if (!offload_abi_host_opts)
    fatal_error (input_location,
		 "%<-foffload-abi-host-opts%> not specified.");
  obstack_ptr_grow (&argv_obstack, offload_abi_host_opts);
  obstack_ptr_grow (&argv_obstack, infile);
  obstack_ptr_grow (&argv_obstack, "-c");
  obstack_ptr_grow (&argv_obstack, "-o");
  obstack_ptr_grow (&argv_obstack, outfile);
  obstack_ptr_grow (&argv_obstack, NULL);

  const char **new_argv = XOBFINISH (&argv_obstack, const char **);
  fork_execute (new_argv[0], CONST_CAST (char **, new_argv), true,
		".gccnative_args");
  obstack_free (&argv_obstack, NULL);
}

int
main (int argc, char **argv)
{
  FILE *in = stdin;
  FILE *out = stdout;
  const char *outname = 0;

  progname = tool_name;
  gcc_init_libintl ();
  diagnostic_initialize (global_dc, 0);
  diagnostic_color_init (global_dc);

  if (atexit (mkoffload_cleanup) != 0)
    fatal_error (input_location, "atexit failed");

  char *collect_gcc = getenv ("COLLECT_GCC");
  if (collect_gcc == NULL)
    fatal_error (input_location, "COLLECT_GCC must be set");

  expandargv (&argc, &argv);

  bool fopenmp = false;
  bool fopenacc = false;
  uint32_t omp_requires = 0;
  bool fPIC = false;
  bool fpic = false;

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
      else if (strcmp (argv[i], "-save-temps") == 0)
	save_temps = true;
      else if (strcmp (argv[i], "-v") == 0)
	verbose = true;
      else if (strcmp (argv[i], "-fPIC") == 0)
	fPIC = true;
      else if (strcmp (argv[i], "-fpic") == 0)
	fpic = true;
      else if (strcmp (argv[i], "-o") == 0 && i + 1 < argc)
	outname = argv[++i];
      else if (startswith (argv[i], "-l")
	       || startswith (argv[i], "-Wl,")
	       || startswith (argv[i], "-msl-version="))
	{
	  if (startswith (argv[i], "-msl-version="))
	    msl_version = argv[i] + strlen ("-msl-version=");
	  continue;
	}
      else if (strcmp (argv[i], "-dumpbase") == 0 && i + 1 < argc)
	msl_dumpbase = argv[++i];
      else if (startswith (argv[i], "-dumpbase-ext="))
	continue;
      else if (startswith (argv[i], "-dumpdir="))
	continue;
    }

  if (!(fopenacc ^ fopenmp))
    fatal_error (input_location, "either %<-fopenacc%> or %<-fopenmp%> "
		 "must be set");

  struct obstack argv_obstack;
  obstack_init (&argv_obstack);

  if (offload_abi == OFFLOAD_ABI_UNSET)
    fatal_error (input_location, "offload ABI not set");

  const char *input_file = NULL;
  for (int i = 1; i < argc; i++)
    {
      if (!startswith (argv[i], "-") && !input_file)
	{
	  input_file = argv[i];
	  break;
	}
    }

  if (input_file)
    {
      in = fopen (input_file, "r");
      if (!in)
	fatal_error (input_location, "cannot open input file %qs",
		     input_file);
    }

  char *msl_cfile = NULL;
  int fd = -1;

  if (!outname)
    {
      char *tmp = getenv ("TMPDIR");
      if (!tmp)
	tmp = (char *) "/tmp";
      msl_cfile = concat (tmp, "/ccXXXXXX.c", NULL);
      fd = mkstemps (msl_cfile, 2);
      if (fd == -1)
	fatal_error (input_location, "cannot create temporary file");
      msl_cfile_name = msl_cfile;
      out = fdopen (fd, "w");
      if (!out)
	fatal_error (input_location, "cannot open temporary file");
    }

  if (!omp_requires_file)
    omp_requires = 0;
  else
    {
      FILE *f = fopen (omp_requires_file, "rb");
      if (!f)
	fatal_error (input_location, "cannot open omp_requires file %qs",
		     omp_requires_file);
      if (fread (&omp_requires, sizeof (omp_requires), 1, f) != 1)
	fatal_error (input_location, "cannot read omp_requires file %qs",
		     omp_requires_file);
      fclose (f);
    }

  process (in, out, omp_requires);
  fclose (out);
  if (input_file)
    fclose (in);

  if (!outname)
    {
      char *objname = concat (msl_cfile, ".o", NULL);
      compile_native (msl_cfile, objname, collect_gcc, fPIC, fpic);

      obstack_ptr_grow (&argv_obstack, collect_gcc);
      obstack_ptr_grow (&argv_obstack, "-o");

      char *default_outname = concat ("a.out", NULL);
      obstack_ptr_grow (&argv_obstack, default_outname);
      obstack_ptr_grow (&argv_obstack, objname);
      obstack_ptr_grow (&argv_obstack, NULL);

      maybe_unlink (objname);
    }

  obstack_free (&argv_obstack, NULL);

  return 0;
}
