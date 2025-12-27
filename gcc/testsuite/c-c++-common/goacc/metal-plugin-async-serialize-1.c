/* { dg-do compile } */
/* { dg-additional-options "-fopenacc" } */
/* { dg-final { global srcdir; set plugin_file [file normalize [file join $srcdir ../../libgomp/plugin/plugin-metal.cpp]]; if { ![file exists $plugin_file] } { untested "metal plugin source file not found" } else { scan-file $plugin_file "GOMP_OFFLOAD_openacc_async_serialize" ; scan-file $plugin_file "metal_async_queue_encode_wait \\(aq2, aq1, wait_cmd\\)" ; scan-file-not $plugin_file "return GOMP_OFFLOAD_openacc_async_synchronize" } } } */

int
main (void)
{
  return 0;
}
