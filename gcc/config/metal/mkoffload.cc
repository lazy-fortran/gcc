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
static void rewrite_msl_control_flow (const char *);
static void fixup_msl_locals_and_addresses (const char *);
static void maybe_stub_undeclared_identifiers (const char *);

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

  fprintf (out, "#include <stdint.h>\n"
		"#include <stddef.h>\n\n");

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

/* Metal Shading Language forbids labels and goto.  The current Metal backend
   emits label/goto-style control flow, so rewrite such regions into a simple
   basic-block interpreter (switch inside an infinite loop).  */

static bool
msl_parse_label_line (const char *line, int *label)
{
  while (*line == ' ' || *line == '\t')
    line++;
  if (*line != 'L')
    return false;
  line++;
  if (*line < '0' || *line > '9')
    return false;
  int v = 0;
  while (*line >= '0' && *line <= '9')
    {
      v = v * 10 + (*line - '0');
      line++;
    }
  if (*line != ':')
    return false;
  line++;
  while (*line)
    {
      if (*line != ' ' && *line != '\t' && *line != '\r')
	return false;
      line++;
    }
  *label = v;
  return true;
}

static const char *
msl_ltrim (const char *line)
{
  while (*line == ' ' || *line == '\t')
    line++;
  return line;
}

static bool
msl_is_blank (const char *line)
{
  for (const char *p = line; *p; p++)
    if (*p != ' ' && *p != '\t' && *p != '\r')
      return false;
  return true;
}

static bool
msl_is_reg_decl (const char *line)
{
  const char *t = msl_ltrim (line);
  return strstr (t, "_reg") != NULL && strchr (t, ';') != NULL
    && strchr (t, '=') == NULL;
}

static bool
msl_parse_align_comment (const char *line, unsigned *align)
{
  const char *t = msl_ltrim (line);
  if (!startswith (t, COMMENT_PREFIX " .align "))
    return false;
  t += strlen (COMMENT_PREFIX " .align ");
  char *end = NULL;
  unsigned long v = strtoul (t, &end, 10);
  if (!end || end == t)
    return false;
  *align = (unsigned) v;
  return true;
}

static bool
msl_parse_constpool_label (const char *line, const char **out_name)
{
  const char *t = msl_ltrim (line);
  if (!startswith (t, "LC"))
    return false;
  t += 2;
  if (*t < '0' || *t > '9')
    return false;
  while (*t >= '0' && *t <= '9')
    t++;
  if (*t != ':' || t[1] != '\0')
    return false;
  *out_name = msl_ltrim (line);
  return true;
}

static bool
msl_parse_dot_long (const char *line, long *out_val)
{
  const char *t = msl_ltrim (line);
  if (!startswith (t, ".long"))
    return false;
  t += strlen (".long");
  while (*t == ' ' || *t == '\t')
    t++;
  char *end = NULL;
  long v = strtol (t, &end, 0);
  if (!end || end == t)
    return false;
  *out_val = v;
  return true;
}

