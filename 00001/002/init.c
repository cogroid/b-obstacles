/* Copyright (C) 1995-2004, 2006, 2009-2014 Free Software Foundation, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */



/* Include the headers for just about everything.
   We call all their initialization functions.  */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gmp.h>

#include "libguile/_scm.h"

/* Everybody has an init function.  */
#include "libguile/alist.h"
#include "libguile/async.h"
#include "libguile/atomic.h"
#include "libguile/backtrace.h"
#include "libguile/bitvectors.h"
#include "libguile/boolean.h"
#include "libguile/bytevectors.h"
#include "libguile/chars.h"
#include "libguile/control.h"
#include "libguile/continuations.h"
#include "libguile/debug.h"
#ifdef GUILE_DEBUG_MALLOC
#include "libguile/debug-malloc.h"
#endif
#include "libguile/deprecation.h"
#include "libguile/dynl.h"
#include "libguile/dynwind.h"
#include "libguile/eq.h"
#include "libguile/error.h"
#include "libguile/eval.h"
#include "libguile/evalext.h"
#include "libguile/expand.h"
#include "libguile/fdes-finalizers.h"
#include "libguile/feature.h"
#include "libguile/filesys.h"
#include "libguile/finalizers.h"
#include "libguile/fluids.h"
#include "libguile/fports.h"
#include "libguile/frames.h"
#include "libguile/gc.h"
#include "libguile/generalized-arrays.h"
#include "libguile/generalized-vectors.h"
#include "libguile/goops.h"
#include "libguile/gsubr.h"
#include "libguile/hash.h"
#include "libguile/hashtab.h"
#include "libguile/hooks.h"
#include "libguile/gettext.h"
#include "libguile/i18n.h"
#include "libguile/instructions.h"
#include "libguile/ioext.h"
#include "libguile/keywords.h"
#include "libguile/list.h"
#include "libguile/load.h"
#include "libguile/macros.h"
#include "libguile/mallocs.h"
#include "libguile/memoize.h"
#include "libguile/modules.h"
#include "libguile/net_db.h"
#include "libguile/numbers.h"
#include "libguile/loader.h"
#include "libguile/objprop.h"
#include "libguile/options.h"
#include "libguile/pairs.h"
#include "libguile/poll.h"
#include "libguile/ports.h"
#include "libguile/posix.h"
#ifdef ENABLE_REGEX
#include "libguile/regex-posix.h"
#endif
#include "libguile/print.h"
#include "libguile/procprop.h"
#include "libguile/procs.h"
#include "libguile/programs.h"
#include "libguile/promises.h"
#include "libguile/array-map.h"
#include "libguile/random.h"
#include "libguile/rdelim.h"
#include "libguile/read.h"
#include "libguile/rw.h"
#include "libguile/scmsigs.h"
#include "libguile/script.h"
#include "libguile/simpos.h"
#include "libguile/smob.h"
#include "libguile/socket.h"
#include "libguile/sort.h"
#include "libguile/srcprop.h"
#include "libguile/stackchk.h"
#include "libguile/stacks.h"
#include "libguile/stime.h"
#include "libguile/strings.h"
#include "libguile/srfi-1.h"
#include "libguile/srfi-4.h"
#include "libguile/srfi-13.h"
#include "libguile/srfi-14.h"
#include "libguile/srfi-60.h"
#include "libguile/strorder.h"
#include "libguile/strports.h"
#include "libguile/struct.h"
#include "libguile/symbols.h"
#include "libguile/syntax.h"
#include "libguile/throw.h"
#include "libguile/arrays.h"
#include "libguile/trees.h"
#include "libguile/unicode.h"
#include "libguile/values.h"
#include "libguile/variable.h"
#include "libguile/vectors.h"
#include "libguile/version.h"
#include "libguile/vm.h"
#include "libguile/vports.h"
#include "libguile/guardians.h"
#include "libguile/extensions.h"
#include "libguile/uniform.h"
#include "libguile/deprecated.h"

#include "libguile/init.h"
#include "libguile/private-options.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <unistd.h>


FILE* fh_init = NULL;


/* initializing standard and current I/O ports */

