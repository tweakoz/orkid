// obt_app_launcher.c
//
// Tiny Mach-O launcher used as CFBundleExecutable for orkid-based macOS
// .app bundles produced by ork.deploy.macos.relocatable.
//
// Why this exists
// ---------------
// When CFBundleExecutable is a shell script, the kernel exec-chain runs
// /usr/bin/env (or /bin/bash) as the actual Mach-O image. macOS TCC then
// attributes the running process to com.apple.env, NOT to the bundle.
// Privacy grants (Full Disk Access, Files & Folders → Desktop, etc.)
// silently fail to bind, and any attempt to read sibling files inside a
// TCC-protected folder (like ~/Desktop) is denied at the kernel sandbox
// layer with no first-launch prompt.
//
// The only fix is to make CFBundleExecutable a real signed Mach-O binary
// that the kernel can attribute to the bundle's identity. This file is
// that binary. One pre-built copy is shipped with every orkid-based .app
// bundle; per-app configuration lives entirely in
//   Contents/Resources/launch.args
//
// What it does
// ------------
//   1. Locates itself via _NSGetExecutablePath / realpath.
//   2. Walks up to the enclosing folder of the .app, treats <enclosing>/.staging
//      as the deploy root, and exports it as $DEPLOY_ROOT to the child.
//   3. Redirects stdout/stderr to <deploy_root>/logs/<AppName>.log so debug
//      output is recoverable (LaunchServices gives .apps no TTY).
//   4. Reads Contents/Resources/launch.args (one extra argv element per line;
//      blank lines and lines beginning with '#' are ignored).
//   5. posix_spawn()s /bin/bash with obt-launch-env's path as argv[1] (rather
//      than letting the kernel walk the script's #!/usr/bin/env shebang
//      chain, which would substitute /usr/bin/env as the running image and
//      lose the bundle attribution). The launcher process stays alive as
//      the parent and waitpid()s the child. By keeping the launcher alive
//      and NOT setting POSIX_SPAWN_SETEXEC or responsibility_spawnattrs_setdisclaim,
//      the child inherits the launcher's responsible_pid — which LaunchServices
//      set to the launcher itself when launching the .app. TCC therefore
//      walks the responsibility chain back to *this* signed bundle, finds
//      our NSDesktopFolderUsageDescription / Full Disk Access grants, and
//      allows the child's reads of .staging/.
//
// One binary, byte-identical, used by every orkid-based .app across every
// project. Stable cdhash → stable TCC identity →
// the user grants Full Disk Access once per bundle id, and it persists
// across re-deploys forever.

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#define MAX_ARGS 128
#define MAX_LINE 4096
#define MAX_PATH 4096