static void
rewrite_msl_control_flow (const char *msl_file)
{
  FILE *in = fopen (msl_file, "r");
  if (!in)
    fatal_error (input_location, "opening %s: %m", msl_file);

  size_t len = 0;
  char *input = CONST_CAST (char *, read_file (in, &len));
  fclose (in);

  /* Split into NUL-terminated lines in-place.  */
  size_t nlines = 0, cap = 1024;
  char **lines = XNEWVEC (char *, cap);
  lines[nlines++] = input;
  for (char *p = input; *p; p++)
    if (*p == '\n')
      {
	*p = 0;
	if (nlines == cap)
	  {
	    cap *= 2;
	    lines = XRESIZEVEC (char *, lines, cap);
	  }
	lines[nlines++] = p + 1;
      }

  bool changed = false;
  struct obstack out_obstack;
  obstack_init (&out_obstack);

  char **constpool_labels = NULL;
  size_t constpool_n = 0, constpool_cap = 0;

  auto remember_constpool_label = [&] (const char *label_line) {
    const char *t = msl_ltrim (label_line);
    size_t n = strlen (t);
    gcc_assert (n > 0 && t[n - 1] == ':');
    char *name = XNEWVEC (char, n);
    memcpy (name, t, n - 1);
    name[n - 1] = '\0';
    if (constpool_n == constpool_cap)
      {
	constpool_cap = constpool_cap ? constpool_cap * 2 : 16;
	constpool_labels = XRESIZEVEC (char *, constpool_labels,
				       constpool_cap);
      }
    constpool_labels[constpool_n++] = name;
  };

  auto line_needs_const_cast_rewrite = [&] (const char *s) -> bool {
    if (constpool_n == 0)
      return false;
    if (strstr (s, "(device ") == NULL)
      return false;
    for (size_t li = 0; li < constpool_n; li++)
      {
	const char *lab = constpool_labels[li];
	char needle[64];
	snprintf (needle, sizeof needle, "*)%s", lab);
	if (strstr (s, needle) != NULL)
	  return true;
      }
    return false;
  };

  auto append_line = [&] (const char *s) {
    if (line_needs_const_cast_rewrite (s))
      {
	/* Rewrite device casts for constant-pool label references to use
	   constant address space.  */
	const char *p = s;
	while (*p)
	  {
	    const char *hit = strstr (p, "(device ");
	    if (!hit)
	      {
		obstack_grow (&out_obstack, p, strlen (p));
		break;
	      }
	    obstack_grow (&out_obstack, p, hit - p);
	    obstack_grow (&out_obstack, "(constant ", strlen ("(constant "));
	    p = hit + strlen ("(device ");
	  }
	obstack_1grow (&out_obstack, '\n');
	return;
      }

    obstack_grow (&out_obstack, s, strlen (s));
    obstack_1grow (&out_obstack, '\n');
  };

  for (size_t i = 0; i < nlines; )
    {
      /* Rewrite simple constant-pool blocks emitted as pseudo-asm into valid
	 MSL declarations.  */
      {
	size_t label_line = i;
	unsigned align = 0;
	bool has_align = msl_parse_align_comment (lines[i], &align);
	if (has_align)
	  label_line = i + 1;
	if (label_line < nlines)
	  {
	    const char *lab_line = lines[label_line];
	    const char *lab_raw = NULL;
	    if (msl_parse_constpool_label (lab_line, &lab_raw))
	      {
		size_t k = label_line + 1;
		long vals[64];
		size_t nvals = 0;
		long v;
		while (k < nlines && msl_parse_dot_long (lines[k], &v))
		  {
		    if (nvals < ARRAY_SIZE (vals))
		      vals[nvals++] = v;
		    k++;
		  }
		if (nvals)
		  {
		    changed = true;
		    remember_constpool_label (lab_line);
		    const char *name = msl_ltrim (lab_line);
		    size_t n = strlen (name);
		    gcc_assert (n > 0 && name[n - 1] == ':');

		    if (has_align)
		      {
			char hdr[128];
			snprintf (hdr, sizeof hdr,
				  "alignas(%u) constant uint ", align);
			obstack_grow (&out_obstack, hdr, strlen (hdr));
		      }
		    else
		      obstack_grow (&out_obstack, "constant uint ",
				    strlen ("constant uint "));

		    obstack_grow (&out_obstack, name, n - 1);
		    obstack_grow (&out_obstack, "[] = { ",
				  strlen ("[] = { "));
		    for (size_t vi = 0; vi < nvals; vi++)
		      {
			char num[64];
			snprintf (num, sizeof num, "%s(uint) %ld",
				  vi ? ", " : "", vals[vi]);
			obstack_grow (&out_obstack, num, strlen (num));
		      }
		    obstack_grow (&out_obstack, " };", strlen (" };"));
		    obstack_1grow (&out_obstack, '\n');

		    i = k;
		    continue;
		  }
	      }
	  }
      }

      /* Function signature followed by an opening brace.  Signatures may span
	 multiple lines (kernel parameter lists).

	 Avoid matching single-line helper definitions that contain '{' (e.g.
	 the MSL prelude may define inline helpers like abort()).  */
      if (strstr (lines[i], "void ") != NULL
	  && strchr (lines[i], '{') == NULL)
	{
	  size_t func_start = i;
	  size_t brace_line = i + 1;
	  while (brace_line < nlines
		 && strcmp (msl_ltrim (lines[brace_line]), "{") != 0)
	    {
	      /* Bail if we run into a prototype/decl.  */
	      if (strchr (lines[brace_line], ';') != NULL)
		break;
	      /* Avoid scanning too far on false positives.  */
	      if (brace_line - func_start > 64)
		break;
	      brace_line++;
	    }
	  if (brace_line >= nlines
	      || strcmp (msl_ltrim (lines[brace_line]), "{") != 0)
	    {
	      append_line (lines[i]);
	      i++;
	      continue;
	    }

	  size_t j = brace_line;
	  int brace_depth = 0;
	  for (; j < nlines; j++)
	    {
	      for (const char *q = lines[j]; *q; q++)
		{
		  if (*q == '{')
		    brace_depth++;
		  else if (*q == '}')
		    brace_depth--;
		}
	      if (brace_depth == 0 && j > brace_line)
		break;
	    }
	  if (j >= nlines)
	    {
	      for (; i < nlines; i++)
		append_line (lines[i]);
	      break;
	    }

	  for (size_t k = func_start; k <= brace_line; k++)
	    append_line (lines[k]);

	  size_t body_begin = brace_line + 1;
	  while (body_begin < j && msl_is_reg_decl (lines[body_begin]))
	    {
	      append_line (lines[body_begin]);
	      body_begin++;
	    }

	  struct block { int label; size_t start; size_t end; };
	  size_t nb = 0, bcap = 16;
	  block *blocks = XNEWVEC (block, bcap);
	  blocks[nb++] = { 0, body_begin, j };

	  for (size_t k = body_begin; k < j; k++)
	    {
	      int lab;
	      if (msl_parse_label_line (lines[k], &lab))
		{
		  blocks[nb - 1].end = k;
		  if (nb == bcap)
		    {
		      bcap *= 2;
		      blocks = XRESIZEVEC (block, blocks, bcap);
		    }
		  blocks[nb++] = { lab, k + 1, j };
		}
	    }

	  bool has_cf = false;
	  for (size_t bi = 0; bi < nb; bi++)
	    for (size_t k = blocks[bi].start; k < blocks[bi].end; k++)
	      if (strstr (lines[k], "goto L") != NULL)
		has_cf = true;

	  if (!has_cf && nb == 1)
	    {
	      for (size_t k = body_begin; k < j; k++)
		append_line (lines[k]);
	      append_line (lines[j]);
	      i = j + 1;
	      free (blocks);
	      continue;
	    }

	  changed = true;
	  append_line ("  int __gcc_bb = 0;");
	  append_line ("  for (;;)");
	  append_line ("    {");
	  append_line ("      switch (__gcc_bb)");
	  append_line ("        {");

	  for (size_t bi = 0; bi < nb; bi++)
	    {
	      int this_lab = blocks[bi].label;
	      int next_lab = (bi + 1 < nb) ? blocks[bi + 1].label : -1;
	      char hdr[64];
	      snprintf (hdr, sizeof hdr, "        case %d:", this_lab);
	      append_line (hdr);
	      append_line ("          {");

	      for (size_t k = blocks[bi].start; k < blocks[bi].end; k++)
		{
		  const char *ln = lines[k];
		  if (msl_is_blank (ln))
		    continue;

		  const char *goto_pos = strstr (ln, "goto L");
		  if (goto_pos)
		    {
		      const char *num = goto_pos + 6;
		      int lab = 0;
		      bool ok = false;
		      while (*num >= '0' && *num <= '9')
			{
			  ok = true;
			  lab = lab * 10 + (*num - '0');
			  num++;
			}
		      if (ok)
			{
			  const char *trim = msl_ltrim (ln);
			  if (startswith (trim, "if ("))
			    {
			      size_t prefix_len = goto_pos - ln;
			      char *prefix = XNEWVEC (char, prefix_len + 1);
			      memcpy (prefix, ln, prefix_len);
			      prefix[prefix_len] = 0;
			      char repl[128];
			      snprintf (repl, sizeof repl,
					"%s{ __gcc_bb = %d; continue; };",
					prefix, lab);
			      append_line (repl);
			      free (prefix);
			      continue;
			    }
			  char repl[96];
			  snprintf (repl, sizeof repl,
				    "          __gcc_bb = %d; continue;",
				    lab);
			  append_line (repl);
			  continue;
			}
		    }

		  append_line (ln);
		}

	      const char *last = NULL;
	      for (size_t k = blocks[bi].end; k-- > blocks[bi].start; )
		{
		  if (msl_is_blank (lines[k]))
		    continue;
		  last = msl_ltrim (lines[k]);
		  break;
		}
	      bool terminates = false;
	      if (last)
		terminates = (strstr (last, "continue;") != NULL
			      || startswith (last, "return"));
	      if (!terminates)
		{
		  if (next_lab >= 0)
		    {
		      char fall[96];
		      snprintf (fall, sizeof fall,
				"            __gcc_bb = %d; continue;",
				next_lab);
		      append_line (fall);
		    }
		  else
		    append_line ("            return;");
		}

	      append_line ("          }");
	    }

	  append_line ("        default:");
	  append_line ("          return;");
	  append_line ("        }");
	  append_line ("    }");

	  append_line (lines[j]);
	  i = j + 1;
	  free (blocks);
	  continue;
	}

      append_line (lines[i]);
      i++;
    }

  if (changed)
    {
      obstack_1grow (&out_obstack, '\0');
      const char *outbuf = (const char *) obstack_finish (&out_obstack);
      FILE *outf = fopen (msl_file, "w");
      if (!outf)
	fatal_error (input_location, "writing %s: %m", msl_file);
      fwrite (outbuf, 1, strlen (outbuf), outf);
      fclose (outf);
    }

  obstack_free (&out_obstack, NULL);
  for (size_t i = 0; i < constpool_n; i++)
    free (constpool_labels[i]);
  free (constpool_labels);
  free (lines);
  free (input);
}