typedef struct
{
  int fdes;
  char *mode;
} stream_body_data;

/* proc to be called in scope of exception handler stream_handler. */
static SCM
stream_body (void *data)
{
  stream_body_data *body_data = (stream_body_data *) data;
  SCM port = scm_fdes_to_port (body_data->fdes, body_data->mode, SCM_BOOL_F);
  scm_set_port_revealed_x (port, SCM_INUM1);
  return port;
}

/* exception handler for stream_body.  */
static SCM
stream_handler (void *data SCM_UNUSED,
		SCM tag SCM_UNUSED,
		SCM throw_args SCM_UNUSED)
{
  return SCM_BOOL_F;
}

/* Convert a file descriptor to a port, using scm_fdes_to_port.
   - set the revealed count for FILE's file descriptor to 1, so
   that fdes won't be closed when the port object is GC'd.
   - catch exceptions: allow Guile to be able to start up even
   if it has been handed bogus stdin/stdout/stderr.  replace the
   bad ports with void ports.  */
static SCM
scm_standard_stream_to_port (int fdes, char *mode)
{
  SCM port;
  stream_body_data body_data;

  body_data.fdes = fdes;
  body_data.mode = mode;
  port = scm_internal_catch (SCM_BOOL_T, stream_body, &body_data, 
			     stream_handler, NULL);
  if (scm_is_false (port))
    port = scm_void_port (mode);
  return port;
}

/* Create standard ports from stdin, stdout, and stderr.  */
static void
scm_init_standard_ports ()
{
  /* From the SCSH manual:

     It can be useful to turn I/O buffering off in some cases, for
     example when an I/O stream is to be shared by multiple
     subprocesses.  For this reason, scsh allocates an unbuffered port
     for file descriptor 0 at start-up time.

     Because shells frequently share stdin with subprocesses, if the
     shell does buffered reads, it might ``steal'' input intended for
     a subprocess.  For this reason, all shells, including sh, csh,
     and scsh, read stdin unbuffered.  Applications that can tolerate
     buffered input on stdin can reset \ex{(current-input-port)} to
     block buffering for higher performance.  */

  scm_set_current_input_port 
    (scm_standard_stream_to_port (0, isatty (0) ? "r0" : "r"));
  scm_set_current_output_port
    (scm_standard_stream_to_port (1, isatty (1) ? "w0" : "w"));
  scm_set_current_error_port
    (scm_standard_stream_to_port (2, isatty (2) ? "w0" : "w"));
  scm_set_current_warning_port (scm_current_error_port ());
}



/* Loading the startup Scheme files.  */

/* The boot code "ice-9/boot-9" is only loaded by scm_boot_guile when
   this is false.  The unexec code uses this, to keep ice_9 from being
   loaded into dumped guile executables.  */
int scm_ice_9_already_loaded = 0;

void
scm_load_startup_files ()
{
fprintf(fh_init, "scm_load_startup_files #1");
fflush(fh_init);


  /* We want a path only containing directories from GUILE_LOAD_PATH,
     SCM_SITE_DIR and SCM_LIBRARY_DIR when searching for the site init
     file, so we do this before loading Ice-9.  */
  SCM init_path =
    scm_sys_search_load_path (scm_from_locale_string ("init.scm"));

fprintf(fh_init, "scm_load_startup_files #2: %s", scm_from_locale_string ("init.scm"));
fflush(fh_init);

  /* Load Ice-9.  */
  if (!scm_ice_9_already_loaded)
    {
fprintf(fh_init, "scm_load_startup_files #3");
fflush(fh_init);

      scm_c_primitive_load_path ("ice-9/boot-9");

fprintf(fh_init, "scm_load_startup_files #4");
fflush(fh_init);

      /* Load the init.scm file.  */
      if (scm_is_true (init_path))
	scm_primitive_load (init_path);

fprintf(fh_init, "scm_load_startup_files #5");
fflush(fh_init);

    }

fprintf(fh_init, "scm_load_startup_files #6");
fflush(fh_init);

}


/* The main init code.  */

#ifdef _UNICOS
typedef int setjmp_type;
#else
typedef long setjmp_type;
#endif

