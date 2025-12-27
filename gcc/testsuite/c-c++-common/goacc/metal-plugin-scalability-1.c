/* { dg-do compile } */
/* { dg-additional-options "-fopenacc" } */
/* { dg-final { global srcdir; set plugin_file [file normalize [file join $srcdir ../../../../libgomp/plugin/plugin-metal.cpp]]; if { ![file exists $plugin_file] } { untested "metal plugin source file not found" } else { scan-file-not $plugin_file "MAX_METAL_BUFFERS" ; scan-file-not $plugin_file "MAX_METAL_IMAGES" ; scan-file $plugin_file "std::unordered_map" } } } */

int
main (void)
{
  return 0;
}