static bool
is_ident_start (char c)
{
  return (c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

static bool
is_ident_char (char c)
{
  return is_ident_start (c) || (c >= '0' && c <= '9') || c == '$';
}

static bool
is_keyword_name (const char *name, size_t len)
{
  /* Keep this small and conservative.  */
  static const char *const kws[] = {
    "if", "for", "while", "switch", "return",
    "static_cast", "reinterpret_cast",
    "kernel", "constant", "device", "threadgroup",
    "abort"
  };
  for (size_t i = 0; i < ARRAY_SIZE (kws); i++)
    {
      const char *kw = kws[i];
      if (strlen (kw) == len && strncmp (name, kw, len) == 0)
	return true;
    }
  return false;
}

static bool
is_constpool_label_name (const char *name, size_t len)
{
  if (len < 3)
    return false;
  if (name[0] != 'L' || name[1] != 'C')
    return false;
  for (size_t i = 2; i < len; i++)
    if (name[i] < '0' || name[i] > '9')
      return false;
  return true;
}

/* When enabled via METAL_MKOFFLOAD_STUB_UNDECL=1, try to keep the Metal toolchain
   moving by injecting stub decls for undeclared identifiers that appear in the
   lowered MSL (typically due to non-standalone tests referencing extern symbols).
   This is intentionally a best-effort hack for bring-up.  */
static void
maybe_stub_undeclared_identifiers (const char *msl_file)
{
  const char *env = getenv ("METAL_MKOFFLOAD_STUB_UNDECL");
  if (!env || env[0] != '1')
    return;

  FILE *in = fopen (msl_file, "r");
  if (!in)
    fatal_error (input_location, "opening %s: %m", msl_file);
  size_t len = 0;
  const char *buf = read_file (in, &len);
  fclose (in);

  if (strstr (buf, "// mkoffload: stubs for undeclared identifiers") != NULL)
    {
      free (CONST_CAST (char *, buf));
      return;
    }

  /* Collect names seen in simple call contexts:  name();  */
  auto_vec<char *> call_names;
  auto_vec<char *> var_names;

  const char *p = buf;
  while (*p)
    {
      if (!is_ident_start (*p))
	{
	  p++;
	  continue;
	}

      const char *start = p;
      p++;
      while (is_ident_char (*p))
	p++;
      size_t n = p - start;
      if (n == 0)
	continue;
      if (n >= 4 && strncmp (start, "_reg", 4) == 0)
	continue;
      if (is_constpool_label_name (start, n))
	continue;
      if (is_keyword_name (start, n))
	continue;

      const char *q = p;
      while (*q == ' ' || *q == '\t' || *q == '\r')
	q++;

      bool is_call0 = false;
      if (*q == '(')
	{
	  q++;
	  while (*q == ' ' || *q == '\t' || *q == '\r')
	    q++;
	  if (*q == ')')
	    is_call0 = true;
	}

      bool is_cast_deref = false;
      if (!is_call0)
	{
	  /* Look for:  *)name  (as in *(device T*)name).  */
	  const char *r = start;
	  while (r > buf && (r[-1] == ' ' || r[-1] == '\t' || r[-1] == '\r'))
	    r--;
	  if (r - buf >= 2 && r[-1] == ')' && r[-2] == '*')
	    is_cast_deref = true;
	}

      bool is_simple_intptr_rhs = false;
      if (!is_call0 && !is_cast_deref)
	{
	  /* Look for simple RHS uses that are very likely pointer-ish temporaries
	     lowered into an int64 slot, e.g.:
	       *(device int64_t*)(...) = name;
	     This is a bring-up hack to keep non-standalone tests compiling.  */
	  const char *after = q;
	  while (*after == ' ' || *after == '\t' || *after == '\r')
	    after++;
	  if (*after == ';')
	    {
	      const char *ls = start;
	      while (ls > buf && ls[-1] != '\n')
		ls--;
	      const char *le = start;
	      while (*le && *le != '\n')
		le++;
	      if (memchr (ls, '=', (size_t) (le - ls)) != NULL)
		{
		  const char *tgt = strstr (ls, "*(device int64_t*)");
		  if (tgt && tgt < le)
		    is_simple_intptr_rhs = true;
		}
	    }
	}

      /* Skip if it looks like we're already in a declaration.  */
      {
	char needle[256];
	if (n < sizeof needle)
	  {
	    memcpy (needle, start, n);
	    needle[n] = 0;
	    char pat1[300], pat2[300], pat3[300];
	    snprintf (pat1, sizeof pat1, "kernel void %s", needle);
	    snprintf (pat2, sizeof pat2, "static inline %s", needle);
	    snprintf (pat3, sizeof pat3, "constant %s", needle);
	    if (strstr (buf, pat1) != NULL
		|| strstr (buf, pat2) != NULL
		|| strstr (buf, pat3) != NULL)
	      continue;
	  }
      }

      char *name = XNEWVEC (char, n + 1);
      memcpy (name, start, n);
      name[n] = '\0';

      if (is_call0)
	call_names.safe_push (name);
      else if (is_cast_deref)
	var_names.safe_push (name);
      else if (is_simple_intptr_rhs)
	var_names.safe_push (name);
      else
	free (name);
    }

  if (call_names.is_empty () && var_names.is_empty ())
    {
      free (CONST_CAST (char *, buf));
      return;
    }

  auto is_decl_keyword = [&] (const char *name, size_t n) -> bool {
    /* Qualifiers / storage.  */
    if ((n == 5 && strncmp (name, "const", 5) == 0)
	|| (n == 8 && strncmp (name, "volatile", 8) == 0)
	|| (n == 6 && strncmp (name, "static", 6) == 0)
	|| (n == 6 && strncmp (name, "inline", 6) == 0)
	|| (n == 6 && strncmp (name, "kernel", 6) == 0)
	|| (n == 6 && strncmp (name, "device", 6) == 0)
	|| (n == 8 && strncmp (name, "constant", 8) == 0)
	|| (n == 11 && strncmp (name, "threadgroup", 11) == 0)
	|| (n == 6 && strncmp (name, "thread", 6) == 0))
      return true;

    /* Common scalar types.  */
    if ((n == 4 && strncmp (name, "void", 4) == 0)
	|| (n == 4 && strncmp (name, "char", 4) == 0)
	|| (n == 5 && strncmp (name, "short", 5) == 0)
	|| (n == 3 && strncmp (name, "int", 3) == 0)
	|| (n == 4 && strncmp (name, "long", 4) == 0)
	|| (n == 5 && strncmp (name, "uchar", 5) == 0)
	|| (n == 6 && strncmp (name, "ushort", 6) == 0)
	|| (n == 4 && strncmp (name, "uint", 4) == 0)
	|| (n == 5 && strncmp (name, "ulong", 5) == 0)
	|| (n == 5 && strncmp (name, "float", 5) == 0)
	|| (n == 4 && strncmp (name, "half", 4) == 0)
	|| (n == 4 && strncmp (name, "bool", 4) == 0)
	|| (n == 6 && strncmp (name, "size_t", 6) == 0))
      return true;

    /* Composite types.  */
    if ((n == 6 && strncmp (name, "struct", 6) == 0)
	|| (n == 4 && strncmp (name, "enum", 4) == 0))
      return true;

    return false;
  };

  auto has_decl_or_def = [&] (const char *name) -> bool {
    size_t n = strlen (name);
    const char *p = buf;
    while (*p)
      {
	if (!is_ident_start (*p))
	  {
	    p++;
	    continue;
	  }

	const char *start = p;
	p++;
	while (is_ident_char (*p))
	  p++;

	if ((size_t) (p - start) != n || strncmp (start, name, n) != 0)
	  continue;

	/* Check for a function-like occurrence: name(...).  */
	const char *q = p;
	while (*q == ' ' || *q == '\t' || *q == '\r')
	  q++;
	if (*q != '(')
	  continue;

	/* Decide based on the first token on this line.  If it's a likely
	   declaration/definition keyword/type, avoid injecting a stub that would
	   collide with a real definition (e.g. "void NAME()").  */
	const char *ls = start;
	while (ls > buf && ls[-1] != '\n')
	  ls--;
	while (*ls == ' ' || *ls == '\t' || *ls == '\r')
	  ls++;
	if (startswith (ls, "//") || startswith (ls, "/*"))
	  continue;
	if (!is_ident_start (*ls))
	  continue;
	const char *tok = ls;
	tok++;
	while (is_ident_char (*tok))
	  tok++;
	size_t tlen = tok - ls;
	if (tlen == n && strncmp (ls, name, n) == 0)
	  continue;
	if (is_decl_keyword (ls, tlen))
	  return true;
      }
    return false;
  };

  /* Build injected text.  */
  struct obstack stub_obstack;
  obstack_init (&stub_obstack);
  obstack_grow (&stub_obstack, "\n// mkoffload: stubs for undeclared identifiers\n",
		strlen ("\n// mkoffload: stubs for undeclared identifiers\n"));

  auto vec_contains = [] (const auto_vec<char *> &v, const char *s) -> bool {
    for (unsigned i = 0; i < v.length (); i++)
      if (strcmp (v[i], s) == 0)
	return true;
    return false;
  };

  auto emit_stub = [&] (auto_vec<char *> &seen, const char *s, bool is_call) {
    if (vec_contains (seen, s))
      return;
    seen.safe_push (CONST_CAST (char *, s));
    if (is_call)
      {
	obstack_grow (&stub_obstack, "static inline long ", strlen ("static inline long "));
	obstack_grow (&stub_obstack, s, strlen (s));
	obstack_grow (&stub_obstack, "(void) { return 0; }\n",
		      strlen ("(void) { return 0; }\n"));
      }
    else
      {
	obstack_grow (&stub_obstack, "constant ulong ", strlen ("constant ulong "));
	obstack_grow (&stub_obstack, s, strlen (s));
	obstack_grow (&stub_obstack, " = 0;\n", strlen (" = 0;\n"));
      }
  };

  auto_vec<char *> seen;
  for (unsigned i = 0; i < call_names.length (); i++)
    if (!has_decl_or_def (call_names[i]))
      emit_stub (seen, call_names[i], true);
  for (unsigned i = 0; i < var_names.length (); i++)
    if (!has_decl_or_def (var_names[i]))
      emit_stub (seen, var_names[i], false);

  obstack_1grow (&stub_obstack, '\0');
  const char *stub_text = (const char *) obstack_finish (&stub_obstack);

  /* Insert stubs before the first kernel function.  */
  const char *best = NULL;

  auto consider = [&] (const char *hit) {
    if (!hit)
      return;
    const char *ls = hit;
    while (ls > buf && ls[-1] != '\n')
      ls--;
    /* Skip leading whitespace.  */
    while (*ls == ' ' || *ls == '\t' || *ls == '\r')
      ls++;
    /* Ignore helper decls like "static inline void ...".  */
    if (startswith (ls, "static ") || startswith (ls, "inline ")
	|| startswith (ls, "static inline "))
      return;
    if (!(startswith (ls, "kernel void ") || startswith (ls, "void ")))
      return;
    if (!best || ls < best)
      best = ls;
  };

  for (const char *s = buf; (s = strstr (s, "kernel void ")) != NULL; s++)
    consider (s);
  for (const char *s = buf; (s = strstr (s, "void ")) != NULL; s++)
    consider (s);

  if (!best)
    {
      obstack_free (&stub_obstack, NULL);
      for (unsigned i = 0; i < call_names.length (); i++)
	free (call_names[i]);
      for (unsigned i = 0; i < var_names.length (); i++)
	free (var_names[i]);
      free (CONST_CAST (char *, buf));
      return;
    }

  /* Back up to the start of the line so we don't split tokens.  */
  const char *line_start = best;
  while (line_start > buf && line_start[-1] != '\n')
    line_start--;
  size_t prefix_len = (size_t) (line_start - buf);

  struct obstack out_obstack;
  obstack_init (&out_obstack);
  obstack_grow (&out_obstack, buf, prefix_len);
  obstack_grow (&out_obstack, stub_text, strlen (stub_text));
  obstack_grow (&out_obstack, line_start, strlen (line_start));

  obstack_1grow (&out_obstack, '\0');
  const char *outbuf = (const char *) obstack_finish (&out_obstack);
  FILE *outf = fopen (msl_file, "w");
  if (!outf)
    fatal_error (input_location, "writing %s: %m", msl_file);
  fwrite (outbuf, 1, strlen (outbuf), outf);
  fclose (outf);

  obstack_free (&out_obstack, NULL);
  obstack_free (&stub_obstack, NULL);
  for (unsigned i = 0; i < call_names.length (); i++)
    free (call_names[i]);
  for (unsigned i = 0; i < var_names.length (); i++)
    free (var_names[i]);
  free (CONST_CAST (char *, buf));
}

static const char *
find_first_real_function_line (const char *buf)
{
  const char *best = NULL;

  auto consider = [&] (const char *hit) {
    if (!hit)
      return;
    const char *ls = hit;
    while (ls > buf && ls[-1] != '\n')
      ls--;
    /* Skip leading whitespace.  */
    while (*ls == ' ' || *ls == '\t' || *ls == '\r')
      ls++;
    /* Ignore helper decls like "static inline void ...".  */
    if (startswith (ls, "static ") || startswith (ls, "inline ")
	|| startswith (ls, "static inline "))
      return;
    if (!(startswith (ls, "kernel void ") || startswith (ls, "void ")))
      return;
    if (!best || ls < best)
      best = ls;
  };

  for (const char *s = buf; (s = strstr (s, "kernel void ")) != NULL; s++)
    consider (s);
  for (const char *s = buf; (s = strstr (s, "void ")) != NULL; s++)
    consider (s);

  if (!best)
    return NULL;

  const char *line_start = best;
  while (line_start > buf && line_start[-1] != '\n')
    line_start--;
  return line_start;
}

/* Fix up a couple of patterns that clang/Metal rejects:

   1) Trailing "Local variable:" constant decls are emitted after functions, but
      referenced inside kernels.  Hoist them to before the first function.
   2) Some lowered code assigns a constant char array name into an int64 slot,
      which needs an explicit cast to an integer address value.  */
static void
fixup_msl_locals_and_addresses (const char *msl_file)
{
  FILE *in = fopen (msl_file, "r");
  if (!in)
    fatal_error (input_location, "opening %s: %m", msl_file);
  size_t len = 0;
  const char *buf = read_file (in, &len);
  fclose (in);

  if (strstr (buf, "// mkoffload: hoisted local variable decls") != NULL)
    {
      free (CONST_CAST (char *, buf));
      return;
    }

  const char *insert_at = find_first_real_function_line (buf);
  if (!insert_at)
    {
      free (CONST_CAST (char *, buf));
      return;
    }

  auto vec_contains = [] (const auto_vec<char *> &v, const char *s) -> bool {
    for (unsigned i = 0; i < v.length (); i++)
      if (strcmp (v[i], s) == 0)
	return true;
    return false;
  };

  auto_vec<char *> char_arrays;
  /* Collect constant char array names.  */
  for (const char *p = buf; *p;)
    {
      const char *ls = p;
      const char *le = p;
      while (*le && *le != '\n')
	le++;
      p = *le ? le + 1 : le;

      /* Quick filter: most lines won't match.  */
      if (strstr (ls, "constant") == NULL || strstr (ls, " char ") == NULL)
	continue;

      const char *c = strstr (ls, " char ");
      if (!c || c >= le)
	continue;
      c += strlen (" char ");
      if (!is_ident_start (*c))
	continue;
      const char *ns = c;
      c++;
      while (is_ident_char (*c))
	c++;
      if (*c != '[')
	continue;

      size_t n = (size_t) (c - ns);
      char *name = XNEWVEC (char, n + 1);
      memcpy (name, ns, n);
      name[n] = '\0';
      if (!vec_contains (char_arrays, name))
	char_arrays.safe_push (name);
      else
	free (name);
    }

  struct obstack hoist_obstack;
  struct obstack out_obstack;
  obstack_init (&hoist_obstack);
  obstack_grow (&hoist_obstack,
		"\n// mkoffload: hoisted local variable decls\n",
		strlen ("\n// mkoffload: hoisted local variable decls\n"));

  bool changed = false;
  bool hoisted_any = false;

  /* First pass: collect trailing local decls for hoisting.  */
  for (const char *p = buf; *p;)
    {
      const char *ls = p;
      const char *le = p;
      while (*le && *le != '\n')
	le++;
      p = *le ? le + 1 : le;

      if (ls < insert_at || !startswith (ls, "// Local variable:"))
	continue;

      const char *ls2 = p;
      const char *le2 = p;
      while (*le2 && *le2 != '\n')
	le2++;

      if (!startswith (ls2, "constant /* local */ char "))
	continue;

      obstack_grow (&hoist_obstack, ls, (size_t) (le - ls));
      obstack_1grow (&hoist_obstack, '\n');
      obstack_grow (&hoist_obstack, ls2, (size_t) (le2 - ls2));
      obstack_1grow (&hoist_obstack, '\n');
      hoisted_any = true;
      changed = true;

      /* Skip the decl line too.  */
      p = *le2 ? le2 + 1 : le2;
    }

  obstack_init (&out_obstack);

  /* Second pass: write output, skipping hoisted decls and rewriting a couple of
     invalid constructs.  */
  const char *p = buf;
  while (*p)
    {
      const char *ls = p;
      const char *le = p;
      while (*le && *le != '\n')
	le++;
      p = *le ? le + 1 : le;

      if (ls == insert_at && hoisted_any)
	{
	  const char *ht = (const char *) obstack_base (&hoist_obstack);
	  size_t hlen = (size_t) ((const char *) obstack_next_free (&hoist_obstack) - ht);
	  obstack_grow (&out_obstack, ht, hlen);
	}

      /* Skip local decl blocks that we hoisted.  */
      if (ls >= insert_at && startswith (ls, "// Local variable:"))
	{
	  const char *ls2 = p;
	  const char *le2 = p;
	  while (*le2 && *le2 != '\n')
	    le2++;

	  if (startswith (ls2, "constant /* local */ char "))
	    {
	      changed = true;
	      p = *le2 ? le2 + 1 : le2;
	      continue;
	    }
	}

      /* Rewrite:  *(device int64_t*)(...) = <char-array>;
	 into:      *(device int64_t*)(...) = (ulong)(&<char-array>[0]);  */
      const char *tgt = strstr (ls, "*(device int64_t*)");
      if (tgt && tgt < le)
	{
	  const char *eq = strchr (ls, '=');
	  if (eq && eq < le)
	    {
	      const char *rhs = eq + 1;
	      while (rhs < le && (*rhs == ' ' || *rhs == '\t' || *rhs == '\r'))
		rhs++;
	      const char *rhs_id_end = rhs;
	      while (rhs_id_end < le && is_ident_char (*rhs_id_end))
		rhs_id_end++;
	      const char *rhs_ws = rhs_id_end;
	      while (rhs_ws < le && (*rhs_ws == ' ' || *rhs_ws == '\t' || *rhs_ws == '\r'))
		rhs_ws++;

	      if (rhs < rhs_id_end && rhs_ws < le && *rhs_ws == ';')
		{
		  size_t n = (size_t) (rhs_id_end - rhs);
		  char namebuf[256];
		  if (n < sizeof namebuf)
		    {
		      memcpy (namebuf, rhs, n);
		      namebuf[n] = 0;
		      if (vec_contains (char_arrays, namebuf))
			{
			  obstack_grow (&out_obstack, ls, (size_t) ((eq + 1) - ls));
			  obstack_grow (&out_obstack, " (ulong)(&", strlen (" (ulong)(&"));
			  obstack_grow (&out_obstack, namebuf, strlen (namebuf));
			  obstack_grow (&out_obstack, "[0]);", strlen ("[0]);"));
			  obstack_1grow (&out_obstack, '\n');
			  changed = true;
			  continue;
			}
		    }
		}
	    }
	}

      obstack_grow (&out_obstack, ls, (size_t) (le - ls));
      obstack_1grow (&out_obstack, '\n');
    }

  if (changed)
    {
      obstack_1grow (&out_obstack, '\0');
      const char *outbuf = (const char *) obstack_finish (&out_obstack);
      FILE *outf = fopen (msl_file, "w");
      if (!outf)
	fatal_error (input_location, "writing %s: %m", msl_file);
      fwrite (outbuf, 1, strlen (outbuf), outf);
      fclose (outf);
    }

  obstack_free (&out_obstack, NULL);
  obstack_free (&hoist_obstack, NULL);
  for (unsigned i = 0; i < char_arrays.length (); i++)
    free (char_arrays[i]);
  free (CONST_CAST (char *, buf));
}

/* Compile MSL to metallib using Metal toolchain.  */

static void
compile_metal (const char *msl_file, const char *metallib_file)
{
  if (!air_name)
    air_name = make_temp_file (".air");

  /* Rewrite labels/goto into switch-based control flow.  */
  rewrite_msl_control_flow (msl_file);
  fixup_msl_locals_and_addresses (msl_file);
  maybe_stub_undeclared_identifiers (msl_file);

  /* Compile .metal to .air.  */
  struct obstack argv_obstack;
  obstack_init (&argv_obstack);
  obstack_ptr_grow (&argv_obstack, "xcrun");
  obstack_ptr_grow (&argv_obstack, "-sdk");
  obstack_ptr_grow (&argv_obstack, "macosx");
  obstack_ptr_grow (&argv_obstack, "metal");
  obstack_ptr_grow (&argv_obstack, "-c");
  /* The current generated MSL is very low-level and may trigger a lot of
     diagnostics (e.g. unused registers).  Keep the noise down so tool output
     stays manageable.  */
  obstack_ptr_grow (&argv_obstack, "-Wno-unused-variable");
  obstack_ptr_grow (&argv_obstack, "-Wno-uninitialized");
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
  /* Note: Apple's metallib tool uses -v for "version" (prints and exits),
     not for "verbose".  We already print the command line ourselves when
     verbose, so do not add any extra flags here.  */
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
  char *driver_buf = NULL;

  /* If the user configured the offload toolchain location via GCC_OFFLOAD_PATH,
     try to locate the offload compiler driver there.  */
  if (!driver)
    {
      const char *offload_path = getenv ("GCC_OFFLOAD_PATH");
      if (offload_path && offload_path[0] != '\0')
	{
	  size_t len = strlen (offload_path) + strlen ("/bin/")
	    + strlen (GCC_INSTALL_NAME) + 1;
	  driver_buf = XNEWVEC (char, len);
	  sprintf (driver_buf, "%s/bin/%s", offload_path, GCC_INSTALL_NAME);
	  if (access_check (driver_buf, X_OK) == 0)
	    driver = driver_buf;
	  else
	    {
	      XDELETEVEC (driver_buf);
	      driver_buf = NULL;
	    }
	}
    }

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

      if (gcc_path != NULL)
	{
	  size_t len = strlen (gcc_path) + 1 + strlen (GCC_INSTALL_NAME) + 1;
	  driver_buf = XNEWVEC (char, len);
	  sprintf (driver_buf, "%s/%s", gcc_path, GCC_INSTALL_NAME);
	  if (access_check (driver_buf, X_OK) == 0)
	    found = true;
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
  /* The offload compiler produces MSL source, not assembly/object files.
     Prevent the driver from invoking the system assembler on the generated
     MSL text.  */
  obstack_ptr_grow (&argv_obstack, "-S");
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

      /* mkoffload runs a driver to produce the intermediate MSL.  For
	 in-tree builds (COLLECT_GCC is typically "xgcc"), make sure the
	 driver can find its internal executables (lto1/cc1/...) even after
	 we scrub environment variables below.

	 When OFFLOAD_TARGET_DEFAULT points at an installed offload compiler,
	 do not inject a -B path for COLLECT_GCC: that would cause the offload
	 compiler to accidentally pick up the host build-tree's lto1.  */
      bool add_b = false;
      const char *b_dir = NULL;
      if (driver && collect_gcc && strcmp (driver, collect_gcc) == 0)
	{
	  b_dir = dirname (ASTRDUP (collect_gcc));
	  if (b_dir && *b_dir)
	    {
	      char *lto1 = concat (b_dir, "/lto1", NULL);
	      add_b = (access_check (lto1, X_OK) == 0);
	      free (lto1);
	    }
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
	  exec_argv[2] = CONST_CAST (char *, b_dir);
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
