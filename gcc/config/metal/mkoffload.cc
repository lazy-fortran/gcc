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

enum offload_abi offload_abi = OFFLOAD_ABI_UNSET;
const char *offload_abi_host_opts = NULL;

/* Files to unlink.  */
static const char *metal_name;
static const char *air_name;
static const char *metallib_name;
static const char *metal_cfile_name;

/* Delete tempfiles.  */

void
tool_cleanup (bool from_signal ATTRIBUTE_UNUSED)
{
  if (metal_cfile_name)
    maybe_unlink (metal_cfile_name);
  if (metallib_name)
    maybe_unlink (metallib_name);
  if (air_name)
    maybe_unlink (air_name);
  if (metal_name)
    maybe_unlink (metal_name);
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

/* Compile Metal source (.metal) to Metal IR (.air) using xcrun metal.  */

static void
compile_metal_to_air (const char *metal_file, const char *air_file,
		      bool debug_info, int opt_level)
{
  struct obstack argv_obstack;
  obstack_init (&argv_obstack);

  obstack_ptr_grow (&argv_obstack, "xcrun");
  obstack_ptr_grow (&argv_obstack, "-sdk");
  obstack_ptr_grow (&argv_obstack, "macosx");
  obstack_ptr_grow (&argv_obstack, "metal");

  if (debug_info)
    obstack_ptr_grow (&argv_obstack, "-g");

  if (opt_level > 0)
    {
      char *opt_flag = XNEWVEC (char, 8);
      sprintf (opt_flag, "-O%d", opt_level);
      obstack_ptr_grow (&argv_obstack, opt_flag);
    }

  obstack_ptr_grow (&argv_obstack, "-c");
  obstack_ptr_grow (&argv_obstack, metal_file);
  obstack_ptr_grow (&argv_obstack, "-o");
  obstack_ptr_grow (&argv_obstack, air_file);
  obstack_ptr_grow (&argv_obstack, NULL);

  const char **argv = XOBFINISH (&argv_obstack, const char **);

  if (verbose)
    {
      fprintf (stderr, "Compiling Metal shader: ");
      for (const char **p = argv; *p; p++)
	fprintf (stderr, "%s ", *p);
      fprintf (stderr, "\n");
    }

  fork_execute (argv[0], CONST_CAST (char **, argv), false,
		".metal_args");
  obstack_free (&argv_obstack, NULL);
}

/* Link Metal IR (.air) to Metal library (.metallib) using xcrun metallib.  */

static void
link_air_to_metallib (const char *air_file, const char *metallib_file)
{
  struct obstack argv_obstack;
  obstack_init (&argv_obstack);

  obstack_ptr_grow (&argv_obstack, "xcrun");
  obstack_ptr_grow (&argv_obstack, "-sdk");
  obstack_ptr_grow (&argv_obstack, "macosx");
  obstack_ptr_grow (&argv_obstack, "metallib");
  obstack_ptr_grow (&argv_obstack, air_file);
  obstack_ptr_grow (&argv_obstack, "-o");
  obstack_ptr_grow (&argv_obstack, metallib_file);
  obstack_ptr_grow (&argv_obstack, NULL);

  const char **argv = XOBFINISH (&argv_obstack, const char **);

  if (verbose)
    {
      fprintf (stderr, "Linking Metal library: ");
      for (const char **p = argv; *p; p++)
	fprintf (stderr, "%s ", *p);
      fprintf (stderr, "\n");
    }

  fork_execute (argv[0], CONST_CAST (char **, argv), false,
		".metallib_args");
  obstack_free (&argv_obstack, NULL);
}

int
main (int argc, char **argv)
{
  progname = tool_name;
  gcc_init_libintl ();
  diagnostic_initialize (global_dc, 0);
  diagnostic_color_init (global_dc);

  if (atexit (mkoffload_cleanup) != 0)
    fatal_error (input_location, "atexit failed");

  /* We may be called with all the arguments stored in some file and
     passed with @file.  Expand them into argv before processing.  */
  expandargv (&argc, &argv);

  /* Scan the argument vector.  */
  bool fopenmp = false;
  bool fopenacc = false;
  bool debug_info = false;
  int opt_level = 0;
  const char *infile = NULL;
  const char *outname = NULL;

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
      else if (strcmp (argv[i], "-g") == 0)
	debug_info = true;
      else if (startswith (argv[i], "-O"))
	{
	  const char *opt_str = argv[i] + 2;
	  if (*opt_str >= '0' && *opt_str <= '3' && opt_str[1] == '\0')
	    opt_level = *opt_str - '0';
	  else if (strcmp (opt_str, "s") == 0 || strcmp (opt_str, "fast") == 0)
	    opt_level = 2;
	}
      else if (strcmp (argv[i], "-o") == 0 && i + 1 < argc)
	outname = argv[++i];
      else if (argv[i][0] != '-')
	infile = argv[i];
    }

  if (!(fopenacc ^ fopenmp))
    fatal_error (input_location, "either %<-fopenacc%> or %<-fopenmp%> "
		 "must be set");

  if (!infile)
    fatal_error (input_location, "no input file specified");

  if (!outname)
    fatal_error (input_location, "no output file specified");

  /* Generate temporary filenames for intermediate files.  */
  const char *dumppfx_var = getenv ("GCC_OFFLOAD_DUMP_PREFIX");
  const char *dumppfx = dumppfx_var ? dumppfx_var : "a";

  if (save_temps)
    {
      metal_name = concat (dumppfx, ".metal", NULL);
      air_name = concat (dumppfx, ".air", NULL);
      metallib_name = concat (dumppfx, ".metallib", NULL);
      metal_cfile_name = concat (dumppfx, ".mkoffload.c", NULL);
    }
  else
    {
      metal_name = make_temp_file (".metal");
      air_name = make_temp_file (".air");
      metallib_name = make_temp_file (".metallib");
      metal_cfile_name = make_temp_file (".mkoffload.c");
    }

  /* For now, assume the input file is already a .metal file.
     In the future, this would be generated by the Metal compiler backend.
     Copy input to metal_name if needed.  */
  if (strcmp (infile, metal_name) != 0)
    {
      FILE *in = fopen (infile, "rb");
      if (!in)
	fatal_error (input_location, "cannot open input file %qs", infile);

      FILE *out_metal = fopen (metal_name, "wb");
      if (!out_metal)
	fatal_error (input_location, "cannot create %qs", metal_name);

      char buffer[4096];
      size_t n;
      while ((n = fread (buffer, 1, sizeof (buffer), in)) > 0)
	{
	  if (fwrite (buffer, 1, n, out_metal) != n)
	    fatal_error (input_location, "writing to %qs failed", metal_name);
	}

      fclose (in);
      fclose (out_metal);
    }

  /* Step 1: Compile .metal to .air using xcrun metal.  */
  compile_metal_to_air (metal_name, air_name, debug_info, opt_level);

  /* Step 2: Link .air to .metallib using xcrun metallib.  */
  link_air_to_metallib (air_name, metallib_name);

  /* Step 3: Package .metallib into a C file.
     For now, create a simple wrapper that embeds the metallib as binary data.
     This will be expanded later to include proper registration functions.  */
  FILE *cfile = fopen (metal_cfile_name, "w");
  if (!cfile)
    fatal_error (input_location, "cannot create %qs", metal_cfile_name);

  /* Read the metallib file.  */
  FILE *metallib_file = fopen (metallib_name, "rb");
  if (!metallib_file)
    fatal_error (input_location, "cannot open %qs", metallib_name);

  fseek (metallib_file, 0, SEEK_END);
  long metallib_size = ftell (metallib_file);
  fseek (metallib_file, 0, SEEK_SET);

  unsigned char *metallib_data = XNEWVEC (unsigned char, metallib_size);
  if (fread (metallib_data, 1, metallib_size, metallib_file) != (size_t) metallib_size)
    fatal_error (input_location, "reading %qs failed", metallib_name);
  fclose (metallib_file);

  /* Write the C file with embedded metallib data.  */
  fprintf (cfile, "/* This file was generated by metal mkoffload.  */\n\n");
  fprintf (cfile, "#include <stddef.h>\n\n");
  fprintf (cfile, "static const unsigned char __metal_library_data[] = {\n  ");

  for (long i = 0; i < metallib_size; i++)
    {
      fprintf (cfile, "0x%02x", metallib_data[i]);
      if (i < metallib_size - 1)
	fprintf (cfile, ", ");
      if ((i + 1) % 12 == 0 && i < metallib_size - 1)
	fprintf (cfile, "\n  ");
    }

  fprintf (cfile, "\n};\n\n");
  fprintf (cfile, "const unsigned char *__metal_library = __metal_library_data;\n");
  fprintf (cfile, "const size_t __metal_library_size = %ld;\n", metallib_size);

  fclose (cfile);
  XDELETEVEC (metallib_data);

  /* Step 4: Compile the C file to the output object file.
     This uses the host compiler.  */
  const char *collect_gcc = getenv ("COLLECT_GCC");
  if (!collect_gcc)
    fatal_error (input_location, "COLLECT_GCC must be set");

  struct obstack cc_argv_obstack;
  obstack_init (&cc_argv_obstack);

  obstack_ptr_grow (&cc_argv_obstack, collect_gcc);
  obstack_ptr_grow (&cc_argv_obstack, "-c");
  obstack_ptr_grow (&cc_argv_obstack, metal_cfile_name);
  obstack_ptr_grow (&cc_argv_obstack, "-o");
  obstack_ptr_grow (&cc_argv_obstack, outname);

  if (save_temps)
    obstack_ptr_grow (&cc_argv_obstack, "-save-temps");
  if (verbose)
    obstack_ptr_grow (&cc_argv_obstack, "-v");

  obstack_ptr_grow (&cc_argv_obstack, NULL);

  const char **cc_argv = XOBFINISH (&cc_argv_obstack, const char **);

  if (verbose)
    {
      fprintf (stderr, "Compiling wrapper C file: ");
      for (const char **p = cc_argv; *p; p++)
	fprintf (stderr, "%s ", *p);
      fprintf (stderr, "\n");
    }

  fork_execute (cc_argv[0], CONST_CAST (char **, cc_argv), true,
		".gcc_args");
  obstack_free (&cc_argv_obstack, NULL);

  return 0;
}
