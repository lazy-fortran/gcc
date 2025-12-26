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

/* Offload image generation tool for Apple Metal.

   Processes LTO object files to generate Metal Shading Language code,
   compiles it to metallib binary format, and packages it into a C source
   file with offload tables for runtime registration.

   Pipeline:
   1. Extract LTO sections via driver with -xlto
   2. Driver invokes lto1 to generate MSL from GIMPLE
   3. Extract mapping directives from MSL
   4. Compile MSL to .air with xcrun metal
   5. Link .air to .metallib with xcrun metallib
   6. Generate C source with embedded metallib and symbol tables
   7. Compile C source to host object file  */

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

/* Free id_map linked list.  */
static void
free_id_maps (id_map *map)
{
  while (map)
    {
      id_map *next = map->next;
      free (map->msl_name);
      free (map->dim);
      free (map);
      map = next;
    }
}

/* Files to unlink.  */
static const char *msl_name;
static const char *msl_cfile_name;
static const char *metallib_name;
static const char *air_name;
static const char *omp_requires_file;
static const char *msl_dumpbase;

enum offload_abi offload_abi = OFFLOAD_ABI_UNSET;
const char *offload_abi_host_opts = NULL;

/* Check whether NAME can be accessed in MODE.  This is like access,
   except that it never considers directories to be executable.  */

static int
access_check (const char *name, int mode)
{
  if (mode == X_OK)
    {
      struct stat st;

      if (stat (name, &st) < 0
	  || S_ISDIR (st.st_mode))
	return -1;
    }

  return access (name, mode);
}

/* Parse STR, saving found tokens into PVALUES and return their number.
   Tokens are assumed to be delimited by ':'.  */

static unsigned
parse_env_var (const char *str, char ***pvalues)
{
  const char *curval, *nextval;
  char **values;
  unsigned num = 1, i;

  if (str == NULL || *str == '\0')
    {
      *pvalues = NULL;
      return 0;
    }

  curval = strchr (str, ':');
  while (curval)
    {
      num++;
      curval = strchr (curval + 1, ':');
    }

  values = (char **) xmalloc (num * sizeof (char *));
  curval = str;
  nextval = strchr (curval, ':');
  if (nextval == NULL)
    nextval = strchr (curval, '\0');

  for (i = 0; i < num; i++)
    {
      int l = nextval - curval;
      values[i] = (char *) xmalloc (l + 1);
      memcpy (values[i], curval, l);
      values[i][l] = 0;
      curval = nextval + 1;
      nextval = strchr (curval, ':');
      if (nextval == NULL)
	nextval = strchr (curval, '\0');
    }
  *pvalues = values;
  return num;
}

/* Auxiliary function that frees elements of PTR and PTR itself.
   N is number of elements to be freed.  If PTR is NULL, nothing is freed.
   If an element is NULL, subsequent elements are not freed.  */

static void
free_array_of_ptrs (void **ptr, unsigned n)
{
  if (ptr == NULL)
    return;
  for (unsigned i = 0; i < n; i++)
    {
      if (ptr[i] == NULL)
	break;
      free (ptr[i]);
    }
  free (ptr);
}

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

/* LTO bytecode extraction is handled by the driver when invoked with -xlto.
   The driver reads LTO sections from input object files and invokes lto1
   internally to generate MSL output.  */
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

/* Forward declaration.  */
static void compile_metal (const char *, const char *);

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

/* Process input file and generate metallib binary output.  */