// Walk N path components up by truncating at trailing '/'. Returns 0 on ok.
static int walk_up(char *path, int n) {
  for (int i = 0; i < n; i++) {
    char *p = strrchr(path, '/');
    if (!p) return -1;
    *p = '\0';
  }
  return 0;
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  // ---- 1. Locate ourselves -----------------------------------------------
  char exe_path[MAX_PATH];
  uint32_t sz = sizeof(exe_path);
  if (_NSGetExecutablePath(exe_path, &sz) != 0) {
    fprintf(stderr, "obt-app-launcher: _NSGetExecutablePath buffer too small\n");
    return 2;
  }

  char real_exe[MAX_PATH];
  if (!realpath(exe_path, real_exe)) {
    fprintf(stderr, "obt-app-launcher: realpath: %s\n", strerror(errno));
    return 2;
  }

  // ---- 2. Compute paths --------------------------------------------------
  // real_exe = .../Foo.app/Contents/MacOS/obt-app-launcher
  //   walk up 1 → .../Foo.app/Contents/MacOS
  //   walk up 2 → .../Foo.app/Contents
  //   walk up 3 → .../Foo.app
  //   walk up 4 → .../  (the enclosing folder of the .app)

  char enclosing[MAX_PATH];
  strncpy(enclosing, real_exe, sizeof(enclosing) - 1);
  enclosing[sizeof(enclosing) - 1] = '\0';
  if (walk_up(enclosing, 4) != 0) {
    fprintf(stderr, "obt-app-launcher: cannot resolve enclosing dir from %s\n", real_exe);
    return 2;
  }

  // Walk upward from enclosing looking for a .staging/ dir. This handles:
  //   • top-level bundles (.../MyApp.app) — enclosing IS the deploy
  //     root, first iteration hits .staging immediately
  //   • nested-utility bundles (.../MyApp Utilities/SubTool.app)
  //     — enclosing is the Utilities folder; .staging lives one level up
  //   • any deeper nesting used by future deploy layouts
  // If nothing is found, fall back to the sibling-of-enclosing path so
  // downstream error messaging stays consistent with the old single-
  // level assumption.
  char deploy_root[MAX_PATH];
  deploy_root[0] = '\0';
  {
    char cur[MAX_PATH];
    strncpy(cur, enclosing, sizeof(cur) - 1);
    cur[sizeof(cur) - 1] = '\0';
    while (cur[0] != '\0' && !(cur[0] == '/' && cur[1] == '\0')) {
      char candidate[MAX_PATH];
      snprintf(candidate, sizeof(candidate), "%s/.staging", cur);
      struct stat st;
      if (stat(candidate, &st) == 0 && S_ISDIR(st.st_mode)) {
        strncpy(deploy_root, candidate, sizeof(deploy_root) - 1);
        deploy_root[sizeof(deploy_root) - 1] = '\0';
        break;
      }
      char *slash = strrchr(cur, '/');
      if (!slash) break;
      if (slash == cur) { cur[1] = '\0'; break; }
      *slash = '\0';
    }
    if (deploy_root[0] == '\0') {
      snprintf(deploy_root, sizeof(deploy_root), "%s/.staging", enclosing);
    }
  }

  char launch_env[MAX_PATH];
  snprintf(launch_env, sizeof(launch_env), "%s/obt-launch-env", deploy_root);

  char contents_dir[MAX_PATH];
  strncpy(contents_dir, real_exe, sizeof(contents_dir) - 1);
  contents_dir[sizeof(contents_dir) - 1] = '\0';
  walk_up(contents_dir, 2);  // .../Foo.app/Contents

  char args_path[MAX_PATH];
  snprintf(args_path, sizeof(args_path), "%s/Resources/launch.args", contents_dir);

  // App name (used for log file name)
  char app_dir[MAX_PATH];
  strncpy(app_dir, real_exe, sizeof(app_dir) - 1);
  app_dir[sizeof(app_dir) - 1] = '\0';
  walk_up(app_dir, 3);  // .../Foo.app
  const char *app_base = strrchr(app_dir, '/');
  app_base = app_base ? app_base + 1 : app_dir;
  char bundle_name[MAX_PATH];
  strncpy(bundle_name, app_base, sizeof(bundle_name) - 1);
  bundle_name[sizeof(bundle_name) - 1] = '\0';
  char *dot_app = strstr(bundle_name, ".app");
  if (dot_app) *dot_app = '\0';

  // ---- 3. Redirect stdout/stderr to a log file ---------------------------
  char log_dir[MAX_PATH];
  snprintf(log_dir, sizeof(log_dir), "%s/logs", deploy_root);
  mkdir(log_dir, 0755);

  char log_path[MAX_PATH];
  snprintf(log_path, sizeof(log_path), "%s/%s.log", log_dir, bundle_name);

  int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (log_fd >= 0) {
    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);
    close(log_fd);
  }

  // Write a header marker
  time_t now = time(NULL);
  struct tm tmv;
  localtime_r(&now, &tmv);
  char tbuf[64];
  strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S %Z", &tmv);
  fprintf(stderr, "=== obt-app-launcher %s | %s ===\n", bundle_name, tbuf);
  fprintf(stderr, "    bundle:      %s\n", app_dir);
  fprintf(stderr, "    deploy_root: %s\n", deploy_root);
  fprintf(stderr, "    launch_env:  %s\n", launch_env);
  fprintf(stderr, "    args_file:   %s\n", args_path);
  fflush(stderr);

  // ---- 4. Build child argv from launch.args ------------------------------
  // We invoke /bin/bash directly with obt-launch-env as a script argument,
  // bypassing the kernel's #!/usr/bin/env shebang chain. This keeps the
  // launch chain shallow and the responsibility attribution intact.
  char *child_argv[MAX_ARGS + 3];
  int n = 0;
  child_argv[n++] = "/bin/bash";
  child_argv[n++] = launch_env;

  FILE *f = fopen(args_path, "r");
  if (f) {
    char buf[MAX_LINE];
    while (n < MAX_ARGS && fgets(buf, sizeof(buf), f)) {
      size_t len = strlen(buf);
      while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[--len] = '\0';
      }
      if (len == 0 || buf[0] == '#') continue;
      child_argv[n++] = strdup(buf);
    }
    fclose(f);
  } else {
    fprintf(stderr, "obt-app-launcher: WARNING: cannot open %s: %s "
                    "(launching with no extra args)\n",
            args_path, strerror(errno));
  }
  child_argv[n] = NULL;

  // Trace what we're about to spawn
  fprintf(stderr, "    argv:");
  for (int i = 0; i < n; i++) fprintf(stderr, " %s", child_argv[i]);
  fprintf(stderr, "\n");
  fflush(stderr);

  // ---- 5. Set DEPLOY_ROOT in environment for the child -------------------
  setenv("DEPLOY_ROOT", deploy_root, 1);

  // ---- 6. posix_spawn the child as a subprocess --------------------------
  // Critically: NO POSIX_SPAWN_SETEXEC and NO disclaim. We want the launcher
  // to stay alive as the parent so the child inherits its responsible_pid
  // (which LaunchServices already set to the launcher itself, anchoring TCC
  // to this bundle's signed identity).
  posix_spawnattr_t attr;
  posix_spawnattr_init(&attr);

  pid_t child_pid;
  int rc = posix_spawn(&child_pid, child_argv[0], NULL, &attr, child_argv, environ);
  posix_spawnattr_destroy(&attr);

  if (rc != 0) {
    fprintf(stderr, "obt-app-launcher: posix_spawn(%s) failed: %s (rc=%d)\n",
            child_argv[0], strerror(rc), rc);
    return 1;
  }

  fprintf(stderr, "    spawned child pid=%d, waiting...\n", child_pid);
  fflush(stderr);

  // Wait for the child to exit and propagate its status
  int status = 0;
  while (waitpid(child_pid, &status, 0) < 0) {
    if (errno == EINTR) continue;
    fprintf(stderr, "obt-app-launcher: waitpid failed: %s\n", strerror(errno));
    return 1;
  }

  if (WIFEXITED(status)) {
    int code = WEXITSTATUS(status);
    fprintf(stderr, "    child exited with code %d\n", code);
    return code;
  } else if (WIFSIGNALED(status)) {
    int sig = WTERMSIG(status);
    fprintf(stderr, "    child killed by signal %d\n", sig);
    return 128 + sig;
  }
  return 0;
}
