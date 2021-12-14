#pragma comment(copyright, " © Copyright Rocket Software, Inc. 2016, 2020 ")

/*
pushd $BASE/devel-dist/bin
/bin/xlc -qlanglvl=extc99 -qgonum -o xlc $PORTS/src/xlc.c
rm xlc.o
cp xlc xlc++
cp xlc xlc++_echocmd
cp xlc xlc_test
cp xlc xlc_test_debug
cp xlc xlc_echocmd
cp xlc xlc_echocmd_debug
popd
 */

/*
$BASE/dist/bin/xlc_test_debug conftest.c -o conftest

$BASE/dist/bin/xlc_test_debug -q64  -qlanglvl=extc99 -qnocse -L/u/pdharr/ports/lib -L/rsusr/ported/lib -L/rsusr/local/lib -L/lib -L/usr/lpp/tcpip/X11R66/lib  -D_LARGEFILE64_SOURCE=1 -DNO_STRERROR -DNO_vsnprintf -I. -laa -c -o example.o test/example.c -Llib1 -L lib2 x.y -lqq

*/

#define _OPEN_SYS_FILE_EXT 1
#define _ALL_SOURCE
#define _POSIX_SOURCE
#include <unistd.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>

/* Max total length of a pathname appears to be 1024, with the maximum length
 * of any individual component (file or folder name) being 255. */
#define SAFE_PATH_LEN 1024

extern char **environ;

pid_t child_pid = 0;
int child_status;

void sigchld_handler(int signal_code, siginfo_t *info, void *context)
{
  child_pid = info->si_pid;
  child_status = info->si_status;
}

void establish_handlers(void)
{
  struct sigaction a;
  sigemptyset(&a.sa_mask);
  a.sa_flags = 0;
  a.sa_sigaction = 0;
  a.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &a, 0);
  sigemptyset(&a.sa_mask);
  a.sa_flags = SA_SIGINFO;
  a.sa_sigaction = sigchld_handler;
  sigaction(SIGCHLD, &a, 0);
}

int debug = 0;

void tag_binary_file(char *file)
{
  attrib_t attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.att_filetagchg = 1;
  attrs.att_filetag.ft_txtflag = 0;
  attrs.att_filetag.ft_ccsid = FT_BINARY;
  errno = 0;
  int r = __chattr(file, &attrs, sizeof(attrs));
  if (debug) {
    printf("__chattr: file=%s, ret=%d, errno=%d (%s)\n", file, r, errno, strerror(errno));
  }
}

void tag_text_file(char *file, int ccsid)
{
  attrib_t attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.att_filetagchg = 1;
  attrs.att_filetag.ft_txtflag = 1;
  attrs.att_filetag.ft_ccsid = ccsid;
  errno = 0;
  int r = __chattr(file, &attrs, sizeof(attrs));
  if (debug) {
    printf("__chattr: file=%s, ret=%d, errno=%d (%s)\n", file, r, errno, strerror(errno));
  }
}

/* args[low + (i + count) % n] = args[low + i] for i in (0..n-1) */
void rotate_args(char **args, char **work, int low, int n, int count)
{
  if (debug) printf("rotate: low=%d, n=%d, count=%d\n", low, n, count);
  for (int i=0; i<n; i++) {
    work[i] = args[low+i];
  }
  for (int i_old = 0; i_old < n; i_old++) {
    int i_new = (i_old + count) % n;
    if (i_new < 0) i_new += n;
    args[low + i_new] = work[i_old];
    if (debug) printf("new=%d old=%d value=%s\n", low+i_new, low+i_old, args[low + i_new]);
  }
}

void copy_stdin_to_file_and_tag(char *file)
{
  /* if we need to autodetect, the most likely first chars are '#', '/', ' ', and '\n' */
  FILE *out = fopen(file, "wt");
  char line[1024];
  line[1023] = 0;
  while (fgets(line, sizeof(line)-1, stdin)) {
    fputs(line, out);
  }
  fclose(out);
  tag_text_file(file, 1047);
}

void prepend_lib_to_env(const char *env_name, const char *val_to_prepend) {
  char *old, new_val[SAFE_PATH_LEN];
  int rc;
  old = getenv(env_name);
  if (old == NULL || old[0] == '\0') {
    /* Need to have /lib and /usr/lib in the search path. */
    snprintf(new_val, sizeof new_val, "%s /lib /usr/lib", val_to_prepend);
  } else {
    snprintf(new_val, sizeof new_val, "%s %s", val_to_prepend, old);
  }
  errno = 0;
  rc = setenv(env_name, new_val, 1);
  if (rc) {
    fprintf(stderr, "failed to set \"%s\" to \"%s\", errno=%d (%s)\n",
            env_name, new_val, errno, (strerror(errno)));
  }
}

