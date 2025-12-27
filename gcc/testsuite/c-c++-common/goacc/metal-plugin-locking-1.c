/* { dg-do compile } */
/* { dg-additional-options "-fopenacc" } */
/* { dg-final { global srcdir; set plugin_file [file normalize [file join $srcdir ../../../../libgomp/plugin/plugin-metal.cpp]]; if { ![file exists $plugin_file] } { untested "metal plugin source file not found" } else { scan-file $plugin_file "pthread_mutex_t buffer_lock" ; scan-file $plugin_file "pthread_mutex_lock (&metal_devices\\[ord\\]\\.buffer_lock" ; scan-file $plugin_file "release_exec_queue" ; scan-file $plugin_file "exec_queue->retain" } } } */

int
main (void)
{
  return 0;
}
