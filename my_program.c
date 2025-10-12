#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/stat.h>

/*
 * my_program: CPU-bound vs IO-bound workload generator for scheduling labs
 *
 * Usage:
 *   ./my_program [mode] [seconds] [label]
 *     mode:    "cpu" (default) or "io"
 *     label:   optional tag shown in logs (e.g., CFS/FIFO/RR)
 *     seconds: runtime in seconds (default: 20)
 *
 * Behavior:
 *   - Prints "loops per second" so students can see CPU-heavy vs IO-heavy differences.
 *   - "cpu" mode performs floating-point work in a tight loop.
 *   - "io"  mode performs the SAME floating-point work as "cpu" for fairness,
 *     then writes the NEWLY computed results (elapsed_ms,x) to a disk-backed file
 *     at /var/tmp/io_test_<PID>.csv with synchronous semantics to emphasize I/O cost.
 */

static const char* policy_name(int pol) {
    switch (pol) {
        case SCHED_OTHER: return "SCHED_OTHER (CFS)";
        case SCHED_FIFO:  return "SCHED_FIFO";
        case SCHED_RR:    return "SCHED_RR";
#ifdef SCHED_BATCH
        case SCHED_BATCH: return "SCHED_BATCH";
#endif
#ifdef SCHED_IDLE
        case SCHED_IDLE:  return "SCHED_IDLE";
#endif
        default:          return "UNKNOWN";
    }
}

static long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000L + tv.tv_usec / 1000;
}

/* Do a chunk of floating point work.
 * Returns the updated accumulator.
 * Using volatile prevents over-optimization.
 */
static inline double do_cpu_chunk(double acc, int iters) {
    volatile double x = acc;
    for (int i = 0; i < iters; ++i) {
        x += 3.141592653589793 * 2.718281828459045;
        if (x > 1e12) x = 0.0;
    }
    return x;
}

/* Bind current process to a specific core.
 * core_id : 0 ~ (nproc-1)
 * return 0 on success, -1 on error
 */
int bind_to_cores(int cores[], int n) {
    cpu_set_t mask;
    CPU_ZERO(&mask);

    for (int i = 0; i < n; i++) {
        CPU_SET(cores[i], &mask);
    }

    if (sched_setaffinity(0, sizeof(mask), &mask) == -1) {
        perror("sched_setaffinity");
        return -1;
    }
    return 0;
}

int main(int argc, char* argv[]) {

    const char* mode = "cpu";
    int seconds = 20;
    const char* label = "";
    
    if (argc >= 2) mode = argv[1];
    if (argc >= 3) label = argv[2];
    if (argc >= 4) seconds = atoi(argv[3]);
    if (seconds <= 0) seconds = 20;

    // Set program name
    char  program_name[20] = "my_program";
    if (strlen(label)) snprintf(program_name, sizeof(program_name), "my_program(%s)", label);

    // Bind to single CPU
    int cores[] = {0};
    bind_to_cores(cores, 1);


    // Identify current scheduling policy & RT priority
    int pol = sched_getscheduler(0);
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    (void)sched_getparam(0, &sp);

    pid_t pid = getpid();
    printf("[%s] PID=%d, policy=%s, rt_priority=%d, mode=%s, seconds=%d, label=%s\n",
           program_name, pid, policy_name(pol), sp.sched_priority, mode, seconds, label);
    fflush(stdout);

    const long start_ms = now_ms();
    long last_print_sec = -1;     // last "elapsed_sec" printed
    unsigned long loops_total = 0;
    unsigned long loops_this_sec = 0;
    const int CPU_ITERS = 1000000; // cost per loop (same for cpu and io)

    if (strcmp(mode, "io") == 0) {
        // Open a disk-backed path (avoid tmpfs) and enforce data sync.
        char path[256];
        snprintf(path, sizeof(path), "/var/tmp/io_test_%d.csv", (int)pid);
        int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC | O_DSYNC, 0644);
        if (fd < 0) {
            fprintf(stderr, "[%s] PID=%d, ERROR: open(%s): %s\n", 
                            program_name, pid, path, strerror(errno));
            return 1;
        }
        FILE* f = fdopen(fd, "w");
        if (!f) {
            fprintf(stderr, "[%s] PID=%d, ERROR: fdopen(%s): %s\n", 
                            program_name, pid, path, strerror(errno));
            close(fd);
            return 1;
        }
        // CSV header
        fprintf(f, "elapsed_ms,x\n");
        fflush(f);

        double acc = 0.0;
        long long bytes_written = 0;
        const long long MAX_FILE_BYTES = 256LL * 1024 * 1024;

        while (1) {
            long elapsed_ms = now_ms() - start_ms;
            int  elapsed_sec = (int)(elapsed_ms / 1000);

            // SAME CPU WORK as "cpu" mode
            acc = do_cpu_chunk(acc, CPU_ITERS);

            // Write NEWLY computed result (elapsed_ms, acc) as CSV
            int n = fprintf(f, "%ld,%.6f\n", elapsed_ms, acc);
            if (n < 0) {
                fprintf(stderr, "[%s] PID=%d, WARN: fprintf failed: %s\n", 
                                program_name, pid, strerror(errno));
            }
            fflush(f);
            (void)fdatasync(fd); // explicit sync for teaching clarity

            bytes_written += n > 0 ? n : 0;
            if (bytes_written >= MAX_FILE_BYTES) {
                // Rewind to avoid unbounded growth (rewrite header too)
                freopen(path, "w", f);
                fprintf(f, "elapsed_ms,x\n");
                fflush(f);
                (void)fdatasync(fd);
                bytes_written = 0;
            }

            loops_this_sec++;

            if (elapsed_sec != last_print_sec) {
                if (last_print_sec >= 0) {
                    printf("[%s] PID=%d, mode=%s,sec=%ld, loops=%lu\n", 
                            program_name, pid, mode, last_print_sec, loops_this_sec);
                    fflush(stdout);
                    loops_total += loops_this_sec;
                    loops_this_sec = 0;
                }
                last_print_sec = elapsed_sec;
            }

            if (elapsed_sec >= seconds) break;
        }

        fclose(f); // also closes fd
        // Leave the CSV for inspection; uncomment to delete:
        // unlink(path);

    } else {
        // CPU-bound loop (no file I/O). Same CPU work per loop as in IO mode.
        double acc = 0.0;
        while (1) {
            long elapsed_ms = now_ms() - start_ms;
            int  elapsed_sec = (int)(elapsed_ms / 1000);

            acc = do_cpu_chunk(acc, CPU_ITERS);
            loops_this_sec++;

            if (elapsed_sec != last_print_sec) {
                if (last_print_sec >= 0) {
                    printf("[%s] PID=%d, mode=%s, sec=%ld, loops=%lu\n", 
                            program_name, pid, mode, last_print_sec, loops_this_sec);
                    fflush(stdout);
                    loops_total += loops_this_sec;
                    loops_this_sec = 0;
                }
                last_print_sec = elapsed_sec;
            }
            if (elapsed_sec >= seconds) break;
        }
    }

    // Final partial second print
    int final_sec = (int)((now_ms() - start_ms) / 1000);
    if (loops_this_sec > 0) {
        printf("[%s] PID=%d, sec=%d, loops=%lu\n", 
                program_name, pid, final_sec, loops_this_sec);
        fflush(stdout);
    }

    fprintf(stderr, "[%s] PID=%d, loops_in_total=%lu\n", 
                    program_name, pid, loops_total);
    fflush(stdout);
    printf("[%s] PID=%d, done\n", program_name, pid);
    return 0;
}