static void
process (FILE *in, FILE *out, uint32_t omp_requires)
{
  size_t len = 0;
  const char *input = read_file (in, &len);
  const char *comma;
  id_map const *id;

  /* First pass: extract mapping directives and save MSL to temporary file.  */
  const char *msl_source_file;
  if (save_temps)
    msl_source_file = concat (msl_dumpbase, ".metal", NULL);
  else
    msl_source_file = make_temp_file (".metal");

  FILE *msl_out = fopen (msl_source_file, "w");
  if (!msl_out)
    fatal_error (input_location, "cannot open '%s'", msl_source_file);

  /* Parse input and extract mapping directives.  */
  for (size_t i = 0; i < len; i++)
    {
      /* Check for mapping directives at start of line.  */
      bool is_directive = false;
      if (startswith (input + i, "//VAR_MAP "))
	{
	  record_id (input + i + 10, &vars_tail);
	  is_directive = true;
	}
      else if (startswith (input + i, "//FUNC_MAP "))
	{
	  record_id (input + i + 11, &funcs_tail);
	  is_directive = true;
	}
      else if (startswith (input + i, "//IND_FUNC_MAP "))
	{
	  record_id (input + i + 15, &funcs_tail);
	  is_directive = true;
	}

      if (is_directive)
	{
	  /* Skip the entire directive line.  */
	  while (i < len && input[i] != '\n')
	    i++;
	}
      else
	{
	  /* Write character to MSL file.  */
	  fputc (input[i], msl_out);
	}
    }

  fclose (msl_out);

  /* Compile MSL to metallib.  */
  if (!metallib_name)
    {
      if (save_temps)
	metallib_name = concat (msl_dumpbase, ".metallib", NULL);
      else
	metallib_name = make_temp_file (".metallib");
    }

  compile_metal (msl_source_file, metallib_name);

  if (!save_temps)
    maybe_unlink (msl_source_file);

  fprintf (out, "#include <stdint.h>\n\n");

  /* Embed the metallib binary using #embed directive.  */
  fprintf (out, "static const unsigned char metal_code[] = {\n"
	   "#embed \"%s\" if_empty (error_metallib_is_empty)\n"
	   "};\n\n", metallib_name);

  /* Create metal_obj array pointing to the embedded binary.
     Must match struct definition in libgomp/plugin/plugin-metal.cpp.  */
  fprintf (out, "static const struct metal_obj {\n"
	   "  const void *data;\n"
	   "  __SIZE_TYPE__ size;\n"
	   "} metal_objs[] = {\n"
	   "  {metal_code, sizeof (metal_code)}\n"
	   "};\n\n");

  /* Dump out variable idents.  */
  fprintf (out, "static const char *const var_mappings[] = {");
  if (!var_ids)
    fprintf (out, "\n\tNULL");
  else
    for (comma = "", id = var_ids; id; comma = ",", id = id->next)
      fprintf (out, "%s\n\t\"%s\"", comma, id->msl_name);
  fprintf (out, "\n};\n\n");

  /* Dump out function idents.
     Must match struct definition in libgomp/plugin/plugin-metal.cpp.  */
  fprintf (out, "static const struct metal_fn_launch {\n"
	   "  const char *fn;\n"
	   "  unsigned short dim[%d];\n"
	   "} func_mappings[] = {\n", GOMP_DIM_MAX);
  if (!func_ids)
    fprintf (out, "\n\t{NULL, {0, 0, 0}}");
  else
    for (comma = "", id = func_ids; id; comma = ",", id = id->next)
      fprintf (out, "%s\n\t{\"%s\"%s}", comma, id->msl_name,
	       id->dim ? id->dim : ", {0, 0, 0}");
  fprintf (out, "\n};\n\n");

  /* Offload image header structure passed to runtime plugin.
     Must match struct definition in libgomp/plugin/plugin-metal.cpp.
     This structure is passed to GOMP_OFFLOAD_load_image as target_data.  */
  fprintf (out,
	   "static const struct metal_tdata {\n"
	   "  uintptr_t omp_requires_mask;\n"
	   "  const struct metal_obj *metal_objs;\n"
	   "  unsigned metal_num;\n"
	   "  const char *const *var_names;\n"
	   "  unsigned var_num;\n"
	   "  const struct metal_fn_launch *fn_descs;\n"
	   "  unsigned fn_num;\n"
	   "  unsigned ind_fn_num;\n"
	   "} metal_data = {\n"
	   "  %d,\n"
	   "  metal_objs, sizeof (metal_objs) / sizeof (metal_objs[0]),\n"
	   "  var_mappings, sizeof (var_mappings) / sizeof (var_mappings[0]),\n"
	   "  func_mappings, sizeof (func_mappings) / sizeof (func_mappings[0]),\n"
	   "  0\n"
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

  /* Free input buffer allocated by read_file.  */
  free (CONST_CAST (char *, input));
}

/* Compile MSL to metallib using Metal toolchain.  */

static void
compile_metal (const char *msl_file, const char *metallib_file)
{
  if (!air_name)
    air_name = make_temp_file (".air");

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
  obstack_ptr_grow (&argv_obstack, air_name);
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
  obstack_ptr_grow (&argv_obstack, air_name);
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
    maybe_unlink (air_name);
}

static void
compile_native (const char *infile, const char *outfile, const char *compiler,
		bool fPIC, bool fpic)
{
  const char *collect_gcc_options = getenv ("COLLECT_GCC_OPTIONS");
  if (!collect_gcc_options)
    fatal_error (input_location,
		 "environment variable COLLECT_GCC_OPTIONS must be set");

  /* If we're using an in-tree driver (typically xgcc), add -B<dir> so it can
     locate its internal executables (lto1/cc1/...).  */
  const char *compiler_dir = dirname (ASTRDUP (compiler));
  bool add_b = false;
  if (compiler_dir && *compiler_dir)
    {
      char *lto1 = concat (compiler_dir, "/lto1", NULL);
      add_b = (access_check (lto1, X_OK) == 0);
      free (lto1);
    }

  struct obstack argv_obstack;
  obstack_init (&argv_obstack);
  obstack_ptr_grow (&argv_obstack, compiler);
  if (add_b)
    {
      obstack_ptr_grow (&argv_obstack, "-B");
      obstack_ptr_grow (&argv_obstack, compiler_dir);
    }
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
    {
      /* Find the offload compiler similarly to nvptx/gcn mkoffload: prefer a
	 compiler adjacent to COLLECT_GCC, then search COMPILER_PATH.  This
	 avoids relying on PATH and makes in-tree builds usable via -B.  */
      const char *gcc_exec = basename (ASTRDUP (collect_gcc));
      const char *gcc_path = dirname (ASTRDUP (collect_gcc));

      if (strcmp (gcc_exec, collect_gcc) == 0)
	/* collect_gcc has no path, so it was found in PATH.  */
	gcc_path = NULL;

      bool found = false;
      char *driver_buf = NULL;

      if (gcc_path != NULL)
	{
	  size_t len = strlen (gcc_path) + 1 + strlen (GCC_INSTALL_NAME) + 1;
	  driver_buf = XNEWVEC (char, len);
	  sprintf (driver_buf, "%s/%s", gcc_path, GCC_INSTALL_NAME);
	  if (access_check (driver_buf, X_OK) == 0)
	    found = true;
	  else if (access_check (collect_gcc, X_OK) == 0)
	    {
	      /* In build-tree builds, COLLECT_GCC is usually "xgcc", not
		 GCC_INSTALL_NAME.  Fall back to COLLECT_GCC itself so that
		 mkoffload can run without requiring an installed driver or
		 manual OFFLOAD_TARGET_DEFAULT setup.  */
	      driver = collect_gcc;
	      found = true;
	      XDELETEVEC (driver_buf);
	      driver_buf = NULL;
	    }
	}
      else
	found = true;

      if (!found)
	{
	  /* Look in all COMPILER_PATHs for GCC_INSTALL_NAME.  */
	  char **paths = NULL;
	  unsigned n_paths = parse_env_var (getenv ("COMPILER_PATH"), &paths);
	  for (unsigned i = 0; i < n_paths; i++)
	    {
	      size_t len = strlen (paths[i]) + 1 + strlen (GCC_INSTALL_NAME) + 1;
	      driver_buf = XRESIZEVEC (char, driver_buf, len);
	      sprintf (driver_buf, "%s/%s", paths[i], GCC_INSTALL_NAME);
	      if (access_check (driver_buf, X_OK) == 0)
		{
		  found = true;
		  break;
		}
	    }
	  free_array_of_ptrs ((void **) paths, n_paths);
	}

      if (!found)
	fatal_error (input_location,
		     "offload compiler %s not found (consider using %<-B%> "
		     "or setting %<OFFLOAD_TARGET_DEFAULT%>)",
		     GCC_INSTALL_NAME);

      if (driver)
	;
      else if (gcc_path == NULL)
	driver = GCC_INSTALL_NAME;
      else
	driver = driver_buf;
    }

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

  if (!outname)
    fatal_error (input_location, "no input files specified");

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

      /* mkoffload runs the host driver to produce the intermediate MSL.
	 For in-tree builds (COLLECT_GCC is typically "xgcc"), make sure the
	 driver can find its internal executables (lto1/cc1/...) even after
	 we scrub environment variables below.  */
      const char *collect_gcc_dir = dirname (ASTRDUP (collect_gcc));
      bool add_b = false;
      if (collect_gcc_dir && *collect_gcc_dir)
	{
	  char *lto1 = concat (collect_gcc_dir, "/lto1", NULL);
	  add_b = (access_check (lto1, X_OK) == 0);
	  free (lto1);
	}
      char **exec_argv = CONST_CAST (char **, new_argv);
      if (add_b)
	{
	  unsigned n = 0;
	  while (new_argv[n])
	    n++;
	  exec_argv = XNEWVEC (char *, n + 3);
	  exec_argv[0] = CONST_CAST (char *, new_argv[0]);
	  exec_argv[1] = CONST_CAST (char *, "-B");
	  exec_argv[2] = CONST_CAST (char *, collect_gcc_dir);
	  for (unsigned i = 1; i <= n; i++)
	    exec_argv[i + 2] = CONST_CAST (char *, new_argv[i]);
	}

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
      fork_execute (exec_argv[0], exec_argv, true,
		    ".gcc_args");
      obstack_free (&argv_obstack, NULL);
      unsetenv ("GCC_OFFLOAD_OMP_REQUIRES_FILE");
      if (add_b)
	XDELETEVEC (exec_argv);

      if (execpath)
	xputenv (concat ("GCC_EXEC_PREFIX=", execpath, NULL));
      if (cpath)
	xputenv (concat ("COMPILER_PATH=", cpath, NULL));
      if (lpath)
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
	fatal_error (input_location, "cannot open intermediate msl file");

      process (in, out, omp_requires);
      fclose (in);
    }

  fclose (out);

  compile_native (msl_cfile_name, outname, collect_gcc, fPIC, fpic);

  free_id_maps (func_ids);
  free_id_maps (var_ids);

  return 0;
}
