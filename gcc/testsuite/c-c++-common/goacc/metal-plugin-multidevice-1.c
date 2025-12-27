/* { dg-do compile } */
/* { dg-additional-options "-fopenacc" } */
/* { dg-final { global srcdir; set plugin_file [file normalize [file join $srcdir ../../../../libgomp/plugin/plugin-metal.cpp]]; if { ![file exists $plugin_file] } { untested "metal plugin source file not found" } else { scan-file $plugin_file "metal_enumerate_devices" ; scan-file $plugin_file "MTL::CopyAllDevices" ; scan-file-not $plugin_file "CreateSystemDefaultDevice" ; scan-file-not $plugin_file "num_metal_devices = 1" } } } */

int
main (void)
{
  return 0;
}
