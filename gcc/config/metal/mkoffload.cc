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

/* Packages compiled metallib binaries into GCC offload image format.

   Input: metallib binary file (compiled MSL)
   Output: C source file with embedded metallib and registration code

   The generated C file contains:
   - Binary metallib data as a byte array
   - Kernel symbol table mapping host names to Metal kernel names
   - Variable symbol table
   - GOMP registration constructor/destructor functions  */

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

struct id_map
{
    id_map *next;
    char *metal_name;
};

static id_map *func_ids, **funcs_tail = &func_ids;
static id_map *var_ids, **vars_tail = &var_ids;

/* Files to unlink.  */
static const char *metallib_name;
static const char *metal_cfile_name;
static const char *omp_requires_file;
static const char *metal_dumpbase;

/* Delete tempfiles.  */

void
tool_cleanup (bool from_signal ATTRIBUTE_UNUSED)
{
    if (metal_cfile_name)
        maybe_unlink (metal_cfile_name);
    if (metallib_name)
        maybe_unlink (metallib_name);
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

/* Read entire binary file into memory.  */

static const unsigned char *
read_file (FILE *stream, size_t *plen)
{
    size_t alloc = 16384;
    size_t base = 0;
    unsigned char *buffer;

    if (!fseek (stream, 0, SEEK_END))
    {
        long s = ftell (stream);
        if (s >= 0)
            alloc = s + 100;
        fseek (stream, 0, SEEK_SET);
    }
    buffer = XNEWVEC (unsigned char, alloc);

    for (;;)
    {
        size_t n = fread (buffer + base, 1, alloc - base, stream);
        if (!n)
            break;
        base += n;
        if (base == alloc)
        {
            alloc *= 2;
            buffer = XRESIZEVEC (unsigned char, buffer, alloc);
        }
    }
    *plen = base;
    return buffer;
}

static void
record_id (const char *name, id_map ***where)
{
    id_map *v = XNEW (id_map);
    size_t len = strlen (name);
    v->metal_name = XNEWVEC (char, len + 1);
    memcpy (v->metal_name, name, len + 1);
    v->next = NULL;
    id_map **tail = *where;
    *tail = v;
    *where = &v->next;
}

/* Process metallib binary and symbol information, generating C code.  */

static void
process (FILE *in, FILE *out, uint32_t omp_requires)
{
    size_t metallib_len = 0;
    const unsigned char *metallib_data = read_file (in, &metallib_len);
    const char *comma;
    id_map const *id;

    fprintf (out, "#include <stdint.h>\n\n");

    /* Embed metallib as binary data.  */
    fprintf (out, "static const unsigned char metallib_data[] = {");
    for (size_t i = 0; i < metallib_len; i++)
    {
        if (i % 12 == 0)
            fprintf (out, "\n  ");
        fprintf (out, "0x%02x", metallib_data[i]);
        if (i + 1 < metallib_len)
            fprintf (out, ",");
    }
    fprintf (out, "\n};\n\n");

    /* Create metallib object structure.  */
    fprintf (out, "static const struct metallib_obj {\n"
        "  const unsigned char *data;\n"
        "  __SIZE_TYPE__ size;\n"
        "} metallib_obj = {\n"
        "  metallib_data, sizeof (metallib_data)\n"
        "};\n\n");

    /* Dump out variable idents.  */
    fprintf (out, "static const char *const var_mappings[] = {");
    for (comma = "", id = var_ids; id; comma = ",", id = id->next)
        fprintf (out, "%s\n  \"%s\"", comma, id->metal_name);
    fprintf (out, "\n};\n\n");

    /* Dump out function idents.  */
    fprintf (out, "static const char *const func_mappings[] = {");
    for (comma = "", id = func_ids; id; comma = ",", id = id->next)
        fprintf (out, "%s\n  \"%s\"", comma, id->metal_name);
    fprintf (out, "\n};\n\n");

    /* Create Metal-specific target data structure.  */
    fprintf (out,
        "static const struct metal_data {\n"
        "  uintptr_t omp_requires_mask;\n"
        "  const struct metallib_obj *metallib;\n"
        "  const char *const *var_names;\n"
        "  unsigned var_num;\n"
        "  const char *const *fn_names;\n"
        "  unsigned fn_num;\n"
        "} metal_data = {\n"
        "  %d,\n"
        "  &metallib_obj,\n"
        "  var_mappings,\n"
        "  sizeof (var_mappings) / sizeof (var_mappings[0]),\n"
        "  func_mappings,\n"
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
    obstack_ptr_grow (&argv_obstack, metal_dumpbase);
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
        fatal_error (input_location, "COLLECT_GCC must be set.");
    const char *gcc_path = dirname (ASTRDUP (collect_gcc));
    const char *gcc_exec = basename (ASTRDUP (collect_gcc));

    size_t len = (strlen (gcc_path) + 1 + strlen (GCC_INSTALL_NAME) + 1);
    char *driver = XALLOCAVEC (char, len);

    if (strcmp (gcc_exec, collect_gcc) == 0)
        gcc_path = NULL;

    int driver_used = 0;
    if (gcc_path != NULL)
        driver_used = sprintf (driver, "%s/", gcc_path);
    sprintf (driver + driver_used, "%s", GCC_INSTALL_NAME);

    bool found = false;
    if (gcc_path == NULL)
        found = true;
    else if (access_check (driver, X_OK) == 0)
        found = true;
    else
    {
        driver = NULL;
        char **paths = NULL;
        unsigned n_paths;
        n_paths = parse_env_var (getenv ("COMPILER_PATH"), &paths);
        for (unsigned i = 0; i < n_paths; i++)
        {
            len = strlen (paths[i]) + 1 + strlen (GCC_INSTALL_NAME) + 1;
            driver = XRESIZEVEC (char, driver, len);
            sprintf (driver, "%s/%s", paths[i], GCC_INSTALL_NAME);
            if (access_check (driver, X_OK) == 0)
            {
                found = true;
                break;
            }
        }
        free_array_of_ptrs ((void **) paths, n_paths);
    }

    if (!found)
        fatal_error (input_location,
            "offload compiler %s not found (consider using %<-B%>)",
            GCC_INSTALL_NAME);

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
        else if (strcmp (argv[i], "-dumpbase") == 0 && i + 1 < argc)
            dumppfx = argv[++i];
    }

    if (!(fopenacc ^ fopenmp))
        fatal_error (input_location, "either %<-fopenacc%> or %<-fopenmp%> "
            "must be set");

    /* Process remaining arguments to find input metallib and output file.  */
    const char *infile = NULL;
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp (argv[i], "-o") && i + 1 != argc)
            outname = argv[++i];
        else if (argv[i][0] != '-')
            infile = argv[i];
    }

    if (!infile)
        fatal_error (input_location, "no input metallib file specified");

    if (!dumppfx)
        dumppfx = outname;

    metal_dumpbase = concat (dumppfx, ".c", NULL);
    if (save_temps)
        metal_cfile_name = metal_dumpbase;
    else
        metal_cfile_name = make_temp_file (".c");

    out = fopen (metal_cfile_name, "w");
    if (!out)
        fatal_error (input_location, "cannot open '%s'", metal_cfile_name);

    /* For now, read metallib directly. In future, this will come from
       the Metal compiler backend output.  */
    in = fopen (infile, "rb");
    if (!in)
        fatal_error (input_location, "cannot open input metallib '%s'", infile);

    /* Read OMP requires mask.  */
    uint32_t omp_requires = 0;
    if (save_temps)
        omp_requires_file = concat (dumppfx, ".mkoffload.omp_requires", NULL);
    else
        omp_requires_file = make_temp_file (".mkoffload.omp_requires");

    FILE *req_file = fopen (omp_requires_file, "rb");
    if (req_file)
    {
        if (fread (&omp_requires, sizeof (omp_requires), 1, req_file) != 1)
            omp_requires = 0;
        fclose (req_file);
    }

    /* TODO: Parse metallib to extract kernel names and populate func_ids.
       For now, generate empty symbol tables.  */

    process (in, out, omp_requires);
    fclose (in);
    fclose (out);

    compile_native (metal_cfile_name, outname, collect_gcc, fPIC, fpic);

    return 0;
}