/* All the data needed to invoke the main function.  */
struct main_func_closure
{
  /* the function to call */
  void (*main_func)(void *closure, int argc, char **argv);
  void *closure;		/* dummy data to pass it */
  int argc;
  char **argv;			/* the argument list it should receive */
};

static void *invoke_main_func(void *body_data);


/* Fire up the Guile Scheme interpreter.

   Call MAIN_FUNC, passing it CLOSURE, ARGC, and ARGV.  MAIN_FUNC
   should do all the work of the program (initializing other packages,
   reading user input, etc.) before returning.  When MAIN_FUNC
   returns, call exit (EXIT_FAILURE); this function never returns.
   If you want some other exit value, MAIN_FUNC may call exit itself.

   scm_boot_guile arranges for program-arguments to return the strings
   given by ARGC and ARGV.  If MAIN_FUNC modifies ARGC/ARGV, should
   call scm_set_program_arguments with the final list, so Scheme code
   will know which arguments have been processed.

   scm_boot_guile establishes a catch-all catch handler which prints
   an error message and exits the process.  This means that Guile
   exits in a coherent way when system errors occur and the user isn't
   prepared to handle it.  If the user doesn't like this behavior,
   they can establish their own universal catcher to shadow this one.

   Why must the caller do all the real work from MAIN_FUNC?  The
   garbage collector assumes that all local variables of type SCM will
   be above scm_boot_guile's stack frame on the stack.  If you try to
   manipulate SCM values after this function returns, it's the luck of
   the draw whether the GC will be able to find the objects you
   allocate.  So, scm_boot_guile function exits, rather than
   returning, to discourage people from making that mistake.  */


void
scm_boot_guile (int argc, char ** argv, void (*main_func) (), void *closure)
{
  void *res;
  struct main_func_closure c;

  /* On Windows, convert backslashes in argv[0] to forward
     slashes.  */
  if (argc > 0)
    scm_i_mirror_backslashes (argv[0]);

  c.main_func = main_func;
  c.closure = closure;
  c.argc = argc;
  c.argv = argv;

  res = scm_with_guile (invoke_main_func, &c);

  /* If the caller doesn't want this, they should exit from main_func
     themselves.
  */
  if (res == NULL)
    exit (EXIT_FAILURE);
  else
    exit (EXIT_SUCCESS);
}

static void *
invoke_main_func (void *body_data)
{
  struct main_func_closure *closure = (struct main_func_closure *) body_data;

  scm_i_set_boot_program_arguments (closure->argc, closure->argv);
  (*closure->main_func) (closure->closure, closure->argc, closure->argv);

  scm_restore_signals ();

  /* This tick gives any pending
   * asyncs a chance to run.  This must be done after
   * the call to scm_restore_signals.
   */
  scm_async_tick ();

  /* Indicate success by returning non-NULL.
   */
  return (void *)1;
}

scm_i_pthread_mutex_t scm_i_init_mutex = SCM_I_PTHREAD_MUTEX_INITIALIZER;
int scm_initialized_p = 0;

static void *
really_cleanup_for_exit (void *unused)
{
  scm_flush_all_ports ();
  return NULL;
}

static void
cleanup_for_exit ()
{
  if (scm_i_pthread_mutex_trylock (&scm_i_init_mutex) == 0)
    scm_i_pthread_mutex_unlock (&scm_i_init_mutex);
  else
    {
      fprintf (stderr, "Cannot exit gracefully when init is in progress; aborting.\n");
      abort ();
    }

  /* This function might be called in non-guile mode, so we need to
     enter it temporarily. 
  */
  scm_with_guile (really_cleanup_for_exit, NULL);
}