#define option_has_argument(opt) (opt=='D'||opt=='L'||opt=='I'||opt=='l'||opt=='o'||opt=='F'||opt=='u'||opt=='W')

int main(int argc, char **argv)
{
  establish_handlers();
  char cwd[SAFE_PATH_LEN];
  getcwd(cwd, sizeof(cwd));
  char argv0_dir[SAFE_PATH_LEN];
  char argv0[SAFE_PATH_LEN];
  char unixcompat_lib[SAFE_PATH_LEN];  /* libunixcompat library dir */
  char *slash = strrchr(argv[0], '/');
  if (argv[0][0]=='/') {
    /* path is absolute, like /usr/local/bin/xlc */
    snprintf(argv0_dir, sizeof(argv0_dir), "%.*s", slash-argv[0], argv[0]);
    if (slash - argv[0] > 0) {
      /* If xlc-wrapper is installed into $PREFIX/something, use $PREFIX/unixcompat */
      const char *second_slash = strrchr(argv0_dir, '/');
      snprintf(unixcompat_lib, sizeof(unixcompat_lib), "%.*s/unixcompat",
               second_slash - argv0_dir, argv0_dir);
    } else {
      /* xlc is installed as /xlc. Use /unixcompat in this case. */
      strcpy(unixcompat_lib, "/unixcompat");
    }
  } else if (slash) {
    snprintf(argv0_dir, sizeof(argv0_dir), "%s/%.*s", cwd, slash-argv[0], argv[0]);
    const char *second_slash = strrchr(argv0_dir, '/');
    snprintf(unixcompat_lib, sizeof(unixcompat_lib), "%.*s/unixcompat",
             second_slash - argv0_dir, argv0_dir);
  } else {
    /* Fall back to using PATH */
    char *path = getenv("PATH");
    if (path == NULL) path = "";
    int pathlen = strlen(path);
    int success = 0;
    char *dir = path;
    while (*dir) {
      char *sep = strchr(dir, ':');
      char *dir_end = sep ? sep : path+pathlen;
      if (dir == dir_end)
        snprintf(argv0_dir, sizeof(argv0_dir), "%s", cwd);
      else if (dir[0]!='/')
        snprintf(argv0_dir, sizeof(argv0_dir), "%s/%.*s", cwd, dir_end-dir, dir);
      else
        snprintf(argv0_dir, sizeof(argv0_dir), "%.*s", dir_end-dir, dir);
      snprintf(argv0, sizeof(argv0), "%s/%s", argv0_dir, argv[0]);
      struct stat finfo;
      if (access(argv0, X_OK) == 0 && stat(argv0, &finfo) == 0 && S_ISREG(finfo.st_mode)) {
        const char *second_slash = strrchr(argv0_dir, '/');
        snprintf(unixcompat_lib, sizeof(unixcompat_lib), "%.*s/unixcompat",
                 second_slash - argv0_dir, argv0_dir);
        success = 1;
        break;
      }
      dir = sep ? sep+1 : "";
    }
  }
  char *name = slash ? slash+1 : argv[0];
  debug = strstr(name, "debug") != 0;
  char *fullname = strstr(argv[0], "++") ? "/bin/xlc++" : "/bin/xlc";
  char **args = (char **)calloc(argc+4, sizeof(char *));
  char **work = (char **)calloc(argc+4, sizeof(char *));
  args[0] = fullname;
  args[1] = "-F";
  char cfg_file[SAFE_PATH_LEN];
  char *q;
  if ((q = getenv("DISABLE_UNIXCOMPAT")) && q[0] != '\0') {
    snprintf(cfg_file, sizeof(cfg_file), "%s/xlc.cfg", argv0_dir);
  } else {
    snprintf(cfg_file, sizeof(cfg_file), "%s/xlc_unixcompat.cfg", unixcompat_lib);
    /* To get the library to always be in (and at the front of) the search path,
     * we need to prepend the unixcompat folder to the binder's _{prog}_LIBDIRS
     * variable. In our config, the binder is invoked through either c89 or cxx,
     * so we need to set both variables. While we're at it, might as well do it
     * for cc, just in case.  */
    prepend_lib_to_env("_C89_LIBDIRS", unixcompat_lib);
    prepend_lib_to_env("_CXX_LIBDIRS", unixcompat_lib);
    prepend_lib_to_env("_CC_LIBDIRS", unixcompat_lib);
  }
  args[2] = cfg_file;
  int has_qnooptimize = 0;
  for (int i=1; i<argc; i++) {
    if (0==strcmp(argv[i], "-qnooptimize")) {
      has_qnooptimize = 1;
      break;
    }
  }
  int found_E = 0;
  int found_c = 0;
  int found_o = 0;
  char *o_arg = NULL;
  int o_dbl = 0;
  int o_is_so = 0;
  int found_Wl = 0;
  int found_xc_ = 0;
  char temp_c_file[SAFE_PATH_LEN];
  temp_c_file[0] = 0;
  int found_file_before_o = 0;
  int dbl;
  int i_next;
  int args_offset = 2;
  for (int i=1; i<argc; i += 1+dbl) {
    char *arg = argv[i];
    int args_i = i+args_offset;
    args[args_i]=arg;
    char opt = (arg[0]=='-') ? arg[1] : 0;
    if (opt == 'M')
      dbl = ((arg[2] == 'F') || (arg[2] == 'Q')) ? 1 : 0;
    else
      dbl = (option_has_argument(opt) && (arg[2]==0)) ? 1 : 0;
    if (dbl) args[args_i+1]=argv[i+1];
    if (has_qnooptimize && 0==strcmp(arg, "-qhot")) {
      args_offset -= 1;
    }
    if (opt == 'x' && arg[2]=='c' && 0==strcmp(argv[i+1],"-")) {
      /* This is to help R's anticonf */
      found_xc_ = 1;
      dbl = 1;
      args_offset -= 1;
      snprintf(temp_c_file, sizeof(temp_c_file), "%s.c", tempnam(NULL, "xlc_xc_input"));
      args[args_i] = temp_c_file;
      copy_stdin_to_file_and_tag(temp_c_file);
    }
    if (opt == 'E') found_E = 1;
    if (opt == 'c') found_c = 1;
    if (opt == 'W' && (!dbl ? arg[2] : argv[i+1][0]) == 'l') found_Wl = 1;
    if (opt == 0) {
      int arglen = strlen(arg);
      int d = 0;
      if ((arglen > (d = 3) && 0 == strcmp(arg+arglen-d, ".so")) ||
          (arglen > (d = 4) && 0 == strcmp(arg+arglen-d, ".dll"))) {
        char *argx = strdup(arg);
        argx[arglen-d+1]='x';
        argx[arglen-d+2]=0;
        args[args_i]=argx;
      } else if ((arglen > (d = 2) && 0 == strcmp(arg+arglen-d, ".c")) ||
		 (arglen > (d = 4) && 0 == strcmp(arg+arglen-d, ".cpp"))) {
	/* this is for src/fastLm.cpp in r-rcppeigen */
	struct stat stat_s;
	int stat_rc = lstat(arg, &stat_s);
	if ((stat_rc == -1 && errno == ENOENT) ||
	    (stat_rc == 0 && S_ISLNK(stat_s.st_mode))) {
	  char *ifile = strdup(arg);
	  char *dot = strrchr(ifile, '.');
	  *(dot+1) = 'i';
	  *(dot+2) = 0;
	  stat_rc = stat(ifile, &stat_s);
	  if (stat_rc == 0) {
	    args[args_i] = ifile;
	  }
	}
      }
    }
    if (found_o) {
    } else if (opt=='o') {
      found_o = args_i;
      o_dbl = dbl;
      o_arg = dbl ? argv[i+1] : arg+2;
      int o_arg_len = strlen(o_arg);
      o_is_so = (o_arg_len > 3 && 0 == strcmp(o_arg+o_arg_len-3, ".so")) ||
        (o_arg_len > 4 && 0 == strcmp(o_arg+o_arg_len-4, ".dll"));
    } else if (found_file_before_o) {
    } else if (opt==0) {
      found_file_before_o = args_i;
    }
    if (debug) printf("i=%d, arg=%s, args_i=%d, opt=%c, dbl=%d, found_o=%d, o_dbl=%d, found_file_before_o=%d\n",
                      i, arg, args_i, opt ? opt : ' ', dbl, found_o, o_dbl, found_file_before_o);
  }
  argc += args_offset;
  if (found_o && o_is_so && !found_Wl) {
    char *XLC_LINK_DLL = getenv("XLC_LINK_DLL");
    if (XLC_LINK_DLL == NULL) XLC_LINK_DLL = "-Wl,dll";
    XLC_LINK_DLL = strdup(XLC_LINK_DLL);
    args[argc++] = XLC_LINK_DLL;
  }
  if (found_o) {
    if (found_file_before_o) {
      rotate_args(args, work, found_file_before_o, 1+found_o+o_dbl-found_file_before_o, 1+o_dbl);
      found_o = found_file_before_o;
    }
    for (int i=found_o; i<argc; i += 1+dbl){
      char *arg = args[i];
      char opt = (arg[0]=='-') ? arg[1] : 0;
      dbl = (option_has_argument(opt) && (arg[2]==0)) ? 1 : 0;
      if (debug) printf("i=%d, arg=%s, opt=%c, dbl=%d\n", i, arg, opt ? opt : ' ', dbl);
      if (opt && opt!='l' && opt!='o'){
        rotate_args(args, work, found_o, 1+i+dbl-found_o, 1+dbl);
        found_o += 1+dbl;
      }
    }
    int no_l_count = 1;
    int l_count = 0;
    for (int i=1; i<argc; i += 1+dbl) {
      char *arg = args[i];
      char opt = (arg[0]=='-') ? arg[1] : 0;
      dbl = (option_has_argument(opt) && (arg[2]==0)) ? 1 : 0;
      if (debug) printf("i=%d, arg=%s, opt=%c, dbl=%d\n", i, arg, opt ? opt : ' ', dbl);
      if (opt=='l') {
        work[l_count++] = arg;
        if (dbl) work[l_count++] = args[i+1];
      } else {
        args[no_l_count++] = arg;
        if (dbl) work[no_l_count++] = args[i+1];
      }
    }
    for (int i=0; i<l_count; i++) {
      args[no_l_count++] = work[i];
    }
  }
  int test = 0 != strstr(name, "test");
  if (test) {
    FILE *out = stdout;
    for (int i=0; args[i]; i++){
      fprintf(out, "arg[%d]=%s\n", i, args[i]);
    }
    for (int i=0; environ[i]; i++){
      fprintf(out, "environ[%d]=%s\n", i, environ[i]);
    }
    return 0;
  }
  int echocmd = 0 != strstr(name, "echocmd");
  if (echocmd) {
    FILE *out = stderr;
    for (int i=0; args[i]; i++){
      fprintf(out, "%s ", args[i]);
    }
    fprintf(out, "\n");
    fflush(out);
  }
  if (!test) {
    struct inheritance inherit = {0};
    int pid = spawn(fullname, 0, NULL, &inherit, (const char **)args, (const char **)environ);
    if (pid > 0) {
      int child_pid_or_error;
      int status;
      while (1) {
        errno = 0;
        child_pid_or_error = waitpid(pid, &status, 0);
        if (child_pid_or_error == -1 && errno == EINTR) continue;
        break;
      }
      if (child_pid_or_error > 0) {
        child_pid = child_pid_or_error;
        child_status = status;
      }
      if (debug) {
        printf("pid=%d, child_pid_or_error=%d, child_pid=%d, child_status=%X, errno=%d (%s), found_E=%d, o_arg=%s\n",
               pid, child_pid_or_error, child_pid, child_status, errno, strerror(errno), found_E, o_arg ? o_arg : "NULL");
      }
      if (child_pid > 0 && !found_E) {
        if (o_arg) {
          tag_binary_file(o_arg);
          int o_arg_len = strlen(o_arg);
          int d = 0;
          if ((o_arg_len > (d = 3) && 0 == strcmp(o_arg+o_arg_len-d, ".so")) ||
              (o_arg_len > (d = 4) && 0 == strcmp(o_arg+o_arg_len-d, ".dll"))) {
            char *o_arg_slash = strrchr(o_arg, '/');
            if (o_arg_slash) {
              char *name = o_arg_slash+1;
              int dir_len = name-o_arg;
              int name_len = o_arg_len-dir_len;
              char old_name[SAFE_PATH_LEN];
              snprintf(old_name, sizeof(old_name), "%.*s.x", name_len-d, name);
              char new_name[SAFE_PATH_LEN];
              snprintf(new_name, sizeof(new_name), "%.*s.x", o_arg_len-d, o_arg);
              int rc = rename(old_name, new_name);
              if (rc && errno == EXDEV) {
                /* here we should do a copy */
              } else if (rc)
                fprintf(stderr, "failed to rename \"%s\" to \"%s\", errno=%d (%s)\n",
                        old_name, new_name, errno, (strerror(errno)));
              else
                fprintf(stderr, "renamed \"%s\" to \"%s\"\n",
                        old_name, new_name);
            }
          }
        } else {
          for (int i=1; args[i]; i += 1+dbl) {
            char *arg = args[i];
            char opt = (arg[0]=='-') ? arg[1] : 0;
            dbl = (option_has_argument(opt) && (arg[2]==0)) ? 1 : 0;
            int len = strlen(arg);
            if (opt == 0 && 2<len && 0==strcmp((arg+len-2), ".c")) {
              arg[len-1] = 'o';
              tag_binary_file(arg);
            }
          }
        }
      }
      return (child_pid > 0 && WIFEXITED(status)) ? WEXITSTATUS(status) : -1;
    }
    return -1;
  }
}

/*
 © 2016-2020 Rocket Software, Inc. or its ffiliates. All Rights Reserved.
ROCKET SOFTWARE, INC. 
*/

