#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <mach-o/dyld.h>
#include <time.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    // Create log file
    char log_path[1024];
    snprintf(log_path, sizeof(log_path), "/tmp/orkid_launcher_%d.log", getpid());
    FILE *log = fopen(log_path, "w");
    
    // Log timestamp
    time_t now = time(NULL);
    fprintf(log, "=== Orkid Launcher Wrapper Log ===\n");
    fprintf(log, "Time: %s", ctime(&now));
    fprintf(log, "PID: %d\n", getpid());
    fprintf(log, "PPID: %d\n", getppid());
    
    // Log arguments
    fprintf(log, "argc: %d\n", argc);
    for (int i = 0; i < argc; i++) {
        fprintf(log, "argv[%d]: %s\n", i, argv[i]);
    }
    
    // Log environment
    fprintf(log, "\nEnvironment:\n");
    fprintf(log, "PATH: %s\n", getenv("PATH") ? getenv("PATH") : "(null)");
    fprintf(log, "HOME: %s\n", getenv("HOME") ? getenv("HOME") : "(null)");
    fprintf(log, "PWD: %s\n", getenv("PWD") ? getenv("PWD") : "(null)");
    fprintf(log, "TMPDIR: %s\n", getenv("TMPDIR") ? getenv("TMPDIR") : "(null)");
    
    // Get the directory containing this executable
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) {
        fprintf(log, "ERROR: _NSGetExecutablePath failed\n");
        fclose(log);
        return 1;
    }
    fprintf(log, "\nExecutable path: %s\n", path);
    
    // Make a copy for dirname (which may modify the string)
    char path_copy[1024];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    // Get directory name
    char *dir = dirname(path_copy);
    fprintf(log, "Directory: %s\n", dir);
    
    // Build path to the shell script
    char script_path[1024];
    snprintf(script_path, sizeof(script_path), "%s/launcher.sh", dir);
    fprintf(log, "Script path: %s\n", script_path);
    
    // Check if script exists
    if (access(script_path, F_OK) != 0) {
        fprintf(log, "ERROR: Script does not exist at %s\n", script_path);
        fprintf(log, "errno: %d (%s)\n", errno, strerror(errno));
        fclose(log);
        return 1;
    }
    
    if (access(script_path, X_OK) != 0) {
        fprintf(log, "ERROR: Script is not executable at %s\n", script_path);
        fprintf(log, "errno: %d (%s)\n", errno, strerror(errno));
        fclose(log);
        return 1;
    }
    
    fprintf(log, "\nExecuting: /bin/bash %s\n", script_path);
    fflush(log);
    fclose(log);
    
    // Execute the shell script
    execl("/bin/bash", "bash", script_path, NULL);
    
    // If we get here, exec failed - reopen log to record error
    log = fopen(log_path, "a");
    fprintf(log, "ERROR: execl failed\n");
    fprintf(log, "errno: %d (%s)\n", errno, strerror(errno));
    fclose(log);
    
    return 1;
}