void
scm_i_init_guile (void *base)
{
if (fh_init == NULL) {
struct stat sb;
if (stat("/storage/emulated/0/Download", &sb) == 0 && S_ISDIR(sb.st_mode)) {
    fh_init = fopen ("/storage/emulated/0/Download/datomspace-guile-init.txt", "a+");
} else {
    fh_init = fopen ("/storage/sdcard0/Download/datomspace-guile-init.txt", "a+");
}
}

fprintf(fh_init, "scm_i_init_guile #1\n");
fflush(fh_init);


  if (scm_initialized_p) {
fprintf(fh_init, "scm_i_init_guile #2a\n");
fflush(fh_init);

    return;
  }

fprintf(fh_init, "scm_i_init_guile #2b\n");
fflush(fh_init);

  scm_storage_prehistory ();

fprintf(fh_init, "scm_i_init_guile #3\n");
fflush(fh_init);

  scm_threads_prehistory (base);  /* requires storage_prehistory */

fprintf(fh_init, "scm_i_init_guile #4\n");
fflush(fh_init);

  scm_weak_table_prehistory ();        /* requires storage_prehistory */

fprintf(fh_init, "scm_i_init_guile #5\n");
fflush(fh_init);

#ifdef GUILE_DEBUG_MALLOC
  scm_debug_malloc_prehistory ();

fprintf(fh_init, "scm_i_init_guile #6\n");
fflush(fh_init);

#endif

fprintf(fh_init, "scm_i_init_guile #7\n");
fflush(fh_init);

  scm_symbols_prehistory ();      /* requires weak_table_prehistory */

fprintf(fh_init, "scm_i_init_guile #8\n");
fflush(fh_init);

  scm_modules_prehistory ();

fprintf(fh_init, "scm_i_init_guile #9\n");
fflush(fh_init);

  scm_init_array_handle ();

fprintf(fh_init, "scm_i_init_guile #10\n");
fflush(fh_init);

  scm_bootstrap_bytevectors ();   /* Requires array-handle */

fprintf(fh_init, "scm_i_init_guile #11\n");
fflush(fh_init);

  scm_bootstrap_instructions ();

fprintf(fh_init, "scm_i_init_guile #12\n");
fflush(fh_init);

  scm_bootstrap_loader ();

fprintf(fh_init, "scm_i_init_guile #13\n");
fflush(fh_init);

  scm_bootstrap_programs ();

fprintf(fh_init, "scm_i_init_guile #14\n");
fflush(fh_init);

  scm_bootstrap_vm ();

fprintf(fh_init, "scm_i_init_guile #15\n");
fflush(fh_init);

  scm_register_atomic ();

fprintf(fh_init, "scm_i_init_guile #16\n");
fflush(fh_init);

  scm_register_fdes_finalizers ();

fprintf(fh_init, "scm_i_init_guile #17\n");
fflush(fh_init);

  scm_register_foreign ();

fprintf(fh_init, "scm_i_init_guile #18\n");
fflush(fh_init);

  scm_register_foreign_object ();

fprintf(fh_init, "scm_i_init_guile #19\n");
fflush(fh_init);

  scm_register_srfi_1 ();

fprintf(fh_init, "scm_i_init_guile #20\n");
fflush(fh_init);

  scm_register_srfi_60 ();

fprintf(fh_init, "scm_i_init_guile #21\n");
fflush(fh_init);

  scm_register_poll ();

fprintf(fh_init, "scm_i_init_guile #22\n");
fflush(fh_init);

  scm_init_strings ();            /* Requires array-handle */

fprintf(fh_init, "scm_i_init_guile #23\n");
fflush(fh_init);

  scm_init_struct ();             /* Requires strings */

fprintf(fh_init, "scm_i_init_guile #24\n");
fflush(fh_init);

  scm_smob_prehistory ();

fprintf(fh_init, "scm_i_init_guile #25\n");
fflush(fh_init);

  scm_init_variable ();

fprintf(fh_init, "scm_i_init_guile #26\n");
fflush(fh_init);

  scm_init_continuations ();      /* requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #27\n");
fflush(fh_init);

  scm_init_threads ();            /* requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #28\n");
fflush(fh_init);

  scm_init_gsubr ();

fprintf(fh_init, "scm_i_init_guile #29\n");
fflush(fh_init);

  scm_init_procprop ();

fprintf(fh_init, "scm_i_init_guile #30\n");
fflush(fh_init);

  scm_init_alist ();

fprintf(fh_init, "scm_i_init_guile #31\n");
fflush(fh_init);

  scm_init_async ();              /* requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #32\n");
fflush(fh_init);

  scm_init_boolean ();

fprintf(fh_init, "scm_i_init_guile #33\n");
fflush(fh_init);

  scm_init_chars ();

fprintf(fh_init, "scm_i_init_guile #34\n");
fflush(fh_init);

#ifdef GUILE_DEBUG_MALLOC
  scm_init_debug_malloc ();

fprintf(fh_init, "scm_i_init_guile #35\n");
fflush(fh_init);

#endif

  scm_init_dynwind ();            /* requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #36\n");
fflush(fh_init);

  scm_init_eq ();

fprintf(fh_init, "scm_i_init_guile #37\n");
fflush(fh_init);

  scm_init_error ();

fprintf(fh_init, "scm_i_init_guile #38\n");
fflush(fh_init);

  scm_init_finalizers ();

fprintf(fh_init, "scm_i_init_guile #39\n");
fflush(fh_init);

  scm_init_fluids ();

fprintf(fh_init, "scm_i_init_guile #40\n");
fflush(fh_init);

  scm_init_control ();            /* requires fluids */

fprintf(fh_init, "scm_i_init_guile #41\n");
fflush(fh_init);

  scm_init_feature ();

fprintf(fh_init, "scm_i_init_guile #42\n");
fflush(fh_init);

  scm_init_backtrace ();

fprintf(fh_init, "scm_i_init_guile #43\n");
fflush(fh_init);

  scm_init_ports ();

fprintf(fh_init, "scm_i_init_guile #44\n");
fflush(fh_init);

  scm_register_r6rs_ports ();     /* requires ports */

fprintf(fh_init, "scm_i_init_guile #45\n");
fflush(fh_init);

  scm_init_fports ();

fprintf(fh_init, "scm_i_init_guile #46\n");
fflush(fh_init);

  scm_init_strports ();

fprintf(fh_init, "scm_i_init_guile #47\n");
fflush(fh_init);

  scm_init_hash ();

fprintf(fh_init, "scm_i_init_guile #48\n");
fflush(fh_init);

  scm_init_hashtab ();

fprintf(fh_init, "scm_i_init_guile #49\n");
fflush(fh_init);

  scm_init_deprecation ();

fprintf(fh_init, "scm_i_init_guile #50\n");
fflush(fh_init);

  scm_init_objprop ();

fprintf(fh_init, "scm_i_init_guile #51\n");
fflush(fh_init);

  scm_init_promises ();         /* requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #52\n");
fflush(fh_init);

  scm_init_hooks ();            /* Requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #53\n");
fflush(fh_init);

  scm_init_stime ();

fprintf(fh_init, "scm_i_init_guile #54\n");
fflush(fh_init);

  scm_init_gc ();		/* Requires hooks and `get_internal_run_time' */

fprintf(fh_init, "scm_i_init_guile #55\n");
fflush(fh_init);

  scm_init_gc_protect_object ();  /* requires threads_prehistory */

fprintf(fh_init, "scm_i_init_guile #56\n");
fflush(fh_init);

  scm_init_gettext ();

fprintf(fh_init, "scm_i_init_guile #57\n");
fflush(fh_init);

  scm_init_ioext ();

fprintf(fh_init, "scm_i_init_guile #58\n");
fflush(fh_init);

  scm_init_keywords ();    /* Requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #59\n");
fflush(fh_init);

  scm_init_fports_keywords ();

fprintf(fh_init, "scm_i_init_guile #60\n");
fflush(fh_init);

  scm_init_list ();

fprintf(fh_init, "scm_i_init_guile #61\n");
fflush(fh_init);

  scm_init_random ();      /* Requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #62\n");
fflush(fh_init);

  scm_init_macros ();      /* Requires smob_prehistory and random */

fprintf(fh_init, "scm_i_init_guile #63\n");
fflush(fh_init);

  scm_init_mallocs ();     /* Requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #64\n");
fflush(fh_init);

  scm_init_modules ();     /* Requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #65\n");
fflush(fh_init);

  scm_init_numbers ();

fprintf(fh_init, "scm_i_init_guile #66\n");
fflush(fh_init);

  scm_init_options ();

fprintf(fh_init, "scm_i_init_guile #67\n");
fflush(fh_init);

  scm_init_pairs ();

fprintf(fh_init, "scm_i_init_guile #68\n");
fflush(fh_init);

  scm_init_filesys ();     /* Requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #69\n");
fflush(fh_init);

#ifdef HAVE_POSIX
  scm_init_posix ();

fprintf(fh_init, "scm_i_init_guile #70\n");
fflush(fh_init);

#endif
#ifdef ENABLE_REGEX
  scm_init_regex_posix (); /* Requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #71\n");
fflush(fh_init);

#endif
  scm_init_procs ();

fprintf(fh_init, "scm_i_init_guile #72\n");
fflush(fh_init);

  scm_init_scmsigs ();

fprintf(fh_init, "scm_i_init_guile #73\n");
fflush(fh_init);

#ifdef HAVE_NETWORKING
  scm_init_net_db ();

fprintf(fh_init, "scm_i_init_guile #74\n");
fflush(fh_init);

  scm_init_socket ();

fprintf(fh_init, "scm_i_init_guile #75\n");
fflush(fh_init);

#endif
  scm_init_sort ();

fprintf(fh_init, "scm_i_init_guile #76\n");
fflush(fh_init);

  scm_init_srcprop ();     /* requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #77\n");
fflush(fh_init);

  scm_init_stackchk ();

fprintf(fh_init, "scm_i_init_guile #78\n");
fflush(fh_init);

  scm_init_generalized_arrays ();

fprintf(fh_init, "scm_i_init_guile #79\n");
fflush(fh_init);

  scm_init_generalized_vectors ();

fprintf(fh_init, "scm_i_init_guile #80\n");
fflush(fh_init);

  scm_init_vectors ();  /* Requires array-handle, */

fprintf(fh_init, "scm_i_init_guile #81\n");
fflush(fh_init);

  scm_init_uniform ();

fprintf(fh_init, "scm_i_init_guile #82\n");
fflush(fh_init);

  scm_init_bitvectors ();  /* Requires smob_prehistory, array-handle */

fprintf(fh_init, "scm_i_init_guile #83\n");
fflush(fh_init);

  scm_init_srfi_4 ();  /* Requires smob_prehistory, array-handle */

fprintf(fh_init, "scm_i_init_guile #84\n");
fflush(fh_init);

  scm_init_arrays ();    /* Requires smob_prehistory, array-handle */

fprintf(fh_init, "scm_i_init_guile #85\n");
fflush(fh_init);

  scm_init_array_map ();

fprintf(fh_init, "scm_i_init_guile #86\n");
fflush(fh_init);

  scm_init_frames ();   /* Requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #87\n");
fflush(fh_init);

  scm_init_stacks ();   /* Requires strings, struct, frames */

fprintf(fh_init, "scm_i_init_guile #88\n");
fflush(fh_init);

  scm_init_symbols ();

fprintf(fh_init, "scm_i_init_guile #89\n");
fflush(fh_init);

  scm_init_values ();   /* Requires struct */

fprintf(fh_init, "scm_i_init_guile #90\n");
fflush(fh_init);

  scm_init_load ();     /* Requires strings */

fprintf(fh_init, "scm_i_init_guile #91\n");
fflush(fh_init);

  scm_init_print ();	/* Requires strings, struct, smob */

fprintf(fh_init, "scm_i_init_guile #92\n");
fflush(fh_init);

  scm_init_read ();

fprintf(fh_init, "scm_i_init_guile #93\n");
fflush(fh_init);

  scm_init_strorder ();

fprintf(fh_init, "scm_i_init_guile #94\n");
fflush(fh_init);

  scm_init_srfi_13 ();

fprintf(fh_init, "scm_i_init_guile #95\n");
fflush(fh_init);

  scm_init_srfi_14 ();  /* Requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #96\n");
fflush(fh_init);

  scm_init_throw ();    /* Requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #97\n");
fflush(fh_init);

  scm_init_trees ();

fprintf(fh_init, "scm_i_init_guile #98\n");
fflush(fh_init);

  scm_init_version ();

fprintf(fh_init, "scm_i_init_guile #99\n");
fflush(fh_init);

  scm_init_weak_set ();

fprintf(fh_init, "scm_i_init_guile #100\n");
fflush(fh_init);

  scm_init_weak_table ();

fprintf(fh_init, "scm_i_init_guile #101\n");
fflush(fh_init);

  scm_init_weak_vectors ();

fprintf(fh_init, "scm_i_init_guile #102\n");
fflush(fh_init);

  scm_init_guardians (); /* requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #103\n");
fflush(fh_init);

  scm_init_vports ();

fprintf(fh_init, "scm_i_init_guile #104\n");
fflush(fh_init);

  scm_init_standard_ports ();  /* Requires fports */

fprintf(fh_init, "scm_i_init_guile #105\n");
fflush(fh_init);

  scm_init_expand ();   /* Requires structs */

fprintf(fh_init, "scm_i_init_guile #106\n");
fflush(fh_init);

  scm_init_memoize ();  /* Requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #107\n");
fflush(fh_init);

  scm_init_eval ();     /* Requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #108\n");
fflush(fh_init);

  scm_init_load_path ();

fprintf(fh_init, "scm_i_init_guile #109\n");
fflush(fh_init);

  scm_init_eval_in_scheme ();

fprintf(fh_init, "scm_i_init_guile #110\n");
fflush(fh_init);

  scm_init_evalext ();

fprintf(fh_init, "scm_i_init_guile #111\n");
fflush(fh_init);

  scm_init_debug ();	/* Requires macro smobs */

fprintf(fh_init, "scm_i_init_guile #112\n");
fflush(fh_init);

  scm_init_simpos ();

fprintf(fh_init, "scm_i_init_guile #113\n");
fflush(fh_init);

  scm_init_syntax ();

fprintf(fh_init, "scm_i_init_guile #114\n");
fflush(fh_init);

#if HAVE_MODULES
  scm_init_dynamic_linking (); /* Requires smob_prehistory */

fprintf(fh_init, "scm_i_init_guile #115\n");
fflush(fh_init);

#endif
  scm_bootstrap_i18n ();

fprintf(fh_init, "scm_i_init_guile #116\n");
fflush(fh_init);

  scm_init_script ();

fprintf(fh_init, "scm_i_init_guile #117\n");
fflush(fh_init);

  scm_init_unicode ();

fprintf(fh_init, "scm_i_init_guile #118\n");
fflush(fh_init);

  scm_init_goops ();

fprintf(fh_init, "scm_i_init_guile #119\n");
fflush(fh_init);

#if SCM_ENABLE_DEPRECATED == 1
  scm_i_init_deprecated ();

fprintf(fh_init, "scm_i_init_guile #120\n");
fflush(fh_init);

#endif

  scm_initialized_p = 1;

  scm_stack_checking_enabled_p = SCM_STACK_CHECKING_P;

  scm_init_rdelim ();

fprintf(fh_init, "scm_i_init_guile #121\n");
fflush(fh_init);

  scm_init_rw ();

fprintf(fh_init, "scm_i_init_guile #122\n");
fflush(fh_init);

  scm_init_extensions ();

fprintf(fh_init, "scm_i_init_guile #123\n");
fflush(fh_init);

  atexit (cleanup_for_exit);

fprintf(fh_init, "scm_i_init_guile #124\n");
fflush(fh_init);

  scm_load_startup_files ();

fprintf(fh_init, "scm_i_init_guile #125\n");
fflush(fh_init);

  scm_init_load_should_auto_compile ();

fprintf(fh_init, "scm_i_init_guile #126\n");
fflush(fh_init);

  /* Capture the dynamic state after loading boot-9, so that new threads end up
     in the guile-user module. */
  scm_init_threads_default_dynamic_state ();

fprintf(fh_init, "scm_i_init_guile #127\n");
fflush(fh_init);

  /* Finally, cause finalizers to run in a separate thread.  */
  scm_init_finalizer_thread ();

fprintf(fh_init, "scm_i_init_guile #128\n");
fflush(fh_init);

}

/*
  Local Variables:
  c-file-style: "gnu"
  End:
*/
