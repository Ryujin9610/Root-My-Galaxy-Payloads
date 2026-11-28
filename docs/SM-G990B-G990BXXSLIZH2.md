# SM-G990B G990BXXSLIZH2 validation

This Shizuku-required profile is hardware-validated on a Galaxy S21 FE 5G
(`r9q`, `SM-G990B`) with a locked bootloader.

| Field | Value |
| --- | --- |
| Firmware | `G990BXXSLIZH2` |
| Android | 16 / One UI 8 |
| Kernel | `5.4.289-qgki-32192773-abG990BXXSLIZH2` |
| SoC | Snapdragon 888 (`SM8350`) |
| KernelSU | v3.2.5 / version code `32525` |
| KMI asset | `android16-5.4` |

## Hardware results

- The tracefs/COW GhostLock route completed on independent clean boots.
- The app payload now selects that same validated route itself: controlled
  reclaim, the readiness-pause COW overlay, a 16 GiB PTE region, one attempt,
  and a 900-second timeout.
- The current app artifact completed three consecutive independent clean-boot
  ADB hardware retests. Releasing the old drain arrays before the target made
  pipe takeover succeed on attempt 1/3 in all three runs (pipe indices 239,
  238, and 151). Each run brought up the uid-0 daemon in `u:r:kernel:s0`, made
  SELinux permissive, restored the kernel globals, and remained stable. The
  final cleanup reboot returned the phone to enforcing mode.
- The app/Shizuku payload contract completed with exit status 0.
- Pipe takeover gets up to three attempts after a successful FOPS swap. If all
  miss, a detached keeper retains the fake FOPS backing page until reboot so
  an unrelated later file close cannot follow a freed function table.
- The exact KernelSU module passed the manual-relocation audit with no missing
  symbols, no module version entries, and no CRC mismatches.
- The generic Root My Galaxy helper staged the final `ksud`, invoked
  `late-load --ephemeral`, and verified the driver through `/dev/ksu`.
- Vendor `uname` has no Android KMI token, so `ksud` selected its sole embedded
  asset, `android16-5.4`; extracting that asset on-device reproduced SHA-256
  `927753d65ff813087ee84271c56f1e2b720a276cadebcb435c737b0767194823`.
- `PATH=/data/adb/ksu/bin:$PATH su -c id` returned:

```text
uid=0(root) gid=0(root) groups=0(root) context=u:r:kernel:s0
```

- A rebuilt Root My Galaxy APK preserved the exact helper bytes in
  `nativeLibraryDir` and displayed `KernelSU active` on-device.
- The kernel matched the official Manager certificate and crowned
  `me.weishu.kernelsu`. The stock Manager UI still labels Linux 5.4
  unsupported because its native client only scans inherited driver FDs; the
  Root My Galaxy app detects the live LKM through `/proc/modules`.

## 2026-11-23 allocator diagnostic retest

One isolated diagnostic build added sparse `slab_cache` and page-type samples
at pipe-growth counts 128, 256, 384, 512, and 640. It did not change the
reclaim order, target offsets, supported artifact, or manifest. The local
diagnostic payload SHA-256 was
`92a27b6360253823d536a6cb272d633228ead4d2e4001c56d435df257df7478e`.

On the matching Android 16 build and kernel, RMG selected the exact
`r9q-G990BXXSLIZH2` profile, verified the existing KernelSU service, and ran
exactly one exploit attempt. KASLR detection succeeded. The log then showed
controlled mm-group attempts 1 and 8 for the same base page, but no later
progress before RMG's 900-second timeout. The helper was terminated with
signal 9 and RMG reported exit code 255 at `Running kernel exploit`. No
pipe-growth snapshot lines reached the app log, so this run does not establish
whether the sampled cache ownership changed; it points to a stall before
observable pipe-growth diagnostics.

After a clean reboot, the phone returned to the expected firmware and kernel,
ADB remained uid 2000, and neither `/dev/ksu` nor `/proc/ksu` existed. KernelSU
was not loaded. This was one diagnostic attempt only; no retry was made.

## 2026-11-23 bounded mm-collector retest

A second diagnostic-only build instrumented the mm-group collector and exited
before pipe reclaim, FOPS, or root stages. Its child wait was capped at 120
seconds with 15-second progress messages, and the whole collection was capped
at 240 seconds. The local payload SHA-256 was
`47bf8645a3c9d69630c38042899578817712a72c2a1cdb079a73231e1472bfef`. RMG
reported a failed exploit status by design: the diagnostic
returns status 42 after recording its result so the app cannot proceed to
KernelSU loading.

The collector completed normally in 49.943 seconds and found one complete
order-3 mm group: 34 of 34 unique slots at `ffffff8094ba8000`. All 34
KernelSnitch children completed; their measured run times were about 0.3 to
3.4 seconds. Setup took 1 to 4 ms per candidate, `/proc/<pid>/mem` opened in
under 1 ms, and hinted page matches took 0 to 1 ms after the initial 149 ms
full scan. Neither the child timeout nor the overall scan limit was reached.

This does not reproduce the preceding 900-second stall. It shows that the
collector can complete quickly on this phone and that its ordinary workload
does not explain that earlier timeout. The exact cause of the prior stall
remains unconfirmed. No FOPS or root stage ran; after the diagnostic, the
phone remained booted as uid 2000 with no `/dev/ksu` or `/proc/ksu`. The
previous local drain-order payload was restored.

## 2026-11-23 original-payload retest

The original local drain-order payload (`cve-2026-43499-app-drain-order.so`,
SHA-256 `4d97a75d1201ed085c60a9fe78aa33b99837532ebe6a383b94d4c87e4850af73`)
was run once after a clean reboot, with battery saver off. The app used the
exact `r9q-G990BXXSLIZH2` profile and verified the KernelSU service. Tracefs
KASLR detection succeeded with slide `0x1e0000`. Controlled reclaim collected
34 mm objects, prepared the target page in 120.307 seconds, and reduced the
reported mm slab count from 39 to 38.

The readiness COW walk succeeded on its first call, and the CFI write and
read probes both returned 35 bytes. The subsequent pipe-physrw stage failed
all three attempts. Each scan found no pipe-buffer marker; the candidate
order-3 bases were `ffffff80025d0000`, `ffffff80657e8000`, and
`ffffff80965c8000`. The kmalloc cache table read matched the expected 1 KiB
cache pointer, but every candidate subpage reported `slab_cache` at `+0x18`
as zero, so the page ownership gate matched none. The failure therefore
remains in pipe-buffer-page reclaim, after readiness and CFI checks have
passed; it is not a KernelSU module-load failure.

RMG completed the run in 777.7 seconds and reported failure at the exploit
stage. No root or KernelSU driver was loaded. Because all three pipe-physrw
attempts missed, RMG started its stability keeper and marked the FOPS page as
dirty; a reboot was required and completed. The phone returned to the expected
firmware and kernel as uid 2000, with no `/dev/ksu` or `/proc/ksu`.

## 2026-11-23 pipe-growth snapshot diagnostic

A one-attempt diagnostic build added snapshots while the pipe-array reclaim
grew. It sampled allocation counts 128, 192, 193, 256, 384, 512, 640, and
768, including the existing drain-boundary samples. The run used the exact
`r9q-G990BXXSLIZH2` profile and was followed by a reboot because the payload
reported that its FOPS page was dirty. The temporary local payload was then
replaced with the original drain-order payload; its SHA-256 was verified as
`4d97a75d1201ed085c60a9fe78aa33b99837532ebe6a383b94d4c87e4850af73`.

The exploit again passed KASLR, collected a 34-object mm group, and prepared
the controlled target page in 101.344 seconds. Readiness COW and both CFI
probes succeeded. All three pipe-physrw attempts then failed to find a pipe
buffer marker. At every sampled allocation count in each attempt, the
inspected order-3 candidate's `slab_cache` field at `+0x18` remained zero on
all eight 4 KiB subpages. The `page_type` observation retained the head marker
`0xffffff7f` and `0xffffffff` on the other subpages. The cache-table read
still matched the expected normal kmalloc-1k pointer, so the zero ownership
field is not explained by a failed cache-table lookup.

This narrows the failure: the inspected candidate page has no recognized
SLUB cache owner even during intermediate pipe-array growth, rather than
losing that owner only at the final sample. It does not yet prove why the
candidate page is not associated with the expected cache, or whether the
oracle is tracking the intended physical page. No root or KernelSU driver
was loaded. The device rebooted successfully and returned to the expected
firmware and kernel; `/dev/ksu` and `/proc/ksu` remain absent. No additional
exploit attempt was made after this diagnostic.

## 2026-11-23 kernel source layout check

The completed Samsung source archive contains the full `SM-G990B_16_Opensource.zip`
tree plus a separate `G990BXXSLIZH2` update archive. The update README says to
apply its files to the `G990BXXUGIYJ1` base source. The base tree includes
`include/linux/mm_types.h`; the SLIZH2 update contains 35 files and does not
replace that header.

The base `r9q_eur_openx_defconfig` and the repository's saved SLIZH2 config
both set `CONFIG_ARM64`, `CONFIG_64BIT`, `CONFIG_MEMCG`, and `CONFIG_SLUB`.
The base `struct page` layout places `slab_cache` at `+0x18` and `page_type`
at `+0x30`; with this configuration, `struct page` is 0x40 bytes and the
compound-head word is at `+0x08`. These values match the target constants in
`target.h`, so the pipe-growth diagnostic's zero `slab_cache` readings are
not explained by an incorrect field offset. The remaining uncertainty is
whether the tracked direct-map page continues to be the intended page during
reclaim and how its compound-head metadata changes as the allocator splits or
reuses it.

## 2026-11-23 page-identity diagnostic and successful RMG run

A one-attempt build added direct-map page identity and `struct page` ownership
snapshots to the pipe reclaim diagnostic. It watched the same order-3 direct-map
page (`ffffff814dcf8000`, PFN `0x14dcf8`) through allocation counts 128, 192,
193, 256, 384, 512, 640, and 768. Its vmemmap metadata address remained
`ffffffff05173e00` throughout, so the observations refer to one page rather
than a changed candidate.

At counts through 512, the head's `slab_cache` field was zero and `page_type`
was `0xffffff7f`. By count 640, the same page showed the freed-object marker
`dead000000000100`, the expected kmalloc-1k cache pointer
`ffffff8000897600`, and `page_type=0xffffffff`; the eight subpages also pointed
back to the same compound head and showed that cache pointer. The transition
therefore occurred between the 512 and 640 allocation snapshots. At 768, the
ownership state remained consistent.

With this page-identity evidence, the pipe-buffer scan found its marker at
`ffffff814dcf8000`, index 601 (`scan=1/1/1`). The run reported successful
physical read/write, KASLR recovery, and credential transition
`uid=2000->0`. RMG recorded one successful attempt, a 5 min 36 s root time, and
displayed “KernelSU active” with the app-side check successful. The run used
profile `r9q-G990BXXSLIZH2`, payload source commit
`ca9cae33ef85f3ec042d266a20db672221073c7a`, and a diagnostic app payload with
SHA-256
`c8872ae6b3c68a75d721c165a83280069187dab867c283650a9b5d18ec2a55b7`.

There is a remaining verification gap: the run log said the KernelSU driver
file descriptor was unavailable and that KernelSU was staged for the next
boot. RMG's attempted userspace soft restart was rejected because Shizuku did
not have root. A read-only ADB check after the run confirmed
`sys.boot_completed=1`, but the shell remained uid 2000 and could not find
`/dev/ksu` or `/proc/ksu`. Thus the exploit's root result and RMG's success
screen are confirmed, while persistent KernelSU availability through its
device node is not confirmed by the same-boot shell check.

A later user-authorized `adb reboot` performed a full reboot. The user clarified
that this exploit-loaded KernelSU module survives a userspace soft reboot, but
not a full reboot, which starts a new kernel. Therefore, the post-full-reboot
absence of `/dev/ksu`, `/proc/ksu`, and a `kernelsu` module is expected and does
not add evidence about whether its control path worked before that reboot.
RMG's boot-time reroot flow is a separate mechanism; its settings and outcome
were not verified in this check.

The one-off diagnostic payload and its build outputs are retained locally under
`build/diagnostic-page-identity/`; the success-screen capture is
`build/diagnostic-page-identity/rmg-page-identity-success.png`.

## 2026-11-23 follow-up single-attempt diagnostic

After a full reboot, RMG `0.13+local.af081c2` was installed in place and run
once using Shizuku, the exact `r9q-G990BXXSLIZH2` profile, and the same local
page-identity diagnostic payload (SHA-256
`c8872ae6b3c68a75d721c165a83280069187dab867c283650a9b5d18ec2a55b7`). The
phone identified as `SM-G990B` on Android 16 build
`G990BXXSLIZH2`, kernel
`5.4.289-qgki-32192773-abG990BXXSLIZH2`.

The exploit recovered the KASLR slide from tracefs and reached the pipe-based
physical-memory path. The overlay diagnostic reported `cfi write ret=35` and
`cfi read ret=35`, then started `pipe physrw`. It seeded 128 order-3 pages,
but the next controlled mm-group collection stopped at `1/34` and returned
`controlled pipe mm group collection failed`; the pipe-page child did not
report a base address. The one independent exploit attempt exited with status
255 after about 14 minutes. No physical read/write, credential transition,
root handoff, or KernelSU load was reached. In particular, the added late-load
`SYS_reboot` and `/dev/ksu` errno diagnostics did not execute because the
exploit failed before the root helper stage.

After failure, the phone remained booted (`sys.boot_completed=1`) and ADB
connected. A read-only check found neither `/dev/ksu` nor `/proc/ksu`, and no
`ksu` entry in `/proc/modules`. This is consistent with the failure occurring
before KernelSU loading; it does not test the late-load handoff path. No retry
was made on this boot. The next diagnostic should inspect why the second
controlled mm-group collector cannot obtain its 34 objects and why its child
does not return a base; do not repeat the reclaim attempt on the same boot.

### Source-level follow-up

The target collector in `port/util.c` retains leaked mm objects until it has
all 34 slots from one order-3 page. It can scan up to 192 candidates, and it
logs a group only when its count reaches 1, a multiple of 8, or 34. Thus the
last observed `1/34` line does not identify whether later candidates were
misses, duplicate slots, or slow KernelSnitch scans. The detailed
`mmcollect stage=...` logging present in the local source is guarded by
`RMG_DIAG_CONTROLLED_MM`; that instrumentation was not present in the captured
run log.

There is also a separate handoff ambiguity. If collection simply returns
failure, `prepare_controlled_pipe_buffer_page_child()` returns base zero and
the forked pipe-page child should still write the full pointer-sized zero to
its parent. The parent instead logged that it did not receive a full base.
The current single `read()` does not log its byte count or `errno`, and the
child does not log its exit status at that boundary. So scan exhaustion alone
does not explain both messages; a child exit or short/interrupted IPC read is
also possible. The available unprivileged ADB session cannot read the kernel
log (`dmesg` is permission denied), so an OOM or signal cause is unconfirmed.

The working tree has an opt-in `RMG_DIAG_CONTROLLED_MM` mode that reports
per-stage timings and child-wait heartbeats. `RMG_DIAG_PIPE_HANDOFF` runs the
collector through the forked pipe-page child, then returns before FOPS or root
stages. The later one-shot run below used both. Its result-pipe read now
retries on `EINTR` and reports the byte count, error, and child status.

### 2026-11-23 one-shot pipe-child diagnostic

After a fresh reboot, the diagnostic was imported through RMG's Local payload
setting as `rmg-pipe-handoff-diagnostic.so`; SHA-256:
`22144344706AD5785256916F1D59881671364475E38D84B864DB06DBE3502C5C`. The run
used Shizuku and the exact G990B profile. Its compile-time diagnostic branch
ran after KASLR discovery and returned before FOPS and root stages.

The controlled collector succeeded, filling all 34 objects for
`ffffff8094118000` on outer attempt 55; three candidate page groups were
observed. The child wrote all 8 result bytes (`write=8`, `expected=8`,
`errno=0`), and the parent received the full pointer. This rules out
collector exhaustion and a short parent handoff in this run. The pointer was
zero because `pipe_stage_exact_order3()` logged
`pipe order-3 oracle missed target after 512 probes`; no pipe page was
identified. No FOPS/root work, credential transition, or KernelSU load was
reached. RMG marked the run failed; the phone remained booted and connected,
with no `/dev/ksu` or `/proc/ksu`.

The immediate failure is downstream, in releasing/reclaiming the collected
order-3 page or matching its marker in the AF_UNIX probe. This diagnostic
calls pipe preparation immediately after KASLR discovery, so its allocator
history differs from the normal route, which reaches pipe preparation later.
The next local build adds sampled first/last-page read values, read-error
counts, and an `oracle-miss` page-metadata snapshot. It was imported into RMG,
but the follow-up run below used the prior payload, so the new oracle
measurements remain untested. A further controlled reclaim requires another
fresh boot; no retry was made after the follow-up panic.

### 2026-11-23 follow-up run and payload-selection mismatch

The phone was freshly rebooted before one further authorized attempt. RMG
Settings showed `rmg-pipe-oracle-diagnostic.so` after importing it from
Downloads, but the resulting run record says it used
`rmg-pipe-handoff-diagnostic.so`. The saved log contains the earlier
`pipe order-3 oracle missed target after 512 probes` result and does not contain
the new oracle sample or page-metadata diagnostics. Treat this attempt as
another handoff-diagnostic run; it does not test the new oracle build. The
reason RMG selected the prior local file is undetermined.

The collector again reached 34 objects for `ffffff8094118000`, then the run
failed at the order-3 pipe-page oracle. The phone disconnected from ADB during
the attempt and returned after a kernel panic reboot (`ro.boot.bootreason`:
`kernel_panic,bug`). After boot, `/dev/ksu` and `/proc/ksu` were absent, and
KernelSU was not loaded. No second attempt was made. Before any further test,
verify the payload filename in the run log matches the intended oracle build;
do not infer results for the new oracle diagnostics from this run.

### 2026-11-23 corrected-payload attempt

The RMG Settings label still named the oracle payload, but its active
`files/local-payload/local-exploit.so` was SHA-256
`22144344706AD5785256916F1D59881671364475E38D84B864DB06DBE3502C5C`, the old
handoff build. The intended oracle binary was present as
`local-exploit.so.part` and matched the local build SHA-256
`FA502DE327EF3111CE4518F187A4DA7A81FCCBC6E51B88FB247814FECFBFEDE0`. After
preserving the old copy, the verified `.part` file was copied into the active
slot. Its hash matched the build before and after the attempt.

One attempt was started through RMG after it showed Shizuku running. ADB then
disconnected, and the phone returned after another kernel-panic reboot
(`ro.boot.bootreason`: `kernel_panic,fatal_exception`). After reboot,
`/dev/ksu` and `/proc/ksu` were absent. RMG did not retain a new run-history
record, and its post-reboot log only reports that Shizuku was not running; no
payload-stage or oracle diagnostics could be recovered. The panic therefore
does not establish whether this corrected payload reached the oracle. No
further attempt was made.

### 2026-11-23 durable-checkpoint attempt

A benign self-check built from the same checkpoint writer ran as Android's
`shell` user (`uid=2000`) and exited successfully. It created
`/data/local/tmp/rmg-pipe-oracle-checkpoint.log`, wrote three expected records,
and returned no write or `fdatasync` error. The self-check executable was
removed afterward.

The durable oracle payload was imported in RMG and its active
`local-exploit.so` SHA-256 matched the build:
`F178528AC2E47AC17D38D78E3DB41B57093C5E97746F50BF6EB2BB49BCCFDBD0`. One RMG
test was started while the app showed Shizuku running. ADB then disconnected
and returned after reboot. The checkpoint file remained unchanged at the
self-check's three records, and RMG history still showed 45 runs. The reboot
therefore yielded no evidence that the payload reached its exploit entry or
the oracle; no new run record was saved. After reboot, Shizuku was stopped and
`/dev/ksu` and `/proc/ksu` were absent.

The next local build moves the checkpoint reset to the preload constructor and
adds markers before/after the boot-quiet wait, at supervisor setup, and before
the exploit child enters `run_exploit()`. It is not installed or run. Do not
repeat the test until the unexplained reboot and missing run record are
understood.

### 2026-11-23 install-flow oracle run

The prior `Test this phone` page was not the normal install route: it left the
RMG run history and `attempted_payload.xml` unchanged. The correct route is
Home → `Tap to install KernelSU` → the exact
`Galaxy S21 FE 5G (SM-G990B) | Kernel 5.4.289` profile → Confirm. This run used
that route, Shizuku, and the constructor-checkpoint payload
`rmg-pipe-oracle-durable-v2.so`; the app-private active file matched its
SHA-256 `835EAF459D3E0DF5201C8DE5A88BAE87B2F5FE84819E1C8D2ACB4D3C3C55F1FE`.

The durable log confirms payload entry, KASLR discovery, and controlled
collection. It collected 34 objects for `ffffff80c0fc0000` at outer attempt
71, then released the order-3 group and began the oracle. Across all 512 probes,
both endpoint reads returned `0` bytes with `errno=0` (1,024 failed reads),
with no marker matches. RMG saved a failed run at `Running kernel exploit`;
the phone remained booted and connected. No FOPS/root stage ran, and
`/dev/ksu` and `/proc/ksu` remain absent.

This evidence does not show whether the marker was present in the target page:
the read oracle returned EOF for every sample. The durable checkpoint proves
the correct payload did run. The next offline investigation should determine
why `kernel_read_data()`/`configfs_read_once()` returns EOF in this early
pipe-handoff stage and whether the normal install route establishes read state
that this diagnostic skips. Do not run another attempt until a read-only
preflight can return the expected byte count.

The matching Samsung source provides a concrete lead. `ashmem_read_iter()`
returns EOF immediately when `asma->size` is zero. `configfs_read_once()` opens
a fresh ashmem handle for each sample, sets only its name, then calls `read()`;
it does not set the ashmem size or create its backing file with `mmap()`. This
is consistent with all the observed `read=0, errno=0` results if that handle
still uses the ordinary ashmem read path. It is not yet proof that this is the
runtime path, because the exploit may alter ashmem state elsewhere. Inspect
the normal route's ashmem setup and the exact file-operation transition before
changing the oracle or attempting another run.

### 2026-11-23 diagnostic ordering correction

The source comparison identified a more direct explanation for this run. The
`RMG_DIAG_PIPE_HANDOFF` branch invokes `prepare_pipe_buffer_page()` immediately
after KASLR discovery and returns before `prepare_good_kernel_page()`, route
crafting, or `run_main_route_threads()`. The normal route instead reaches
`try_cfi_stage()` only after observing the route signal; that stage verifies
the forged ashmem file operations before `install_child_root()` reaches
`install_pipe_physrw()`. If no pipe page exists then, `install_pipe_physrw()`
requests `prepare_pipe_buffer_page()` from the main-route thread while the
route is active. Thus the early diagnostic oracle ran before the normal
ashmem read callback was installed. Its 1,024 EOF reads are expected to be
non-diagnostic for the normal route and should not be treated as evidence that
the normal route's read primitive fails.

The zero-size ashmem explanation above remains a source-level possibility for
the early diagnostic path, not an established cause. The next useful offline
step is to inspect the exact `try_cfi_stage()` transition and route handoff,
then prepare a bounded diagnostic that observes pipe preparation at its normal
call site without repeating allocator reclaim on the same boot. The subsequent
route-timed attempts and their results are recorded below.

### 2026-11-23 route-timed diagnostic attempts

A diagnostic-only `RMG_DIAG_PIPE_AFTER_CFI` branch was added locally to run
pipe-page preparation after the normal CFI-friendly ashmem write/readback and
before physical read/write or root installation. The initial build used a
30-second coordinator wait. In its one run, the normal route's ashmem write
and read both returned 35 bytes, and the durable log reached
`pipe-after-cfi-start`; pipe-page preparation had begun when the 30-second
wait expired. This run did not reach the pipe oracle. The device was rebooted
afterward to clear the temporary ashmem file-operation state.

The wait was extended locally to 10 minutes and the resulting payload had
SHA-256
`DDA0920374AE368A8F8B641DFB1F540D80F86CA9D9BBFF2DDEB19D4F031C7A9F`. On the
next single run, the durable log stopped during the initial page-collection
phase at `mmcollect-leak a=43`; it never reached `pipe-after-cfi-start`.
ADB disconnected, then returned after a reboot. Android reported
`kernel_panic,sobugarm_watchdog_timer_exhardd_100s` as the boot reason. The
shell user cannot read `/sys/fs/pstore` or `dmesg`, so the detailed panic trace
was unavailable from ADB.

The offline review and a trace-enabled build prepared afterward are described
below; neither provides a detailed panic trace or proves the watchdog cause.

The route-timed build did not define `RMG_DIAG_CONTROLLED_MM`. In this source,
the `CONTROLLED_RECLAIM` environment selects `prepare_controlled_kernel_page()`
for the initial page setup; the collector's 240-second scan deadline and
per-leak timing messages are compiled only with `RMG_DIAG_CONTROLLED_MM`.
Durable checkpoints do record successful leak results, but not the duration of
each unsuccessful scan. Thus the final `mmcollect-leak a=43 b=2` indicates a
successful leak at scan attempt 43 with two groups observed; it does not locate
the watchdog stall precisely. Missing timing data is an instrumentation gap,
not proof of the panic cause. The trace-enabled bounded build described below
addresses observability. The subsequent traced run is recorded below.

The collector was then instrumented locally with a separate
`RMG_DIAG_CONTROLLED_MM_TRACE` flag so tracing does not select the standalone
mmcollect-only mode. Durable checkpoints now bracket collector passes, setup,
child creation, child waits, and brute force. This diagnostic build uses a
45-second child wait limit and a 180-second scan limit. The longer scan limit
allows for the 135-second page preparation observed in the successful route;
durable pass and timeout markers now make a stalled stage visible. It compiled
successfully with SHA-256
`54F0CB0584FF2CCABD9D67FB6D00676AD30704684267574E8BC4FD31BF2538BC`. Its
single normal-route run is recorded below; the source changes remain local.

The matching kernel source does not contain the boot-reason label
`sobugarm_watchdog_timer_exhardd_100s`, so its exact meaning cannot be mapped
from this tree. The target config enables the Qualcomm apps watchdog with a
14,000 ms bark time and a 9,360 ms pet interval. Samsung's `sec_debug_pwdt`
path separately panics after 40 missed sync checks at 10-second intervals
(about 400 seconds). Neither source entry by itself explains the reported
`exhardd_100s` label. Since shell access cannot read pstore or dmesg, the reset
cause remains undetermined.

### 2026-11-23 bounded collector trace run

The trace build was installed through the RMG local payload override and run
once through the normal install route with Shizuku. The CFI-friendly ashmem
write and read each returned 35 bytes, and the durable log reached
`pipe-after-cfi-start`. The pipe-page collector then reached its 180-second
scan deadline at attempt 77 with 18 groups observed. The RMG output shows a
best group of 16/34 objects; the scan ended without a complete group, and the
pipe child reported `controlled pipe mm group collection failed`. It never
reached the pipe oracle, physical read/write, or root installation. The run
returned status 255 without another watchdog reset.

The trace contains `child-reaped` and `bruteforce-done` markers through attempt
76, including the attempt range where the previous run stopped logging at 43.
That makes a deterministic hang at attempt 43 less likely, though it does not
explain the earlier watchdog reboot. The current evidence points to a
stochastic collector shortfall: this pipe-preparation scan did not collect all
34 objects from one order-3 page before its time limit. The collector's
`CONTROLLED_RECLAIM_SCAN_MAX` is 192 attempts, while this run stopped at 77 on
time. Consider a longer bounded scan only after reviewing its impact on the
15-minute RMG run budget; do not repeat the unbounded variant.

For the most populated candidate page, the logs report 1/34 objects at attempt
57, 8/34 at attempt 64, and 16/34 at attempt 72. This suggests the collector
was still making progress toward a complete page when the 180-second bound
expired. The end-to-end run took about 4 minutes 40 seconds; a 360-second
collector bound would add at most three minutes and remain below RMG's
15-minute total limit on a similar run. This is a timing estimate, not a
guarantee that the page will be completed.

The checkpoint log is preserved locally at
`build/diagnostic-pipe-after-cfi-traced/rmg-pipe-oracle-checkpoint-run-93353a1.log`
with SHA-256
`8225A234BC6DD02ABEED20533FE53611EAE39FB5161B9B40331DAE043C98FF89`. The
RMG run record is saved beside it as
`rmg-run-record-b1384848-f572-4a33-8724-77639cc83f63.json`. After the run, the
phone was rebooted to clear the temporary ashmem file-operation state. Its
current boot reason is `reboot,shell`, and `/dev/ksu` remains absent.

## Exact artifacts

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `artifacts/r9q-G990BXXSLIZH2/cve-2026-43499-app.so` | 134072 | `f0a2b39c3d3a0e05beccd839f3e2af2417ca7f60e849b4e5e6bee898688b437b` |
| `artifacts/r9q-G990BXXSLIZH2/cve-2026-43499-root` | 26960 | `fd193ddecbc5db5d13e72281cf54ce3ae054e1b3d8728c1dfc64ff1fc80d0bb8` |
| `kernelsu/android16-5.4_kernelsu-r9q-G990BXXSLIZH2-kdp.ko` | 264456 | `927753d65ff813087ee84271c56f1e2b720a276cadebcb435c737b0767194823` |
| `kernelsu/ksud-r9q-G990BXXSLIZH2-kdp` | 4844920 | `bafc40f0166418ec7564578d9f181083b33db6ae9815095a3cf67eba7ba551e9` |

The KO reports:

```text
5.4.289-qgki-32192773-abG990BXXSLIZH2 SMP preempt mod_unload modversions aarch64
```

## Samsung 5.4 compatibility work

FASTUH RKP watchdog-resets this firmware when KernelSU plants protected-text
kprobes or patches live text/syscall-table entries. This build therefore uses:

- no protected-text kprobes;
- no live text or syscall-table writes;
- a world-readable `/dev/ksu` misc device for the existing ioctl UAPI;
- userspace fallbacks that open `/dev/ksu` when reboot-supercall FD delivery is
  unavailable;
- permissive SELinux with existing `kernel` and `system_file` policy types;
- no kernel umount hiding;
- a hookless `su` applet installed at `/data/adb/ksu/bin/su`.

Samsung 5.4 also lacks CFI jump-table aliases for `prepare_ro_creds`,
`kdp_assign_pgd`, `get_task_creds`, and `set_task_creds`. Calling the raw
resolved symbols through ordinary function pointers caused CFI panics. The
final module uses narrow `__nocfi` wrappers around prototypes verified against
the exact target source.

Native `commit_creds()` was tested and rejected: its
`prepare_ro_creds(CMD_COMMIT_CREDS)` transition caused an EL2 data abort
(`ESR_EL2=0x96000047`, `FAR_EL2=1`) followed by
`TZBSP_ERR_FATAL_NON_SECURE_WDT`. The proven path installs a protected
credential with the task-scoped `CMD_COPY_CREDS` worker, updates the target PGD
through `kdp_assign_pgd`, and synchronizes DEFEX with `CRED_FLAGS_PROOT`.

## Reproduction notes

The exact exploit source is retained under
`src/targets/r9q-G990BXXSLIZH2/`. Its target-specific `port/` source set is
selected only for this profile because the Samsung 5.4 stack overlay depends
on exact compiler geometry. The app payload ignores the generic app retry and
timeout values so it cannot silently leave the phone-validated one-attempt
route. With NDK r27b, `make TARGET=r9q-G990BXXSLIZH2` reproduces the published
app payload and helper hashes byte-for-byte.

The exact config and target symbol versions are retained as:

```text
kernelsu/G990BXXSLIZH2.config.gz
kernelsu/G990BXXSLIZH2.Module.symvers.gz
```

The complete KernelSU source delta is:

```text
kernelsu/patches/KernelSU-v3.2.5-r9q-5.4.patch
```

Force-reboot before every GhostLock attempt. Never run a second controlled
reclaim on the same boot, even after a clean return.

### 2026-11-23 extended route-timed pipe diagnostic

After a fresh reboot, the trace-enabled build was rebuilt with the controlled
collector scan deadline increased from 180 to 360 seconds; the per-child
timeout remained 45 seconds. The binary SHA-256 was
`25BD41CF9F89AEA03A7AFC6681FA1C583B52D116AEB11C5E21F4DEE4BD3FF194`. RMG
Settings showed `rmg-pipe-after-cfi-traced-360s.so`, and the installed
`files/local-payload/local-exploit.so` had the same SHA-256 before the run.
The RMG install-history record confirms the exact
`r9q-G990BXXSLIZH2` profile, the `r9q-g990b-lizh2` source, Shizuku transport,
and the selected local payload.

The normal install route passed support/download checks and the exploit
reached KASLR discovery and the CFI-friendly ashmem callback. The controlled
collector completed an order-3 page of 34/34 objects at attempt 73, then the
pipe oracle matched probe 0. Both 64-byte endpoint reads matched the expected
marker across all eight checked words. The captured skb drained successfully,
and the reclaim snapshot reported slab count `54 -> 53`.

The diagnostic then stopped at its intended boundary. It did not execute
physical read/write or root installation; the RMG record reports
`done=0 root=0 read_ok=0 write_ok=0`, and RMG displayed exit status 255 at
“Running kernel exploit” because this diagnostic branch returns through its
failure path after recording the result. This status is therefore not a
failed pipe-oracle result, nor evidence that KernelSU loaded. The KernelSU
service was prepared by RMG, but the driver was not loaded; after reboot,
`/dev/ksu` and `/proc/ksu` were absent.

The log also reports a stability keeper retaining the dirty fops page and
explicitly requires reboot. The phone was rebooted after evidence collection;
it returned with `sys.boot_completed=1` and boot reason `reboot,shell`, with no
watchdog or kernel-panic reset. The durable checkpoint and RMG run record were
saved under the local build output directory for this diagnostic. The next
investigation should inspect why the controlled pipe reclaim check passes but
the production physical read/write stage has not yet been exercised; do not
interpret this diagnostic as a successful root attempt.

### 2026-11-23 physical read/write finder diagnostic

After the prior clean reboot, one bounded diagnostic build was imported through
RMG and run with Shizuku. It combined the route-timed CFI/pipe preparation with
`RMG_DIAG_PIPE_PHYSRW_ONLY`; its SHA-256 was
`BB5E57A5F5A3D38D34286056F343AC67588797AE30225243E6FC3A2A15681209`, and the
app-private local payload hash matched. RMG history record
`528ccd93-7c38-44c7-9fb8-270d9efdf58d` confirms the intended local payload and
`r9q-G990BXXSLIZH2` profile.

The route passed the CFI-friendly ashmem write/read (35 bytes each). The
controlled collector completed an order-3 page of 34/34 objects at attempt
123. Its marker oracle matched probe 0, and the captured skb drained. The
subsequent production `find_pipe_buffer()` scan did not find a pipe buffer:
`found=0`, index `-1`, and scan counters `vmemmap/ops/len=0/0/0`. The cache-gate
diagnostic also returned `matched=0`; the first four words still contained the
oracle marker. Although the reclaim helper reported the slab count changing
from 54 to 53, this did not establish that the selected page had been reused
for the pipe-buffer structures required by the physical read/write path.

The diagnostic exited before writing the physical read/write proof markers
and before root installation. RMG returned its expected diagnostic status
255; the run summary reports `done=0 root=0 read_ok=0 write_ok=0 rw64=0/0`.
The KernelSU driver was not loaded. The stability keeper reported that the
dirty fops page required reboot; the phone was rebooted and returned with
`sys.boot_completed=1`, boot reason `reboot,shell`, and no `/dev/ksu` or
`/proc/ksu`.

This narrows the current blocker: controlled page collection and marker
matching work, but the reclaimed page is not recognized as containing a
valid `pipe_buffer` for the parent-side physical read/write routine. Review
the reclaim ordering, pipe-buffer allocation ownership across the helper
child/parent handoff, and the page contents before another device attempt.
Checkpoint and RMG run logs are saved in the local
`build/diagnostic-pipe-physrw-only` directory. Do not treat the result as a
physical read/write or root success.

### 2026-11-23 repeated physical read/write finder diagnostic

At 19:50 local time, the same bounded physrw-only payload was selected in RMG
and run once more after a clean reboot. The file staged by RMG was 144,816
bytes and matched the local build SHA-256
`BB5E57A5F5A3D38D34286056F343AC67588797AE30225243E6FC3A2A15681209`.
The phone again passed KASLR setup, completed the controlled collector through
pass 123, released the collected page, and matched oracle probe 0. The
physrw-only stage then returned `done=0` with no pipe-buffer index; RMG ended
with status 255 at “Running kernel exploit”. It did not enter the KernelSU
load stage or write the physical-RW proof markers.

The live checkpoint is saved as
`rmg-pipe-oracle-checkpoint-live.log` in the local conversation workspace;
its SHA-256 is
`FC0E18F4546CFCECF32A0A5752F482439ACB024019799F43B30826A843A295DE`.
The phone was rebooted after the run. Verification showed the expected
G990BXXSLIZH2 fingerprint and 5.4.289 kernel, `sys.boot_completed=1`, ADB
running as `uid=2000(shell)`, and no `/dev/ksu` or `/proc/ksu`. This repeat
confirms the same current blocker: the oracle capture succeeds, but the
captured page is not found as a usable pipe-buffer page by this finder path.
It is not a physical-RW or root success.

### 2026-11-23 pipe-buffer reclaim trace

After the previous attempt's reboot, one bounded reclaim-trace build was
imported into RMG and run through Shizuku. Its SHA-256 was
`2D296F6D7362211480D1AC85EAB4AACF3AEFC0B9DA7308CA74A117E8B3CACA9B`; the
app-private local payload had the same hash. RMG history record
`36856fd2-871b-4f3f-bbc5-e77df06008a6` confirms the selected file
`rmg-pipe-reclaim-trace.so`, the `r9q-G990BXXSLIZH2` profile, Shizuku
transport, and source commit `e5913bc2a7c83826f29f3caf3d80b4813922e938`.

The route passed the tracefs KASLR check (slide `0x118000`), controlled
reclaim (34/34 objects; 251/256 sends), readiness COW, and the CFI callback
probe (35-byte write and read). The new marker sweep checked the reclaimed
pipe rings every 32 allocations. It found no match through 480 pipes; at 512
and every later batch through 768, `find_pipe_buffer()` consistently matched
slot 481. The observed buffer had page pointer `ffffffff04a32c80`, ops pointer
`ffffffc0125b4078`, length 482, flags 0, and scan counters `1/1/1`. The
length agrees with the diagnostic's marker write for pipe 481. This is the
first direct evidence in these runs that the selected reclaimed page appears
in a pipe buffer after enough reclaim-pipe allocations. The earlier “no valid
pipe buffer found” conclusion applies to the prior finder run, not this trace.

This was still only a reclaim/finder success. The build deliberately skipped
the physical read/write proof and root installation. Its summary remained
`done=0 root=0 read_ok=0 write_ok=0 rw64=0/0`; therefore it did not establish a
usable arbitrary physical read/write primitive and did not load KernelSU.
RMG showed its expected diagnostic failure at “Running kernel exploit” with
code 255. The stability keeper reported that a dirty fops page remained held
until reboot. The phone was rebooted after the logs were saved and returned
with `sys.boot_completed=1`, the expected G990BXXSLIZH2 fingerprint and
5.4.289 kernel, ADB `uid=2000(shell)`, and no `/dev/ksu` or `/proc/ksu`.

The full RMG run record, durable checkpoint, and failure screenshot are saved
in `build/diagnostic-pipe-reclaim-trace/`. Their SHA-256 values are:

```text
rmg-run-history.json
FBA69C53D4E12978F9A4E11003E5EEF9DB1704FE2D32A153FC16D55A3542FD11

rmg-pipe-oracle-checkpoint.log
78AD52E386639EEB6E5ED9270DCBF189982160CE9500DC5830EE6018412E2630

failure-screen.png
3B82626FA8777B53B79E2728068DF2B74653D978432EEBDBEEB3DF3E102D55A8
```

The next narrow step is to inspect how slot 481's candidate buffer relates to
the captured page and then add a separate, tightly bounded read-only check of
the pipe's data path. Keep the physical read/write proof and KernelSU load
disabled until a safe, verified primitive exists.

### 2026-11-23 static review of the pipe read/write path

The target's 40-byte `user_pipe_buffer` layout matches the Samsung 5.4
`struct pipe_buffer` layout. The matching kernel source's `pipe_buf_can_merge()`
checks whether `buf->ops` is exactly `anon_pipe_buf_ops`; it does not depend on
a nonzero merge flag. The payload's `ops` pointer and zero flags therefore
match this kernel's merge behavior. In `pipe_write()`, a small write appends
when the buffer uses those ops and `offset + length` stays within the page.
In `pipe_read()`, the kernel copies from `buf->page` at `buf->offset` and
advances the buffer length and offset.

`pipe_phys_read()` temporarily substitutes the target page and offset, sets
the buffer length to the requested read size plus one, reads the requested
bytes, and then restores the saved 40-byte buffer. Leaving one byte avoids
the normal empty-buffer release path during that read. `pipe_phys_write()`
temporarily sets the length to zero so a short write can merge at the target
offset, then restores the saved buffer. The wrapper rejects reads or writes
that cross a page boundary. These mechanics are consistent with the vendor
pipe implementation, but a successful source-level path is not evidence that
the 5.4 CFI-backed configfs writes, target-page conversion, and pipe operation
all succeed together on the handset.

The 20:35 run did not call `install_pipe_physrw()`: the
`RMG_DIAG_PIPE_RECLAIM_TRACE` branch calls `run_pipe_reclaim_diagnostic()` and
then returns through the deliberate failure path. Its `read_ok=0` and
`write_ok=0` fields mean “not attempted,” not failed tests. The run therefore
does not yet explain the absence of `/dev/ksu` in a production run.

One remaining evidence gap is that the trace logs the first candidate fields
and the first four words of the slab, but not `pipebuf_addr`, its offset within
the 32-KiB slab, or a check that the candidate data page contains the marker
from the selected live pipe. All pipes used the same `0x61` marker; the
length `482` strongly identifies pipe 481, but does not independently prove
that the candidate's `page` pointer belongs to that still-open pipe. A
read-only follow-up can log the matched buffer address/alignment and compare a
small sample at `page_to_direct(pb.page) + pb.offset` against a unique marker
for pipe 481.

The next physical read/write self-test is not read-only: it temporarily
rewrites the selected `pipe_buffer` and writes test strings at
`page_base + 0x7000` and `page_base + 0x7100`, then verifies them. Keep that
step separate from the read-only correlation check; it has not been run in
this investigation.

### 2026-11-23 unique pipe marker correlation

A new build writes a unique 64-bit marker at the start of each reclaim pipe's
data and compares the candidate page against the marker for the pipe selected
by `find_pipe_buffer()`. At 32-pipe allocation intervals, the finder returned
no candidate through 704 pipes. At 736 pipes it found slot 711 (the 712th
pipe): the buffer was at slab offset 0 and object offset 0, had page
`ffffffff03e2f480`, ops `ffffffc0124d4078`, length 712 and flags 0. The payload
read eight bytes through its existing kernel-read primitive at the converted
data-page address `ffffff8100bd2000`; the observed marker
`524d4750000002c8` equalled the expected marker for pipe 711. The trace ended
at this positive correlation and deliberately skipped the physical read/write
proof and root installation. This closes the earlier gap: the matched live
pipe's data page was observed to contain its own unique marker.

The RMG run still ended with its expected diagnostic failure (exit 255),
because this payload does not proceed to installation. `done=0`, `root=0`,
`read_ok=0`, and `write_ok=0` mean that the read/write proof was not attempted;
they do not mean it failed. KernelSU remained unloaded, and the handset had no
`/dev/ksu` or `/proc/ksu`. The stability keeper reported that reboot was needed;
after reboot the phone returned with `sys.boot_completed=1`, the expected
firmware fingerprint and kernel, ADB uid 2000, and neither KSU path present.

The build is `build/diagnostic-pipe-reclaim-correlation/cve-2026-43499-app.so`
(146,952 bytes, SHA-256
`EE694C625BA96ABB63EFA1C22894704A285C3E217811E4479E996EFE8F6859F0`). The
RMG run ID is `8a6ff429-a06a-4d65-a50a-febdd189f10d`. Its run record and
checkpoint are saved beside the build artifact; SHA-256 values:

```text
rmg-run-history.json
5596C85D9475FBB136404748263FFD420D3E025240F440662E293633008B326D

rmg-pipe-oracle-checkpoint.log
4C7CDF71C7A6C72F46A6317FC6F76B5F4E4D90FC18685545BB5EF9DDA6ACDA90
```

This correlation is evidence for the candidate buffer-to-pipe data association,
not evidence that arbitrary physical reads or writes work. Keep the
`install_pipe_physrw()` proof and KernelSU load as separate future steps.

### 2026-11-23 physical read/write proof attempts

The first proof-only build followed the normal finder path: it filled all 768
reclaim pipes and then searched once. `find_pipe_buffer()` returned no match,
with counters `0/0/0`; the cache diagnostic also found no matching page. The
proof routine returned before writing either scratch address, so its
`read_ok=0` and `write_ok=0` mean not attempted. This run ID was
`f4bb17d3-c91f-4cf1-b719-d006f30c4074`.

A second build used the previously successful unique-marker correlation
diagnostic and was changed to run the scratch proof immediately after a
positive marker match. This time the finder found no candidate at any
32-pipe checkpoint, through 768 pipes. Each scan reported zero candidate,
ops, and length counters; the first four slab words repeatedly read
`335245444c4f4846`. Because marker correlation failed, the proof remained
gated off and neither the scratch seed nor the physical read/write checks ran.
The run ID was `842745ab-307d-4d15-9313-22552684fcb4`.

Both runs ended before root installation. The handset was rebooted after the
second stability-keeper warning and returned with the expected firmware and
kernel, shell uid 2000, and no `/dev/ksu` or `/proc/ksu`. The earlier successful
correlation remains valid evidence for that run, but repeatability is not yet
established. Next investigate why the candidate slab sometimes contains no
recognizable pipe buffer, especially the `FHOLDER3`-like repeated word, before
another proof attempt.

Artifacts for the combined correlation/proof build are under
`build/diagnostic-pipe-correlated-physrw/`:

```text
cve-2026-43499-app.so
SHA-256 2D1B1C552B99F1EA37C2052008273447F411C79AFAE76FA81E3FFCBA4D85314E

rmg-run-history.json
SHA-256 26A264651BDE6796F05C929BE50B16C113B9E41B5B4BC7C7509D2522035FB285

rmg-pipe-oracle-checkpoint.log
SHA-256 BE140EFFDC57620E8A3B1B6E7CC42513EA396422CBC0F64EAC1A5D52E4124363
```

The separate full-fill proof-only build is under
`build/diagnostic-pipe-physrw-proof/`; its run record is
`rmg-run-history.json` (SHA-256
`A43B8CEADF03298E37A91482AC6B132F320E4AE94BE5FAA5ACA41275BE27D7F7`).

### 2026-11-23 comparison of the successful and missed reclaim runs

Comparing the saved logs shows that the successful correlation was a
different allocator outcome, not a different firmware profile. All three
runs report the same physical offset, kernel physical load, and slide-logger
address. The successful run found the target slab at
`ffffff8027538000`; its first page metadata identified cache
`ffffff8000897600`, and the first pipe-buffer record plus its unique payload
marker both matched. The proof-only miss instead used candidate base
`ffffff8002758000`; its cache diagnostic reported `matched=0`, and the slab
scan counters remained zero. The combined correlation/proof miss used
`ffffff814f010000`; scans again found no candidate, and the first four words
were repeatedly `335245444c4f4846` (little-endian bytes `FHOLDER3`).

This evidence points to the controlled order-3 page not being reclaimed into
the expected pipe-object slab on the two failed runs. It does not yet identify
why the allocator stopped reusing that page; the successful one-run correlation
still proves that the path can work under at least one allocation layout. The
different KASLR slides are expected run-to-run variation and do not explain
the missing slab-cache match by themselves. The proof remains correctly gated:
neither miss wrote to the scratch addresses or attempted KernelSU installation.

Next, keep the work read-only and compare the controlled page’s allocator
identity and reclaim progress at each batch boundary. A future proof run
should remain disabled unless the page metadata confirms the expected pipe
cache and the unique marker correlates to the selected live pipe. Do not treat
the repeated `FHOLDER3` word as a valid pipe buffer or bypass the correlation
gate.

The marker can now be identified more precisely. The source defines
`PIPE_ORDER3_MAGIC` as `0x335245444c4f4847`; probe zero XORs it with one, giving
`0x335245444c4f4846`, whose little-endian bytes are `FHOLDER3`. Each run logs
`pipe order-3 oracle captured probe=0`, so the repeated first word in the two
miss runs is the expected residue of the captured socket-buffer page after
the skb was drained and freed. It remained intact after 768 reclaim-pipe
allocations. Together with the zero slab-cache identity, this is stronger
evidence that the freed order-3 page was not reused by the pipe-object cache;
the failure is upstream of `find_pipe_buffer()`.

The global controlled-mm slab count still decreased from 54 to 53 in all
three runs, including the misses. That confirms the collector discarded a
controlled slab, but does not prove that the exact captured order-3 page was
reallocated to pipe objects. The next implementation check should make this
page-specific reclaim condition an explicit gate and improve cache shaping or
reclaim ordering only after studying the allocator behavior. Retrying the same
scan, or loosening the buffer matcher, would not address this evidence.

### 2026-11-23 proof-only run with reclaim/cache gate

The run started at 22:45:57 and RMG marked it failed at 22:49:25 after the handset rebooted. The saved RMG detail view ends during attempt 52, but the durable checkpoint file at `/data/local/tmp/rmg-pipe-oracle-checkpoint.log` continued farther: attempt 53 was reaped successfully, the controlled-mm scan ended with three groups and a selected full group (`mmcollect-scan-end a=3 b=1`). The collector therefore completed; the earlier assumption that it stalled at attempt 52 was based on the truncated app log and is incorrect.

The device reported `ro.boot.bootreason=kernel_panic,null` after recovery. No later durable checkpoint was present after `mmcollect-scan-end`, so the available evidence does not identify the exact post-collection stage that panicked. No cache/marker gate result, scratch physical read/write result, or KernelSU installation result was recorded. The phone returned online with the expected kernel and shell-only ADB access.

A follow-up local diagnostic build adds durable checkpoints at the pipe-page request/handoff, probe setup, drain allocation, and reclaim spray boundaries. It remains proof-only and skips root installation. Use its checkpoint tail to locate the next failure before changing allocator behavior or relaxing any reclaim or marker checks.

### 2026-11-23 post-collector proof-only run

The follow-up payload was staged as `/sdcard/Download/rmg-pipe-post-collector.so` and its SHA-256 matched the local build (`9FB5BD2B542FFAC99DC2A6FE2443E430B223C770DD1B0C9F65F2557DC78950D4`). RMG confirmed it used this local payload. The durable checkpoints show an initial controlled-mm collection selecting a complete group, followed by the pipe-page preparation child. That second collection completed at attempt 94 with 21 groups and a selected full group.

The pipe-page path then completed every newly instrumented stage: probe allocation, 192-object drain preparation, controlled-mm release, exact order-3 oracle capture (`probe=0`), and all 768 reclaim-pipe allocations. The child handed the candidate page back to the parent. The final `pipe-physrw-only-done a=1 b=716` checkpoint means marker correlation and the scratch physical read/write proof both returned success for pipe index 716. This is the first verified end-to-end pass through the cache, unique-marker, and scratch-read/write gates on this firmware.

This build intentionally skipped Android root and KernelSU installation. RMG therefore marked the overall run failed at “Running kernel exploit” with command code 255; that status is not a failed reclaim or physical read/write proof. Afterward the phone remained online at the expected kernel with ADB shell uid 2000, and `/dev/ksu` and `/proc/ksu` remained absent. No new disconnect or reboot was observed during this run. No allocator logic was changed based on this result; the earlier apparent attempt-52 stall was a truncated UI log, and the new checkpoints show the actual successful path.

### 2026-11-23 root-gated post-CFI continuation attempt

The root-gated candidate (`13A62821D6B2D3F41107091109C1B3C0723BA6384723F4A5AB9AB0FEC0CD6378`) was selected as the local payload and RMG confirmed that filename before starting. It retained the route-timed pipe preparation and enabled continuation into the existing physical-RW/root path. The run log also confirms that Shizuku permission and the KernelSU service were prepared.

The durable checkpoint shows the second controlled-mm collection completed (`mmcollect-scan-end a=20 b=1`), followed by probe setup, drain preparation, controlled-mm release, exact order-3 oracle capture (`probe=0`), and all 768 reclaim-pipe allocations. Unlike the preceding proof-only run, the child then reported `pipe-page-child-pre-handoff a=0 b=11862`, followed by `pipe-page-parent-handoff a=0 b=11862`. Thus this run did not produce a candidate page for the caller. The strict controlled reclaim checks require both a reduced mm-slab count and expected pipe-cache page identity; the available durable trace does not identify which of those checks rejected this run because kernel warning output was not retained.

After recovery, ADB reported the same device and expected booted state, shell uid 2000, and `ro.boot.bootreason=kernel_panic,sobugarm_watchdog_timer_exhardd_100s`. No `/dev/ksu` or `/proc/ksu` evidence was obtained and no KernelSU load was confirmed. The watchdog is consistent with the earlier observed handset recovery behavior, but this trace alone does not establish which in-kernel stage triggered it. Do not retry this candidate unchanged. Next, preserve and retrieve kernel/RMG diagnostics around the failed controlled-reclaim gate, then compare the rejected cache/slab measurements with the successful proof-only run before changing the reclaim strategy. Keep the cache-identity, marker-correlation, and scratch read/write checks intact; do not treat a zero base as permission to continue to root.

### 2026-11-24 instrumented cache-gate capture

An instrumented candidate was compiled with NDK r27c to `build/root-gated-instrumented-capture/cve-2026-43499-app.so` (SHA-256 `E6B5434BC2DFF8BD6E408EC63B8ADE3BF9AE13A746ECA9B8985498E5A8D35807`) and selected in RMG under the unique name `rmg-gate-capture-20261123.so`; the on-device hash matched. This build adds durable slab/cache-gate result checkpoints and aborts before the root stage if the route returns a null candidate base.

The controlled-mm scan completed and the exact order-3 oracle captured `probe=0`. All 768 reclaim pipes were allocated. The new checkpoints then identified the rejection precisely: `mm-slab-gate-pass a=56 b=55`, followed by `pipe-cache-gate-miss a=18446744073709551615 b=18446743525696405504`. The global slab count decreased, but the target page was not observed in the expected pipe cache (`page_index=-1`), so the helper returned base zero. The candidate did not pass the page-identity gate; marker correlation, scratch read/write, and KernelSU were not reached.

The copied durable trace is `build/root-gated-instrumented-capture/rmg-gate-capture-checkpoint.log` (SHA-256 `C34F01EA547672C6C49B261E2DEB20FCB437EC3BB2D0E977B3391B68C8D6DAAC`). After `pipe-page-parent-handoff a=0 b=4676`, no `pipe-prepare-return` or `pipe-after-cfi-null-base` checkpoint appeared for over 100 seconds. The handset remained ADB-responsive and was rebooted explicitly to clear temporary kernel state. It returned with `ro.boot.bootreason=reboot,shell`, the expected firmware, shell uid 2000, and no `/dev/ksu` or `/proc/ksu`. The new null-base guard was therefore not confirmed to execute.

The cleanup instrumentation adds durable markers immediately before and after the parent-side drain-pipe cleanup, with progress every 16 pipes (`pipe-parent-drain-close-start`, `pipe-parent-drain-close-progress`, and `pipe-parent-drain-close-done`). The existing `pipe-prepare-return` marker identifies whether control returns to the route thread.

### 2026-11-24 parent-cleanup instrumentation attempt

The cleanup-instrumented candidate was built with NDK r27c to `build/root-gated-cleanup-capture/cve-2026-43499-app.so` (SHA-256 `40D76E9C58D5A399458F2CADC5FFA5AD3528C8CAD5182FCD7F0CADFB0CB2104E`) and selected in RMG as `rmg-parent-cleanup-capture-20261124.so`; the on-device hash matched. RMG confirmed the exact SM-G990B / kernel 5.4.289 profile and Shizuku transport.

This run did not reach the pipe-page handoff or exercise the new parent-cleanup checkpoints. The durable trace contains four completed controlled-mm scans: `3/selected`, `20/selected`, `26/selected`, and `26/selected` groups. It ends after the fourth `mmcollect-scan-end` at monotonic time 1392.224, with no `pipe-prepare-request`, `pipe-page-parent-handoff`, or `pipe-parent-drain-close-*` stage. RMG marked the run failed at “Running kernel exploit” with code 255. The available evidence does not establish the exact reason the route stopped after the scans.

The full checkpoint copy is `build/root-gated-cleanup-capture/rmg-parent-cleanup-checkpoint.log` (SHA-256 `C8BBCCA5DFBBC1C5960A369C6F936900103FF45097E1C985F19CFE521FC0B035`). The phone stayed booted and ADB-responsive on the expected firmware, with shell uid 2000; no KernelSU nodes appeared. Treat this as a pre-handoff failure, not a result about parent drain cleanup. Preserve the binary and investigate the missing route progress before trying to exercise the new cleanup markers.

### 2026-11-24 analysis of the pre-handoff timeout

The four scan pairs are consistent with page setup for the app route: one initial `prepare_good_kernel_page()` followed by the three refreshes needed for the four-attempt app route (`PSELECT_CFI_ROUTE_ATTEMPTS=4`). In the payload environment, `CONTROLLED_RECLAIM` makes each page setup one controlled-mm collection rather than several internal setup retries. Each scan found a complete group, so the controlled-mm selector itself did not report failure.

These four scans span 730.688 seconds from the first `mmcollect-scan-start` to the last `mmcollect-scan-end`. RMG’s captured log view is 127,430 bytes and ends earlier in the collector trace, so its generic exit code 255 does not identify the final route result. No durable route-attempt or `try_cfi_stage()` result checkpoint exists after the fourth scan. The repeated page refreshes consumed most of RMG’s 900-second run budget; a timeout before the post-CFI pipe request is the leading explanation, but the exact termination stage is unproven. Do not infer that parent cleanup or the cache gate was reached in this run.

The next source revision adds durable route checkpoints for each attempt start, page refresh, COW setup, monitor readiness, pselect entry/return and elapsed time, route signal, and attempt result. `try_cfi_stage()` now records entry, ashmem open, configfs write/read, pipe-stage return, and its final success/failure step. It compiled with NDK r27c to `build/root-gated-route-cfi-capture/cve-2026-43499-app.so` (150,232 bytes; SHA-256 `4ABB04AD6E91AD5A3B50624261B0281A3182BEC0E704D1E45BA6F19B91BFB0BE`). The compiler reported only unused-function warnings. The candidate was subsequently staged and run once; see the next section for its result.

### 2026-11-24 readiness-route checkpoint capture

The route/CFI-instrumented candidate above was staged under the unique name `rmg-route-cfi-capture-20261124.so`; the device-side hash matched. The durable trace is `build/root-gated-route-cfi-capture/rmg-route-cfi-checkpoint.log` (SHA-256 `FF2B515F25F7572698D35F083F917D31A2E1DD41FB882CD05B39A4D3218252F7`). The first controlled-mm scan completed with four groups and one selected group (`mmcollect-scan-end a=4 b=1`). Readiness route attempt 1 then started, the COW setup completed with a 16-GiB region, and both the block monitor and mmap locker reported ready. The trace reached `readiness-cow-entered` and `readiness-pselect-start` at monotonic time 3570.981, but has no `readiness-pselect-done`, route-signal, or CFI-stage checkpoint.

ADB shell stopped responding while pselect was outstanding. The phone recovered after a watchdog reset with `ro.boot.bootreason=kernel_panic,sobugarm_watchdog_timer_exhardd_100s`, the expected firmware, shell uid 2000, and no `/dev/ksu` or `/proc/ksu`. This run localizes the hang to the outstanding readiness pselect/COW path, before CFI validation and before any pipe-page or cleanup gate. The route-timeout hypothesis from the four-scan run does not explain this separate attempt, which reached the pselect call after only one completed scan. Do not rerun this build unchanged. The next build adds block-monitor and mmap-locker checkpoints to determine why pselect did not return.

The follow-up source records monitor open/armed state, sparse progress while polling the target task state, whether the task reaches `D`, the `sched_setattr` result, and `mprotect` entry/result/time. It compiled with NDK r27c to `build/root-gated-monitor-capture/cve-2026-43499-app.so` (150,856 bytes; SHA-256 `19B16C5293CAF213BB32F1060EB327E097E5FEDF0EA373323A58F57F5E6AC084`).

The candidate was staged under the unique name `rmg-monitor-capture-20261124.so`; the device-side hash matched. The saved checkpoint is `build/root-gated-monitor-capture/rmg-monitor-checkpoint.log` (SHA-256 `504E67C5F4B42C6D2E2AD180241418660960B67A99D95C6D9F4D0D202A7E294C`). Its first controlled-mm scan completed with three groups and one selected group (`mmcollect-scan-end a=3 b=1`). Readiness attempt 1 completed COW setup for a 16-GiB region; the monitor opened its proc handle, the monitor and mmap-locker threads reported ready, and `mprotect` and pselect were entered. The monitor then reported `readiness-monitor-armed` and immediately `readiness-monitor-blocked`. No monitor progress/exit, sched_setattr result, `readiness-mprotect-done`, `readiness-mprotect-time`, or `readiness-pselect-done` was recorded. This isolates the stall to the blocked-thread readiness/mprotect path after the monitor detects the target in `D` state; it does not establish whether the blocked task is the intended one or whether `mprotect` itself is stuck.

ADB disconnected during the attempt. The phone recovered with `ro.boot.bootreason=kernel_panic,null` and the expected `5.4.289-qgki-32192773-abG990BXXSLIZH2` kernel. It returned with ADB shell uid 2000 and no `/dev/ksu` or `/proc/ksu`. RMG remains `0.13+local.af081c2`. No root or KernelSU installation succeeded. Stop retrying this unchanged readiness route; the next work should be source-level review of the monitor/mprotect synchronization and validation of which task PID is being watched before considering another diagnostic run.

### 2026-11-24 source review of readiness stall

Static review confirms the monitor watches `waiter_tid`: the normal waiter thread stores its own TID from `gettid()`, and the readiness route runs in that same thread's `SIGUSR1` handler. The Samsung 5.4.289 source contains the exact vulnerable `remove_waiter()` behavior described by CVE-2026-43499. In `futex_requeue()`, `rt_mutex_start_proxy_lock()` receives the sleeping waiter task as `waiter->task`; on its `-EDEADLK` rollback, `remove_waiter()` dequeues the proxy waiter but uses `current` for `pi_lock` and `pi_blocked_on` cleanup. Here `current` is the requeue caller, not the sleeping waiter, so the latter can retain a dangling `pi_blocked_on` pointer. The upstream fix changes `remove_waiter()` to operate on `waiter->task` instead of `current` ([CVE announcement](https://lists.openwall.net/linux-cve-announce/2026/05/21/7); upstream fix commit `3bfdc63936dd4773109b7b8c280c0f3b5ae7d349`).

The matching source defines `rt_mutex_waiter` with `task` at offset `0x30`, `lock` at `0x38`, `prio` at `0x40`, and `deadline` at `0x48`, matching the payload's declared waiter offsets. The firmware-specific source delta archive contains no `kernel/futex.c`, `kernel/locking/rtmutex.c`, or `kernel/sched/core.c` entries; this supports, but cannot by itself prove, that the base-source behavior is what shipped in the image. In `kernel/sched/core.c`, `sched_setattr()` calls `rt_mutex_adjust_pi(p)`, which follows `pi_blocked_on` into `rt_mutex_adjust_prio_chain()`. This explains why the payload's monitor calls `sched_setattr()` after seeing the waiter blocked: it is trying to exercise the stale PI pointer left by the rollback bug. The captured trace has `readiness-monitor-blocked` but no `readiness-monitor-sched`; the scheduler call is therefore a likely point of the watchdog, although the concurrent 16-GiB `mprotect` also has no completion marker. The retry loop in the chain walker can spin on a contended lock without advancing its chain-depth limit, so persistent contention remains a plausible failure mode. The source confirms the intended vulnerable primitive and structure offsets, but does not identify which individual pointer or lock state causes this device's reset. No source behavior was changed and no new phone run was performed.

The downloaded AP firmware archive contains `boot.img.lz4` (29,227,208 bytes). After the capture, the phone is booted and ADB shell reports uid 2000, but `/sys/fs/pstore` is not readable to that shell; querying the kernel log buffer returned no matching rtmutex/watchdog lines. Thus there is no post-reset kernel trace available through the current unprivileged ADB session.

### 2026-11-24 stock boot-image verification

The stock `boot.img.lz4` was decompressed read-only using the existing WSL `liblz4` library. The result is an Android boot-image v3 container with a 52,490,752-byte ARM64 kernel image. The embedded release string is exactly `5.4.289-qgki-32192773-abG990BXXSLIZH2`, matching both the connected phone and the Samsung 5.4.289 source bundle. Hashes: compressed `boot.img.lz4` `6AD0EE88955D71E1C56EA66653B851CFECD3057253122DB0BFCE034B3555B77D`; unpacked `boot.img` `E233F580B42458A12FC99D19AAB199EECA390236D707B5140BDD3EB39176C0E0`; extracted `kernel.bin` `65345CD2969F22A10749F4B5B31ED631954A749C3CAD53292C5ED7D116484C40`.

The kernel image contains the `Maximum lock depth` rtmutex diagnostic string, but its function symbols are stripped (`remove_waiter` and `rt_mutex_start_proxy_lock` names are absent). The vendor source and firmware-specific delta still provide the strongest function-level evidence: the base 5.4.289 `remove_waiter()` uses `current`, while the firmware delta contains no changes to that file. This verifies the image's exact build identity and strongly supports that it contains the vulnerable source path, but the stripped binary prevents direct disassembly-level confirmation from this artifact. No phone-side action or exploit attempt was performed.

### Next diagnostic proposal (superseded by run below)

`preload.c` explicitly sets `READINESS_PTE_MB=16384`, selecting the maximum 16-GiB PTE region. The previous trace entered `mprotect` on that full region and never recorded its return; the monitor then detected the waiter in `D` state and called `sched_setattr`, also without recording a return. The proposed discriminating experiment was to use the smallest supported PTE region (64 MiB) and a diagnostic-only monitor that logs the blocked state then exits without calling `sched_setattr`, stopping before CFI/root installation. The result is recorded below. The futex rollback path still exercises vulnerable kernel code, so this was limited to one final diagnostic run.


### 2026-11-24 64-MiB COW-only readiness diagnostic

The proof-only diagnostic was built with NDK r27c and the explicit make variable `APP_TARGET_CFLAGS=-DRMG_DIAG_COW_ONLY=1 -DRMG_DIAG_DURABLE_LOG=1`. The final imported payload was `rmg-cow-only-capture-fixed-20261124.so`, SHA-256 `C887544F0F1E214FA9A9E611CE05F29DAF411A697787692E9BEFAEAA7B3FA7F9`. Two preliminary starts were stopped before the COW trigger: the first build did not receive the diagnostic flags because they were passed as environment variables, and the second selected an older copy already imported by RMG. The final run used a fresh filename, and RMG's live log confirmed `pte_mb=64 diagnostic=cow-only` before the exploit began.

The final one-run payload kept its existing single exploit attempt, limited the readiness region to 64 MiB, logged the blocked waiter, skipped `sched_setattr`, and forced the route signal off so CFI and root installation were unreachable. It completed all four bounded readiness-route attempts. Attempt 1 recorded `readiness-monitor-blocked`, then `readiness-monitor-sched-skipped`; its 64-MiB `mprotect` returned `0` with errno `0` in about 1.26 ms, and `pselect` returned `115` in about 670 us. Attempts 2–4 also recorded successful `mprotect` completion (about 7.86 ms, 2.79 ms, and 1.81 ms respectively), but the monitor did not observe the waiter blocked on those attempts. Their `pselect` calls returned `109`, `121`, and `118`. Every route signal was forced to zero, and the final checkpoint was `readiness-route-status a=33 b=0`; no CFI or root-stage checkpoint followed. The pulled checkpoint log is `build/root-gated-cow-only-capture/phone-checkpoints.log` (SHA-256 `B502D9ED08EFB5501E58477329A007F8FB4659C6E323CDF553BF106B959E58E2`).

RMG therefore marked the install failed at “Running kernel exploit” with command code 255, which is the expected result for this stop-before-CFI diagnostic. The phone stayed connected and booted on `5.4.289-qgki-32192773-abG990BXXSLIZH2`; shell access remained uid 2000, `/dev/ksu` and `/proc/ksu` were absent, and no KernelSU load occurred. The boot-reason property remained `kernel_panic,null` from the earlier boot; the uptime continued increasing during this run, so no new reboot was observed.

This narrows the previous stall: the 16-GiB PTE region was not needed for the COW setup, and the 64-MiB `mprotect` completed quickly in all four attempts. The first attempt confirms that the waiter can reach `D` state, but later attempts did not reproduce that timing. The scheduler/PI-chainwalk behavior remains untested because the diagnostic intentionally skipped the scheduler call. Keep the stop-before-CFI gate and do not infer a working exploit or KernelSU driver from this result.


### 2026-11-24 follow-up analysis: readiness timing

The four COW readiness samples differ in both monitor timing and overlap with the helper `mprotect`. Attempt 1 logged a blocked waiter on the monitor's first poll; its `mprotect` took 1.261 ms, and `pselect` took 670 us. Attempt 2's `pselect` completed in 500 us before the monitor's `readiness-monitor-armed` checkpoint, so its single poll after return cannot rule out a brief earlier state transition. Attempt 4 has the same limitation (`pselect` 800 us, one poll after completion). Attempt 3 is the strongest negative sample: the monitor was active during a 5.628-ms `pselect` and polled 537 times without seeing `D`.

The checkpoint clock is recorded only to millisecond precision, while the operations take fractions of a millisecond to several milliseconds. It does not provide enough ordering detail to tell whether the helper's `mprotect` held the address-space lock during the COW fault. The `readiness_mmap_lock_thread` sets `ctx.entered` before calling `mprotect`, and the main thread waits a fixed 100-us head start; that is not an acknowledgment that the syscall acquired the mmap lock. The trace also shows `mprotect` completing before the logged `pselect` start on attempts 1 and 3, but the current timestamp resolution and checkpoint I/O limit fine-grained conclusions.

The existing per-attempt `route_pi_sanitize()` is another confounder. `sanitize_holder_thread()` intentionally holds `f_pi_sanitize` until the overall route exits, while each route attempt tries to lock it with a 50-ms timeout. RMG logs `lock=-1 errno=110` and `unlock=-1 errno=1` each time, so the sanitizer does not acquire or release that lock during these attempts. This is consistent across the four samples; the evidence does not prove that it caused the differing `D` observations.

The monitor reads `/proc/self/task/<tid>/stat` in a busy polling loop and performs a durable file write plus `fdatasync` immediately after detecting `D`. That observer work can affect scheduling, and a transient `D` interval can fall between polls. The first sample's one-poll hit proves `D` was observable once; the later no-hit samples do not establish a single deterministic cause.

For a future diagnostic, keep the scheduler call skipped and use one readiness route attempt per run. Record monitor activation, state transitions, helper syscall entry/exit, and pselect entry/exit into an in-memory trace with microsecond timestamps, then flush it only after the route returns. Add an explicit monitor-start handshake before entering `pselect`; do not treat `ctx.entered` as proof that `mprotect` acquired the mmap lock. No additional phone run was started for this analysis.

### 2026-11-24 one-route in-memory COW timing trace

A new proof-only payload was built with NDK r27c using `APP_TARGET_CFLAGS=-DRMG_DIAG_COW_ONLY=1 -DRMG_DIAG_COW_TRACE=1`, with durable per-event checkpoint writes disabled. RMG imported `rmg-cow-only-trace-20261124.so`; SHA-256 was `950FBF3705CA94DB446F094295C5ED6C2EDD0DC93401521EB4E7A769ED10D29A`. This build limited the route to one readiness attempt, kept the 64-MiB PTE region, skipped `sched_setattr`, and forced the route signal off before CFI/root.

The in-memory trace showed `mprotect` before/after at monotonic microseconds `40180624856` and `40180625152` (296 us). The monitor then observed its armed state at `40180634311`, sampled the waiter as ASCII state `82` (`R`) at `40180634360`, and `pselect` began at `40180639392`. Thus `pselect` began about 9.54 ms after `mprotect` returned and about 5.03 ms after the monitor's initial sample. It returned at `40180639622` (231 us duration) with result `124`. There was no blocked-state trace event and the overlay check passed.

This run isolated a flaw in the diagnostic ordering: the main thread starts `mprotect` before arming and handshaking with the monitor. The handshake let the helper finish well before `pselect`, so this run cannot test whether a COW fault blocks behind the helper's mmap write lock. It does show that the monitor was polling before `pselect`, and that the observed attempt returned quickly without entering `D`.

RMG marked the diagnostic failed at “Running kernel exploit” with code 255, as expected when the COW-only route is forced to stop before CFI/root. The phone remained connected and booted on the expected kernel, shell uid 2000, with no `/dev/ksu` or `/proc/ksu`; no reboot or KernelSU load occurred.

For the next single-route trace, start and arm the monitor, wait for its first state sample, and only then start the `mprotect` helper. Trigger `pselect` immediately after the helper's pre-syscall marker and the short head start. Keep the scheduler skipped and retain the in-memory microsecond trace. The trace must still be treated as inconclusive about actual mmap-lock acquisition; it records syscall entry/exit but cannot prove exactly when the kernel acquired that lock.

### 2026-11-24 monitor-before-helper and zero-head-start traces

The first ordering correction armed the monitor and waited for its first state sample before creating the `mprotect` thread. Payload `rmg-cow-only-trace-armed-20261124.so` had SHA-256 `27539D4F4D32FA8EA75E325DD2AF94586B04B180799DCEB9EDE9ABD54060DD76`. Its trace put monitor arming/sample at `40480940012` / `40480940028` us (initial state `R`), `mprotect` before/after at `40480956205` / `40480956306` us (101 us), and `pselect` before/after at `40480956472` / `40480956537` us (65 us). `pselect` began 166 us after `mprotect` returned. No `D` state was observed.

A final single-route build set the diagnostic head start to zero. Payload `rmg-cow-only-trace-immediate-20261124.so` had SHA-256 `4642E88EEAED102C3953F9720C9173FA7A6DA0962DDA8B51BED81E635E8D62C1`. The monitor sampled `R` before helper work. `mprotect` ran from `40693284277` to `40693284390` us (113 us), then `pselect` began at `40693284478` us, 88 us after `mprotect` returned, and ended at `40693284565` us (87 us duration). Again, there was no `D` state event. The monitor handshake and zero head start were active, but scheduling still left a gap after the short `mprotect` syscall.

Both RMG runs stopped before CFI/root by design and failed at “Running kernel exploit” with code 255. The phone remained booted on the expected kernel with shell uid 2000 and no KernelSU nodes. These traces explain why these new single-route probes did not reproduce the blocked state: `pselect` began after the helper's `mprotect` had returned. However, the earlier four-route diagnostic did observe `D` once even though its millisecond checkpoints place `mprotect` completion before `pselect`; lock overlap alone therefore does not explain the first-only observation. The remaining trigger may be another transient scheduling or kernel wait condition. These runs do not establish whether an overlapping COW fault would enter `D`.

A next candidate could lengthen the helper's lock-held interval moderately (for example, a 256-MiB PTE region with zero head start) while preserving the one-route limit, active monitor, in-memory trace, scheduler skip, and stop-before-CFI gate. Avoid returning to the 16-GiB setting; the 256-MiB candidate was then tested in one route attempt.




### 2026-11-24 256-MiB one-route COW timing diagnostic

The one-route candidate used NDK r27c with `APP_TARGET_CFLAGS=-DRMG_DIAG_COW_ONLY=1 -DRMG_DIAG_COW_TRACE=1 -DRMG_DIAG_COW_PTE_MB=256`. RMG confirmed the fresh local file `rmg-cow-only-256m-20261124.so`; its SHA-256 was `0C8BF82290D72573B62FE8FF4F41A6AB5A536B64217E11DB6E010C77B5544301`. The monitor was armed and sampled state `R` before the `mprotect` helper began. The route remained one attempt, skipped `sched_setattr`, and forced route-signal zero before CFI/root.

The trace shows `mprotect` entered at `41551646394` us, `pselect` entered at `41551646664` us (270 us later), `mprotect` returned at `41551647459` us, and `pselect` returned at `41551647550` us. `mprotect` took 1,065 us; `pselect` took 886 us. They overlapped for roughly 795 us, but `mprotect` returned 91 us before `pselect` completed. The monitor saw no `D` state event. The COW overlay check passed and the readiness route remained gated.

RMG marked the run failed at “Running kernel exploit” with code 255, as expected for the stop-before-CFI diagnostic. The phone stayed connected and booted on the expected kernel with shell uid 2000; no new reboot occurred and `/dev/ksu` and `/proc/ksu` remained absent.

This is the first timing trace with helper `mprotect` and `pselect` overlapping, but it still does not prove the exact COW copyout interval. Since the `pselect` return happened just after `mprotect` released its lock, the COW writeback could have reached the fault path after the lock was gone. A next bounded experiment could increase the PTE region moderately to 512 MiB so the helper remains active beyond `pselect` return, retaining the same single-route, in-memory trace, scheduler skip, and stop-before-CFI/root guards. Do not jump back to the 16-GiB region.

### 2026-11-24 512-MiB COW trace

A one-route COW-only trace build was compiled with NDK r27c and `APP_TARGET_CFLAGS=-DRMG_DIAG_COW_ONLY=1 -DRMG_DIAG_COW_TRACE=1 -DRMG_DIAG_COW_PTE_MB=512`. The app variant is `build/root-gated-cow-512m/cve-2026-43499-app.so` (139,128 bytes; SHA-256 `CC99B682DD01A8F662E892A98B02CA45AD0A57D851EC13DC38122FCDA12BD79C`), copied to `rmg-cow-only-512m-20261124.so` and staged on the phone at `/sdcard/Download/rmg-cow-only-512m-20261124.so`. RMG's Local payload setting displayed this exact filename after import. The existing one-attempt trace configuration still skips `sched_setattr` and forces route-signal zero before CFI/root.

RMG later imported the payload under the unique local name `rmg-cow-only-512m-20261124.so` and completed one run using Shizuku. The durable record is app-private `files/install-history/b9315856-63c9-4c16-8f86-b19943145f02.json`; it identifies profile `r9q-G990BXXSLIZH2`, source commit `3095e6f2a0659570b5adc54a4cb6b121f96d2fa2`, and the local payload. The route guard reported `pte_mb=512 diagnostic=cow-only`; only one attempt ran. This diagnostic skips `sched_setattr` and forces the route signal off before CFI/root, so it cannot load KernelSU.

The COW trace was:

| Event | Monotonic time (us) | Value |
| --- | ---: | ---: |
| Monitor armed | 44,204,010,831 | 0 |
| First waiter state | 44,204,010,875 | 82 (`R`) |
| `mprotect` before | 44,204,031,195 | 0 |
| `mprotect` after | 44,204,032,690 | 0 |
| `pselect` before | 44,204,033,644 | 0 |
| `pselect` after | 44,204,033,821 | 106 |

Thus `mprotect` took 1,495 us and `pselect` took 177 us; the first state sample preceded `mprotect` by about 20.3 ms. There was no `MONITOR_BLOCKED` event. The overlay geometry check returned 106 and passed, with `blocked=0`, `sched=0/0`, and `mprotect=0/0`. The route's PI sanitize reported lock timeout (`errno=110`) and a failed unlock (`errno=1`). The COW readiness walk made zero calls. Since this diagnostic deliberately forces `route_signal=0`, the code assigned step 33 before calling `try_cfi_stage()`; this is a pre-CFI stop, not a CFI attempt that failed. The pipe-physical-RW summary reported `done=0 root=0`, `read_ok=0 write_ok=0`, and uid unchanged at `4294967295`. RMG ended at “Running kernel exploit” with code 255. This is the expected stop-before-CFI/root diagnostic failure, not a successful exploit and not evidence that the kernel rejected a root attempt.

After the run, ADB remained available. The phone still reported fingerprint `samsung/r9qxeea/r9q:16/BP2A.250605.031.A3/G990BXXSLIZH2:user/release-keys` and kernel `5.4.289-qgki-32192773-abG990BXXSLIZH2`; `/dev/ksu` and `/proc/ksu` were absent. Uptime was 44,357.58 seconds when checked, so the phone had not rebooted during this observation. No root or KernelSU load occurred.

Interpretation: compared with the prior 256-MiB run, this 512-MiB helper kept `mprotect` active longer, but the monitor still sampled only `R` before the operation and no blocked-state event was observed. The trace confirms that `pselect` returned promptly with 106 and the route stopped at its deliberate pre-CFI gate. It does not prove whether a COW writeback overlapped the relevant kernel lock interval, nor does it diagnose the scheduler/PI-chainwalk behavior, which this build intentionally did not execute. Preserve this as a bounded timing result; do not treat it as an exploit success or as validation of `/dev/ksu`.

### 2026-11-24 1-GiB COW timing diagnostic

To widen the helper window without returning to the prior 16-GiB region, a one-attempt COW-only build was compiled with Android NDK `27.1.12297006` using `APP_TARGET_CFLAGS=-DRMG_DIAG_COW_ONLY=1 -DRMG_DIAG_COW_TRACE=1 -DRMG_DIAG_COW_PTE_MB=1024`. The binary is `build/root-gated-cow-1024m/cve-2026-43499-app.so` (139,288 bytes; SHA-256 `5314190525E8D9194BE415737AC302280A2DFBC9C20B3E873AA20848E350C311`). It was imported into RMG as `rmg-cow-only-1024m-20261124.so`; the on-device staged file hash matched. RMG confirmed `pte_mb=1024 diagnostic=cow-only` and ran one exploit attempt. As with the preceding probes, the build skipped `sched_setattr` and forced `route_signal=0`, stopping before CFI/root.

The saved RMG record is `files/install-history/7cc51cde-5204-40a4-96bf-3c58330e6066.json`. Its trace places the monitor armed/first state at `44940399683` / `44940399703` us (state `R`), `mprotect` start/end at `44940412068` / `44940413506` us, and `pselect` start/end at `44940416362` / `44940416459` us. Thus `mprotect` took 1,438 us; `pselect` began 2,856 us after the helper's start marker and 1,418 us after `mprotect` returned, then returned in 97 us with result 112. No blocked-state event was recorded. Overlay geometry passed (`overlay=1`), but the COW walk made zero calls; the deliberate pre-CFI gate assigned step 33. The PI sanitize again timed out (`errno=110`) and its unlock returned `errno=1`. The pipe-physrw summary remained `done=0 root=0 read_ok=0 write_ok=0`.

The 1-GiB region did not lengthen the measured `mprotect` window compared with the 512-MiB attempt (1,438 us versus 1,495 us), and it still did not overlap `pselect`. Preparing the kernel page took 53,582 ms. RMG therefore reported code 255 at “Running kernel exploit,” as expected for the guarded pre-CFI diagnostic; this was not a CFI or root failure. ADB remained available on the expected firmware and kernel, uptime continued increasing, and `/dev/ksu` and `/proc/ksu` remained absent. The larger region added setup time without improving the timing result; the next useful work is source-level synchronization/scheduling analysis, not another increase in region size.

### 2026-11-24 COW timing trace with alternate-CPU selection

The source review found that the trace build's mmap helper tried to pin to its preferred CPU before considering the waiter CPU to avoid. A diagnostic-only change now calls `sched_pin_highest_allowed(waiter_core)` directly and logs the selected core. The one-attempt 512-MiB candidate was built with NDK `27.1.12297006` and the same COW-only/trace guards as above: `build/root-gated-cow-512m-affinity/cve-2026-43499-app.so` (139,256 bytes; SHA-256 `6A37EDC536DDDD696B93AF289FED58EA58EC65ACD6DE8BC307A7ACAE25031B97`). RMG imported `rmg-cow-only-512m-affinity-20261124.so` and confirmed `pte_mb=512 diagnostic=cow-only`.

In the saved record `files/install-history/f09c9776-3fb5-4de6-b1f9-dfa3f0ae63a3.json`, the helper reported `core=-1 waiter_core=6`: it could not pin to a different allowed CPU. Even so, this trace did achieve 210 us of overlap: `mprotect` ran from `45229720188` to `45229722002` us, while `pselect` ran from `45229721792` to `45229722041` us. `mprotect` took 1,813 us and `pselect` took 250 us, returning 39 us after `mprotect` completed. The monitor again saw only state `R`; there was no blocked-state event. Overlay geometry passed (`ret=112`, `overlay=1`), but the COW walk made zero calls and the forced pre-CFI gate assigned step 33. The scheduler remained skipped, and no CFI/root or physrw stage ran (`done=0 root=0`).


### 2026-11-24 cause of the unavailable alternate CPU

Read-only ADB checks after the run showed CPUs `0-7` online, the ADB shell allowed on `0-7`, the top-app cpuset as `0-7`, and global battery saver (`settings get global low_power`) set to `0`. The RMG and Shizuku process thread masks were also `0-7` when checked after the run. Thus the phone-wide CPU set and power mode do not explain the helper's `core=-1` result.

The payload trace reported `waiter_core=6` and the mmap helper's alternate-core selection returned `-1`. Source review explains the narrower per-thread mask: `waiter_thread()` pins itself before waiting for the signal; the readiness route then creates `mmap_locker` with `pthread_create()` from that waiter thread while handling the signal. On Linux, a new pthread inherits a copy of its creator's CPU affinity mask ([`pthread_create(3)` Linux manual page](https://man7.org/linux/man-pages/man3/pthread_create.3.html)). The helper therefore inherits the waiter's single-CPU mask (`6`), and `sched_pin_highest_allowed(6)` has no other CPU to select. The online and cpuset masks can be broader while that individual helper thread is restricted to CPU 6.

This makes inherited thread affinity the direct explanation for `core=-1`; power saver and the system cpuset are ruled out by the available checks. A diagnostic-only correction should restore or explicitly pass the original allowed CPU mask to the helper before selecting a CPU other than the waiter, then log the effective mask and selected core. Preserve the one-attempt, scheduler-skipped, stop-before-CFI/root guards. Do not raise the PTE region size; the existing 512-MiB run already achieved 210 us of measured overlap, and its remaining issue was that the monitor saw no blocked state.

### 2026-11-24 inherited-affinity reset verification

The diagnostic helper now snapshots its inherited affinity, requests the full CPU mask (letting the kernel intersect it with the process cpuset), records the resulting effective mask, and selects the highest allowed CPU other than the waiter. This change is compiled only with `RMG_DIAG_COW_TRACE`; the ordinary route retains its prior affinity behavior. The one-attempt 512-MiB candidate is `build/root-gated-cow-512m-affinity-reset/cve-2026-43499-app.so` (139,832 bytes; SHA-256 `08EA5806759E25E6067D600249040C1FDB689C07705D6F76C68A1308A3CC1157`). RMG imported it as `rmg-cow-only-512m-affinity-reset-20261124.so` and confirmed the guarded `pte_mb=512 diagnostic=cow-only` configuration.

The saved record is `files/install-history/5b467313-2e37-44e8-a7e4-487598b608de.json`. It reports inherited mask `0x1` (CPU 0), successful restoration (`restore=0/0`), effective mask `0x3f` (CPUs 0–5), selected helper core 5, and waiter core 0. The trace timestamps show `mprotect` and `pselect` both began at `46402576531` us; `mprotect` returned at `46402578611` us and `pselect` returned at `46402578636` us. Their measured durations were 2,080 us and 2,106 us, with about 2,080 us of overlap. This confirms and fixes the inherited-affinity restriction that prevented the previous build from choosing a separate core.

Despite that overlap, the monitor again observed only state `R` and recorded no blocked-state event. The overlay check passed (`ret=115`, `overlay=1`), but the COW readiness walk made zero calls. The route was forced to stop before CFI/root, with step 33 assigned by the diagnostic gate; the pipe-physrw summary remained `done=0 root=0`. RMG reported its expected code 255, and the phone stayed ADB-responsive on the expected firmware/kernel with `/dev/ksu` and `/proc/ksu` absent and uptime increasing. The affinity problem is resolved for this diagnostic; the remaining question is why the overlapping COW/pselect path did not produce an observed blocked waiter. Do not enable `sched_setattr`, CFI, or root based on this probe.

### 2026-11-24 soft-dirty measurement follow-up

The next 512-MiB proof run did not overlap its critical operations: `mprotect` ran from `48411300234` to `48411300908` us (674 us), while `pselect` ran from `48411302476` to `48411302549` us, starting 1,568 us after `mprotect` returned. The overlay remained valid (`pselect ret=112`). `/proc/self/clear_refs` accepted the soft-dirty reset and `/proc/self/pagemap` was readable, but the mapped page entry was `0x8100000000000000` (bit 55 clear). The monitor made 6,226 polls and observed only state `R` (`first_state=82`, `last_state=82`), with zero blocked samples. The route again stopped before CFI/root as designed; RMG code 255 was expected, and the phone remained responsive without `/dev/ksu` or `/proc/ksu`.

This result is inconclusive because the timing window was missed. To validate soft-dirty on this mapping, the diagnostic-only trace now samples the pagemap entry immediately after `pselect`, writes one byte at the end of the mapped page after that observation, and samples the entry again. That control write is outside the fd_set words and after the timing-sensitive call. A follow-up should retain the 512-MiB, one-attempt, COW-only guards and compare both samples; keep scheduler, CFI, and root stages disabled.

The follow-up candidate was compiled with NDK `27.1.12297006` using `APP_TARGET_CFLAGS=-DRMG_DIAG_COW_ONLY=1 -DRMG_DIAG_COW_TRACE=1 -DRMG_DIAG_COW_PTE_MB=512`. It is `rmg-cow-only-512m-softdirty-control-20261124.so` (140,800 bytes; SHA-256 `AE161BA13ED00F6358D859606296782008BF19C6128F2034C8984D065123552A`). The Windows UI automation path remained blocked by `GetCursorPos` returning `0x80070005`, but ADB shell control succeeded: the file was pushed to `/sdcard/Download`, its on-device SHA-256 matched, RMG imported it as the local payload, and one guarded attempt ran. The handset was on the expected firmware/kernel with Shizuku available.

RMG record `666385b7-5a4d-4a51-bc36-d4ead514a277` ran from 15:03:16 to 15:04:24 local time. It confirmed `r9q-G990BXXSLIZH2`, `pte_mb=512`, `diagnostic=cow-only`, and `attempts=1`. The controlled page preparation took 56.3 seconds. The event trace shows the COW `mprotect` helper and `pselect` starting together; `mprotect` ended at `49608188894` us and `pselect` returned at `49608188905` us, giving about 686 us of overlap. Affinity was restored from inherited mask `0x40` to allowed mask `0x5f`; the helper ran on CPU 4 and the waiter on CPU 6.

Despite the overlap, the monitor made 4,519 polls, observed only state `R`, and recorded zero blocked samples. The soft-dirty reset and both pagemap reads succeeded, but bit 55 remained clear (`0x8100000000000000`) both after `pselect` and after a successful one-byte control write to the end of the same mapped page. The overlay still passed (`ret=115`, `overlay=1`); the route was stopped at step 33 before CFI/root, with `pipe-physrw-summary done=0 root=0`. RMG code 255 is the expected result for this guarded diagnostic. The phone stayed responsive, uptime increased, and `/dev/ksu` and `/proc/ksu` remained absent. The same-page control did not validate soft-dirty, so bit 55 cannot currently be used to infer whether `pselect` copied out. Next calibrate pagemap bit 55 on a known anonymous private mapping in the same process; keep the COW-only, one-attempt, stop-before-CFI/root gates.

### 2026-11-24 anonymous soft-dirty calibration

The next candidate adds a known-write control on a private anonymous page after the timing-sensitive COW attempt. It was built with the same NDK and COW-only/trace/512-MiB flags as `rmg-cow-only-512m-anon-softdirty-20261124.so` (141,440 bytes; SHA-256 `A985BE9DA9FF46578FAA119EECE3BA7F2D5CA732D489812F3B261333B942E7FC`). RMG imported the candidate from Downloads and ran it once through Shizuku with profile `r9q-G990BXXSLIZH2`; the route remained `diagnostic=cow-only`, `pte_mb=512`, and stopped before CFI/root. Record: `files/install-history/9f0f611a-2939-482f-a061-75d7b49a8750.json`.

The run took 113.5 seconds; controlled page preparation took 101.7 seconds. `pselect` returned 115 after 195 us, while the mprotect helper lasted 753 us but started about 1.3 ms before `pselect`, so these operations did not overlap in this run. The mapped COW page again reported bit 55 clear before and after the same-page write. More importantly, the anonymous control succeeded at every operation: anonymous mmap, `/proc/self/clear_refs` reset, initial pagemap read (`0x8100000000000000`, dirty 0), byte write, and second pagemap read. Its bit 55 also remained clear (`0x8100000000000000`). The monitor recorded 4,209 polls, only state `R`, and zero blocked samples; overlay geometry passed and the route remained gated (`step=33`, `done=0 root=0`). The phone stayed responsive on the expected firmware/kernel, with no KernelSU nodes.

ADB exposed `/proc/config.gz`; its config contains `CONFIG_PROC_PAGE_MONITOR=y` but no `CONFIG_MEM_SOFT_DIRTY` entry. Together with the successful anonymous write that failed to set pagemap bit 55, this makes soft-dirty unavailable or ineffective on this kernel build. Stop using bit 55 as evidence about the COW path. A later probe should use a different observable, such as validating file-backed versus anonymous mapping state or instrumenting the waiter state with a reliable kernel-side signal, while preserving the same stop-before-CFI/root gate.

### 2026-11-24 smaps-based COW confirmation

The next guarded candidate measures the target VMA's `Anonymous` and `Private_Dirty` counters in `/proc/self/smaps` before `pselect`, immediately after it, and after the existing post-call control write. It was built with NDK `27.1.12297006` and the same one-attempt, 512-MiB `RMG_DIAG_COW_ONLY=1` / `RMG_DIAG_COW_TRACE=1` guards as `rmg-cow-only-512m-smaps-20261124.so` (142,664 bytes; SHA-256 `6BC04F89B530FAE7737084CB68A5E5B5EF4E622280B3F3BA4CD058C2CF49EE73`). The local file was staged with a matching hash and selected by RMG. Record: `files/install-history/c99705f7-8eb9-4ec2-b820-f498be60003f.json`.

This run reached the desired overlap and blocked-waiter condition. `mprotect` started at `52333692770` us; `pselect` started 87 us later. The monitor saw the waiter in `D` at `52333693281` us. `mprotect` ended at `52333697440` us and `pselect` returned at `52333697513` us, giving 4.58 ms of overlap. The COW mapping's smaps counters changed from `Anonymous=0 kB, Private_Dirty=4 kB` before the call to `Anonymous=4 kB, Private_Dirty=4 kB` after it; the later control-write sample remained `4/4`. This is the first direct confirmation in these probes that `pselect` created a private anonymous page while the waiter entered uninterruptible sleep. The overlay passed (`ret=103`, `overlay=1`).

The monitor collected 2,465 samples, including one `D` sample (`first_state=R`, `last_state=D`). The COW-only guard skipped scheduling and stopped at step 33 before CFI/root; the physrw summary remained `done=0 root=0` and RMG returned its expected diagnostic code 255. The phone stayed online on the expected firmware/kernel, uptime increased, and `/dev/ksu` and `/proc/ksu` remained absent. This identifies the correct synchronization window; repeat the same guarded probe to establish reproducibility before considering any later stage.

### 2026-11-24 smaps COW repeat

The exact same local payload was run one more time under Shizuku with the same 512-MiB, one-attempt, COW-only profile. RMG record: `files/install-history/0b89c06f-31e4-4514-84dc-b2ae6c337d7f.json`. It again reported `smaps_before` as `Anonymous=0 kB, Private_Dirty=4 kB` and `smaps_after_pselect` as `Anonymous=4 kB, Private_Dirty=4 kB`; the after-control sample stayed `4/4`. This reproduces the COW mapping transition.

The blocked waiter did not reproduce because the timing window was missed: `mprotect` ran from `52662311033` to `52662311710` us (677 us), and `pselect` started at `52662316010` us, about 4.3 ms after `mprotect` ended. The monitor made 8,972 polls and saw only state `R`; overlay geometry passed (`ret=112`, `overlay=1`). The route stopped before CFI/root as configured, with `done=0 root=0` and the expected RMG code 255. The phone remained online and responsive with no KernelSU nodes. The two probes now establish that the COW write occurs reproducibly; the remaining nondeterminism is timing the mprotect lock window over the pselect call. Next improve the start synchronization so pselect begins while the mprotect helper is still inside the syscall, without enabling the scheduler or later exploit stages.

### 2026-11-24 synchronized mprotect start

A diagnostic-only release barrier now keeps the helper parked until the waiter is ready, releases it immediately before `pselect`, waits for the helper's mprotect-start marker, and gives it a 50-us headstart. The candidate is `rmg-cow-only-512m-sync-barrier-20261124.so` (142,760 bytes; SHA-256 `6DCA05B7DA993335965D53006F63E32BEB98983B7368634DCCF0A6F38A7AABCD`). RMG ran one attempt with `pte_mb=512 diagnostic=cow-only`; record `files/install-history/f98d1204-cd71-4426-9f6a-368dd90d43b8.json`.

The barrier aligned the syscalls: mprotect began at `53122065410` us, `pselect` began 50 us later, and mprotect ended at `53122066152` us. Their syscall windows overlapped for about 692 us. Smaps again changed from `Anonymous=0 kB` to `4 kB`, confirming the COW transition, and overlay geometry passed (`ret=109`, `overlay=1`). However, the waiter monitor saw no `D` state in 7,828 polls. This contrasts with the earlier `D` sample when the mprotect call lasted about 4.67 ms; the synchronized call here lasted only 742 us. The guard still skipped scheduling and stopped before CFI/root (`done=0 root=0`); RMG code 255 was expected. The phone stayed online at the expected kernel with no KernelSU nodes. The start barrier fixed launch skew, but the lock window was too short to reproduce the blocked state. A further diagnostic should extend the lock window while retaining the COW-only guard.

### 2026-11-24 repeated mprotect COW-only diagnostic

The next guarded build keeps the prefaulted anonymous mapping at 512 MiB and, only in the trace build, alternates its protection between `PROT_NONE` and `PROT_READ` until `pselect` returns. The waiter sets a stop flag immediately after the syscall, and the helper restores `PROT_READ` before exiting if needed. This was intended to offer repeated short mmap-lock windows during the COW copyout without adding CFI, scheduler, root, or physical-RW behavior. The build used `RMG_DIAG_COW_ONLY=1`, `RMG_DIAG_COW_TRACE=1`, and `RMG_DIAG_COW_PTE_MB=512`; candidate `rmg-cow-only-512m-repeat-lock-20261124.so` is 142,968 bytes, SHA-256 `CFAF203E7436917C2D021C9D6A674100EFE29E894B3341CDEBD8135864C5069B`. The candidate hash was verified after staging as the app's `local-exploit.so`.

RMG record `files/install-history/41e80fd2-77ad-4df6-b81e-816c1311a70c.json` ran one attempt on source commit `3095e6f2a0659570b5adc54a4cb6b121f96d2fa2`. Its log reported `diagnostic=cow-only`, one exploit attempt, and the new helper result `iterations=2 elapsed_us=8122`. Although the local payload label in RMG's log still named the earlier anon-softdirty candidate, the active app-private binary had been replaced with the new candidate and its SHA-256 matched; the trace and new iteration log line are consistent with execution of the repeated-lock build.

The aligned trace was:

| Event | Monotonic time (us) | Value |
| --- | ---: | ---: |
| `mprotect` loop before | 53,912,720,909 | 0 |
| `pselect` before | 53,912,720,961 | 0 |
| Monitor blocked sample | 53,912,721,149 | 2,447 polls |
| `pselect` after | 53,912,726,407 | 115 |
| `mprotect` loop after | 53,912,729,032 | 0 |

The `pselect` trace interval overlapped the helper interval for about 5.45 ms. The monitor recorded 2,447 polls and one blocked sample (`first_state=R`, `last_state=D`). The mapping's `/proc/self/smaps` counters changed from `Anonymous=0 kB, Private_Dirty=4 kB` to `Anonymous=4 kB, Private_Dirty=4 kB`, confirming the private COW page was created during this run. The overlay check passed (`ret=115`, `overlay=1`). Soft-dirty remained unusable on this kernel (`pagemap` bit 55 stayed clear, including the known-write calibration). The COW-only route stopped at step 33 before any CFI/root walk; the walk made zero calls, `done=0 root=0`, and RMG's code 255 is the expected result for this deliberately gated diagnostic.

The handset stayed online and boot-complete at fingerprint `samsung/r9qxeea/r9q:16/BP2A.250605.031.A3/G990BXXSLIZH2:user/release-keys` and kernel `5.4.289-qgki-32192773-abG990BXXSLIZH2`; `/dev/ksu` and `/proc/ksu` were absent. This is a useful reproduction of the COW transition and a blocked waiter while the lock helper is active. It still does not test whether the later CFI/PI-chainwalk stage succeeds, because that stage remained intentionally disabled.

### 2026-11-24 one-shot PI-trigger-only diagnostic

A separate build allowed the existing monitor to make at most one `sched_setattr_tid()` call when it sampled the waiter in state `D`. It retained the 512-MiB repeated-mprotect/COW trace and one-attempt limit. Two independent guards forced the route signal off and made `try_cfi_stage()` immediately refuse entry; therefore CFI, pipe physical-RW, and root installation were disabled. The candidate was `rmg-pi-probe-only-512m-20261124.so` (141,040 bytes; SHA-256 `58094496F36C7E26F7688D1B521F2DBE13E66F61277345AB5B7D08B3C3F15F4B`). The staged app-private binary hash matched before the run.

RMG record `files/install-history/86a359bc-fdc4-4bf7-975b-8bb555c77fd4.json` reached the readiness overlay setup, but remained `Running` after the handset stopped responding. Its last persisted lines are the overlay setup and preflight geometry output; it does not contain the COW trace, scheduler return/errno, or the guarded CFI-stop message. The exact point of the stall is therefore unknown, and the log does not prove whether `sched_setattr_tid()` was entered or returned.

ADB shell commands stopped responding, then the device disconnected and reconnected after a watchdog reset. Post-boot `ro.boot.bootreason` was `kernel_panic,sobugarm_watchdog_timer_exhardd_100s`; the phone returned to boot-complete on the same fingerprint and kernel. `/dev/ksu` and `/proc/ksu` were absent. Access to `/sys/fs/pstore` was denied, so no saved kernel crash trace was available to this unprivileged session. The app's install record remains incomplete. The PI-probe binary was removed from the active local payload; the previously tested COW-only repeated-lock binary was restored and its SHA-256 verified. Do not repeat the PI trigger until the watchdog stall is understood from kernel-side crash data or source analysis.

### 2026-11-24 source analysis of PI-trigger stall

**Superseded by the later source trace below:** the earlier interpretation that the no-change setter entered `rt_mutex_adjust_pi()` was incorrect. The PI-trigger-only attempt's missing final checkpoint still leaves its exact stall location unknown, but the later completed no-change run proves that this helper can return successfully without entering the PI chain at all. Therefore its success cannot be used to infer that a forged waiter was followed or that the PI chain walker stalled.

Read-only review of the matching Samsung 5.4.289 base source confirms that `sched_setattr()` reaches `__sched_setscheduler()` and calls `rt_mutex_adjust_pi(p)` after releasing the task's runqueue lock (`kernel/sched/core.c`, around line 5309). `rt_mutex_adjust_pi()` reads `task->pi_blocked_on` under `task->pi_lock` and passes its waiter and lock pointers to `rt_mutex_adjust_prio_chain()` (`kernel/locking/rtmutex.c`, around line 1125). This is the chain-walk the payload is trying to trigger through the stale PI pointer from the futex rollback bug.

The chain walker has a concrete unbounded retry path: if `raw_spin_trylock(&lock->wait_lock)` fails, it releases `task->pi_lock`, calls `cpu_relax()`, and jumps to `retry` (`rtmutex.c`, around lines 581–589). Its `depth` limit of 1,024 is incremented at the separate `again` label, so repeated failures in this `retry` path do not advance the limit. A contended or invalid forged lock pointer could therefore leave the syscall spinning until the platform watchdog resets the phone. This is a plausible explanation for the roughly 100-second watchdog reset, but the run record lacks the scheduler-entry checkpoint and kernel crash data, so it is not proven.

The PI-probe candidate called `sched_setattr_tid()`, which constructs a new `SCHED_BATCH` policy and a changed nice value (`monitor->seq * 7`, capped at 19). At the time, `sched_setattr_tid_nochange()` appeared to be a way to avoid a class/nice change that could provoke a reschedule IPI and overwrite the stale stack layout. The later source trace below shows that this assumption was wrong: with the target already at `SCHED_NORMAL` and unchanged nice, the no-change setter takes the kernel's early-return path and skips the PI chain. The incomplete changed-policy run still cannot be localized from its missing checkpoint; neither candidate is evidence that the intended PI chain was reached.

The incomplete RMG log ends after the readiness overlay's preflight select output. This build did not enable `RMG_DIAG_DURABLE_LOG`, and the trace buffer is flushed only after `pselect` returns. Thus the missing final trace cannot distinguish a stall in the COW/pselect path from one inside `sched_setattr()`/the PI chain-walk. `dmesg` and `/sys/fs/pstore` remain unavailable to the shell user. Continue with source review and, if another probe is considered later, first add a durable checkpoint immediately before the target scheduler syscall. The no-change helper cannot test PI-chain behavior on the observed path; a trigger that actually reaches PI adjustment must be understood from source and guarded before any new device attempt. Do not rerun the changed-policy candidate unchanged.

The static structure layout itself matches the base source: `rt_mutex_waiter` places `task`, `lock`, `prio`, and `deadline` at `0x30`, `0x38`, `0x40`, and `0x48`; `rt_mutex` places `wait_lock`, the cached waiters root/leftmost, and `owner` at `0x0`, `0x8`, `0x10`, and `0x18`. The payload's r9q target selects the legacy waiter layout and fills those offsets. It initializes the fake lock's `wait_lock` word to zero (unlocked), its cached waiter pointers to the fake waiter, and its owner word to `1`. Therefore, a repeated `trylock(wait_lock)` failure is only a plausible cause if the runtime pointer/content differs from the intended spray or another lock is contended; source inspection does not show that the designed fake lock begins locked.

The user'space `pselect`/overlay result checks only confirm the returned fd-set words and COW page accounting. They do not read back the kernel-stack `rt_mutex_waiter` that `rt_mutex_adjust_pi()` follows. The central unresolved check is still whether the live `task->pi_blocked_on` waiter and its `lock` pointer match this crafted layout at the instant of the scheduler call. Static offsets match; the runtime pointer and lock state remain unverified.

### 2026-11-24 durable no-change PI probe candidate

The PI-probe-only monitor called `sched_setattr_tid_nochange()` for the target rather than changing it to `SCHED_BATCH` with a new nice value. Durable checkpoints identify monitor entry, `sched_getattr` completion, `sched_setattr` entry, and `sched_setattr` return; each checkpoint is flushed with `fdatasync()`. These checkpoints distinguish a stall in the setter from a return, but **do not** establish whether the PI-chain walk ran. Matching kernel source shows this no-change call returns before that walk. The one-attempt limit, 512-MiB COW trace, forced-zero route signal, and refusal at `try_cfi_stage()` kept CFI, physical-RW, and root installation unreachable in this diagnostic.

Built with NDK r27c and `APP_TARGET_CFLAGS=-DRMG_DIAG_PI_PROBE_ONLY=1 -DRMG_DIAG_COW_TRACE=1 -DRMG_DIAG_COW_PTE_MB=512 -DRMG_DIAG_DURABLE_LOG=1`. The app payload is `rmg-pi-probe-nochange-512m-durable-20261124.so` (147,424 bytes; SHA-256 `A9E5509440A2BC5979D696AF2FDD5B316BF024561C06D966F4B48F57B3F2D30F`). The build succeeded with only existing unused-function warnings. Before the run, the binary was staged as the app-private active payload and its hash was verified. RMG's persisted selection label still read `rmg-cow-only-512m-anon-softdirty-20261124.so`, but the active `local-exploit.so` hash matched this PI-probe binary and the runtime guard reported `diagnostic=pi-probe-only`. This supersedes the earlier staging status below.

#### One approved run: `ec3c9cf4-4508-401f-ab78-6b7d0ece4483`

RMG executed exactly one attempt through Shizuku on the expected `r9q-G990BXXSLIZH2` profile and source commit `3095e6f2a0659570b5adc54a4cb6b121f96d2fa2`. The durable checkpoint file was pulled to `outputs/ec3c9cf4-4508-401f-ab78-6b7d0ece4483-checkpoints.log`; the RMG history JSON is saved at `outputs/ec3c9cf4-4508-401f-ab78-6b7d0ece4483.json`.

The controlled-memory preparation completed, followed by one repeated-lock/COW readiness attempt. The durable trace recorded both the consumer baseline scheduler setter and the readiness monitor's no-change `sched_setattr` call entering and returning. For the monitor call, `sched_getattr` completed, `sched_setattr` entered for TID 12603, and returned with result 0; its readiness checkpoint also recorded scheduler result 0 and 1,728 us elapsed. The monitor observed a blocked waiter sample (`first_state=82`/`R`, `last_state=68`/`D`, 1,244 polls). The COW operation completed (`mprotect=0`); the overlay returned 106 and passed its geometry check.

The route then reported `readiness-cow cfi miss ... step=33`, `readiness-cow-only-stop-before-cfi`, and ended the single attempt. Here, step 33 is the compile-time diagnostic guard's sentinel: it is assigned when the PI-probe guard forces the route signal off, before `try_cfi_stage()` is called. There was no CFI walk and no CFI failure to diagnose. The recorded predicates immediately before the guard were positive: the monitor observed a blocked waiter, overlay geometry passed, and the consumer walk reported `calls=1 success=1`. The call returned normally, but source review below establishes that this no-change call did not execute the PI-chain walk; the positive readiness predicates were insufficient to show the intended kernel state transition. CFI, pipe physical-RW, and root installation were not executed. RMG consequently marked the run `Failed` with code 255 at “Running kernel exploit,” which is the expected guarded diagnostic result, not a failed root installation.

ADB remained connected and `sys.boot_completed=1`; uptime advanced continuously through the attempt and follow-up checks. The reported boot reason remains the prior `kernel_panic,sobugarm_watchdog_timer_exhardd_100s` value, but there was no reboot during this run. The kernel remained `5.4.289-qgki-32192773-abG990BXXSLIZH2`. `/dev/ksu` and `/proc/ksu` remain absent, as expected because this diagnostic never attempts root or KernelSU loading.

Afterward the prior COW-only repeated-lock payload was restored as `local-exploit.so`; its verified SHA-256 is `CFAF203E7436917C2D021C9D6A674100EFE29E894B3341CDEBD8135864C5069B`. The PI-probe candidate remains backed up separately in app-private storage. No second attempt was made.

### 2026-11-24 prepared and run CFI-only candidate

At the time, the positive readiness predicates were treated as justification for examining the CFI transition. The later source trace supersedes that interpretation: the no-change scheduler call skipped the PI walk, so the readiness signal was a false positive and did not establish a successful PI/COW kernel transition. The guarded CFI-only candidate was nevertheless prepared and run once, as recorded below; the CFI write result must be interpreted as a downstream probe reached through an invalid readiness signal, not evidence that the exploit's fops transition succeeded. Physical-RW and root remained disabled.

Built using NDK r27c with `APP_TARGET_CFLAGS=-DRMG_DIAG_CFI_ONLY=1 -DRMG_DIAG_PIPE_AFTER_CFI=1 -DRMG_DIAG_COW_TRACE=1 -DRMG_DIAG_COW_PTE_MB=512 -DRMG_DIAG_DURABLE_LOG=1`. Candidate: `rmg-cfi-only-nochange-512m-durable-20261124.so`, 149,440 bytes, SHA-256 `A34FAE205F0C195286F2642B55C79FC5F7168588CB2C24BD52E7E04E00732B92`. It compiled with only unused-function warnings. It was staged and run once in the follow-up below.

This is a bounded stage, not a no-risk run. The CFI stage itself changes the ashmem/fops state. The existing failure cleanup explicitly avoids attempting a global `.data` restoration when there is no prepared pipe page, because hardened usercopy plus `panic_on_oops` can make that restoration fatal; the code retains the backing page and indicates that a reboot may be required. Earlier G990B runs already observed successful 35-byte CFI write/readback, followed by slow pipe-page preparation and, in one case, a watchdog reboot before this new gate. Before any run, keep the prior COW-only payload backed up, ensure ADB recovery and a reboot are available, and present this candidate for explicit approval. Do not enable pipe physical-RW or root installation in this attempt.

#### One approved CFI-only attempt: `10928a22-d159-4aa6-b026-f359b079b3ec`

Before launch, the candidate SHA-256 was verified as `A34FAE205F0C195286F2642B55C79FC5F7168588CB2C24BD52E7E04E00732B92` in the app-private active payload path. RMG used Shizuku, profile `r9q-G990BXXSLIZH2`, commit `3095e6f2a0659570b5adc54a4cb6b121f96d2fa2`, and one exploit attempt. The startup log confirmed `diagnostic=cfi-only pipe_physrw_root=disabled`.

The monitor again observed the blocked waiter, and the no-change scheduler setter returned `0` in 2,398 us. The COW route completed with `mprotect=0/0`, overlay success (`ret=115`, `overlay=1`), and readiness signal `1`. In light of the source trace below, that readiness signal was a false positive: the no-change setter skipped the PI walk, and no fops transition was established. The candidate entered `try_cfi_stage()`, but the first configfs write returned `-1` with `errno=22` (`EINVAL`). The durable checkpoints are `cfi-stage-start`, `cfi-stage-write` (`a=UINT64_MAX`, `b=22`), and `cfi-stage-fail` (`step=1`, `errno=22`). The code sets `cfi_dirty_seen` only after the full write succeeds, so this attempt recorded no successful CFI write and never reached `pipe-after-cfi-start`. It did not run pipe physical-RW or root installation. RMG marked the guarded attempt failed with code 255 at “Running kernel exploit”; this is a downstream CFI-write failure after an invalid readiness signal, not a root-stage result or proof the exploit's fops stage worked.

ADB stayed connected, `sys.boot_completed=1`, and uptime continued increasing through the follow-up checks. The kernel remained `5.4.289-qgki-32192773-abG990BXXSLIZH2`; the reported boot reason remains the older watchdog reason and no reboot occurred during this attempt. `/dev/ksu` and `/proc/ksu` remain absent. The pre-run COW-only payload was restored as `local-exploit.so` and its SHA-256 verified as `CFAF203E7436917C2D021C9D6A674100EFE29E894B3341CDEBD8135864C5069B`.

The app record and durable checkpoint capture are saved at `outputs/10928a22-d159-4aa6-b026-f359b079b3ec.json` and `outputs/10928a22-d159-4aa6-b026-f359b079b3ec-checkpoints.log`. Source review narrows the `EINVAL` to `configfs_write_once()`'s two stages: it can return `-1` if one of the `ASHMEM_SET_NAME` ioctls used to install the 128-byte configfs metadata blob fails, or if the subsequent `pwrite()` fails. The previous checkpoint covered the whole helper and did not record which substep failed. Prior G990B runs accepted the same 35-byte write/readback, so this is not yet evidence that the kernel rejected the route generally. A stage-specific tracing candidate is prepared below; the failed attempt was not repeated.

#### Stage-specific CFI write trace candidate (not staged or run)

Added durable `cfi-setname-enter` and `cfi-setname-return` checkpoints around each `ASHMEM_SET_NAME` ioctl used for the CFI write's metadata blob. The checkpoint values identify the full-prefix request or zero-position request and its result/errno. Additional checkpoints mark metadata failure, transition to `pwrite`, and its return value/errno. This distinguishes failure in the initial name ioctl, a later zero-prefix ioctl, and `pwrite`; if a call stalls, the last durable entry identifies the ioctl phase and position. Instrumentation is enabled only for the CFI-only diagnostic's `binwrite_target` payload-size write.

Built with NDK r27c and the same guarded flags as the preceding CFI-only candidate. The binary is `rmg-cfi-only-nochange-512m-name-trace-durable-20261124.so` (150,472 bytes; SHA-256 `9F9F8F6A33E70A8C129F2EFE1DF857B1ED754D14E50FDB0BAF1331F9D066F716`). Compilation succeeded with only unused-function warnings. Its embedded strings confirm the CFI-only identity and the route-timed stop before pipe physical-RW/root. It was staged and run once in the approved follow-up below.

#### One approved name-trace attempt: `a0c93f6e-09b2-4eb2-8abb-c6f34255ff79`

The run used the expected profile, source commit, Shizuku transport, and one-attempt limit. The readiness route again passed its userspace checks: the waiter was sampled blocked, no-change `sched_setattr` returned `0`, and the COW overlay/readiness predicates succeeded. The later source trace shows this was a false positive for PI-chain execution, so entry into the CFI stage did not establish that the fops transition had succeeded. All 120 `ASHMEM_SET_NAME` ioctls for the metadata blob returned `0` with `errno=0` (one full-prefix call and 119 zero-position calls). The subsequent `pwrite(fd, payload, 35, 0)` returned `-1` with `errno=22` (`EINVAL`). There is no `cfi-configfs-name-fail`; the checkpoints show `cfi-configfs-pwrite-enter` immediately followed by `cfi-configfs-pwrite-return` with `a=UINT64_MAX`, `b=22`. This identifies the exact failing substep: the configfs binary write itself, after successful ashmem metadata setup, though the route did not verify the intended fops state.

The failed pwrite left the CFI stage at step 1; the run did not reach `pipe-after-cfi-start`, pipe physical-RW, or root installation. RMG marked it failed with code 255 at “Running kernel exploit.” ADB remained connected, the phone stayed boot-complete on the expected kernel without reboot, and `/dev/ksu` and `/proc/ksu` remain absent. The prior COW-only payload was restored and hash-verified. The RMG record and complete detailed trace are `outputs/a0c93f6e-09b2-4eb2-8abb-c6f34255ff79.json` and `outputs/a0c93f6e-09b2-4eb2-8abb-c6f34255ff79-checkpoints.log`.


#### Source analysis of the CFI `pwrite()` `EINVAL`

Read-only review of the locally available Samsung kernel source narrows the likely source of the observed `EINVAL` to the VFS writable-operation check. In `fs/read_write.c`, `ksys_pwrite64()` passes the zero offset to `vfs_write()` when `FMODE_PWRITE` is set. `vfs_write()` returns `-EINVAL` immediately if `FMODE_CAN_WRITE` is clear, before calling `__vfs_write()` or the selected `.write` callback. `fs/open.c:do_dentry_open()` sets `FMODE_CAN_WRITE` after the device's open callback only when the resulting file operations contain `.write` or `.write_iter`.

This matches the stock ashmem definition: `drivers/staging/android/ashmem.c:ashmem_fops` contains `.read_iter`, `.llseek`, `.mmap`, and ioctl handlers, but no `.write` or `.write_iter`. On a normal ashmem open, `drivers/char/misc.c:misc_open()` replaces the generic misc fops with the fops currently stored in `ashmem_misc.fops`; that happens before `do_dentry_open()` decides whether the descriptor can write. The CFI stage opens a fresh ashmem descriptor after the route walk. Therefore, if that fresh descriptor still received stock ashmem fops, the exact observed `pwrite(..., 35, 0) = -1/EINVAL` follows directly, even though `open()` itself succeeded.

The forged table is intended to provide a non-null `.write` slot pointing at `configfs_write_bin_file`; the payload's table builder writes that slot explicitly. If that callback were reached with the expected zero offset, the reviewed `fs/configfs/file.c:configfs_write_bin_file()` / `fs/libfs.c:simple_write_to_buffer()` path does not normally explain `EINVAL`: the helper returns `EINVAL` for a negative position, while this `pwrite()` supplies offset zero. Other ordinary callback failures in the inspected code use different errors (for example `-ETXTBSY`, `-EFBIG`, `-ENOMEM`, or `-EFAULT`).

**Current assessment:** strongest explanation is that the CFI route signal was a false positive for installing the ashmem fops pointer, or otherwise did not leave a writable `.write` operation on `ashmem_misc.fops` when the CFI stage opened its descriptor. The route's `consumer_success` only means the `sched_setattr` call succeeded; `route_signal` is derived from that result (plus the monitor/overlay predicates on the current path). The subsequent `fops probe: open` only proves that open succeeded. Neither is a readback of `ashmem_misc.fops` or the opened file's `f_op`/`f_mode`. This diagnosis is strongly supported by the exact VFS errno and stock fops layout, but remains an inference: the run did not capture `f_mode`, `f_op`, or prove which callback handled the syscall, and malformed callback state is not fully excluded.

**Next safe step:** continue with static review and prepare a narrowly scoped diagnostic that can distinguish the selected fops/write slot from a false route signal before any further stage is enabled. Do not retry the exploit based only on the current readiness log. Any on-device attempt would need a separate explicit go-ahead; no new payload was built, staged, or run during this source analysis.

#### Fops-slot readback-only diagnostic

Added a compile-time `RMG_DIAG_FOPS_READBACK_ONLY` branch to `try_cfi_stage()`. After the readiness route signal, it uses the existing configfs read callback path to read only the eight-byte `ashmem_misc.fops` slot and compares that value to the expected `fake_fops` pointer. It returns before the configfs write, pipe-page preparation, physical read/write, or root stages. The build guard requires CFI-only mode and durable checkpoints; the existing CFI-only guard also requires the pipe-after-CFI stop configuration. Candidate startup text identifies `fops-readback-only` and explicitly states the later stages are disabled.

Built with the installed Android NDK 27.1.12297006 for API 30. The resulting candidate is `outputs/rmg-fops-readback-only-512m-durable-20261124.so` (149,144 bytes; SHA-256 `DE83C79120DB5FACE7CEE3765A0B92A2B3A50F686C4795CFACE0059A59F11A18`). Compilation succeeded with only unused-function warnings. The previous active COW-only payload was backed up before staging and restored after the attempt; its SHA-256 remains `CFAF203E7436917C2D021C9D6A674100EFE29E894B3341CDEBD8135864C5069B`.

#### One readback-only attempt: `c9891c1e-791a-46c2-8fe9-c73530566c7e`

RMG used Shizuku with the expected `r9q-G990BXXSLIZH2` profile and one exploit attempt. Startup output confirmed `diagnostic=fops-readback-only cfi_write_pipe_physrw_root=disabled`. The readiness predicates again signaled success (`calls=1`, `success=1`), but `configfs_read_once()` returned `0` bytes with `errno=0`; the initialized result stayed `0`, while the expected fops pointer was `ffffff809e231180`. The route then ended at diagnostic step 40. It never entered the CFI write, pipe physical-RW, or root stages. RMG marked the guarded attempt failed at “Running kernel exploit” with code 255; that is the intended result when the pointer readback is absent, not a root-stage failure.

This result materially strengthens the false-positive route diagnosis. The stock `ashmem_read_iter()` returns EOF when an ashmem area's size is zero, which matches the zero-byte, zero-errno result. By contrast, the intended fake table has `.read = configfs_read_file` and `.read_iter = NULL`; with the diagnostic's `needs_read_fill=0`, that callback should copy eight bytes from the requested kernel pointer. The observed EOF therefore strongly indicates that the fresh descriptor followed stock ashmem's `read_iter` path instead of the intended fake configfs `.read` callback. This still infers the selected fops from behavior rather than directly reading `file->f_op`, but it is independent confirmation that the previous route signal did not establish a usable fake fops table.

The handset remained boot-complete on kernel `5.4.289-qgki-32192773-abG990BXXSLIZH2`; uptime advanced through the attempt and `/dev/ksu` and `/proc/ksu` remain absent. The previous COW-only active payload was restored and hash-verified. Saved evidence is `outputs/c9891c1e-791a-46c2-8fe9-c73530566c7e.json` and `outputs/rmg-fops-readback-only-checkpoints-20261124.log`. Do not retry the CFI write unchanged. The next investigation should focus on why the PI/readiness route reports success without changing the fops used by newly opened ashmem descriptors.

#### Source trace: readiness route returns before the PI walk

The false-positive route now has a specific source-level explanation. `sched_setattr_tid_nochange()` first calls `sched_getattr()` and then submits those same scheduler attributes with `sched_setattr()`. In the matching kernel source, `sys_sched_getattr()` returns the target's policy/nice and any reset-on-fork flag, but does not set `SCHED_FLAG_UTIL_CLAMP`. The run checkpoint recorded policy `0` (`SCHED_NORMAL`) and the no-change `sched_setattr()` returned `0`.

In `kernel/sched/core.c:__sched_setscheduler()`, an unchanged policy with unchanged nice (and no util-clamp flag) takes the explicit “no changes” branch: it sets the reset-on-fork field, sets `retval = 0`, and jumps directly to `unlock`. That label releases the runqueue lock and returns. The later `rt_mutex_adjust_pi(p)` call is only on the changed-attributes path, after scheduler reconfiguration. So the successful no-change syscall does **not** invoke the PI-chain adjustment or the `rt_mutex` rb-tree erase/requeue operations that are supposed to write `ashmem_misc.fops`.

The payload's readiness `route_signal` does not detect that skipped path. It is computed from `monitor.saw_block`, `overlay_ok`, `calls > 0`, and `success > 0`. Here `overlay_ok` only checks that the pselect output fdset words equal `fake_task` and `fake_lock`; the successful syscall only means `sched_setattr()` returned zero. `route_payload_canary()` is called for logging, but it returns no value and its mutation result is not part of `route_signal`. In this attempt the log has no `canary MUTATED` record, yet `readiness-route-signal` is `1`; the fops readback then returns EOF. These results align with the kernel's no-op early return and explain why the CFI stage had been entered without an fops swap.


### User-confirmed device modification constraints

The user reports that flashing ROMs, kernels, or root solutions that write to system partitions hard-bricks this SM-G990B. Treat system-partition flashing as unsupported and out of scope for this device. Magisk/KernelSU modules that place files under `/data/adb` may be possible, but compatibility is not guaranteed: some modules are problematic with temporary-root frameworks. Any future module suggestion must therefore be reviewed for its actual write locations, boot-time behavior, and compatibility with the specific temporary-root setup before use. This is a user-provided device constraint, not independently verified here.

### Source-only scheduler trigger analysis

The matching scheduler source has one apparent way past the observed no-change early return without changing the target's policy or nice: submit `SCHED_FLAG_UTIL_CLAMP` together with the target's current `sched_util_min` and `sched_util_max` values. `sched_getattr()` copies those values to userspace but does not return either util-clamp flag. In `__sched_setscheduler()`, the unchanged-policy branch explicitly jumps to `change` when a util-clamp flag is present. That path reaches `__setscheduler_uclamp()` and then, after releasing the runqueue lock, calls `rt_mutex_adjust_pi(p)`. The r9q EUX defconfig enables `CONFIG_UCLAMP_TASK` and `CONFIG_UCLAMP_TASK_GROUP`.

This is a source-level candidate only, not a proven safe trigger. `uclamp_validate()` requires the resulting minimum to be no greater than the maximum and the maximum to be no greater than `SCHED_CAPACITY_SCALE`; the r9q defconfig sets `CONFIG_UCLAMP_TASK=y`. With both flags set, `__setscheduler_uclamp()` marks the requested values user-defined, and the surrounding path dequeues and re-enqueues the task even though policy/nice stay the same. In the reviewed generic code, `check_class_changed()` does not request rescheduling when scheduler class and priority remain unchanged; this suggests it avoids the known class/nice-change reschedule path, but does not establish runtime behavior for the Samsung vendor hooks, task state, or the crafted PI chain. The open source exports `android_rvh_setscheduler` and calls it from `__setscheduler()`, but the exact vendor callback behavior was not available in the inspected tree. This path can alter task clamp bookkeeping. Do not run it on the phone based only on this source review.

Before considering an isolated diagnostic, obtain evidence about the vendor hook behavior, then make the diagnostic stop unless it observes the intended PI-canary mutation and independently reads back the expected `ashmem_misc.fops` pointer. Both must be true before any CFI transition. No payload change, build, device command, or exploit attempt was made for this analysis.

### Evidence search: Samsung scheduler vendor hook

A read-only search of the local SM-G990B Android 16 kernel source found the hook declaration in `include/trace/hooks/sched.h`, its export in `drivers/android/vendor_hooks.c`, and the call `trace_android_rvh_setscheduler(p)` in generic `__setscheduler()` (`kernel/sched/core.c`, around line 5045). The declaration is a restricted hook and the export permits an external GPL module to attach a callback. A tree-wide search for `register_trace_android_rvh_setscheduler` and the hook name found no registration callback in the available source files.

This does **not** prove that the running firmware has no callback. A callback could be in a prebuilt vendor module or other proprietary object not included in the open-source source package. At the time of this initial source-only entry, the downloaded AP firmware tar's compressed images had not yet been inspected; the follow-up below records that later read-only image scan. No firmware image was flashed or modified; no phone command or exploit attempt was made.

#### Follow-up: packaged firmware image scan

The exact AP archive for `G990BXXSLIZH2` was read and its `boot.img.lz4`, `vendor_boot.img.lz4`, and `super.img.lz4` members were decompressed to temporary WSL files; the archive itself was not changed. The boot image is Android boot header v3 and contains a 52,490,752-byte kernel image plus a gzip CPIO ramdisk. The generic kernel image contains `android_rvh_setscheduler` in the generic vendor-hook name table and `__tracepoint_android_rvh_setscheduler` in the tracepoint symbol-name array. Those are hook-definition evidence only; the string `register_trace_android_rvh_setscheduler` does not occur in the kernel image. The boot ramdisk has eight CPIO entries, no `.ko` files, and no scheduler-hook string.

The vendor_boot v3 gzip CPIO contains 46 `.ko` files under `lib/modules/5.4-gki`; none contains `android_rvh_setscheduler`, `__tracepoint_android_rvh_setscheduler`, or `register_trace_android_rvh_setscheduler`. The Android sparse `super` metadata identifies `system`, `product`, `vendor`, and `odm` logical partitions. The vendor filesystem has 316 `.ko` files in `/lib/modules`; none contains those hook strings. The system filesystem's `/system_dlkm` directory is empty. A chunk-aware scan of all 10,961,651,840 bytes in the decompressed sparse `super` image also found zero occurrences of all three strings.

This is substantially stronger evidence against a module-based callback than the source-only search: none of the uncompressed module files in boot, vendor_boot, or vendor carries the tracepoint reference, and the whole sparse super payload has no matching string. It still is not proof of runtime absence. A callback could be linked into the monolithic kernel without an identifiable string reference, or live in a separately compressed/otherwise omitted payload. The available source and images reveal no registration callback, so the uclamp-flag path remains unvalidated against Samsung-specific runtime hook behavior. Do not treat this scan as permission to run the trigger. No image was flashed, no phone command was issued, and no exploit attempt was made.

### Prepared uclamp trigger-only diagnostic (run once; rebooted before trigger)

Added a compile-time `RMG_DIAG_UCLAMP_TRIGGER_ONLY` profile, requiring the one-attempt PI-probe profile, durable checkpoints, and fops-readback-only support. The readiness monitor now reads the target's current scheduler attributes and submits the same policy, nice, and clamp values with both `SCHED_FLAG_UTIL_CLAMP_MIN` and `SCHED_FLAG_UTIL_CLAMP_MAX` set. It rejects invalid clamp ranges before calling `sched_setattr()`. This is intended to bypass the matching kernel's no-change fast path while preserving policy/nice; the request still enters the scheduler dequeue/re-enqueue path and changes the clamp requests' `user_defined` bookkeeping.

After the route, the diagnostic records whether the PI-tree canary changed. Only if it changed does it perform the read-only `ashmem_misc.fops` pointer readback. It always forces the readiness route signal off and stops before CFI, pipe physical-RW, or root, even when both gates pass. Compile-time guards also make `try_cfi_stage()`, `install_pipe_physrw()`, `install_child_root()`, and `install_android_root()` refuse work in this profile.

Built with the local Android NDK r27c using `RMG_DIAG_PI_PROBE_ONLY=1`, `RMG_DIAG_UCLAMP_TRIGGER_ONLY=1`, `RMG_DIAG_FOPS_READBACK_ONLY=1`, `RMG_DIAG_DURABLE_LOG=1`, `RMG_DIAG_COW_TRACE=1`, and a 512-MiB readiness mapping. Candidate: `build/uclamp-trigger-only-512m-20261124/cve-2026-43499-app.so` (142,744 bytes; SHA-256 `6F20BF8654E1B4DE4982388199A62003E52FB4D3685B4242184D9F3E0E3C116E`). Compilation succeeded with only unused-function warnings. Its startup identity and checkpoint strings were confirmed in the binary. The run result is recorded below.

### 2026-11-24 uclamp diagnostic ended before scheduler trigger

The one-attempt candidate above was imported into RMG Next as `rmg-uclamp-trigger-only-20261124.so`; the device-side SHA-256 matched `6f20bf8654e1b4de4982388199a62003e52fb4d3685b4242184d9f3e0e3c116e`. RMG confirmed support profile `r9q-G990BXXSLIZH2`, Shizuku transport, and the local payload name. The binary printed `diagnostic=uclamp-trigger-only` and `cfi_write_pipe_physrw_root=disabled`. Its preload supervisor reported `attempts=1`, and the exploit child started as shell UID 2000. The target was on Android 16, build `G990BXXSLIZH2`, kernel `5.4.289-qgki-32192773-abG990BXXSLIZH2`.

The last captured live RMG output was still in controlled-mm setup: group attempt 49 had reached 16/34 objects. No `sched-uclamp-getattr`, `sched-uclamp-setattr`, COW-trigger, canary, or fops-readback checkpoint appeared before the ADB connection dropped. Therefore this run did **not** exercise the uclamp scheduler request, and provides no evidence about its effect or vendor-hook behavior. The crash occurred during the earlier reclaim / controlled-mm phase; the exact faulting instruction or kernel stage is unknown.

ADB reconnected after about a minute. `sys.boot_completed` was `1`, `ro.boot.bootreason` and `sys.boot.reason` reported `kernel_panic,null`, and the phone returned on the expected kernel. Battery saver was off before the run and remained off after recovery. `/dev/ksu` and `/proc/ksu` were absent. RMG did not persist the interrupted attempt in run history, and `/sys/fs/pstore` was not readable to the shell user, so no panic trace was recovered. No CFI write, pipe physical-RW, Android root, KernelSU load, or partition write was reached. The previous local payload selection (`rmg-cow-only-512m-anon-softdirty-20261124.so`) was restored after reboot. Do not retry this build unchanged; first reduce the controlled-mm setup exposure or instrument that phase so it can be isolated before attempting to reach the scheduler trigger.

### 2026-11-24 bounded controlled-mm collector diagnostic

To isolate the earlier collector, the app payload gained a `RMG_DIAG_CONTROLLED_MM` branch that performs one standalone collector pass and returns before page preparation, scheduler/COW, FOPS, pipe physical-RW, and root. This profile requires durable checkpoints and forces the preload supervisor to one attempt. It was built with `RMG_DIAG_CONTROLLED_MM_TRACE=1`, `RMG_DIAG_DURABLE_LOG=1`, and a 45-second `RMG_DIAG_MM_SCAN_TIMEOUT_MS` bound. Candidate `build/controlled-mm-isolate-20261124/cve-2026-43499-app.so` is 149,752 bytes, SHA-256 `2337FD9CC88857A89B4F37FCC19E829252BFC402B30470766E56295C2727CD98`.

RMG selected the exact G990B / 5.4.289 support profile, Shizuku transport, and local diagnostic filename. Its startup log confirmed `diagnostic=controlled-mm-only`, `scan_timeout_ms=45000`, `attempts=1`, and `fops_root=disabled`. KASLR discovery succeeded. The collector seeded 128/128 order-3 pages and ran 22 leak attempts, with each KernelSnitch child reaped successfully. It observed three candidate pages: the first reached 8/34 objects by attempt 8, a second reached 8/34 by attempt 21, and a third began at attempt 22. The scan timed out at 48,025 ms when beginning attempt 23 and returned no complete 34-object group (`retained=0/34`). The 45-second deadline is checked between leak operations, so the final child operation overshot it by about 3 seconds.

RMG reported the diagnostic as failed with code 255 because the deliberate stop path returns status 42; this is the expected stop-before-exploit result, not a crash or root result. ADB remained connected, boot uptime continued increasing, battery saver remained off, and the phone stayed on `5.4.289-qgki-32192773-abG990BXXSLIZH2`. `/dev/ksu` and `/proc/ksu` remained absent. The previous local payload selection (`rmg-cow-only-512m-anon-softdirty-20261124.so`) was restored afterward. The captured RMG log is `build/controlled-mm-isolate-20261124/rmg-live-log.txt`.

This run confirms that one standalone collector pass can survive the 45-second bounded scan, but it did not obtain a full order-3 page. The earlier reboot happened later in the exploit's separate page-preparation pass; this diagnostic neither reproduced that reset nor reached the scheduler trigger. The useful next source-level question is why the hinted collection loses page continuity and starts a second candidate group at attempt 14, then a third at attempt 22. Inspect the hint-match and reuse path before another device run; do not simply increase the scan deadline.

#### Trace and collector source review

Review of the saved trace and `port/util.c` shows that the diagnostic did not stall in hint matching or child cleanup. All 22 completed leak operations returned a candidate; the candidates accumulated in three different order-3 pages: 13 objects in the first page, 8 in the second, and 1 in the third. The group counters are emitted at 1, every 8 objects, and 34, so these totals are reconstructed from the contiguous successful attempts and the two page-transition points, not directly printed final counters. The collector timed out before a fourth candidate could be processed.

The hint is a page-local search optimization, not a request to allocate from that page. On a hinted miss, `controlled_leak_mm()` retries the same leaked task with a full KernelSnitch scan; the trace shows this fallback at attempts 14 and 22 (`hint-match found=0`, followed by `bruteforce-done found=1`). Each fallback discovered an allocation on another page, and the collector updated its hint to that newly observed page. The four-collision check and requirement for exactly one matching candidate within the hinted page are retained. This behavior is internally consistent and the evidence does not support weakening the match or treating a miss as a valid address.

The limiting factor in this first standalone run was page locality: the target `mm_struct` cache did not supply enough distinct objects from one order-3 page before the 45-second diagnostic bound. Per-leak candidate addresses and explicit duplicate-slot/page-transition outcomes were not logged, so the exact allocator reuse pattern could not be established from that run. The follow-up trace below adds those observations. Do not simply lengthen the on-device run or relax collision validation. The diagnostic closed its retained descriptors at exit and made no persistent kernel changes.

The seed path itself sends up to 128 socket-buffer messages while pinned to CPU 0, then closes both socket endpoints and yields four times. Its `sends=128/128` checkpoint confirms successful sends, but does not verify how many order-3 pages were allocated, when they were freed, or whether the `mm_struct` cache later reused those pages. The first run also lacked the helper CPU and per-candidate `(base, slot)` logs; the follow-up trace below adds those observations. The physical lifetime and reuse of the seed pages remain unmeasured. Keep the present four-collision/unique-match validation and do not change the seed, affinity, or retention behavior based on send counts alone.

### 2026-11-24 per-candidate allocator trace

Added trace-only log lines for each recovered candidate's address, order-3 base, slot, group count, hint, and observed CPU; page creation/reuse; duplicate slots; invalid candidates; and the helper's CPU before and after `kernelsnitch_find_collisions()`. The logging is behind `RMG_DIAG_CONTROLLED_MM_TRACE` and does not change the matching or selection logic. The one-attempt controlled-mm mode, durable checkpoints, and 45-second scan limit remained enabled; FOPS, physical-RW, and root remain unreachable in this build.

The candidate `build/controlled-mm-page-trace-20261124/cve-2026-43499-app.so` compiled with NDK r27c (150,648 bytes; SHA-256 `6C574691DD5FCA951B6515AF37B3BF1838A490ECA7722653FFB1E73B640581EB`). The on-device hash matched. RMG selected the exact firmware profile and local candidate; the log confirms `diagnostic=controlled-mm-only`, `scan_timeout_ms=45000`, `attempts=1`, and `fops_root=disabled`.

This run behaved differently from the preceding collector sample: all 26 successful candidates mapped to one order-3 page (`ffffff80b97b8000`), and all 26 occupied distinct slots. The observed slots were 6, 8–31, and 33; slots 0–5, 7, and 32 were not obtained. There were no duplicate-slot events and no page transition. The first candidate used a full scan, and attempts 2–26 all reported a successful hinted-page match. The collector timed out at 45,505 ms while beginning attempt 27, so it retained no complete 34-object group and returned the expected deliberate diagnostic failure (RMG code 255 / payload status 42).

The helper's `sched_getcpu()` samples were CPU 0 at both entry and exit for every collision-finding call. The collector-side sample after candidate 1 was CPU 2; later candidate-processing samples were CPU 0. That post-candidate sample is not the CPU at the `clone()` call that allocates the child `mm_struct`, so a future trace should log the parent CPU immediately before `clone_leak_child()` to close that gap. The run did not reboot: ADB stayed connected, `sys.boot_completed=1`, the expected 5.4.289 kernel remained active, battery saver was off, and `/dev/ksu` plus `/proc/ksu` were absent. The previous local payload (`rmg-cow-only-512m-anon-softdirty-20261124.so`) was restored. Captures are in `build/controlled-mm-page-trace-20261124/`, including `rmg-live-log.txt`, `rmg-checkpoint.log`, `rmg-window.xml`, and the progress screenshot.

### 2026-11-24 kernel allocator source and live cache evidence

The downloaded Samsung source bundle contains a 5.4.289 base tree and a small `G990BXXSLIZH2` update archive. The base `Makefile` reports `VERSION=5`, `PATCHLEVEL=4`, `SUBLEVEL=289`, matching the phone's running kernel release. The update archive's README says to apply it over `G990BXXUGIYJ1`; its file list changes USB, GPU, HID, `/proc` integrity, vsock, and SDP files, but contains no `mm/slub.c` or `kernel/fork.c`. Its `/proc` change only adjusts PROCA conditional compilation; the `proc_mem_open()` / `mem_release()` lifetime code is unchanged. The source inputs and extracted files are under `build/source-audit/`.

The phone's live `/proc/config.gz` reports `CONFIG_SLUB=y`, `CONFIG_SLUB_CPU_PARTIAL=y`, `CONFIG_ARM64_4K_PAGES=y`, and `CONFIG_NR_CPUS=8`; `CONFIG_SLAB_FREELIST_RANDOM` and `CONFIG_SLAB_FREELIST_HARDENED` are unset. Runtime `/proc/slabinfo` reports `mm_struct 476 476 960 34 8 ... slabdata 14 14`, meaning 960-byte objects, 34 objects per slab, and 8 pages per slab. This independently confirms the 4 KiB order-3 geometry used by the collector. In the matching 5.4.289 SLUB source, a 960-byte cache receives `cpu_partial=13` when CPU partials are enabled. The sysfs cache attributes exist but are not readable to the shell user; `/proc/slabinfo` is readable.

The 26 unique slots in the first trace and 21 unique slots in the follow-up both came from the same observed page, but the follow-up yielded a different subset/order and timed out before attempt 22. Partial slab occupancy and allocator/reclaim timing remain possible explanations; the evidence does not identify which slots belong to live address spaces. The follow-up also found unchanged global `mm_struct` active/total counts before seeding and after cleanup. This rules out a net aggregate count increase in that run, but not transient object churn or per-page ownership. Do not treat either the counts or the slot pattern as proof of a descriptor leak or of the missing slots' owners.

The follow-up trace resolved parent placement: the parent was restricted to CPU 0 immediately before each child `clone()`. Thus parent migration at the allocation point does not explain the incomplete group in that run. The 45-second timeout still prevents determining whether later candidates would have completed the page or transitioned elsewhere. A useful next diagnostic should distinguish per-attempt allocations/cleanup or provide a page-specific ownership signal; simply extending the timeout would add duration without resolving that ambiguity. Keep the collision and uniqueness checks unchanged.

### 2026-11-24 parent-affinity and cache-count diagnostic

Added two read-only observations to the bounded controlled-mm diagnostic: the parent CPU and allowed affinity mask immediately before each `clone_leak_child()`, and global `/proc/slabinfo` `mm_struct` active/total counts before seeding and after scan cleanup. The diagnostic still stops before page preparation, scheduler/COW, FOPS, pipe physical-RW, and root; matching and collector behavior are unchanged. Build `build/controlled-mm-occupancy-trace-20261124/cve-2026-43499-app.so` is 151,704 bytes with SHA-256 `E159CD20D384CAE70233E8596144DC3FE7526F48E65C6223B43C4998D139A00E`.

RMG confirmed the exact G990B firmware profile and ran one controlled-mm-only attempt with the 45-second scan bound. Each of the 21 child creations logged `parent_cpu=0`, `affinity_ok=1`, and `allowed_count=1 mask=0000000000000001`; CPU placement at the `clone()` call is therefore no longer an unknown for this run. All 21 candidates were on the same order-3 page (`ffffff80b97b8000`) and in distinct slots, in observed order: 24, 22, 21, 23, 20, 19, 18, 17, 15, 16, 14, 13, 12, 11, 8, 10, 6, 9, 28, 25, 26. There was no page transition or duplicate-slot event. The 45-second deadline was reached at 46,468 ms while starting attempt 22, so this trace did not establish whether later attempts would move to another page.

The durable checkpoints reported `mm_struct` `active_objs=476`, `num_objs=476` both before seeding and after descriptor cleanup. This shows no net change in those global aggregate counts across this run; concurrent system activity and slab-level identity are not observable from these samples, so it does not prove that no individual object was temporarily leaked or identify ownership of the slots that were not found. The diagnostic exited through its deliberate stop path (RMG code 255 / payload status 42). The phone stayed online on the expected kernel, with battery saver off; `/dev/ksu` and `/proc/ksu` were absent. The known local payload selection, `rmg-cow-only-512m-anon-softdirty-20261124.so`, was restored afterward. Captures are in `build/controlled-mm-occupancy-trace-20261124/`, including the live RMG log and durable checkpoint log.

### 2026-11-24 per-candidate cache trace and complete-page diagnostic

Added a trace-only `/proc/slabinfo` sample after every unique candidate was retained. This uses the same read-only parser as the before/after samples and does not alter collector selection or matching. Build `build/controlled-mm-cache-per-candidate-20261124/cve-2026-43499-app.so` is 152,064 bytes, SHA-256 `0E93B877F628B94CB20EA28DB25C2ADC2A9CF7E4A7BA7B49B268948C65E54046`. With the 45-second bound, the run found 31 distinct slots on `ffffff80b97b8000` and timed out at 46,767 ms before attempt 32. All 31 per-candidate cache samples read `active_objs=476`, `num_objs=476`. Slots 20, 21, and 24 were the only unobserved positions in that run. The phone remained online on the expected kernel, battery saver was off, and KernelSU remained unloaded. The prior local payload was restored.

Because the 31/34 result was close to a full page and still stopped at the deadline, a second collector-only build used a 60-second scan bound. Candidate `build/controlled-mm-page-completion-60s-20261124/cve-2026-43499-app.so` is 152,064 bytes, SHA-256 `50D3AF32B810158F2E3A644DCD35354BB9C462F6A02F8DCA3488FF27A761787D`. It found all 34 slots on the same page, in order: 15, 17, 16, 22, 18, 19, 23, 26, 25, 27, 28, 29, 30, 31, 32, 24, 33, 20, 21, 13, 14, 11, 12, 10, 9, 8, 7, 2, 6, 5, 4, 3, 0, 1. The final group reached `34/34`; the collector returned success (`mmcollect-only result=1`) with `retained=34/34` and total elapsed time 48,983 ms. All 34 per-candidate samples again read `476/476`, and the post-close sample remained `476/476`.

This confirms that the collector can assemble a complete order-3 `mm_struct` page on the phone's running kernel. The earlier 45-second stop was a time-bound miss, not evidence that three slots are permanently unavailable. The unchanged global cache counts are not a useful indicator of the retained-page contents at this resolution; they cannot be used to infer per-object ownership or the exact allocator lifetime. The diagnostic returned through the intentional status-42 stop path after collection, before FOPS, pipe physical-RW, or root. The phone remained booted on `5.4.289-qgki-32192773-abG990BXXSLIZH2`; battery saver stayed off, `/dev/ksu` and `/proc/ksu` remained absent, and the previous local payload selection was restored. Captures and UI logs are in `build/controlled-mm-cache-per-candidate-20261124/` and `build/controlled-mm-page-completion-60s-20261124/`.

The next risky boundary is `release_controlled_mm_slab_page()` and `prepare_controlled_kernel_page()`: they create trigger `memfd` children, close retained target/trigger descriptors to release slabs, then prewarm `io_uring` and submit reclaiming socket buffers. The source makes clear this goes beyond collection and exercises the page-reclaim path implicated by earlier reboot evidence. Review and instrument that transition separately, with an explicit stop before FOPS/root; do not run that reclaim-stage diagnostic until separately approved.

### 2026-11-24 controlled reclaim-stage probe

With approval, built and ran a one-pass `RMG_DIAG_CONTROLLED_RECLAIM_ONLY` diagnostic that invokes `prepare_kernel_page(PAGE_PAYLOAD_FOPS)` under `CONTROLLED_RECLAIM_PROBE=1`. It records durable checkpoints across collection, creation of trigger descriptors, target/trigger closure, `io_uring` prewarm/spectrum, and socket-buffer sends. The branch returns status 42 after the probe and before the PI walk, FOPS trigger, pipe physical-RW, or root. Build `build/controlled-reclaim-only-20261124/cve-2026-43499-app.so` is 154,872 bytes, SHA-256 `93BAFA1C03F8FBE3232C5AB7023673AFC57994AFAC3D84F52E4335C9BC680831`.

The collector again completed all 34 objects on `ffffff80b97b8000`. The reclaim stage created 816 trigger `memfd` descriptors (24 slabs × 34 objects) plus anchor fd 855, closed all 33 target descriptors, then closed one trigger descriptor from each of 24 trigger groups. `io_uring` prewarm created eight rings. The spectrum stage succeeded for entry sizes 512 through 8192 (five successful sprays); the 16384 and 32768 setups returned `ENOMEM` (`errno=12`). The socket-buffer phase sent 251 of 256 messages. The slab-count check passed: `mm_struct` slabs decreased from 39 to 38. The probe then logged `controlled reclaim probe complete; skipping PI walk` and returned deliberately before the subsequent exploit stages.

RMG showed code 255 / payload status 42, which is the expected diagnostic stop result, not a reclaim failure. ADB stayed connected, `sys.boot_completed=1`, the phone remained on `5.4.289-qgki-32192773-abG990BXXSLIZH2`, battery saver stayed off, and `/dev/ksu` plus `/proc/ksu` remained absent. The known local payload `rmg-cow-only-512m-anon-softdirty-20261124.so` was restored. The UI trace and durable checkpoints are in `build/controlled-reclaim-only-20261124/`, including `rmg-reclaim-live-log.txt` and `rmg-reclaim-checkpoint.log`.

This probe demonstrates that the controlled collector and page-reclaim stage both complete on the current firmware without rebooting, and that the target slab count decreases as expected. The remaining untested boundary is the subsequent PI walk / page-use path; it was intentionally not entered in this run. Do not interpret this result as evidence that FOPS, physical-RW, root, or KernelSU loading works.

### 2026-11-24 one-shot uclamp PI-chain trigger diagnostic

Built a fresh one-attempt diagnostic from the current source using NDK `27.1.12297006` and explicit `APP_TARGET_CFLAGS=-DRMG_DIAG_PI_PROBE_ONLY=1 -DRMG_DIAG_UCLAMP_TRIGGER_ONLY=1 -DRMG_DIAG_FOPS_READBACK_ONLY=1 -DRMG_DIAG_DURABLE_LOG=1 -DRMG_DIAG_COW_TRACE=1 -DRMG_DIAG_COW_PTE_MB=512`. Candidate: `build/pi-chain-trigger-only-20261124/cve-2026-43499-app.so` (142,904 bytes; SHA-256 `A10034222CF3251DFE5760D4D2F46ACC74C1A89BC42D6154FE53DB5F1625BA06`). The compile-time uclamp profile uses one attempt, forces the route signal off, and refuses CFI, pipe physical-RW, and root. If the PI-tree canary changes, its only downstream action is an eight-byte read-only `ashmem_misc.fops` readback.

RMG used the exact `SM-G990B` / kernel `5.4.289` profile and Shizuku transport. The source-controlled local payload slot was staged with the candidate after preserving the prior file; the active candidate hash was verified before launch. The app's displayed payload label remained `rmg-cow-only-512m-anon-softdirty-20261124.so`, so that label alone is not evidence of the active bytes. The run was explicitly one attempt.

Durable checkpoints show KASLR discovery succeeded, the controlled collector completed all 34 entries, and the 512-MiB readiness region was prepared. The monitor observed the waiter blocked and entered the changed `sched_setattr` request with `SCHED_FLAG_UTIL_CLAMP_MIN|MAX` (`sched_flags=96`). The last checkpoint is `readiness-mprotect-start`; neither `sched-uclamp-setattr-return` nor the later pselect completion, PI canary, or FOPS-readback checkpoint appeared. A contemporaneous `/proc/18060/status` sample showed the monitor thread in state `R` and `/proc/18060/wchan` reported `0`, consistent with a CPU spin in the kernel scheduler path. This is consistent with the previously identified `rt_mutex_adjust_prio_chain()` retry-loop hypothesis, but without a kernel stack or pstore trace the exact instruction and pointer remain unproven.

The handset later returned on the expected firmware and kernel with `sys.boot_completed=1`; `ro.boot.bootreason` reported `kernel_panic,sobugarm_watchdog_timer_exhardd_100s`. Battery saver reported off. No FOPS write, physical-RW, root installation, or KernelSU load was reached. `/dev/ksu` and `/proc/ksu` remain absent. The original local payload was restored and verified by SHA-256 `A985BE9DA9FF46578FAA119EECE3BA7F2D5CA732D489812F3B261333B942E7FC`; the temporary staged phone copy and backup were removed after restoration. Checkpoints and device captures are in `build/pi-chain-trigger-only-20261124/`, including `rmg-pi-chain-checkpoint.log`.

Do not repeat this uclamp trigger unchanged. The successful entry into `sched_setattr` followed by a watchdog reset establishes that the changed scheduler request reaches a hazardous path, but it does not establish that the forged waiter/lock pointers are valid. Any next investigation should remain source- or crash-trace-focused until the runtime waiter, lock address, and `wait_lock` state can be safely observed.

### Source-level follow-up: scheduler trigger preceded the intended pselect

The `RMG_DIAG_COW_TRACE` path in `do_readiness_pause_route()` enables `monitor.armed` before launching the mmap-lock helper and before the target thread enters `readiness_pselect_asm()`. `readiness_block_monitor_thread()` interprets the first observed `/proc/self/task/<tid>/stat` state `D` as the COW wait and immediately calls `sched_setattr_tid_uclamp_nochange()`. That state check identifies an uninterruptible sleep, but not which syscall or fault caused it.

The saved run checkpoints order the events as follows: `readiness-monitor-blocked` at 9134.036, `readiness-monitor-sched-enter` at 9134.037, `sched-uclamp-setattr-enter` at 9134.038, then `readiness-mprotect-start` at 9134.038. There is no `readiness-cow-entered` or `readiness-pselect-start` checkpoint. In the source, both markers precede the `readiness_pselect_asm()` call, and the monitor is already armed before either. Therefore this attempt triggered the scheduler request before the intended COW pselect was proven to be in flight. The synthetic waiter fields may not yet have occupied the scheduler-visible stack when the PI adjustment began.

This is a more specific explanation for the run than attributing the watchdog directly to contention on the planned fake lock. The kernel's `rt_mutex_adjust_prio_chain()` does have a `raw_spin_trylock(&lock->wait_lock)` retry path with no depth increment on each retry, so a persistent spin is still compatible with the observed `R` state and watchdog reset. But because the trigger was early, the actual waiter and lock pointers may have been the ordinary pre-overlay values. The run produced no canary or kernel-stack readback, so it does not identify which lock was retried.

The local diagnostic source now separates passive sampling from permission to trigger. The monitor can sample before pselect, but `D` before the gate records `readiness-monitor-pre-gate-block` and aborts the route before pselect or `sched_setattr`; cleanup stops the mprotect helper and joins both helper threads. After the durable `readiness-pselect-start` checkpoint, the route opens `trigger_gate`. The monitor requires an `R` sample after that gate before accepting a later `D` sample; a `D` state without that post-gate runnable observation records `readiness-monitor-block-before-runnable` and aborts the trigger. The intended order is therefore pselect-start → trigger-gate-open → post-gate-runnable → monitor-blocked → sched-enter. The change was subsequently built and exercised once as recorded below.

### 2026-11-24 gated uclamp diagnostic run

Built with NDK `27.1.12297006`, target `r9q-G990BXXSLIZH2`, and flags `RMG_DIAG_PI_PROBE_ONLY=1`, `RMG_DIAG_UCLAMP_TRIGGER_ONLY=1`, `RMG_DIAG_FOPS_READBACK_ONLY=1`, `RMG_DIAG_DURABLE_LOG=1`, `RMG_DIAG_COW_TRACE=1`, and `RMG_DIAG_COW_PTE_MB=512`. Candidate: `build/uclamp-gated-20261124/cve-2026-43499-app.so` (143,448 bytes; SHA-256 `5FDBCFC736A525C7EF74E69DBAA6E2FEC3C5DA86C3462BC5147A8566CB1F6999`). The build succeeded with only unused-function warnings. The binary contains the uclamp-trigger-only identity, pre-gate abort, pselect-start/gate, post-gate-runnable, and stop-before-CFI checkpoints. This diagnostic profile disables CFI, pipe physical-RW, and root installation.

RMG Next imported this local payload and launched one run through Shizuku on the expected Android 16 firmware and kernel. The controlled-mm scan completed with three groups and one selected group (`mmcollect-scan-end a=3 b=1`). In the readiness route, the monitor observed the waiter in `D` at poll 4,507 before `readiness-cow-entered`, `readiness-mprotect-start`, or `readiness-pselect-start`. It recorded `readiness-monitor-pre-gate-block`; the main route then recorded `readiness-pselect-abort-pre-gate-block`. The mmap helper stopped and completed cleanup (`readiness-mprotect-done a=0 b=0`, elapsed 1,879 μs), and the route returned without opening the trigger gate. There is no `readiness-pselect-start`, `readiness-monitor-trigger-gate-open`, post-gate runnable sample, `readiness-monitor-sched-enter`, or `sched-uclamp-setattr-enter` in this run.

This confirms the new guard caught an early `D` state and prevented the hazardous scheduler request. It also shows that `/proc/<tid>/stat == D` alone is not evidence that the target is blocked in the intended pselect COW fault: the state was observed before the route's mmap helper began mprotect. The available trace does not identify what the target thread was waiting on at that moment. The RMG UI reported code 255 at “Running kernel exploit” because the guarded diagnostic returned without acquiring temporary root; it did not report a phone crash. ADB stayed connected, uptime continued increasing, the fingerprint and kernel remained unchanged, battery saver remained off, and `/dev/ksu` and `/proc/ksu` were absent.

The durable checkpoint copy is `build/uclamp-gated-20261124/rmg-pi-chain-checkpoint.log` (SHA-256 `19E058279214CA2FB1099C0EABE33611A52A98342975703B40AB30B5E9A4B8E8`); RMG's result UI is captured in `build/uclamp-gated-20261124/rmg-run-result.xml` and `phone-run-result.png`. The original local RMG payload, `rmg-cow-only-512m-anon-softdirty-20261124.so`, was restored and its SHA-256 matches `A985BE9DA9FF46578FAA119EECE3BA7F2D5CA732D489812F3B261333B942E7FC`. The temporary candidate and restore copies were removed from the phone's Downloads folder. Do not repeat the scheduler trigger based on this run; the next source investigation should identify the target TID's pre-gate wait and consider arming passive sampling closer to the pselect transition.

### 2026-11-25 gated uclamp retry: PI route stopped before readiness COW

The same guarded candidate was selected again from RMG Next's local-payload setting: `rmg-uclamp-gated-retry-20261124.so` (143,448 bytes; SHA-256 `5FDBCFC736A525C7EF74E69DBAA6E2FEC3C5DA86C3462BC5147A8566CB1F6999`). RMG reported the expected `r9q-G990BXXSLIZH2` profile, Shizuku transport with permission granted, and `diagnostic=uclamp-trigger-only`; it also reported `cfi_write_pipe_physrw_root=disabled`. The preload supervisor and exploit child each had one attempt. The child ran as shell UID 2000. The handset remained on Android 16 / `G990BXXSLIZH2` / kernel `5.4.289-qgki-32192773-abG990BXXSLIZH2`.

KASLR discovery succeeded using tracefs. The controlled order-3 seed sent 128/128 messages and the collector found a complete 34-object group. The controlled reclaim stage reported `sends=251/256`, `anchor=855`, and slab count 38→37. Kernel-page preparation took 153,013 ms. The PI path then reported `CMP_REQUEUE_PI ret=-1 errno=35 (EDEADLK)`, signalled waiter TID 9386, and completed one mmap-locker iteration in 5,398 μs. PI sanitization subsequently reported `lock=-1 errno=110` and `unlock=-1 errno=1`; `readiness-cow route done calls=0 success=0 step=0 errno=0`. The overlay handler ran, but pipe physical-RW reported `done=0 root=0`, and the sole exploit attempt ended with status 1 / RMG code 255.

The durable trace confirms the same pre-gate abort as the previous run. At 4520.952 the monitor armed for waiter TID 9386. At 4520.962 the locker thread was ready, then the monitor recorded `readiness-monitor-pre-gate-block a=9386 b=4577` before the pselect gate opened. The route marked `readiness-cow-entered`, recorded `readiness-pselect-abort-pre-gate-block a=1 b=68` (`68` is `'D'`), stopped the 512-MiB mprotect helper successfully (`ret=0`, 5,398 μs), and ended with `readiness-route-done a=0 b=0`. There is no `readiness-pselect-start`, `readiness-monitor-trigger-gate-open`, post-gate runnable sample, or scheduler checkpoint. This directly establishes that the guarded candidate again detected `D` before pselect and refused to trigger `sched_setattr`.

The checkpoint was written to `/data/local/tmp/rmg-pipe-oracle-checkpoint.log`; its local copy is `build/uclamp-gated-20261124/rmg-retry-checkpoints.log` (SHA-256 `06FC3FC25B5DAAEF5D2B128858599E1D624A3FDFFC2848C453FF5E6E4AF58AC5`). The common filename is historical and misleading, but the `run-start` record and timestamps identify this retry. The full RMG UI log is `build/uclamp-gated-20261124/rmg-retry-result.log` (SHA-256 `737E7C44641EE096E0667E4A4D5A0E84640808DFEBE8912FA30552B26E98B0C1`), and the result screen is `build/uclamp-gated-20261124/rmg-retry-result.png`.

RMG reported the exploit failure and never reported KernelSU loaded. Afterward the phone remained booted and ADB-responsive; `/dev/ksu` and `/proc/ksu` were absent. The original local payload was restored and verified by its expected SHA-256 `A985BE9DA9FF46578FAA119EECE3BA7F2D5CA732D489812F3B261333B942E7FC`; temporary candidate and staging files were removed from Downloads. Do not rerun this same image unchanged. Source review shows the `errno=110` sanitizer timeout is expected in this design: `sanitize_holder_thread()` intentionally holds `f_pi_sanitize` until the route exits, while `route_pi_sanitize()` tries to take it with a 50-ms timeout. That line is not evidence of an invalid lock address or the cause of the pre-gate abort. The next useful diagnostic is to explain why `/proc/self/task/9386/stat` reports `D` while the signal handler is entering the readiness route, before the helper's mprotect and before pselect. Keep the trigger gate closed on all pre-gate samples; add console-visible checkpoints for handler entry/current TID, waiter TID, monitor first state, and the waiter’s `wchan`, then export the existing durable trace from `/data/local/tmp` after the run. Preserve the one-attempt and root-disabled guards.

### 2026-11-25 waiter-state context diagnostic

Added durable signal-handler identity, a console-visible `/proc/self/task/<tid>/wchan` sample on pre-gate `D`, a console-visible pre-gate monitor summary, and flushing of the existing microsecond COW trace on that abort path. The candidate retained the same compile-time profile: one exploit attempt, the `pselect`/uclamp trigger gate, and CFI, pipe physical-RW, and root disabled. It was built with the Windows-hosted NDK `27.1.12297006` using its `aarch64-linux-android30-clang` wrapper: `build/uclamp-state-context-20261125/cve-2026-43499-app.so` (144,424 bytes; SHA-256 `C648289F8519CA5D995D7C1B9FA29C8A76FC2A309297BD0E10BDD7FF9F7B6D0A`). The new handler and wchan log strings and explicit root/physical-write guards were confirmed in the binary.

RMG used the expected firmware profile and Shizuku. The collector completed one 34-object group; controlled reclaim reported 251/256 sends and slab count 38→37. The route checkpoint `readiness-signal-handler-entry a=30762 b=30762` confirms that the monitored waiter TID was the same TID running the signal handler. The monitor's first state was `R` (COW trace event 4, value 82), then it saw `D` at poll 4,503 with the trigger gate closed. Its `wchan` sample was `wait_on_page_bit_common`. The blocked sample preceded the mprotect helper start by 5,834 μs; mprotect later completed normally in 2,391 μs. The trace ends with `readiness-pselect-abort-pre-gate-block a=1 b=68` and `readiness-route-done a=0 b=0`. No pselect start, gate open, post-gate `R`, uclamp `sched_setattr`, canary/readback, CFI, physical-RW, or root stage followed. RMG returned code 255, while the phone stayed booted and ADB-responsive; `/dev/ksu` and `/proc/ksu` remain absent.

This directly confirms that the early `D` is not caused by the mprotect helper or the pselect syscall. It also rules out a wrong monitor TID: handler TID and monitored TID match. The `wchan` narrows the state to a page-bit wait, but does not identify which earlier operation entered that wait. Between the monitor's first `R` sample and its `D` sample, the route performs the COW-trace probes (`/proc/self/clear_refs` and `/proc/self/smaps`) and starts the mmap-locker thread. The current trace does not timestamp those operations individually, so attributing the wait to one of them remains an open hypothesis.

The checkpoint copy is `build/uclamp-state-context-20261125/rmg-state-context-checkpoints.log` (SHA-256 `ED351AAB74E2BC7F9E99A0E8F89A7C4446D2A53FAEDFEA5EA6C3D328690C06A`). The full RMG output is `build/uclamp-state-context-20261125/rmg-state-context-result.log` (SHA-256 `DEDDDB9A5C08D290E508B8E6885F54E105B5BA6AFCC468E6693E478BE7871037`); the result screen is `build/uclamp-state-context-20261125/rmg-state-context-result.png`. The original local payload was restored with the expected SHA-256 `A985BE9DA9FF46578FAA119EECE3BA7F2D5CA732D489812F3B261333B942E7FC`, and temporary staging files were removed from Downloads.

Next, instrument completion and duration of `readiness_clear_soft_dirty()`, `readiness_read_smaps()`, and mmap-locker thread creation, while keeping the scheduler monitor disarmed during those pre-pselect probes. Arm it only when the main route is about to open the pselect gate; retain the post-gate-runnable requirement and unconditional abort on any `D` before that gate. This separates diagnostic page waits from the actual pselect COW wait without loosening the trigger guard.

### 2026-11-25 deferred-checkpoint uclamp diagnostic

The follow-up candidate defers durable checkpoint file I/O during the monitor-to-pselect critical window, retaining those events in the in-memory trace and flushing after monitor/helper cleanup. This tests whether checkpoint `open`/`write`/`fdatasync` activity was contributing to the earlier pre-gate page-bit wait. Candidate `build/uclamp-deferred-log-20261125/cve-2026-43499-app.so` is 145,608 bytes (SHA-256 `CE6973CD57496E5FAE5918CB016D0FF39E8859F4DB594601B36FA1A6BA273C97`). Its compile-time profile remains one attempt with CFI, pipe physical-RW, and root disabled.

The one RMG/Shizuku attempt completed the controlled reclaim and reached the intended pselect path. The pre-probes completed (`clear_refs` 819 μs; smaps reported `Anonymous: 0 kB`, `Private_Dirty: 4 kB`), the mmap-locker completed two iterations in 1,902 μs, and pselect returned 112 after 17,300 μs. The monitor saw 687 polls, one blocked sample, and the scheduler no-change request returned `0/0`. The overlay geometry check passed. The COW proof showed the target file-backed mapping's soft-dirty bit stayed clear, while smaps changed from 0 to 4 kB anonymous and from 4 to 4 kB private dirty; this confirms a private COW page appeared but does not prove the intended kernel waiter/PI-chain mutation occurred.

Crucially, `readiness-cow walk ... success=1` is only the consumer walk routine's return counter, not proof that the target structures changed. The subsequent non-destructive `MSG_PEEK` canary returned `0` (no expected lock/PI-tree words changed), so fops readback was skipped. The diagnostic explicitly stopped at step 42 before CFI; pipe physical-RW/root remained disabled. RMG displayed code 255 at “Running kernel exploit” because the guarded payload intentionally ended without temporary root. This was not a successful KernelSU load. `/dev/ksu` and `/proc/ksu` are absent; Android remains booted and ADB-connected.

The source confirms that the guarded uclamp helper calls `sched_getattr()` for the monitored TID, retains the returned policy, nice, and utilization-clamp min/max values, then sets `SCHED_FLAG_UTIL_CLAMP_MIN | SCHED_FLAG_UTIL_CLAMP_MAX` (numeric flags value 96) before calling `sched_setattr()`. This is intended to bypass the unchanged-policy/no-clamp-flags early return while keeping the values unchanged. The route log's `sched=0/0` is consistent with a successful syscall, but the deferred checkpoint file contains no detailed `sched-uclamp-current` or `sched-uclamp-setattr` records, so the actual per-thread values for this run are not captured. More importantly, the return value is not evidence that the scheduler performed the intended PI-tree writes: the independent canary remained unchanged.

Artifacts: checkpoints `build/uclamp-deferred-log-20261125/rmg-deferred-checkpoints.log` (SHA-256 `B76D1CFE98F1AD7B8E77CB9BBBF55DE2CEA85DE14DFBA1A6F3CE3183CCE4D63E`), full UI output `build/uclamp-deferred-log-20261125/rmg-deferred-result.log` (SHA-256 `D533EBE46D7AA1C86CEF69BE9F256ED1D20CE43572725DD45EBF588A3042313E`), and result screenshot `build/uclamp-deferred-log-20261125/rmg-deferred-result.png` (SHA-256 `CC1365731466E56F4D371CD17B5126F9FB2FA2B0218E7938537D455F184D9FD8`). This result supersedes the prior proposal to add pre-probe timestamps: those probes now complete successfully, and deferred logging moves the run to pselect. Do not proceed to CFI, physical-RW, or root until an independent canary proves the intended PI-tree mutation. The original `rmg-cow-only-512m-anon-softdirty-20261124.so` payload is now selected in RMG Settings, and both temporary Downloads copies (`rmg-uclamp-deferred-log-20261125.so` and the staged original payload file) have been removed. No second attempt was made.

### 2026-11-25 source audit: scheduler path and partial waiter signature

The downloaded Samsung package contains a small `G990BXXSLIZH2_kernel.tar.gz` update archive (about 240 kB), not a complete kernel tree. Its included build notes say to start from the G990BXXUGIYJ1 kernel source and then apply this update. The update archive lists changes under USB, GPU, HID, SDP, procfs, and vsock; it has no `kernel/sched/core.c`, `kernel/locking/rtmutex.c`, or futex source changes. I inspected those files in the package's base `Kernel.tar.gz`, and the exact-firmware update list does not replace them. Treat the scheduler/rtmutex source reading as Samsung's documented base-plus-update source, not as a direct comparison against the running device's vendor-modified binary.

In that source, `__sched_setscheduler()` takes the `change` path for an unchanged policy when `SCHED_FLAG_UTIL_CLAMP` is present; after dequeue/re-enqueue it calls `rt_mutex_adjust_pi(p)`. That routine still returns immediately if `p->pi_blocked_on` is null or points at the task's own waiter. Thus the uclamp flags make the call eligible to reach PI adjustment, but they do not prove the waiter-chain prerequisites were met. This matches the observed successful syscall while leaving the actual chain traversal unproven.

The deferred-log attempt's preflight `select` returned 112 and verified all three crafted result words: `w1=fake_task`, `w2=fake_lock`, and `w3=page_base+LOCK_OFF+0x100` (`ffffff814e582310` for that run). After the timing-sensitive pselect route, `w1` and `w2` remained exact, while `w3` was `ffffff8100000078`. The source comment says the walk's `[7]` operation overwrites the low 32 bits of result word 3 with `0x78`; the observed value is consistent with that stack-waiter signature. It is evidence of a partial stack/result-word effect, not independent proof that the fake lock's waiter tree or fake task's PI tree changed. The separate non-destructive skb canary still returned zero, and no fops readback was attempted.

The remaining gap is narrower: explain why the expected stack/result-word signature can appear while the payload-page canary sees no fake-lock or fake-task tree mutation. Source review should compare the real `pi_blocked_on`/waiter prerequisites with the fake lock/task structures and verify that the `MSG_PEEK` canary reads the exact reclaimed payload instance and offsets. Do not infer that the PI chain succeeded from `sched_setattr()==0`, pselect's return value, or the `0x78` signature alone. Keep CFI, physical-RW, and root disabled; no new phone attempt was made during this source audit.

One concrete canary blind spot is now instrumented in source: `reclaim_receiver_fd()` is the receiving end of an `AF_UNIX, SOCK_STREAM` socketpair, but `route_payload_canary()` performs one nonblocking `recv(MSG_PEEK)` and previously logged neither its byte count nor its scan coverage. A negative result therefore did not establish that the entire queued reclaim stream was inspected. The local source now reports bytes returned/requested, complete order-3 chunks scanned, trailing bytes, and warns when coverage is incomplete; this is logging-only and does not consume or alter queued data. The last tested binary predates this change; it has not been rebuilt or run.

### 2026-11-25 canary-coverage diagnostic attempt

Built a fresh 145,944-byte candidate with NDK `27.1.12297006`, target `r9q-G990BXXSLIZH2`, and flags `RMG_DIAG_PI_PROBE_ONLY=1`, `RMG_DIAG_UCLAMP_TRIGGER_ONLY=1`, `RMG_DIAG_FOPS_READBACK_ONLY=1`, `RMG_DIAG_DURABLE_LOG=1`, `RMG_DIAG_COW_TRACE=1`, and `RMG_DIAG_COW_PTE_MB=512`. Candidate: `build/uclamp-canary-coverage-20261125/cve-2026-43499-app.so` (SHA-256 `F1273A9A9A9609134932C62310384541AA95BCE864DCD6844B40A3B3F1ACA9B1`). Binary string inspection confirmed the canary-coverage messages and explicit CFI/physical-write/root refusal guards. The candidate was selected in RMG as the local payload; RMG confirmed Shizuku transport, the expected firmware profile, `diagnostic=uclamp-trigger-only`, and `cfi_write_pipe_physrw_root=disabled`.

The single attempt completed KASLR discovery, the controlled-mm scan, and the 512-MiB readiness setup. It reached `readiness-monitor-ready` and `readiness-locker-ready`, then the phone stopped responding to ADB and rebooted. After recovery, `ro.boot.bootreason` was `kernel_panic,null`; the firmware and kernel remained unchanged, the phone reported `sys.boot_completed=1`, and `/dev/ksu` and `/proc/ksu` were absent. The copied durable trace is `build/uclamp-canary-coverage-20261125/canary-checkpoints.log` (SHA-256 `6041A11BFCB20BB9A6986BA1FE12EEC3F8D64744B3E989A5B86EDDAACC0544F0`). Its final checkpoint is `readiness-locker-ready`; the build defers checkpoint writes in the critical monitor-to-pselect window, so the missing later durable records cannot establish whether it aborted pre-gate, entered pselect, or reached the uclamp trigger. The canary coverage message was not captured, and this attempt provides no new result about canary completeness or KernelSU.

RMG returned to its Home screen after reboot and Shizuku is no longer running. The verified original `rmg-cow-only-512m-anon-softdirty-20261124.so` was reselected in Settings (SHA-256 `A985BE9DA9FF46578FAA119EECE3BA7F2D5CA732D489812F3B261333B942E7FC`); the staged candidate and original transfer copies were removed from Downloads. No further attempt was made. Do not repeat this diagnostic unchanged: first improve crash observability without adding durable file I/O to the timing window, or arrange a kernel crash trace that can distinguish the guarded pre-gate exit from an in-gate scheduler event.

### 2026-11-25 log-capture path check

I tried a host-side `adb logcat` filter for the payload's `google_poc_app` tag before the next guarded run. The capture file contains only its start markers, and a live logcat query during execution showed ADB service chatter but no payload messages. The payload progress text is being surfaced by RMG's Shizuku execution/log view, not through the filtered logcat path on this build. The logcat approach therefore does not close the crash-observability gap; the durable checkpoint file remains the available external trace. Stop the unused logcat stream after this check. The `/sys/fs/pstore` and `/proc/last_kmsg` paths remain inaccessible to the shell user.

For this guarded run, RMG's visible log independently confirmed `diagnostic=uclamp-trigger-only`, `cfi_write_pipe_physrw_root=disabled`, and supervisor `attempts=1` / `exploit attempt=1/1`. The earlier `exploit route=default attempts=24` line is an outer route setting and does not contradict the one-attempt diagnostic supervisor limit. See the following run record for the outcome.

### 2026-11-25 guarded uclamp-trigger diagnostic: controlled-mm scan cap

Before the run, the phone was awake and booted on fingerprint `samsung/r9qxeea/r9q:16/BP2A.250605.031.A3/G990BXXSLIZH2:user/release-keys` and kernel `5.4.289-qgki-32192773-abG990BXXSLIZH2`. Shizuku was running, Android battery saver reported `settings get global low_power = 0`, and the imported local payload was `cve-2026-43499-app.so` (145,944 bytes; SHA-256 `F1273A9A9A9609134932C62310384541AA95BCE864DCD6844B40A3B3F1ACA9B1`). The RMG support profile was `r9q-G990BXXSLIZH2` from `Ryujin9610/Root-My-Galaxy-Payloads @ r9q-g990b-lizh2`.

The single supervisor attempt found KASLR (`slide=0x001b8000`, base `ffffffc010238000`) and sent the full controlled order-3 seed (`128/128`). The controlled-mm collector then used its full 192-pass scan cap. It found one candidate order-3 group; progress reached at least 16 of the 34 `mm_struct` slots needed for a complete slab (the last reported slot was 27), but no full group was selected. `controlled mm group collection failed`, `prepare_kernel_page` returned `base=0` after `676139 ms`, and no usable nonzero source pointers were found. The durable trace ends the scan with `mmcollect-scan-end a=1 b=0`; in source, `a` is the number of candidate groups and `b=0` means no complete group was selected.

The run did not reach the readiness COW setup or gated pselect trigger. The diagnostic made a successful no-change scheduler syscall, but that alone is not evidence of PI-chain traversal. The log then reports `CMP_REQUEUE_PI=-1/EDEADLK`, signals the waiter, and aborts with `readiness-pause route missing` because the page, lock, and fops addresses are all zero. There is no `readiness-locker-ready`, pselect result, canary scan, or fops readback in this run. RMG reported code 255 at `Running kernel exploit`; this is the expected guarded failure before CFI, physical write, and root installation.

This run did **not** reboot: ADB remained connected, uptime continued increasing, and `sys.boot_completed=1`. `ro.boot.bootreason` still read `kernel_panic,null` from the preceding boot, so it does not identify a new panic here. The firmware and kernel remained unchanged; `/dev/ksu` and `/proc/ksu` are still absent, and RMG shows KernelSU not loaded. The local payload was restored to `rmg-cow-only-512m-anon-softdirty-20261124.so` (SHA-256 `A985BE9DA9FF46578FAA119EECE3BA7F2D5CA732D489812F3B261333B942E7FC`). The staged diagnostic copy was removed from Downloads.

Evidence is in `build/uclamp-canary-coverage-20261125/rmg-checkpoints-at-cap.log` (SHA-256 `266D86140697ED1DD0A6299C7A0FA613977BDD0C5C5CC7CF98E611D6192A471F`) and `build/uclamp-canary-coverage-20261125/rmg-final-log-from-ui.txt` (SHA-256 `0AC2749C1C8137CC9F1A7BD00D5D48EAE98458EDD95A237ADD3E25598E1433AF`). The filtered `adb logcat` experiment captured no payload messages; RMG's Shizuku log view is the source of the stdout trace for this build. The next useful work is source-side analysis of why the collector repeatedly returned the same partial slab and whether the current firmware/profile produces enough distinct `mm_struct` slots. Do not rerun this 192-pass configuration unchanged.

### 2026-11-25 collector-only trace: collision discovery succeeds, mm search misses

Built a collector-only diagnostic from the local `r9q-G990BXXSLIZH2` sources with `RMG_DIAG_CONTROLLED_MM=1`, `RMG_DIAG_MM_SCAN_TIMEOUT_MS=180000`, and `RMG_DIAG_DURABLE_LOG=1`. The compile-time `controlled-mm-only` route runs `run_mm_collection_diagnostic()` and returns before readiness, FOPS, physical-write, or root stages. The 154,744-byte artifact is `build/mmcollect-only-trace-20261125/cve-2026-43499-app.so` (SHA-256 `DC10ACF73BA0754EB9534C3050559EA9F3080622595550E4751121112C8166BF`). RMG confirmed the local payload and one supervisor attempt.

The run observed the expected 960-byte `mm_struct` allocator stride, 34 objects per order-3 slab, and a full controlled seed (`128/128`). In the captured per-attempt stdout, the first 101 collector attempts reported the collision set ready (`collisions=1`) but `kernelsnitch_bruteforce()` returned `found=0`. The UI log capture ends during attempt 102. The durable checkpoint file covers the remainder: 121 brute-force passes completed, the 180,000-ms timeout fired at 180,853 ms before attempt 122 completed, and the scan ended with `mmcollect-scan-end a=0 b=0` (zero candidate groups, zero selected groups). The cache counters remained `480/480` before and after collection. Thus this attempt narrows the failure to mm-address identification after collision discovery; it did not reproduce the earlier partial/full candidate groups seen on the same phone.

Source review confirms the observed state values: `KERNELSNITCH_COLLISIONS_FOUND` is enum value 2, which the child trace reports as `collisions=1`; the subsequent brute-force path scans the configured identity interval. The target declares a 64-GiB KernelSnitch interval (`0xffffff8000000000`–`0xffffff9000000000`) while its direct-map interval extends to `0xffffffc000000000`. This interval difference remains a configuration point to verify against the actual physical-memory map, not an established cause. The target's 960-byte stride and 34-object slab geometry agree with the live slab statistics.

The downloaded `SM-G990B.zip` contains the Android 16 base kernel source and a separate `G990BXXSLIZH2` update archive; its update instructions say to apply that delta to the `G990BXXUGIYJ1` source. The ZH2 delta does not include `kernel/futex.c`. In the supplied base source, `kernel/futex.c:389–392` hashes the futex key with `jhash2((u32 *)key, offsetof(..., both.offset) / 4, key->both.offset)` and masks by `futex_hashsize - 1`, matching the payload's `futex_hash.h`. The private-key path stores `current->mm` before the page-aligned user address, also matching the payload's key layout. The handset reports `CONFIG_BASE_SMALL=0`, `CONFIG_NR_CPUS=8`, CPUs `0-7` both possible and online, and `getconf _NPROCESSORS_ONLN=8`; the kernel's initial `roundup_pow_of_two(256 * num_possible_cpus())` and the payload's `sysconf(...)*256` therefore both produce 2,048 buckets. This makes a basic hash-formula, private-key-order, or bucket-count mismatch less likely, though it does not validate the mm-address scan itself. `MemTotal` is 5,493,788 kB, well below the nominal 64-GiB scan span, but that alone does not prove the scan interval covers every physical-memory mapping.

For comparison, the earlier same-firmware `uclamp-preprobe` run found and completed a 34/34 group at base `ffffff80bff48000`, within the configured 64-GiB identity interval; its checkpoint ends `mmcollect-scan-end a=1 b=1`. This confirms the configured interval has contained a real candidate slab on this handset. It does not explain why the later collector-only run produced no candidate, so intermittent allocator placement, collision measurements, or brute-force candidate evaluation still need to be separated with targeted source instrumentation.

RMG showed code 255 at “Running kernel exploit,” consistent with this collector-only route failing before later stages; no readiness, pselect, canary, FOPS, CFI, physical write, or root activity was recorded. The phone remained connected and booted (`sys.boot_completed=1`); uptime advanced, battery saver was off, and `/dev/ksu` and `/proc/ksu` were absent. `ro.boot.bootreason` remained `kernel_panic,null` from an earlier boot, so it does not indicate a reboot during this run. RMG's local payload setting was restored to `rmg-cow-only-512m-anon-softdirty-20261124.so`, and the staged diagnostic copy was removed from Downloads.

Evidence: `build/mmcollect-only-trace-20261125/mmcollect-only-checkpoints.log` (SHA-256 `090F53B7EBA43C95993C7464ED42ABCCA24C22364E9E1C2112151E447784DB72`), `build/mmcollect-only-trace-20261125/rmg-log.txt` (SHA-256 `1F1424763F2CA9983342586CEDB99FB111B5A7E5B18BA582638D1EC5FEB7095F`), and the captured RMG hierarchy `build/mmcollect-only-trace-20261125/mmcollect-only-window.xml`. The earlier comparison is `build/uclamp-preprobe-20261125/rmg-preprobe-result.log` and `rmg-preprobe-checkpoints.log`. Do not rerun the old collector candidate unchanged; use the instrumented build below for any later bounded collector-only run, and compare its collision and search records with the prior successful trace before changing the range or match logic.

### 2026-11-25 collision and search instrumentation build

Added compile-time-gated tracing to the controlled-mm diagnostic. After the collision waiters are released, it records the baseline and threshold measurements, local futex hash-table size, selected-key count, and (for the first four attempts, each sixteenth attempt, and any successful set) the sampled key addresses and measurements. The brute-force search now reports its configured identity interval, per-thread candidate totals, and exact rounded worker ranges at the same sampling points. Candidate counters are thread-local and aggregated after `pthread_join`; the search and collision decisions are unchanged. These fields and logs are compiled only when `RMG_DIAG_CONTROLLED_MM_TRACE_ACTIVE` is enabled.

The successful `uclamp-preprobe` trace has a full slab at `ffffff80bff48000` after 34 collection attempts, while the latest collector-only failure completed 121 brute-force passes with no group. Neither existing trace contains the collision measurements or per-worker counts needed to distinguish timing-selected false collisions from address-search behavior. The instrumented run below supplies those measurements and also reproduces a successful controlled-mm collection.

Built the collector-only AArch64 artifact with `RMG_DIAG_CONTROLLED_MM=1`, `RMG_DIAG_MM_SCAN_TIMEOUT_MS=180000`, and `RMG_DIAG_DURABLE_LOG=1`: `build/mmcollect-collision-trace-20261125/cve-2026-43499-app.so` (156,200 bytes; SHA-256 `CA5421A3418F99C1FC79EF1AE5C4CE090263249122C31535D58FB6088E648BEC`). ELF inspection confirms an AArch64 shared object; string inspection confirms the `controlled-mm-only` route and its explicit “FOPS and root stages intentionally skipped” path. This artifact was staged under a unique filename for the single collector-only run below.

The one-attempt RMG Next run selected the exact `r9q-G990BXXSLIZH2` profile and Shizuku transport. Startup confirmed `diagnostic=controlled-mm-only`, `scan_timeout_ms=180000`, `attempts=1`, and `fops_root=disabled`. KASLR discovery succeeded with slide `0x001b8000` and base `ffffffc010238000`; the controlled order-3 seed sent `128/128` pages. Sampled collision sets were consistently ready with `selected=3/3`, hash size 2048, 64 repeats, and average 8; the baseline/threshold varied across samples (20/200 through 32/320). The first search found `mm=ffffff801ef50000` inside the configured 8-GiB identity interval using eight workers and 3,708,553 candidates (216,120–761,157 per worker). Attempt 33 found a second base, `ffffff812fd00000`, after 46,281,420 candidate checks (4,646,566–6,981,534 per worker). Group 0 had reached 32/34 slots by attempt 32; group 1 reached at least 24/34 by attempt 56. The durable trace then records 66 completed leak passes, `mmcollect-scan-end a=2 b=1`, and `mmcollect-done a=1`. In source, `a` is the candidate-group count and `b` reports whether a group was selected; the return value confirms one complete 34-object slab was selected. No `mmcollect-scan-timeout` was recorded.

RMG displayed its generic “Failed at: Running kernel exploit” code 255. This is the expected diagnostic stop: the `RMG_DIAG_CONTROLLED_MM` branch returns status 42 immediately after collection, regardless of success, and is compiled to skip FOPS and root. The collector succeeded, but this run did not attempt temporary root or load KernelSU; `/dev/ksu` and `/proc/ksu` remained absent. The phone stayed booted on `5.4.289-qgki-32192773-abG990BXXSLIZH2`, battery saver was off, and ADB remained connected. The RMG selection was restored to `rmg-cow-only-512m-anon-softdirty-20261124.so` (device SHA-256 `a985be9da9ff46578faa119eece3ba7f2d5ca732d489812f3b261333b942e7fc`), and the staged diagnostic copy was removed from Downloads.

The RMG UI log capture is 100,040 bytes and ends during attempt 57, so use the durable checkpoint for the final outcome. Evidence: `build/mmcollect-collision-trace-20261125/rmg-log.txt` (SHA-256 `F09BB2DE6E989B7947DAAF19DFEA5A01B173A4026DDA40BE94F72ACAE436A4D0`), `build/mmcollect-collision-trace-20261125/mmcollect-collision-checkpoints.log` (SHA-256 `F0997E374222280F7D31F5EE6F6E1969061680B014D4EFC1EFD56CFCCFC49C72`), and captured RMG UI hierarchies in the same build directory. This confirms that the earlier zero-group result was not deterministic. The collector-only route itself closes the selected descriptors, so it does not test reclaim; a separate guarded controlled-reclaim probe already covers that handoff, as summarized below.

### 2026-11-25 source audit: reclaim handoff and canary blind spot

The collector-only harness and the ordinary controlled route exercise different boundaries. `run_mm_collection_diagnostic()` calls the shared collector, counts the retained target descriptors, then closes every target and optional buddy descriptor before returning. Its new successful result proves that a complete slab can be selected, but cannot prove that the slab remains held or is reclaimed into later allocations.

The separate `RMG_DIAG_CONTROLLED_RECLAIM_ONLY` run on 2026-11-24 did exercise that handoff. It collected all 34 objects at `ffffff80b97b8000`, created 816 trigger memfds and anchor fd 855, released the selected slab, completed five `io_uring` spectrum sizes, and sent 251/256 socket-buffer messages. The `mm_struct` slab count fell from 39 to 38 (`reclaim-scan-result a=1 b=38`), after which the probe logged completion and skipped the PI walk. This is evidence that the guarded reclaim stage completes; it is not proof that the later PI walk, FOPS mutation, physical-RW, or root works. The run's checkpoints and UI log are `build/controlled-reclaim-only-20261124/rmg-reclaim-checkpoint.log` (SHA-256 `35E2FC198A64194BA895D30799257054F6E6B7FDC0CBFE4E76BA0D69331CEF3D`) and `build/controlled-reclaim-only-20261124/rmg-reclaim-live-log.txt` (SHA-256 `B3D976BC65902A922E3C81E5E2358E36FD48678D622B0D621FAFBE930FF364B4`).

The probe's zero result is intentional. With `CONTROLLED_RECLAIM_PROBE` set, `prepare_controlled_kernel_page()` closes the reclaim sockets and returns 0 immediately after the slab-count check; the diagnostic branch then returns status 42. The ordinary path omits that probe stop and can return the candidate base when the spectrum or socket send phase succeeds. Thus RMG code 255 in the probe is not a failure of the collector or reclaim stage.

A separate static review found a possible false negative in `route_payload_canary()`. This target defines `PAYLOAD_DUAL_LOCK_SCAN=1`; `prepare_skb_payload()` initializes a second fake lock B and waiter, and the dual-lock route can modify B's waiter tree. The canary reads `lockb_root` and `lockb_left`, but its early "unchanged" check only tests lock A's roots and the fake task's PI roots. If only lock B changed, the code continues without setting `mutated`, so the canary can report 0 despite that mutation. The source does not show that this happened in the prior run: the deferred-log checkpoint records `uclamp-trigger-canary a=0`, while its tested binary predates the later byte-coverage logging and contains no lock-B canary values. The `w3=...0078` route signature is partial-walk evidence, not proof that lock B changed.

Updated `route_payload_canary()` to compare lock A against `fake_w0`, lock B against `fake_w0 + 0x100` when dual-lock scanning is enabled, and the task PI root/leftmost against zero. It now reports per-chunk changed flags and aggregate counts for changed chunks, lock A, lock B, and the task PI tree. The existing byte-coverage count/warning remains in place. In `RMG_DIAG_UCLAMP_TRIGGER_ONLY` builds, a positive result can only enable the diagnostic FOPS readback; that build still forces `route_signal=0` and stops before CFI, physical-RW, and root. The guarded build and one device attempt are documented below.

### 2026-11-25 dual-lock canary selected; Kernel-Check ran instead

Built the updated `r9q-G990BXXSLIZH2` source as the AArch64 artifact `build/uclamp-dual-lock-canary-20261125/cve-2026-43499-app.so` (146,168 bytes; SHA-256 `75D450FD8D8EE3BA5CD2D051F3611C3543F829FEAEFCA475512EFE600EFC4809`). The guarded build enabled `APP_PAYLOAD=1`, `RMG_DIAG_PI_PROBE_ONLY=1`, `RMG_DIAG_UCLAMP_TRIGGER_ONLY=1`, `RMG_DIAG_FOPS_READBACK_ONLY=1`, `RMG_DIAG_DURABLE_LOG=1`, `RMG_DIAG_COW_TRACE=1`, and `RMG_DIAG_COW_PTE_MB=512`. Binary inspection confirmed the dual-lock canary messages and the existing stop before CFI, physical-RW, and root. The device copy was hash-verified and RMG Settings showed that this candidate was selected. That establishes selection only, not execution.

The saved UI sequence resolves what ran. `rmg-kernel-test-start.xml` (captured 14:45:07 local) shows the Home screen's **CVE-2026-43499 Test** card; `rmg-test-confirm.xml` (14:46:21) shows the Kernel-Check warning dialog with **Run the test**. RMG later logged that the check started at `1795614393560` and that the phone restarted under it; that epoch value is 2026-11-25 14:46:33.560 local. The app log entry appeared at 15:19:15.620 after recovery. This is the same navigation mistake later described before the load-only run, not a second canary attempt.

This reboot belongs to Kernel-Check, not to the selected canary payload. RMG History and `rmg-result.xml` still show the previous collector-only payload run at 13:52:49–13:54:53; there is no new payload run record. The file at `/data/local/tmp/rmg-pipe-oracle-checkpoint.log` was stale (SHA-256 `F0997E374222280F7D31F5EE6F6E1969061680B014D4EFC1EFD56CFCCFC49C72`) and ends at the earlier `mmcollect-done`. That is expected when the payload constructor never runs, so the stale checkpoint is not evidence of a canary crash or a checkpoint-write failure. No result for the dual-lock canary was produced; the candidate did not reach `route_payload_canary()` in this event.

After inspection, RMG's local payload selection was restored to `rmg-cow-only-512m-anon-softdirty-20261124.so`; its device SHA-256 is `a985be9da9ff46578faa119eece3ba7f2d5ca732d489812f3b261333b942e7fc`. The staged canary was removed from Downloads. No canary payload attempt was made. Do not count this Kernel-Check reboot as a result for the root payload.

Evidence captured in `build/uclamp-dual-lock-canary-20261125/` includes the pulled checkpoint, post-reboot RMG home/history/result/log/settings hierarchies, and the canary artifact. The retained history/result XMLs identify the older collector-only record; `rmg-settings-final.xml` confirms restoration of the original payload selection. The device hashes and post-boot properties were read directly over ADB after cleanup.

### 2026-11-25 source audit: Kernel-Check navigation error

The apparent missing canary run marker has a navigation explanation. In a build with `RMG_DIAG_DURABLE_LOG=1`, `load()` in `port/preload.c` calls `rmg_diag_checkpoint_reset()` as its first durable action, before profile setup, the boot-quiet wait, or the supervisor fork. The reset opens `/data/local/tmp/rmg-pipe-oracle-checkpoint.log` with `O_TRUNC`, writes `stage=run-start`, and calls `fdatasync()`. The 14:46 Kernel-Check confirmation and matching persisted start time show the payload flow was not started, so the old collector trace was expected to remain.

The current file retained the exact SHA-256 of that earlier collector trace and its old `mmcollect-done` ending. Its reported mode `0666`, owner `shell:shell`, and SELinux label `u:object_r:shell_data_file:s0` were consistent with the successful load-only run's shell uid 2000 context. The permission investigation is no longer needed to explain this event: the wrong RMG card was confirmed, so the canary constructor was not expected to truncate the file.

This event does **not** establish that the candidate reached its preload constructor, collector, readiness route, scheduler trigger, or `route_payload_canary()`: it shows that the separate Kernel-Check path ran instead. The dual-lock canary's execution status remains untested.

For a future payload run, use Home → **Tap to install KernelSU** and confirm the exact local payload before tapping the final install confirmation. Do not use **Kernel test → Test again**; it exercises the CVE probe and can reboot the phone. The later load-only run verified the payload handoff separately, but did not execute the dual-lock canary.

### 2026-11-25 load-only RMG handoff verification

Added a compile-time `RMG_DIAG_LOAD_ONLY` mode for the r9q app payload. The build is rejected unless `APP_PAYLOAD=1` and durable logging are enabled, and it cannot be combined with the controlled-mm, controlled-reclaim, uclamp-trigger, CFI, or pipe-handoff routes. Its constructor resets a build-specific checkpoint at `/data/local/tmp/rmg-load-only-r9q-G990B-20261125.log`, writes `run-start`, `constructor-entry`, and `load-only-before-exit`, prints a unique build ID, and calls `_exit(42)` before profile setup, the supervisor fork, or `run_exploit()`. This mode is in `port/preload.c`, `port/util.c`, and the compile-time guard in `port/common.h`.

Built `build/rmg-load-only-r9q-20261125/rmg-load-only-app.so` with NDK `27.1.12297006` (143,776 bytes; SHA-256 `E0447E1B94F6B569EEF6CC040814C1A73E42ED7C8875057A9A0998A851008E38`). ELF inspection confirmed ELF64/AArch64, and string inspection found the unique `RMG_LOAD_ONLY` ID and checkpoint path. After importing into RMG, `run-as dev.rushiranpise.rmgnext sha256sum files/local-payload/local-exploit.so` returned the exact build hash, proving the app-private payload bytes matched the artifact.

The 14:46 navigation error documented above happened before this load-only run: tapping `Test again` in the separate **Kernel test** card started RMG's Kernel-Check, not the payload run. The post-reboot app log records that the check started at 14:46:33.560 and that the phone restarted under it; the unique load-only checkpoint did not exist yet. This was a Kernel-Check reboot, not evidence that the load-only payload or the updated canary had run. Do not use that Kernel-Check button for payload testing.

The actual RMG payload flow is Home → `Tap to install KernelSU` → select the exact `SM-G990B` / `5.4.289` profile → Confirm. RMG's run log identifies `rmg-load-only-r9q-G990B-20261125.so`, prints `RMG_LOAD_ONLY build=r9q-g990b-load-only-20261125-01`, and ends with the intentional `Failed at: Running kernel exploit` / code 42. The unique checkpoint was pulled as `build/rmg-load-only-r9q-20261125/load-only-checkpoint.log` (SHA-256 `F65866D53124F13933C14D2BD9B2FB17205BB3090302C969C399895B1AD7D880`); it contains `run-start`, `constructor-entry a=14597 b=2000`, and `load-only-before-exit a=14597 b=2000`. Together with the verified imported hash, this proves the correct local payload's constructor ran under shell uid 2000 and that its checkpoint path is writable. The code-42 result is the expected sentinel; the exploit supervisor and trigger did not run.

During the load-only run, ADB stayed connected and uptime advanced from the checkpoint's monotonic time `356.922` to over 400 seconds; `sys.boot_completed=1`, the fingerprint and kernel remained `G990BXXSLIZH2` / `5.4.289-qgki-32192773-abG990BXXSLIZH2`, and `/dev/ksu` and `/proc/ksu` were absent as expected. `ro.boot.bootreason=kernel_panic,bug` still reflected the preceding Kernel-Check reboot; there was no new reboot during this load-only run. Battery saver was off. RMG was restored to `rmg-cow-only-512m-anon-softdirty-20261124.so` and its app-private SHA-256 was verified as `A985BE9DA9FF46578FAA119EECE3BA7F2D5CA732D489812F3B261333B942E7FC`; the staged load-only file was removed from Downloads. No retry was made.

Evidence in `build/rmg-load-only-r9q-20261125/` includes the candidate, its checkpoint, pre/post-import Settings hierarchies, RMG result hierarchy, and `app-log-final.txt` with the Kernel-Check interruption. The load-only handoff succeeded, but the dual-lock canary still has not been run. Any future canary attempt should use the actual payload flow and a fresh per-build checkpoint.

### 2026-11-25 Kernel-Check reboot: source-level analysis

This reboot is tied to RMG Next's separate **Kernel test** screen, not to the local Root-My-Galaxy payload flow. On the next launch, the app recorded: `Kernel check was left unfinished by the check started at 1795614393560: the phone restarted under it.` After recovery, the recorded boot reason was `kernel_panic,bug`. The installed package was `0.13+local.af081c2` (last updated 2026-11-23 15:31), matching the inspected RMG source revision `af081c2`.

At that revision, `MainActivity.kt` saves a pending Kernel-Check record with the current boot token and `attempts=5` immediately before launching the coroutine that calls `VulnerabilityProbe.run()`. The next launch compares boot tokens and emits the interruption message. This durable record proves the check workflow began, but by itself cannot prove that `ProcessBuilder` had already started the binary when the phone restarted. `VulnerabilityProbe.kt` starts `libpoc64.so` as a child process, waits five seconds, then tries SIGTERM and SIGKILL if it remains alive; a reboot observed during the child process is classified as `Vulnerable`. The saved value `attempts=5` is the planned maximum, not a record of which attempt was active. The app log has no completed attempt line, so the exact attempt number and last operation are unknown. Source links: [MainActivity.kt](https://github.com/Ryujin9610/Root-My-Galaxy-Next/blob/af081c2/app/src/main/java/dev/busung/s25uroot/MainActivity.kt), [KernelCheckRecord.kt](https://github.com/Ryujin9610/Root-My-Galaxy-Next/blob/af081c2/app/src/main/java/dev/busung/s25uroot/KernelCheckRecord.kt), and [VulnerabilityProbe.kt](https://github.com/Ryujin9610/Root-My-Galaxy-Next/blob/af081c2/app/src/main/java/dev/busung/s25uroot/VulnerabilityProbe.kt).

I pulled the installed APK read-only and extracted its `lib/arm64-v8a/libpoc64.so`. Its Git object hash is `ea546daeb2dd9064e4d88f4255b18c0b3a895dc8`, exactly matching the RMG repository blob; the extracted file's SHA-256 is `CC1428A5B30266C39E43E12272A987D9D197BC06E31254F97D2C88F382C60FA1`. Static AArch64 disassembly confirms the probe creates owner/waiter/consumer threads and invokes PI futex operations, including `FUTEX_LOCK_PI` (operation 6), `FUTEX_WAIT_REQUEUE_PI` (11), and `FUTEX_CMP_REQUEUE_PI` (12). The ELF init array also calls `init()`, which invokes `run()`, and `main()` invokes `run()` again if the first call returns; a single child can therefore enter the sequence twice. We cannot establish whether the second entry occurred on this phone.

The installed app identifies the bug as **CVE-2026-43499**, a proxy-lock rollback bug in `kernel/locking/rtmutex.c`. Samsung's downloaded source bundle is `C:\Users\admin\Desktop\kernel source\SM-G990B.zip` (SHA-256 `FDDDAD4657D0947106CF6CC6BBFB99C0C495ED48259F7FFB9290E92E7FBFFABE`). Its H2 package says to apply the `G990BXXSLIZH2` update overlay to the `G990BXXUGIYJ1` base source. The base Makefile reports Linux `5.4.289`; the r9q defconfig sets `CONFIG_FUTEX_PI=y`. In the H2 overlay file list, neither `kernel/locking/rtmutex.c` nor `kernel/futex.c` appears, so these core paths remain the base-source versions.

The source path matches the reported bug: `kernel/futex.c` calls `rt_mutex_start_proxy_lock()` with `this->task` while requeueing a waiter (around line 2259). On an error, `rt_mutex_start_proxy_lock()` calls `remove_waiter()` (around line 1806). In the published `kernel/locking/rtmutex.c`, that helper takes `current->pi_lock` and clears `current->pi_blocked_on` (around lines 1076–1079), rather than using the proxy waiter's task. This is the `current` versus `waiter->task` mismatch described by the app's [KernelVulnerability.kt](https://github.com/Ryujin9610/Root-My-Galaxy-Next/blob/af081c2/app/src/main/java/dev/busung/s25uroot/KernelVulnerability.kt). The upstream stable [fix commit](https://github.com/gregkh/linux/blob/f3fa3424bceb128d2be4b3745506b22844b87db7/kernel/locking/rtmutex.c#L1067-L1080) instead sets `waiter_task = waiter->task`, locks that task's `pi_lock`, and clears its `pi_blocked_on`. The Linux kernel CVE team's [official advisory](https://kernel.googlesource.com/pub/scm/linux/security/vulns/%2B/80053de1ed0b18e01c0c49164416aa8b8c0d79ca/cve/published/2026/CVE-2026-43499.mbox) describes the same `futex_requeue()` proxy-lock rollback bug and lists upstream fixes starting at 5.10.261; it notes that fixes may later be backported to older vendor branches. The relevant extracted files are under `build/kernel-check-analysis-20261125/kernel/`; the r9q config is at `build/kernel-check-analysis-20261125/arch/arm64/configs/vendor/r9q_eur_openx_defconfig`.

**Assessment:** a Kernel-Check invocation was pending across a reboot, the boot reason was reported as `kernel_panic,bug`, its packaged PoC is built to exercise PI futex requeue, and Samsung's matching published source contains the CVE's described code pattern with PI futexes enabled. Together this makes the Kernel-Check PoC the likely cause of that reboot, although the pending record alone does not prove the PoC process had started. It is not a captured panic trace: `/sys/fs/pstore` and `/proc/last_kmsg` were inaccessible, so the exact panic frame or instruction cannot be proven. The 2026-11-25 load-only payload run happened after recovery, reached its unique constructor checkpoint, exited intentionally with code 42, and did not reboot. That separates it from this event. The earlier uclamp-canary reboot remains unresolved because its checkpoint was stale and its run stage was not recorded.

No further device test or exploit trigger was run during this source audit. Do not press **Kernel test → Test again** as part of payload diagnostics; it starts this separate CVE probe and can reboot the phone.

### 2026-11-25 repeated Kernel-Check probe

At the user's request, one more **CVE-2026-43499 Test → Test again** attempt was run after the earlier source audit. RMG warned that the phone could become busy until restart; the user had authorized the probe. ADB stopped responding immediately after starting it and the device reappeared about 21 seconds later. RMG's Home screen now says `The phone restarted during the test` and `Only a kernel with the bug can be restarted by this test.` Its app log records `11-25 16:51:15.343 I Kernel-Check Kernel check was left unfinished by the check started at 1795621723881: the phone restarted under it.`

After recovery, ADB reported `sys.boot_completed=1`, `ro.boot.bootreason=kernel_panic,bug`, uptime `224.43` seconds at 16:53 local time, and boot ID `51e421a1-0690-4785-81e9-480457d71830`. The phone remained on fingerprint `samsung/r9qxeea/r9q:16/BP2A.250605.031.A3/G990BXXSLIZH2:user/release-keys` and kernel `5.4.289-qgki-32192773-abG990BXXSLIZH2`; battery saver was off. `/dev/ksu` and `/proc/ksu` remained absent, and RMG showed KernelSU `Unknown` while Shizuku was running. No RMG payload was run during this probe.

The follow-up read-only diagnostic found `/sys/fs/pstore` and `/proc/last_kmsg` inaccessible to the ADB shell (`Permission denied`); `dmesg` likewise returned `klogctl: Permission denied`. The exact panic trace is therefore unavailable through the current shell access.

This is stronger evidence than the previous attempt that the Kernel-Check run was associated with the restart: both the app's result card and its interruption log point to the test, and Android reports a kernel-panic boot reason. The app still writes its pending record before launching the PoC, so the record alone cannot prove the child process reached its first instruction. No pstore/last-kmsg panic trace was recovered, so the exact kernel fault site remains unconfirmed. Do not repeat this probe; it has already reproduced a reboot and did not create a KernelSU device node.

Evidence is saved in `build/kernel-check-analysis-20261125/post-probe-state.txt` and `build/kernel-check-analysis-20261125/phone-screen-after-probe-result.png`.

### 2026-11-25 RMG trigger-path source audit

The Samsung base source layout matches the target's fake `rt_mutex_waiter` offsets. With `CONFIG_DEBUG_RT_MUTEXES` disabled and `CONFIG_RT_MUTEXES=y`, the source places `pi_tree_entry` at `0x18`, `task` at `0x30`, `lock` at `0x38`, `prio` at `0x40`, and `deadline` at `0x48`, matching `target.h`; `CONFIG_FUTEX_PI=y`. The target's `task_struct` PI offsets are recorded in `target.h` as derived from the matching G990B vmlinux disassembly. A basic waiter-layout/config mismatch is therefore less likely, though the source bundle is Samsung's documented base-plus-update source rather than a direct dump of the running kernel.

The real `uclamp-canary-coverage` payload run has a durable `run-start`, entered the RMG constructor, completed KASLR and selected a full controlled-mm group, then stopped at `readiness-locker-ready` before the trace recorded pselect entry or the trigger gate. Because that build deferred durable writes during the critical window and the device rebooted, its exact stop point is unknown. A separate, non-crashing deferred-log run reached `readiness-pselect-done` with return 112 and reported the scheduler request as `0/0`; the older canary returned zero, but that build did not check lock B or report byte coverage. The corrected dual-lock/coverage candidate was selected later but did not run: the 14:46 event was Kernel-Check, as established above.

The current source shows the remaining runtime gate clearly. `rt_mutex_adjust_pi()` returns if the target task has no `pi_blocked_on` waiter or if that waiter compares equal to the task's current priority/deadline. If it proceeds, `rt_mutex_adjust_prio_chain()` requeues the waiter and may update the fake lock's waiter tree and the fake owner's PI-waiter tree. A successful `sched_setattr()` alone does not prove that these steps happened. The observed zero canary from the older build therefore leaves two live possibilities: the PI walk exited before mutation, or the old single-lock/incomplete-stream canary missed the resulting write. There is not yet a run of the corrected dual-lock canary to distinguish them.

For any future payload diagnostic, use the actual Home → **Tap to install KernelSU** flow, keep CFI, physical-RW, and root disabled, and preserve a fresh per-build `run-start` record. The corrected canary should report bytes received and complete order-3 chunks scanned, plus changes to lock A, lock B, and the fake task's PI tree. Do not use the separate Kernel-Check card as a payload test.

### 2026-11-25 corrected dual-lock canary: one-shot payload run

Ran the corrected AArch64 candidate `build/uclamp-dual-lock-canary-20261125/cve-2026-43499-app.so` once through Home → **Tap to install KernelSU** → the exact `SM-G990B` / `5.4.289` profile → Confirm. This was the payload install flow. RMG Settings showed the imported local payload `rmg-uclamp-dual-lock-canary-20261125.so`; its app-private SHA-256 matched the build (`75D450FD8D8EE3BA5CD2D051F3611C3543F829FEAEFCA475512EFE600EFC4809`, 146,168 bytes). The build used `APP_PAYLOAD=1`, `RMG_DIAG_PI_PROBE_ONLY=1`, `RMG_DIAG_UCLAMP_TRIGGER_ONLY=1`, `RMG_DIAG_FOPS_READBACK_ONLY=1`, durable logging, COW tracing, and a 512-MiB COW region. Its guards stop before CFI, physical-RW, and root installation.

The live RMG log showed one run starting at 18:13:20 local, Shizuku running and granted, and KernelSU manager already installed (`KernelSU v3.3.0`). The durable checkpoint file `build/uclamp-dual-lock-canary-20261125/one-shot-run/rmg-pipe-oracle-checkpoint.log` has SHA-256 `3F8BC4993963FA6F3EED2DA3109D5E5E0EF557E58C28252D3E0DFD98E7CD5B09`. It records 61 collector attempts, `mmcollect-scan-end a=4 b=1` (four candidate groups and a selected complete group), the successful `sched_setattr` return, 512-MiB readiness setup, monitor readiness, and ends at `readiness-locker-ready a=1 b=0`. No later durable pselect, trigger, or dual-lock readback checkpoint exists. The RMG live log excerpt is preserved in `build/uclamp-dual-lock-canary-20261125/one-shot-run/rmg-live-run-log-observation.txt`; the post-reboot app log and state are also saved in that directory.

ADB disconnected during the readiness phase. The phone recovered with `sys.boot_completed=1` and `ro.boot.bootreason=kernel_panic,bug`; its Android 16 fingerprint and kernel `5.4.289-qgki-32192773-abG990BXXSLIZH2` were unchanged. RMG displayed KernelSU as **Not loaded**, and `/dev/ksu` and `/proc/ksu` were absent. The history count remained at 102 and contained no completed record for this run. `/sys/fs/pstore` was still denied to the ADB shell, so the kernel fault site could not be identified. The build defers durable writes across the timing-sensitive monitor-to-pselect window; the final checkpoint therefore cannot tell whether the panic happened before the trigger, during it, or after an unrecorded transient canary readback. This run does not establish a dual-lock result or a KernelSU load.

After recovery, RMG's local payload was restored to `rmg-cow-only-512m-anon-softdirty-20261124.so` and verified at SHA-256 `A985BE9DA9FF46578FAA119EECE3BA7F2D5CA732D489812F3B261333B942E7FC`. The temporary canary copy was removed from Downloads. Screenshots and UI hierarchies for the pre-run selection, confirmation, post-reboot state, and history are in `build/uclamp-dual-lock-canary-20261125/one-shot-run/`. Do not repeat this binary unchanged. Continue with offline crash-path analysis or add crash observability that does not write synchronously in the monitor-to-pselect timing window.

### 2026-11-25 post-panic bugreport and exact Oops analysis

The requested ADB bugreport preserved the kernel's last-kmsg even though the
ADB shell could not read `/sys/fs/pstore`. The report is
`build/uclamp-dual-lock-canary-20261125/one-shot-run/post-panic-bugreport.zip`
(SHA-256
`3D36CBF0EB50B20BEE8FDCB752AF6DF2A757CD578E2EC45B00ADC97DC63178A1`). Its
`FS/log/dumpstate_latest_lastkmsg.log.gz` entry is a nested ZIP containing
`dumpstate_lastkmsg.lst` (SHA-256
`167415BADC92183691981C6DDD8E9A829D7CE3D648811251E1CEC3CEC8D8F523`). The
compact panic excerpt and matching firmware disassembly are preserved in
`build/uclamp-dual-lock-canary-20261125/one-shot-run/kernel-oops-analysis.txt`.
The full bugreport remains local because it contains device diagnostics.

The trace identifies `kernel BUG at kernel/locking/rtmutex_common.h:60!` in
`rt_mutex_adjust_prio_chain+0x878/0x890`, called from
`rt_mutex_adjust_pi` → `__sched_setscheduler` → `sched_setattr`. In the
matching extracted firmware ELF (SHA-256
`AC26E724175922F9C893685927D5D866C42DA2FB0F4A8C9F601E84FF76C642DB`), the
path loads the cached leftmost waiter, reads its `lock` field at offset
`0x38`, compares it to the lock being walked, and branches to the `brk` BUG
trap when they differ. The fault registers show `x0=ffffff8030a1f880`,
`x8=0`, and `x23=ffffff810e2ab540`: the tree had a non-null leftmost waiter,
but that waiter's `lock` field was null while the walked lock pointer was
non-null. This matches the Samsung base-source line
`BUG_ON(w->lock != lock);` exactly. The extracted source header is
`build/uclamp-dual-lock-canary-20261125/one-shot-run/rtmutex_common-G990BXXUGIYJ1-base.h`
(SHA-256
`C79353D84625E4D352BE7D96193CF162FBA7857A85171540996F499679EA3D08`); the
SLIZH2 source overlay does not replace this file.
In the matching `rtmutex.c`, this top-waiter invariant check is before step
`[7]` (`rt_mutex_dequeue()` / tree requeue), so the run crashed before the
planned dual-lock tree mutation or canary readback.

This resolves the earlier uncertainty about whether the scheduler trigger was
reached: it entered the PI-chain walk and hit an internal waiter/lock
invariant, about 38 ms after the last durable readiness checkpoint.

The run checkpoint shows the readiness monitor was targeting TID 25258; the
faulting kernel task is `sh` PID 25261. The source calls `sched_setattr` on
that monitored TID after observing it blocked, which strongly correlates this
Oops with the post-block trigger. The earlier preflight `sched-nochange-setattr`
on TID 25260 returned normally and is not the faulting call. This dump also
differs from the earlier stale-stack-frame hypothesis in the source comments:
here `waiter->lock` is zero, and the captured pointers do not match the
prepared A/B pair.

The payload source writes a nonzero fake-lock pointer to each intended waiter
at offset `0x38`. `target.h` places lock A at `0x2210` and waiter A at `0x2350`,
and adds `0x100` to both for lock B/waiter B, so either intended pair has a
`+0x140` lock-to-waiter offset. The captured `x0` and `x23` do not have this
relationship. More strongly, `base` is aligned to the 0x8000 order-3 slab size
and `SKB_DATA_DELTA=-0xe80`, so the emitted lock A/B pointers have low-15-bit
offsets `0x1390` / `0x1490` and waiter A/B have `0x14d0` / `0x15d0`. The
captured x23 has low 15 bits `0x3540`, and x0 has `0x7880`; neither belongs to
those candidate addresses. The checkpoint records `page_base=0xffffff80a2de0000`,
so the source formulas predict lock A `0xffffff80a2de1390`, waiter A
`0xffffff80a2de14d0`, lock B `0xffffff80a2de1490`, and waiter B
`0xffffff80a2de15d0`. None equals the captured x23/x0 pair. The trigger
followed a different lock/waiter pair or the live tree pointer diverged from
the prepared pair before this check. The trace cannot distinguish why and
does not prove that the dual-lock readback ran. The candidate SHA-256 was
`75D450FD8D8EE3BA5CD2D051F3611C3543F829FEAEFCA475512EFE600EFC4809`.

The diagnostic guards stop before CFI, physical read/write, and root/KernelSU
installation. This run therefore ended before any KernelSU load could be
observed. The missing `/dev/ksu` after recovery is expected for this guarded
run and does not establish a separate driver-node problem. Keep this binary
retired. Source review shows `route_craft_place_word()` gives each set bit a
writable empty-pipe end, and the preflight `select()` reads back words `w1`,
`w2`, and `w3`; however, it only prints them to stderr. The durable checkpoint
does not preserve those values, and the post-reboot bugreport did not contain
the payload output. The next diagnostic should persist those three words,
compare them to `fake_task`, lock A, and lock B, then stop before the COW
readiness/scheduler trigger. A match would shift the investigation to the
pselect stack/COW timing; a mismatch would point to the fd-set route craft.
Any new trigger attempt needs a fresh explicit go-ahead because this one
produced a kernel panic.

### 2026-11-25 FD-set preflight-only run

Added `RMG_DIAG_FDSET_PREFLIGHT_ONLY`, which requires an app payload, the
PI-probe stop, and durable checkpoints. Immediately after the existing
zero-timeout `select()` check, it records the return value, `errno`, output
words 1–3, and expected task/lock/priority masks, then returns before mapping
the COW fd set. It cannot proceed to the readiness monitor, the mutating
scheduler trigger, CFI, physical read/write, or root/KernelSU installation.

Built with NDK r27b (`27.1.12297006`) and
`RMG_DIAG_FDSET_PREFLIGHT_ONLY=1`, `RMG_DIAG_PI_PROBE_ONLY=1`, and
`RMG_DIAG_DURABLE_LOG=1`. The app payload is
`build/fdset-preflight-20261125/cve-2026-43499-app.so` (139,312 bytes; SHA-256
`E0590A7BA19C130AF64714413F7D727384C4DE3DF17079A0C2AAD1FBDA7B604E`). It was
imported as `rmg-fdset-preflight-20261125.so`; the app-private active payload
hash matched the build before the run.

One run was started through RMG on the exact G990B / kernel 5.4.289 profile
with Shizuku running and granted. RMG record
`cb20c9c6-2128-4233-bd99-956c637c3ea1` identifies the local payload and
support-source commit `3095e6f2a0659570b5adc54a4cb6b121f96d2fa2`. The run
completed 55 controlled-mm passes, selected a complete collector group, and
prepared page `ffffff80b41b0000`. The baseline no-change `sched_setattr`
returned 0. `FUTEX_CMP_REQUEUE_PI` returned `-1/EDEADLK`, then the signal
handler entered the readiness route and executed the preflight. No COW setup
or readiness-monitor checkpoint follows the preflight stop.

The result closes the fd-set-craft question for this run:

| Value | Observed | Expected | Result |
| --- | --- | --- | --- |
| `select()` return / errno | `112 / 0` | nonnegative / no error | pass |
| output word 1 | `ffffff80b41b2380` | `fake_task = ffffff80b41b2380` | exact match |
| output word 2 | `ffffff80b41b1390` | `fake_lock = ffffff80b41b1390` | exact match |
| output word 3 | `ffffff80b41b2310` | waiter-priority sentinel `ffffff80b41b2310` | exact match |

The third qword lands on `rt_mutex_waiter.prio`, a signed 32-bit integer, so it
is a sentinel value rather than a pointer to a priority object. Its low 32 bits
are `0xb41b2310` (signed `-1273289968`); the exact readback proves that this
bitmap value reached the `select()` result, but does not prove the scheduler
treated it as a valid normal priority or followed the crafted lock.

The handler returned normally. RMG still marked the run failed with code 255
and a generic “command could not be run” message, but its saved native output
contains the full payload startup, the `EDEADLK` result, the signal-handler
messages, and the matching `select()` values. This is the guarded diagnostic's
expected non-rooting exit, not evidence that Shizuku failed. The phone remained
boot-complete with uptime increasing and the kernel unchanged. KernelSU and its
device nodes were not expected to load in this build.

The durable checkpoint is
`build/fdset-preflight-20261125/one-shot-run/checkpoint-after.log` (SHA-256
`1707B199DA9F6BCCD24C5E9BFFC348DDBC2852405094C38FAE070776748C5C09`); the RMG
record and full app log are saved beside it. Afterward, RMG's active local
payload was restored to `rmg-cow-only-512m-anon-softdirty-20261124.so` and
verified at SHA-256
`A985BE9DA9FF46578FAA119EECE3BA7F2D5CA732D489812F3B261333B942E7FC`; the
temporary staged copy was removed from Downloads.

The pipe-backed fd-set is therefore not the cause of the prior rt-mutex
invariant crash. This preflight did not test the subsequent `pselect6` COW
copyout or the PI-chain mutation. Next, audit those stack/copyout offsets
against the now-confirmed runtime page geometry offline before considering a
new trigger attempt.

After that run, the compile-time guard was tightened: this mode now forces the
main readiness route even if `WAITER32` is inherited, rejects alternate
physical-P0/CFI/root/collector-only modes, and returns from `run_exploit()`
immediately after the route. The rebuilt, stricter candidate is
`build/fdset-preflight-guarded-20261125/cve-2026-43499-app.so` (138,064 bytes;
SHA-256
`AF528DED164EB7E7542742930849595DFEC7A3C15099E84FD709DCD15DE3FD86`). It
compiled successfully with only the same unused-function warnings. This
stricter build has not been imported or run; the recorded result above belongs
to the earlier `E059…` artifact.

### 2026-11-25 exact-firmware pselect/futex stack audit

I disassembled `work/firmware-analysis/kernel-recovered.elf`, whose SHA-256 is
`AC26E724175922F9C893685927D5D866C42DA2FB0F4A8C9F601E84FF76C642DB` (the
same hash as the ELF used in the Oops analysis). This is stronger evidence for
the frame sizes than the base-source comments alone:

| Function | Exact firmware frame / local | Resulting location from syscall-entry SP |
| --- | --- | --- |
| `__arm64_sys_pselect6` | frame `0x10` | — |
| `__do_sys_pselect6` | frame `0x90` | — |
| `core_sys_select` | frame `0x1c0`; stack fdsets at `sp+0x50` | fdset base `SP0-0x210` |
| `futex_wait_requeue_pi` | frame `0x1a0`; local waiter at `sp+0x98` | waiter base `SP0-0x198` |

For `nfds=320`, the firmware keeps the five-word fdsets on the stack. The
`res_readfds` array begins at `SP0-0x198`; `res_writefds` begins at
`SP0-0x170`. Thus `res_writefds` words 1, 2, and 3 land at `SP0-0x168`,
`SP0-0x160`, and `SP0-0x158`, respectively. In the futex syscall, the local
`rt_mutex_waiter` begins at `SP0-0x198`, putting its `task`, `lock`, and `prio`
fields at those same three addresses (`+0x30`, `+0x38`, and `+0x40`). The
static pselect/futex stack arithmetic in `target.h` is therefore correct for
this exact firmware ELF, assuming both syscalls reuse the same per-task kernel
stack top. The futex-side calculation is
`0x70 (__arm64_sys_futex) + 0x20 (do_futex) + 0x1a0 - 0x98 = 0x198`; the
pselect result base is `0x10 + 0x90 + 0x1c0 - 0x50 = 0x210` below its entry
SP.

The live C path also matches that model: `readiness_map_cow_fdset()` creates a
prefaulted `MAP_PRIVATE` memfd mapping, and `readiness_pselect_asm()` passes it
as `writefds`. `core_sys_select()` copies the input bitmap, computes its result
fdsets on the kernel stack, and copies `res_writefds` back to the same mapping.
The fd-set preflight returned `112`, exactly the combined popcount of the
observed three masks (`37+38+37`), and read back the intended words. A prior
deferred-log COW run also returned `w1=fake_task` and `w2=fake_lock`; its word 3
ended in `0x78`, consistent with the kernel updating the stack waiter's
priority. That run's independent payload-page canary remained unchanged, so it
did not prove that the fake lock/task trees were mutated.

The exact arithmetic does not resolve the earlier panic. At the `BUG_ON` in
`rt_mutex_top_waiter()`, the captured walked lock was
`ffffff810e2ab540`, while the cached top waiter was `ffffff8030a1f880` and its
`lock` field read as zero. Those values do not match the prepared payload-page
lock/waiter addresses for that run. Static stack overlap and a successful
fd-set readback establish that the route can produce the intended words; they
do not establish that the monitored task's live `pi_blocked_on` chain pointed
to that waiter and lock when `sched_setattr()` ran. The remaining diagnostic
question is the runtime PI-chain identity/state, not the fd-set bit pattern or
the pselect frame-size calculation.

The next useful work remains offline: trace the futex requeue cleanup and
`rt_mutex_adjust_pi()` prerequisites against the exact trigger ordering, then
review why the payload-page canary stayed unchanged despite the prior
priority-field signature. Do not start another scheduler-trigger attempt from
this audit; the previous one panicked and the exact live waiter/lock pair is
still unexplained.

### 2026-11-25 exact Oops task-stack correlation

The full Samsung kernel source archive and the matching recovered ELF allow
the Oops waiter pointer to be located more precisely. The Oops saved
`x19=ffffff81564a4ec0`, `x26=ffffff81564a579c`, and
`x25=fffffffc04006bcd8`. In this function `x19` is the current `task_struct`
being walked, `x26=x19+0x8dc` is its `pi_lock`, and `x25` is the value loaded
from `task->pi_blocked_on`. The chain walker starts with the task passed to
`rt_mutex_adjust_pi()` but can replace it with a lock owner on a later chain
step, so the register dump alone does not map `x19` to a TID.

For the firmware's 4 KiB-page configuration, `THREAD_SIZE` is `0x4000` and
VMAP stack alignment is `0x8000`. The address `x25` lies at offset `0x3cd8`
within the aligned stack region beginning `fffffffc040068000`, whose top is
`fffffffc04006c000`; therefore `x25 = stack_top - 0x328`. This exactly matches
the local `rt_mutex_waiter` in `futex_wait_requeue_pi`: the EL0 exception frame
is `0x140`, `el0_svc_handler` uses `0x10`, `el0_svc_common` uses `0x40`, and
the futex syscall path contributes `0x70 + 0x20 + 0x1a0 - 0x98 = 0x198`.
Together these place the waiter at `stack_top - (0x140 + 0x10 + 0x40 +
0x198) = stack_top - 0x328`. This is strong evidence that `pi_blocked_on`
points to a stack-resident futex waiter, rather than an arbitrary pointer into
the prepared payload page.

The same top-relative calculation confirms the designed pselect overlay
addresses: its result words 1, 2, and 3 land at `stack_top-0x2f8`,
`stack_top-0x2f0`, and `stack_top-0x2e8`, which equal `x25+0x30`, `x25+0x38`,
and `x25+0x40` (`waiter->task`, `waiter->lock`, and `waiter->prio`). The
static overlap arithmetic therefore agrees with the captured waiter location.
However, at the crash the scheduler walked lock `ffffff810e2ab540`, not the
prepared fake lock `ffffff80a2de1390`; its cached leftmost waiter was
`ffffff8030a1f880`, whose lock field was zero. This shows the intended fake
lock/waiter pair was not active at the failing invariant check. It does not
prove whether the trigger ran before pselect's COW copyout was blocked, whether
the monitor observed a different blocked state, or whether another runtime
field changed the chain before the call.

Next, keep the trigger-disabled source audit focused on that ordering. The
monitor currently treats any post-gate `R`-then-`D` transition as the COW-fault
waitpoint based on the route design; the panic's original-looking stack lock
value makes that assumption the main unverified runtime gate. A safer next
diagnostic design is to record the post-gate wait channel and require a stable
wait on the expected mmap-lock/COW path before allowing `sched_setattr()`,
while retaining the existing stop-before-CFI/physrw/root guards. Do not run
that trigger until the wait-channel condition is validated in a
trigger-disabled run and a fresh explicit go-ahead is given.

### 2026-11-25 trigger-disabled wchan diagnostic

Added a wchan read to the COW-only monitor branch that sees the target in
`D` after the trigger gate opens but before the monitor has observed the
target runnable after that gate. The branch records the wait channel and exits
without calling `sched_setattr()`. Built with NDK r27c and
`RMG_DIAG_COW_ONLY=1`, `RMG_DIAG_COW_TRACE=1`,
`RMG_DIAG_DURABLE_LOG=1`, and `RMG_DIAG_COW_PTE_MB=64`:
`build/wchan-cow-only-20261125/cve-2026-43499-app.so` (153,752 bytes; SHA-256
`6392BB5E1E485F71D5263F364025397DDEB483BF59CF412D32E36D5282570751`).

RMG selected this imported local payload on the exact firmware profile. Its
wrapper printed `attempts=24`, but the payload supervisor and exploit both
reported one attempt (`attempts=1`, `attempt=1/1`). The monitor sampled 4,631
times: its first state was `R` (82), its last was `D` (68), and it recorded no
post-gate blocked samples. The console wchan sample was
`futex_wait_queue_me`. The readiness route's `pselect` returned 106 in 1,979
microseconds; the helper mprotect returned `0/0` in 320 microseconds. The
canary scan found zero changed chunks, although it covered only 501 of 512
chunks. The route signal was forced to zero and the COW-only guard stopped
before CFI, physical read/write, and root stages. RMG displayed its expected
code-255 exploit failure; no KernelSU load or `/dev/ksu` creation was
attempted.

There was a logging gap before the gate attribution could be treated as fully
proven: the RMG console log contains the wchan line but not the
`readiness monitor pre-gate block` message emitted by the older pre-gate
branch, while the pulled durable checkpoint file does not contain the new
`readiness-monitor-wchan-before-runnable` marker. The follow-up audit below
found why the durable marker was missing and records the source fix. The run
itself remains consistent with a futex wait before a post-gate runnable sample;
it does not establish that the target reached the intended pselect COW fault
waitpoint.

The phone remained boot-complete and ADB-responsive, with the same firmware
fingerprint and kernel after the run. RMG's active local payload was restored
to `rmg-cow-only-512m-anon-softdirty-20261124.so` and verified against SHA-256
`A985BE9DA9FF46578FAA119EECE3BA7F2D5CA732D489812F3B261333B942E7FC`;
temporary payload copies were removed from Downloads. The console log,
checkpoint log, and after-run screen are in
`build/wchan-cow-only-20261125/`.

### 2026-11-25 durable gate/wchan snapshot follow-up

The missing marker was caused by intentional checkpoint deferral, not by a
file-write failure. `rmg_diag_checkpoint_defer(1)` is set before the monitor
is armed and remains set across the timing-sensitive pselect window;
`rmg_diag_checkpoint()` returns immediately while deferral is active. The
monitor's wchan checkpoint was therefore discarded by design. The existing
trace also had no event in the `D`-after-gate/before-runnable branch, so that
transition was absent from the flushed COW trace as well.

The source now keeps the COW-only block snapshot in monitor memory and writes
it after the monitor joins and checkpoint deferral is lifted. The durable
record includes TID, poll number, task state, gate state, whether a post-gate
`R` was observed and its poll number, whether the pselect call had started, and
the wchan text or read error. The console log prints the same context. The
COW trace now marks both a post-gate runnable observation and the `D`-before-
runnable branch. This preserves the intentional no-I/O timing window.

### 2026-11-25 checkpoint-confirmation run: post-pselect pagemap false positive

The checkpoint-fix candidate was built with NDK r27c and the same guarded
flags as the first wchan diagnostic:
`build/wchan-cow-only-checkpoint-fix-20261125/cve-2026-43499-app.so` (154,816
bytes; SHA-256
`4FB620FD84D4CD963F6A9B0860F2952B7C40C4BA67B5D3B0A5ECA49E37AD063F`). It was
imported as `wchan-cow-only-checkpoint-fix.so` and run once on the matching
SM-G990B firmware/kernel profile. RMG's wrapper printed `attempts=24`, while
the payload supervisor and exploit each reported one attempt. The guard forced
route signal zero, skipped the scheduler call, and stopped before CFI,
physical read/write, and root. RMG ended with its expected code-255 failure;
KernelSU did not load.

The repaired durable snapshot worked. It recorded TID 15379, `D` at poll 124,
gate open, post-gate `R` at poll 123, `pselect_started=1`, and
`wchan=pagemap_read`. The COW trace places `pselect` return (event 7, return
112) at 15,002,639,715 us, then post-gate `R` at 15,002,639,821 us, and the
`D` sample (event 5) at 15,002,643,735 us. Thus the `D` sample occurred about
4.02 ms after `pselect` returned. The source reads `/proc/self/pagemap`
immediately after that return, matching the observed `pagemap_read` wait
channel. This was a post-syscall diagnostic read, not evidence of the intended
COW fault waitpoint. The previous monitor could count that as a blocked sample
because it watched gate and task state but not whether `pselect` was still
active.

The displayed `pselect_us=9309` also included the post-return pagemap/smaps
probes: the COW trace's before/after markers were only about 253 us apart. The
source now measures the syscall window immediately around the call, marks
`pselect_active` only for that window, and stops the monitor as soon as the
syscall returns, before the diagnostic proc-file reads. The monitor also checks
that `pselect_active` is still true before treating `D` as a waitpoint or
calling `sched_setattr()`. The COW-only snapshot distinguishes a block observed
after pselect from one seen while pselect is active.

The new post-pselect guard source compiled in both COW-only and default modes
with NDK r27c; only the pre-existing unused-function warnings remained. The
guarded build is
`build/wchan-cow-only-postpselect-guard-20261125/cve-2026-43499-app.so` (155,328
bytes; SHA-256
`1A33BFA3BFFD87F18DCEC9B3A7058C7EB58ED37D996AD21743F930A52D6822E6`). It was
not yet imported or run when this build note was written; its follow-up
verification is recorded below. RMG's active local payload was restored to
`rmg-cow-only-512m-anon-softdirty-20261124.so` and verified against SHA-256
`A985BE9DA9FF46578FAA119EECE3BA7F2D5CA732D489812F3B261333B942E7FC`. The
phone remained boot-complete on the same fingerprint and kernel; power saver
was off. Temporary payload and screenshot staging files were removed from
Downloads.

The earlier checkpoint-fix run's console log, durable checkpoint, and
after-run screenshot are saved under
`build/wchan-cow-only-checkpoint-fix-20261125/`.

### 2026-11-25 post-pselect guard verification run

The guarded COW-only candidate above was imported into RMG as
`wchan-cow-only-postpselect-guard.so`; the app-private payload hash matched the
build hash. RMG ran one supervisor attempt and one exploit attempt. Its wrapper
still displayed `attempts=24`, but the payload log confirms `attempts=1` and
`attempt=1/1`. The kernel-page preparation scan took 134,622 ms before entering
the readiness route.

The monitor stopped after pselect returned and before the post-return pagemap
and smaps reads. The payload recorded pselect result `106` and a 112 μs syscall
window. The monitor sampled 4,759 times, with `first_state=82` and
`last_state=82` (Linux task state `R`); `blocked_samples=0`. No blocked-state
snapshot or `wchan` sample was emitted. This avoids the prior false positive
from a post-return `pagemap_read`, but it also means this run did not observe a
COW fault waitpoint. The soft-dirty fields are not usable on this firmware:
the connected device's `/proc/config.gz` says
`# CONFIG_CHECKPOINT_RESTORE is not set`, and the bundled `mm/Kconfig` makes
`MEM_SOFT_DIRTY` depend on `CHECKPOINT_RESTORE`. `CONFIG_PROC_PAGE_MONITOR=y`,
so pagemap access is available, but a clear soft-dirty bit here does not show
that no write occurred. The extracted config lines are saved as
`build/wchan-cow-only-postpselect-guard-20261125/kernel-config-softdirty.txt`.

The trigger monitor summary was `sched=0/0`, and the route signal remained zero.
The log also contains an earlier `sched-nochange-*` preflight that reads and
reapplies the current scheduler attributes; it is separate from the trigger
monitor, which made no scheduler call. The RMG log's `readiness-cow cfi miss`
and later `pipe-physrw-summary` are failure summaries: the route did not enter
`try_cfi_stage()`, and the summary reports `done=0`, `root=0`,
`read_ok=0`, `write_ok=0`, and `rw64=0/0`. RMG reported code 255 at “Running
kernel exploit”; KernelSU remained “Not loaded.” The KernelSU service was
downloaded and verified by RMG before the exploit, but it was not loaded.

Afterward, the phone remained boot-complete on the same Android 16 fingerprint
and kernel, power saver remained off, and Shizuku remained running. The RMG
local payload was restored to
`rmg-cow-only-512m-anon-softdirty-20261124.so` and its app-private SHA-256
matched `A985BE9DA9FF46578FAA119EECE3BA7F2D5CA732D489812F3B261333B942E7FC`.
The staged candidate and baseline files were removed from Downloads.

The run evidence is in
`build/wchan-cow-only-postpselect-guard-20261125/`: `rmg-run-log.txt`,
`checkpoint-after.log`, and `phone-after-run.png`. The post-pselect guard
prevented the earlier diagnostic-read false positive. The run provides no
evidence of a blocked COW waitpoint or temporary root. Do not repeat this
binary unchanged; the source-level interpretation below narrows what remains
to investigate.

### Source-level interpretation of the pselect result

The downloaded Samsung source bundle includes the SLIZH2 update archive. Its
README says to apply that update over the G990BXXUGIYJ1 base source. The update
tar contains 15 files and does not replace `fs/select.c`, `mm/memory.c`,
`mm/Kconfig`, or `fs/proc/task_mmu.c`, so the base source's select and pagemap
logic applies to this firmware update.

In that `fs/select.c`, `do_select()` increments its return value for every
ready descriptor. `core_sys_select()` then copies the result sets to userspace
through `set_fd_set()`, which calls `__copy_to_user()`, whenever the return is
nonnegative. The run's `pselect` value `106` matches the earlier zero-timeout
select preflight's `106`; the payload deliberately watches writable pipe ends
whose bits encode the sprayed fields. Thus `106` is a successful ready-fd
count, not an errno, and the quick return is expected. A positive return still
takes the output-copy path that can trigger the private COW fault.

`cow_out` is a newly created `MAP_PRIVATE` memfd mapping that the source only
read-prefaults before `pselect`. The first post-return pagemap read, before the
later control write, was `0x8100000000000000`. The bundled pagemap code defines
bit 63 as present, bit 56 as exclusively mapped, and bit 61 as file-backed;
this entry is present and exclusive with the file bit clear. Given the mapping
setup and lack of an earlier private write, this is strong evidence that
`pselect`'s result copyout produced an anonymous private page. A pre-syscall
pagemap sample is still missing, so the transition was not captured directly.

The likely failure is therefore the *blocked waitpoint*, not necessarily the
COW write itself: the copyout appears to complete within the 112 μs syscall
window, and the monitor never samples `D`. The mmap helper reports two
`mprotect()` iterations over 261 μs, spanning the pselect window, but that
run's trace did not timestamp each individual call. It could not show whether
a particular `mprotect()` held the mm write lock during the copyout fault or
whether a wait was shorter than a monitor sampling interval. The source-only
follow-up adds a pre-call pagemap sample and in-memory entry/return timestamps
for each `mprotect()`, as recorded below. Its measurements remain pending a
separately approved phone run.

### 2026-11-25 COW and mprotect timing instrumentation build

Added diagnostic-only instrumentation to the target port. It records the raw
pagemap entry for `cow_out` before the trigger, then again after `pselect`, so
the private-page transition can be compared directly. It also records
monotonic timestamps immediately before and after each diagnostic
`mprotect()` call, including its requested protection, return value, errno,
loop/restore kind, and call index. These events are held in a fixed in-memory
buffer and printed after the worker threads join; no log I/O is added around
individual `mprotect()` calls. The instrumentation is enabled only by
`RMG_DIAG_COW_TRACE` and keeps the COW-only diagnostic route.

The instrumented arm64 Android shared object compiled with Android NDK r27b
(27.1.12297006), API 30, using
`RMG_DIAG_COW_ONLY=1`, `RMG_DIAG_COW_TRACE=1`, and
`RMG_DIAG_DURABLE_LOG=1`. It is
`build/wchan-cow-pagemap-mprotect-trace-20261125/cve-2026-43499-app.so`
(156,952 bytes; SHA-256
`2FAA16E474F883D41EB7EAD7C4A67CAC39E3382BAAD743C781F56990DC88B949`).
`llvm-readelf` identified it as ELF64, little-endian, AArch64, shared object;
the expected `MPTIME` and COW pagemap log labels are present. Compilation
completed with only the existing unused-function warnings. This artifact has
not yet been copied to the phone or run at the time of this build note; the
follow-up phone run is recorded below.

### 2026-11-26 instrumented COW diagnostic run

The instrumented build was imported into RMG Next 0.13+local.af081c2 as
`wchan-cow-pagemap-mprotect-trace-20261125.so`. RMG identified the exact
`r9q-G990BXXSLIZH2` profile, Shizuku transport, and the local payload name. It
printed a generic `payload origin=downloaded` line later, but the run emitted
the new build's `MPTIME` and pre-pselect pagemap records. KernelSU service
preparation succeeded. The wrapper displayed `attempts=24`; the preload
supervisor and exploit both confirmed one attempt.

This run directly captured the mapping transition that the prior run could
not: immediately before `pselect`, `cow_out` pagemap was
`0xa100000000000000` (present, file-backed, exclusive); after the syscall it
was `0x8100000000000000` (present, anonymous, exclusive). The syscall returned
112 in 154 microseconds. Smaps reported anonymous usage changing from 0 to 4
KiB. This is strong direct evidence that the `pselect` output copy converted
the private file-backed page to an anonymous page. The soft-dirty bit remains
unusable on this firmware because `CONFIG_CHECKPOINT_RESTORE` is disabled.

The monotonic trace places the `pselect` interval at 22,091,996,070 to
22,091,996,225 microseconds. The userspace `mprotect()` intervals were:

| Call | Protection | Interval (microseconds) | Duration |
| --- | --- | ---: | ---: |
| 1 | `PROT_NONE` | 22,091,996,025–22,091,996,116 | 91 μs |
| 2 | `PROT_READ` | 22,091,996,117–22,091,996,188 | 71 μs |
| 3 | `PROT_NONE` | 22,091,996,188–22,091,996,325 | 137 μs |
| 4 | restore `PROT_READ` | 22,091,996,325–22,091,996,402 | 77 μs |

All four calls returned success with errno 0. `pselect` overlapped parts of
calls 1 and 3 and all of call 2; the restore call began after it returned.
These are syscall entry/return times, not direct measurements of when the
kernel held `mmap_lock`, so they show temporal overlap but do not prove lock
ownership throughout each interval. The mmap locker reported three calls and
383 microseconds total.

The monitor made 4,818 polls but captured no blocked-state sample; its first
and last task state were `R` (82). Therefore the page fault/COW completed, but
this run still did not observe the intended blocked waitpoint. In the
`ROUTE_READINESS_SPRAY` source branch, `route_signal` requires a blocked sample,
a valid overlay, and nonzero consumer `calls` and `success`; otherwise it sets
`cfi_last_step=33` without entering `try_cfi_stage()`. This run reported
`blocked=0`, `overlay=1`, `calls=0`, and `success=0`, so the route did not
reach CFI. A separate canary diagnostic scanned 16,420,480 of 16,777,216 bytes
and found `changed_chunks=0`, `task_pi=0`, and `mutated=0`; that incomplete
peek did not set step 33. The physical read/write summary remained `done=0`,
`root=0`, `read_ok=0`, `write_ok=0`, and `rw64=0/0`; RMG returned code 255 and
KernelSU remained not loaded.

After the attempt the phone stayed booted on the same Android 16 fingerprint
and kernel, power saver was off, Shizuku was running, and RMG showed KernelSU
not loaded. The original local payload
`rmg-cow-only-512m-anon-softdirty-20261124.so` was restored and the temporary
candidate and baseline copies were removed from Downloads. The extracted RMG
log and after-run screenshot are saved in
`build/wchan-cow-pagemap-mprotect-trace-20261125/` as
`rmg-run-log-20261126.txt` and `phone-after-run-20261126.png`.

Do not rerun this artifact unchanged. The next useful step is source-level
review of the 5.4.289 `pselect` copyout fault path and the mmap-lock acquisition
inside `mprotect()`, then compare that path with the monitor and consumer
conditions that stayed false. The current `MPTIME` values bracket userspace
syscalls; they do not prove the kernel lock was held for those full intervals.

### Source-level copyout and speculative-fault path

The base kernel archive and SLIZH2 update were checked together. The update
does not replace `fs/select.c`, `arch/arm64/mm/fault.c`, `mm/memory.c`, or
`mm/mprotect.c`. In the matching source, `core_sys_select()` calls `do_select()`
and, for a nonnegative result, copies `fds.res_out` to the caller through
`set_fd_set()` and `__copy_to_user()`. On arm64, the raw copy reaches
`__arch_copy_to_user()`; a write-protection fault is handled as a write fault
by `do_page_fault()`.

The local r9q reference config has `CONFIG_SPECULATIVE_PAGE_FAULT=y`; the
exact running-kernel config was not read back. Arm64 first
tries `handle_speculative_fault()` without `mmap_sem`. However,
`__handle_speculative_fault()` returns `VM_FAULT_RETRY` for a write fault on a
private file-backed VMA when `anon_vma` is absent. The payload creates
`cow_out` as a new writable `MAP_PRIVATE` memfd mapping and only reads it to
prefault the file PTE; it does not privately write that mapping first. The
runtime `anon_vma` pointer is not directly observable from shell, but the
source and setup make the normal retry path the expected case for this first
COW.

On that retry, arm64 takes `mm->mmap_sem` for reading, with `down_read()` after
a failed trylock. In `mm/mprotect.c`, `do_mprotect_pkey()` takes the same
semaphore for writing before `mprotect_fixup()` and releases it with
`up_write()` on return; `mprotect_fixup()` updates VMA flags and page-table
protections while the write lock is held. Thus a normal-path COW fault
arriving while the helper owns the write lock will contend on the same
semaphore. With rwsem owner spinning enabled in the reference config, that
contention may remain runnable instead of appearing as a sleeping D-state
sample; see the source refinement below. The run's pagemap
transition proves the copyout eventually completed, but the userspace
`mprotect()` timestamps cannot tell whether the write lock was already held
when the copyout fault reached `down_read()`.

The device exposes no `events/lock` or `events/mmap` trace group to the shell,
and shell access to `/sys/kernel/tracing/available_events` is denied, so a
direct runtime semaphore trace is not available through the current Shizuku
shell. The COW-only helper used a 64-MiB PTE region; its three protection
changes took 71–137 μs each, while the full `pselect()` took 154 μs. Since
the output copy occurs after `do_select()` computes readiness, the fault may
arrive near the end of the syscall, after the currently active writer has
released the semaphore or while a later writer is queued. The exact fault
timestamp and lock ownership remain unobserved. A future bounded diagnostic
could increase the PTE region to lengthen the first `mprotect()` write-lock
window while retaining the one-attempt COW-only guard; first compare that
variant offline, and do not enable CFI, physical read/write, or root stages in
that experiment.

### Follow-up: waiter identity and read-lock wait state

The payload source confirms the monitor tracks the thread that executes the
route: `waiter_thread()` stores its TID in `waiter_tid`; the signal handler
invoked on that TID calls `do_pselect_fake_lock_route()`; and the readiness
monitor opens `/proc/self/task/<waiter_tid>/stat`. The no-block result is
therefore unlikely to come from monitoring a different thread.

The saved trace gives the active window more precisely. The monitor armed at
22,091,988,008 us and its first sample was `R` at 22,091,988,031 us. The COW
route began at 22,091,996,019 us, about 7.99 ms later. `mprotect()` call 1
started at 22,091,996,025 us; `pselect` ran from 22,091,996,070 to
22,091,996,225 us; call 3 ran from 22,091,996,188 to 22,091,996,325 us.
Thus a protection-change syscall was still in flight for about 100 us after
`pselect` returned, and the monitor had been active well before the syscall.
The affinity log placed the mprotect helper on core 5 and the waiter on core
6, so the two threads were assigned separate CPUs during this interval.
The 4,818 polls span this longer armed window; they are not 4,818 samples
within the roughly 155-us `pselect` interval (the separately logged duration
was 154 us).

In the matching kernel source, a write-protection fault from arm64 uaccess is
marked `FAULT_FLAG_WRITE`. `do_page_fault()` first tries the speculative fault
path; if it returns `VM_FAULT_RETRY`, the handler falls back to
`down_read_trylock()` and then `down_read()`. The rwsem slow path for
`down_read()` sleeps in `TASK_UNINTERRUPTIBLE`, which is reported as `D` in
`/proc/<pid>/stat`. This supports using a `D` sample as evidence of a sustained
wait, but the absence of one does not identify whether the speculative path
completed the COW or the read lock was acquired before the writer queued.
`MPTIME` still records syscall entry/return, not lock acquisition, and the
exact COW-fault timestamp is unavailable.

The payload path creates a fresh memfd, writes the backing file before the
mapping exists, then maps it `MAP_PRIVATE` and reads it to prefault. The
monitor and mprotect helper are created afterward; the first write to
`cow_out` is the pselect copyout, and the explicit calibration write happens
after pselect. No path in this route intentionally primes the mapping's
`anon_vma`. In the kernel source, the speculative-fault check retries when
that pointer is absent, while the normal COW path's `wp_page_copy()` calls
`anon_vma_prepare()` under the mmap semaphore. The expected path for this
fresh mapping is therefore the mmap-sem read path, although the runtime VMA
pointer remains unobserved.

The requested dual-TID follow-up was run once with the same 64-MiB,
one-attempt COW-only setup; its result is recorded below. This remains
observational evidence: userspace state and syscall timing do not directly
prove mmap semaphore ownership.

### 2026-11-26 dual-TID readiness COW-only run

The diagnostic artifact was built with NDK r27c at
`build/readiness-dual-thread-cow-20261126/cve-2026-43499-app.so` (160,160
bytes; SHA-256
`d605061de239b1ee59b4ca2bb1dcabad8468983020dddee5c3a32572616ff6fb`). RMG
Next 0.13+local.af081c2 selected the matching SM-G990B / kernel 5.4.289
profile over Shizuku. Its run history names the imported
`rmg-readiness-dual-thread-cow-20261126.so`; the later generic
`payload origin=downloaded` label does not mean the downloaded exploit ran:
RMG also logged `Using local payload`, and its source replaces the downloaded
exploit with the imported file. The preload supervisor and exploit log each
reported one attempt despite the app's generic `attempts=24` route header.

The run completed normally in about one minute and RMG marked it failed at
“Running kernel exploit” with exit code 255. Its generic message about a
refused or unavailable privilege is not the useful diagnosis here: the
payload did execute and wrote its full diagnostic output. RMG verified and
staged the profile's KernelSU service before the exploit, but did not reach the
post-root KernelSU load step. The handset stayed ADB-responsive on the expected
Android 16 fingerprint and kernel, with shell uid 2000, battery saver off,
Shizuku running, KernelSU not loaded, and neither `/dev/ksu` nor `/proc/ksu`.
Device uptime was about 7 h 22 min after the run;
the post-run `ro.boot.bootreason=kernel_panic,bug` value was not captured before
the attempt and is not evidence that this attempt rebooted the phone.

The dual-thread monitor tracked waiter TID 16247 and mprotect-helper TID
16264. The waiter monitor recorded 2,356 polls with `first_state=R`,
`last_state=R`, and `blocked_samples=0`; the helper recorded 2,355 polls with
the same all-running state and no blocked sample. `wchan` was unavailable
because neither thread was observed in `D`. The two `COWSTATE` records were
the initial `R` samples before the trigger gate opened; the state-event buffer
was not truncated. Thus this run did not capture a sustained rwsem wait in
either thread. A very brief wait shorter than sampling can still not be ruled
out.

`pselect` returned 103 in 146 microseconds, from monotonic time 26,109,144,063
to 26,109,144,209 us. The nonblocking fd-set preflight also returned 103;
consistent with `select()` semantics, this is the number of ready descriptors
in the crafted write set, not an errno. All four traced `mprotect()` calls
returned 0/errno 0:

| Call | Protection | Interval (monotonic microseconds) | Duration |
| --- | --- | ---: | ---: |
| 1 | `PROT_NONE` | 26,109,144,015–26,109,144,109 | 94 μs |
| 2 | `PROT_READ` | 26,109,144,109–26,109,144,180 | 71 μs |
| 3 | `PROT_NONE` | 26,109,144,180–26,109,144,285 | 105 μs |
| 4 | restore `PROT_READ` | 26,109,144,285–26,109,144,439 | 154 μs |

Calls 1–3 overlap the `pselect` interval; call 3's userspace return is 76 us
after `pselect` returned. These are user-side syscall brackets, not lock
acquisition/release timestamps, so they do not establish that
`mmap_sem` was held when the copyout fault occurred.

The page mapping did undergo COW: pagemap changed from
`0xa100000000000000` (present, file-backed, exclusive) before `pselect` to
`0x8100000000000000` (present, anonymous, exclusive) afterward; smaps
anonymous usage changed from 0 to 4 KiB. Soft-dirty remained unavailable on
this firmware. The COW setup used 64 MiB. However, the route gates stayed
false: `overlay=1`, `blocked=0`, consumer `calls=0`, and `success=0`. The
canary peek covered 16,420,480 of 16,777,216 requested bytes, reported zero
changed chunks and zero task PI changes, and was explicitly marked incomplete.
The route signal therefore remained 0. The build's COW-only guard forced it
to 0 before the `try_cfi_stage()` branch; no CFI, physical read/write, or root
stage ran. The `readiness-cow cfi miss` / step 33 text is the shared route's
failure status, not evidence that CFI was attempted. The `pipe-physrw-summary`
lines show `done=0`, `root=0`, `read_ok=0`, and `write_ok=0`.

The evidence narrows the immediate miss: COW completed, but no waiter/helper
blocked state or successful readiness walk was observed. The short
`pselect`/COW window and the helper's mmap-lock timing remain unresolved; the
available userspace trace cannot show whether a brief lock wait happened
between polls. Continue with offline source/scheduling analysis of the COW
copyout and helper lock window before designing another diagnostic. Keep the
CFI, physical read/write, and root guards in place.

The full RMG run log is
`build/readiness-dual-thread-cow-20261126/rmg-run-log-20261126.txt` (SHA-256
`60722a9d583caee400e7ddd51d84d614ef9611e441480380f20863abe17b23a9`); the
durable device checkpoint is
`build/readiness-dual-thread-cow-20261126/rmg-pipe-oracle-checkpoint.log`
(SHA-256 `0fcf769bb0f4f69c867978ce83fe4945b5321671113bab0e93057f146083e687`).
After capture, the previous local payload
`rmg-cow-only-512m-anon-softdirty-20261124.so` was restored and its app-private
SHA-256 verified as
`a985be9da9ff46578faa119eece3ba7f2d5ca732d489812f3b261333b942e7fc`. The
one-off diagnostic `.so` remains in Downloads but is no longer selected in
RMG.

### 2026-11-26 source-level COW and mmap-lock timing review

The local matching Samsung source tree was rechecked at
`/home/mick/src/sm-g990b-lizh2-osrc`. The diagnostic creates a fresh memfd,
writes the initial fd set before mapping it, then maps it `MAP_PRIVATE` and
only reads it to prefault. The file-backed `mmap_region()` path does not
attach an `anon_vma`; the memfd's `shmem_mmap()` sets its VMA operations but
does not attach one either. There is no earlier private write to this mapping.
This makes a preexisting `anon_vma` unlikely for this one-page mapping, though
the runtime pointer was not directly observed.

In this source, arm64 `do_page_fault()` marks a kernel copyout write fault
with `FAULT_FLAG_WRITE`. `__handle_speculative_fault()` returns
`VM_FAULT_RETRY` for a private file-backed VMA when that fault needs an
`anon_vma` and none is attached. The normal fallback then tries
`down_read_trylock(&mm->mmap_sem)` and calls `down_read()` if it cannot acquire
the lock. `wp_page_copy()` calls `anon_vma_prepare()` on that path. Conversely,
`do_mprotect_pkey()` takes `down_write_killable(&mm->mmap_sem)` before
`mprotect_fixup()` and calls `up_write()` on exit. The source therefore
supports the expected contention model: if this first COW fault reached
the normal path while the helper held the write lock, the waiter would contend on
the same semaphore. Whether it sleeps is a separate question; the target
config's rwsem owner-spinning behavior is examined below.

The trace-build constant `READINESS_LOCK_HEADSTART_USEC` is 50 μs. In this
run, the helper's first `mprotect()` entry was at 26,109,144,015 μs; the
`pselect` started at 26,109,144,063 μs, 48 μs later, and returned at
26,109,144,209 μs. Relative to `pselect` entry, the four userspace syscall
brackets were: call 1, −48 to +46 μs; call 2, +46 to +117 μs; call 3, +117 to
+222 μs; and the restore call, +222 to +376 μs. The result-set copy happens
after `do_select()` computes readiness, so its COW fault is late in the
`pselect` path, but this trace does not timestamp that individual copy. The
third call's user-side bracket overlaps the final 29 μs of `pselect`; its
return at +222 μs does not prove the semaphore remained write-locked until
then, because the lock is released inside the kernel before the syscall
returns to userspace.

The run's affinity record placed the waiter on core 6 and the mprotect helper
on core 5. They were not competing for one CPU, although this does not prove
when either thread entered or left the semaphore critical section.

In `fs/select.c`, `core_sys_select()` calls `do_select()` and then copies
`res_out` through `set_fd_set()` / `__copy_to_user()`. The fd-set preflight
and `pselect` both returned 103, which is a ready-fd count, not an errno. This
indicates the crafted write set was ready and the syscall did not sleep
waiting for fd readiness. The COW mapping transition confirms that result
copyout still caused COW. Together with 2,356 waiter
state polls and zero `D` samples, the leading explanation is that the COW
fault did not overlap a sufficiently long interval in which the helper held
the write semaphore, or any wait was too brief for the sampler to capture.
The userspace trace cannot distinguish those cases or exclude a speculative
fault path if the assumed VMA state was wrong.

The source also shows a possible way to widen the lock interval: `mprotect_fixup()`
calls `change_protection()`, which walks the affected PTE range; the PMD walk
calls `cond_resched()` between 2-MiB chunks while `do_mprotect_pkey()` still
holds `mmap_sem` for writing. The 64-MiB mapping spans about 32 such chunks;
256 MiB would span about 128. Since the helper prefaults read-only anonymous
pages that map the shared zero page, `MAP_NORESERVE` avoids allocating 256 MiB
of data pages; a 256-MiB mapping would still need roughly 512 KiB of 4-KiB PTE
pages, plus upper-level tables. These are source-based estimates, not measured
on-device costs. The observed 64-MiB `mprotect()` calls took 71–105 μs, so a
256-MiB region is a plausible next timing candidate, but linear scaling is
not guaranteed.

This cross-check narrows the next design question to the timing of the
copyout fault relative to actual semaphore ownership. The current helper's
`mprotect()` entry/return timestamps only bound that ownership interval; they
do not reveal it. A 256-MiB variant should retain the one-attempt and COW-only
guards and the dual-TID monitor. Do not infer lock ownership from the
overlapping syscall brackets; direct kernel lock tracing is not available
through the current shell access.

### 2026-11-26 256-MiB dual-thread COW-only run

A fresh AArch64 app payload was built for `r9q-G990BXXSLIZH2` with NDK r27c
and explicit flags
`RMG_DIAG_COW_ONLY=1`, `RMG_DIAG_COW_TRACE=1`,
`RMG_DIAG_DURABLE_LOG=1`, and `RMG_DIAG_COW_PTE_MB=256`. The artifact is
`build/readiness-dual-thread-cow-256m-20261126/cve-2026-43499-app.so`
(160,160 bytes; SHA-256
`C7408E4D97F6C1A97EE9A40D72FDBE2B38BEA93024FA555C23A256BD64474C90`).
RMG Next 0.13+local.af081c2 selected the matching
`r9q-G990BXXSLIZH2` profile from
`Ryujin9610/Root-My-Galaxy-Payloads @ r9q-g990b-lizh2` (source commit
`3095e6f2a0659570b5adc54a4cb6b121f96d2fa2`) over Shizuku. The run record is
`92afcd0d-8901-4745-887c-dc11fb43c318`. RMG explicitly logged
`Using local payload: rmg-cow-only-256m-dual-thread-20261126.so`; the later
generic `payload origin=downloaded` line refers to the combined verified
payload set, which still included the separately downloaded KernelSU service.

KernelSU loading was temporarily disabled in RMG for this diagnostic; the
automatic root-on-boot switches were off. RMG still prepared the service file
before launching the exploit, but the post-root KernelSU load stage was not
entered. The payload reported
`diagnostic=cow-only cfi_write_pipe_physrw_root=disabled`, supervisor
`attempts=1`, and `exploit attempt=1/1`. The generic route header still said
`attempts=24`; that is not the number of payload attempts. The attempt began
at phone time 04:45:24 and ended at 04:48:00 with RMG result `Failed` at
“Running kernel exploit”, code 255. This is the expected diagnostic stop,
not a root result.

Controlled reclaim collected four MM-slab groups, filling 34 target objects
in the final group after 84 scan attempts. It reported spectrum total 4 and
251/256 reclaim sends, then prepared one kernel page in 144,653 ms. KASLR
resolution and page preparation succeeded. The 256-MiB readiness mapping
was 268,435,456 bytes. The nonblocking fd-set preflight returned 106, and
`pselect` returned the same ready-descriptor count in 329 us; these values
are not errno values. Both traced helper `mprotect()` calls succeeded:

| Call | Protection | Monotonic interval (μs) | Duration |
| --- | --- | ---: | ---: |
| 1 | `PROT_NONE` | 37,924,176,035–37,924,176,383 | 348 μs |
| 2 | `PROT_READ` | 37,924,176,383–37,924,176,699 | 316 μs |

The helper reported two iterations and 668 μs total. Its monitor reported
2,330 polls, first and last state `R` (the log stores the ASCII value 82),
and zero blocked samples. The `COWSTATE` buffer retained only the two initial
`R` records for the waiter and helper; `wchan` was unavailable. The
`pselect`/helper intervals overlapped, but these userspace brackets do not
show whether the COW copyout fault waited for `mmap_sem`.

COW itself was confirmed: pagemap changed from
`0xa100000000000000` (present, file-backed, exclusive) before `pselect` to
`0x8100000000000000` (present, anonymous, exclusive) afterward. The target
VMA's smaps anonymous count changed from 0 to 4 KiB. Soft-dirty remained 0
and did not add evidence. The readiness walk recorded `calls=0 success=0`,
`blocked=0`, and `sched=0/0`; its overlay geometry check passed. The
incomplete canary peek covered 16,420,480 of 16,777,216 requested bytes and
reported no changed chunks, so its negative result is not complete coverage.
The COW-only compile guard then forced `route_signal=0` and stopped at step
33 before `try_cfi_stage()`. The summary reported `done=0 root=0`,
`read_ok=0 write_ok=0`, and no physical-RW or root stage ran.

The result narrows the miss: the 256-MiB run's helper calls lasted 316–348 μs,
longer than the earlier 64-MiB brackets, but the call sequence differed (two
calls here versus four previously), so the timing difference cannot be
attributed to mapping size alone. COW completed, but the monitor saw no blocked
waiter/helper state and the readiness consumer made no successful calls. The
run therefore does not establish whether the copyout fault missed the actual
write-lock interval or briefly waited between samples. Continue source/timing
analysis before another device attempt.

The full RMG output is
`build/readiness-dual-thread-cow-256m-20261126/rmg-run-log-20261126.txt`
(SHA-256
`482C9A444F0F5465462CA0C187F1BEAA95A27BC72849E64B8A49E67B69B45BA9`);
the run record JSON and result-screen capture (`run-finished.png`) are
alongside it. The durable device checkpoint is
`build/readiness-dual-thread-cow-256m-20261126/rmg-pipe-oracle-checkpoint.log`
(SHA-256
`27F02B0D92B470083EFF5579B0372F15C0B5DB2500513FF23650E4361FD5C98C`). The
phone remained ADB-responsive on the expected firmware/kernel, with battery
saver off, Shizuku running, shell uid 2000, and neither `/dev/ksu` nor
`/proc/ksu`. Uptime increased continuously through the attempt; the
post-run `ro.boot.bootreason=kernel_panic,bug` value was not captured before
this run and is not attributed to it. After capture, RMG's local payload was
restored to `rmg-cow-only-512m-anon-softdirty-20261124.so`; the app-private
SHA-256 again matched `A985BE9DA9FF46578FAA119EECE3BA7F2D5CA732D489812F3B261333B942E7FC`.
The original Install KernelSU setting was restored, and the 256-MiB file was
left in Downloads but is no longer selected.

### 2026-11-26 rwsem owner-spin refinement

The source review found a reason that real mmap-lock contention could produce
no `/proc` `D` sample. The downloaded Samsung source archive is kernel 5.4.289.
Its reference target config,
`arch/arm64/configs/vendor/r9q_eur_openx_defconfig`, enables both
`CONFIG_SPECULATIVE_PAGE_FAULT=y` and `CONFIG_RWSEM_SPIN_ON_OWNER=y`. This is
the target defconfig, not a runtime readback of the exact configuration built
into the phone's running kernel.

The source's initial mapping path is consistent with a first write-protect
fault that has no `anon_vma`: `readiness_map_cow_fdset()` writes the fresh
memfd before mapping it, then maps it `MAP_PRIVATE` and only reads it to
prefault. In `mm/mmap.c`, `mmap_region()` passes a null anon-vma to
`vma_merge()`; for a file-backed VMA it calls the file's mmap method instead
of `vma_set_anonymous()`. `mm/shmem.c:shmem_mmap()` installs shmem VMA
operations but does not attach an anon-vma. The runtime anon-vma pointer was
not directly observed, so this remains a source-backed expectation rather
than a runtime measurement.

With speculative page faults enabled, arm64 marks the kernel copyout fault
`FAULT_FLAG_WRITE`. `mm/memory.c:__handle_speculative_fault()` returns
`VM_FAULT_RETRY` for a private file-backed write fault with no anon-vma. The
fallback in `arch/arm64/mm/fault.c:do_page_fault()` then attempts
`down_read_trylock(&mm->mmap_sem)` and falls through to `down_read()` if that
fails. `wp_page_copy()` subsequently creates the anon-vma as needed.

That `down_read()` is not guaranteed to put the task in `D`. In
`kernel/locking/rwsem.c`, `rwsem_down_read_slowpath()` first tries
`rwsem_optimistic_spin()`. With `CONFIG_RWSEM_SPIN_ON_OWNER`, the reader can
stay runnable and spin on an on-CPU writer; only after spinning fails and it
queues does it set the sleeping task state used by /proc as `D`. The 256-MiB
run pinned the helper to core 4 and waiter to core 6. If the helper owned the
write lock while running on core 4, the waiter could therefore spend the
overlap spinning in `R` without ever producing a `D` sample. The monitor's
`first_state=R`, `last_state=R`, and `blocked_samples=0` do not rule out that
case.

The diagnostic monitor only treats `D` as the intended waitpoint. An `R`
sample after the trigger gate records that the waiter is runnable; it does not
invoke the consumer. In the COW-only build, even a valid `D` sample would log
that the scheduler action was skipped. The 256-MiB attempt therefore remains
an observational COW run and did not test whether the scheduler route can
operate during an rwsem spin.

The monotonic trace brackets are compatible with overlapping work but do not
locate semaphore ownership or the exact copyout fault. The helper's first
`mprotect()` bracket was 37,924,176,035–37,924,176,383 μs; its second was
37,924,176,383–37,924,176,699 μs. The COWTRACE
PSELECT_BEFORE and PSELECT_AFTER markers were recorded at
37,924,176,083 and 37,924,176,412 μs, respectively; the payload reported
pselect_us=329. The post-pselect marker was 29 μs after the first bracket
ended and 287 μs before the second bracket ended. Since `mprotect()`
releases `mmap_sem` inside the kernel before returning to userspace, those
bracket endpoints are not lock-unlock timestamps. Combined with the COW
transition, the trace cannot distinguish among a fault that acquired the
read lock before the helper, optimistic spinning during contention, or a
brief wait missed by sampling.

The 256-MiB trace makes an rwsem-spin explanation plausible but does not
prove that lock contention occurred. The route source is written around a
blocked fault: `port/fops.c` waits for `TASK_UNINTERRUPTIBLE` as its intended
COW waitpoint, and `overlay_asm.S` describes the pselect stack remaining live
while the fault blocks. The rwsem spin path keeps the fault handler on
stack but has not entered `schedule()`, so its stack shape and timing relative
to `sched_setattr` may differ. The source review does not establish that
the current overlay is safe to trigger during R-state spinning. Keep the
D gate and COW-only and post-COW guards in place; continue with offline
stack-depth review before considering another diagnostic.



### 2026-11-26 exact firmware stack and monitor-affinity audit

This review used the existing boot-derived artifacts rather than the reference
defconfig alone. The recovered kernel Image from the Desktop AP package has
SHA-256
65345CD2969F22A10749F4B5B31ED631954A749C3CAD53292C5ED7D116484C40. The
recovered ELF hash recorded above is
AC26E724175922F9C893685927D5D866C42DA2FB0F4A8C9F601E84FF76C642DB. The
extracted `work/firmware-analysis/kernel.config` identifies arm64 5.4.289
and sets `CONFIG_SPECULATIVE_PAGE_FAULT=y`,
`CONFIG_RWSEM_SPIN_ON_OWNER=y`, `CONFIG_PREEMPT=y`,
`CONFIG_VMAP_STACK=y`, and `CONFIG_FRAME_POINTER=y`. This is the firmware
package configuration matching the installed build, not a live query of the
kernel's current config.

The recovered ELF's function prologues give these compiled frame sizes:

| Function | Frame |
| --- | ---: |
| `__arm64_sys_pselect6` | 0x10 |
| `__do_sys_pselect6` | 0x90 |
| `core_sys_select` | 0x1c0 |
| `set_fd_set` | 0x30 |
| `do_mem_abort` | 0x50 |
| `do_page_fault` | 0x70 |
| `rwsem_down_read_slowpath` | 0xc0 |
| `rwsem_optimistic_spin` | 0x60 |
| `rwsem_spin_on_owner` | 0x30 |
| `schedule` | 0x20 |
| `__schedule` | 0x80 |

At the copyout in `set_fd_set`, the pselect syscall frames place the current
stack pointer about `SP0-0x290`, using the syscall-entry SP0 from the earlier
stack audit. The crafted waiter words remain at `SP0-0x168`, `SP0-0x160`, and
`SP0-0x158`. Copyout fault and abort-handler frames grow below the current
pointer. The initial `__handle_speculative_fault()` frame is 0xf0 bytes, but
that call returns `VM_FAULT_RETRY` before the fallback `down_read()` path
begins, so it is not part of the subsequent rwsem spin or sleep stack. If the
reader optimistically spins, the active calls include
`rwsem_optimistic_spin()` and `rwsem_spin_on_owner()`, with preemption disabled.
If it queues, those spin frames unwind and the wait path enters
`schedule()`/`__schedule()` and context-switch code. The pselect call remains
active in either case, but its active stack and scheduling interleaving differ.
These per-function frame sizes do not include all exception-entry, IRQ, or
context-switch frames and do not prove the overlay is safe in the spin case.

One target comment in `port/fops.c` says the 5.4 arm64 route has no
VMAP_STACK. That conflicts with the extracted firmware config's
`CONFIG_VMAP_STACK=y`. The exact pselect/futex offset arithmetic is
independently verified against this recovered ELF, but that comment should be
treated as stale when reasoning about the firmware's stack implementation.

The monitor-affinity setup explains why the last run could miss an R-state
spin entirely. `waiter_thread()` pins the waiter to `CORE=6` before calling
`do_pselect_fake_lock_route()`. That route creates
`readiness_block_monitor_thread` with `pthread_create()` and does not change
the monitor's affinity, so Linux gives it the caller's CPU-6 affinity. The
same route creates the mmap helper; its log records inherited mask 0x40
(CPU 6), then records that it moved to core 4, with waiter_core=6. This
corroborates the parent thread's CPU-6 affinity; the monitor is created
before the helper changes its own mask.

If the waiter enters `rwsem_optimistic_spin()` on CPU 6, preemption is
disabled while it spins. A monitor restricted to CPU 6 cannot run concurrently
to read `/proc/self/task/<tid>/stat`. If the waiter instead sleeps in the
queued rwsem path, CPU 6 can schedule the monitor, which can observe D. The
run logged 2,330 monitor polls, but its first-state sample was at
37,924,168,055 μs, about 8 ms before the trigger-gate marker at
37,924,176,083 μs. There is no `COWTRACE POST_GATE_RUNNABLE` event (event
10), blocked event (event 5), or `readiness-monitor-post-gate-runnable`
record. The short 329-μs pselect window therefore has no recorded monitor
observation. Zero D samples do not rule out the spin path; the monitor's
same-core affinity makes that path specifically unobservable. The poll count
does not show that it sampled during pselect.

This does not make it safe to change the trigger from D to R. The payload's
`sched_setattr_tid()` changes the target's scheduling parameters. In the
matching kernel, `__sched_setscheduler()` calls `check_class_changed()` and
then `rt_mutex_adjust_pi()`; a fair-task priority change can call
`resched_curr()`, which sends a reschedule IPI to a task running on another
CPU. A target inside the rwsem spin has preemption disabled, so rescheduling is
deferred until the spin path reenables preemption. That changes the timing
between the PI walk, COW completion, and the pselect overlay's lifetime. The
existing D-gated route's correctness cannot be assumed for this different
interleaving.

The strongest conclusion is now narrower: a monitor blind spot is a credible
explanation for the absent D/R observation, while actual mmap_sem ownership at
the copyout fault remains unmeasured. A safe diagnostic design to consider
next is a passive COW-only observer pinned away from CPU 6, with the scheduler
consumer still disabled. That could record whether the waiter is R or D during
the fault without changing its scheduling parameters. No such candidate was
built or run in this review.

### 2026-11-26 passive observer design

A follow-up diagnostic can address the monitor's same-core blind spot without
calling the scheduler consumer. The monitor should set and verify its own
affinity to an online CPU allowed by the process cpuset, separate from waiter
CPU 6 and mmap-helper CPU 4. The route should log the requested and effective
CPU masks and `sched_getcpu()` for waiter, monitor, and helper so affinity is
an observed fact rather than an assumption. If no distinct allowed CPU exists,
do not run this design.

Keep the build in `RMG_DIAG_COW_ONLY` mode. The monitor may read the waiter's
`/proc/self/task/<tid>/stat` and record state transitions with monotonic times
bracketed by the existing trigger-gate and pselect markers. It must never call
`sched_setattr_tid*()` or alter waiter priority, policy, or utilization clamp.
A useful result requires an actual state observation during the pselect window;
a poll count by itself is insufficient. The log should also show the observer
was scheduled on its selected CPU during that interval, if the available
interfaces can establish that without changing the waiter.

A sampled `D` state would confirm a blocked interval but would not identify the
lock or prove that the current overlay is safe. A sampled `R` state during the
fault window would be compatible with optimistic spinning, but `/proc` state
alone cannot prove that the waiter owns or is contending on `mmap_sem`. No
sample during the window remains inconclusive. Do not trigger the existing
scheduler path on `R` based on this observation; the priority change can send a
reschedule IPI and change the overlay's timing.

This is a design proposal only. No source change, candidate build, or device
run was made as part of the stack and scheduler-path review.

### 2026-11-26 passive observer candidate built

Implemented the passive observer as a compile-time diagnostic guarded by
`RMG_DIAG_PASSIVE_COW_OBSERVER=1`. In its observer-only profile it requires
both `RMG_DIAG_COW_ONLY=1` and `RMG_DIAG_COW_TRACE=1`; a later guarded PI
variant also permits `RMG_DIAG_PI_PROBE_ONLY=1` with
`RMG_DIAG_UCLAMP_TRIGGER_ONLY=1` and `RMG_DIAG_COW_TRACE=1` (see the final
sections below). The monitor expands only its own inherited affinity mask to
discover the cpuset-allowed CPUs. It predicts
the mmap helper's highest allowed CPU other than the waiter, pins itself to a
different allowed CPU, and verifies the resulting single-CPU affinity and
`sched_getcpu()`. If the waiter core is unknown or no separate observer core
can be pinned, monitor setup reports an error and the COW trigger is not
started.

Every fourth poll, the passive sample counter accepts the waiter's `/proc`
state only when both the trigger gate and `pselect_active` are set before and
after that `/proc` read. The state trace includes the observer CPU and
monotonic sample time. The final `COWPASSIVE` line reports the inherited,
allowed, and effective affinity masks, predicted helper CPU, bracketed sample
count, and first/last states, times, and observer CPUs. This timestamps the
existing userspace pselect-active bracket; it does not reveal the exact kernel
syscall-entry instruction or prove rwsem ownership.

Built the app payload with Android NDK `27.1.12297006`, target
`r9q-G990BXXSLIZH2`, `APP_PAYLOAD=1`, and the four diagnostic flags above plus
`RMG_DIAG_COW_PTE_MB=256` and `RMG_DIAG_DURABLE_LOG=1`. The artifact is
`build/passive-cow-observer-256m-20261126/cve-2026-43499-app.so` (162,552
bytes; SHA-256
`D2CDF589E565DC4F300A096E0F4BA03BD4A4EDB2528CA79FC33AC4F03CF2D8F9`). It is
an AArch64 ELF shared object. Compilation succeeded with only the existing
unused-function warnings. Binary inspection confirmed the passive observer
and scheduler-skipped diagnostic strings. The artifact was subsequently
selected in RMG, staged to the phone, and run once; see the following section.

### 2026-11-26 passive observer one-shot run

The 162,552-byte candidate from the preceding section was staged as
`/sdcard/Download/rmg-passive-cow-observer-256m-20261126.so`. Its device-side
SHA-256 matched the local artifact
(`D2CDF589E565DC4F300A096E0F4BA03BD4A4EDB2528CA79FC33AC4F03CF2D8F9`), and
RMG confirmed the matching app-private payload hash before the run. The
handset was the expected Android 16 / `G990BXXSLIZH2` build on kernel
`5.4.289-qgki-32192773-abG990BXXSLIZH2`; low-power mode was off. RMG reported
`diagnostic=cow-only` with CFI writes, pipe physical read/write, and root
installation disabled.

The observer affinity setup succeeded: inherited mask `0x40` (waiter CPU 6),
allowed mask `0xef`, observer effective mask `0x1` (CPU 0), and predicted
mmap-helper CPU 7. The affinity call returned no error. Three every-fourth-
poll samples met the strict gate-and-pselect-active checks; all were `R` on
CPU 0, from monotonic time 63,636,492,684 through 63,636,492,878 microseconds.
The state-transition trace also recorded waiter state `D` (`state=68`) at
poll 13, time 63,636,492,908 microseconds, with gate and pselect-active both
set and observer CPU 0. The pselect bracket ran from 63,636,492,670 to
63,636,492,977 microseconds, so the recorded D transition falls 69
microseconds before the bracket ended. This supports a D-state observation
during the userspace pselect-active window; it does not identify the exact
kernel wait site or establish rwsem ownership.

The original block handler then synchronously opened and read the waiter's
`wchan` before its post-sample active check. That read returned `pagemap_read`
after pselect had ended (`pselect_active=1/0` in the block snapshot). The
monitor therefore recorded `blocked_samples=0`. This timing is consistent
with the `wchan` read delaying the check past the short remaining pselect
window; the `wchan` value is a later snapshot and cannot be tied to the D-state
sample itself.

The COW transition was reproduced. Pagemap changed from
`a100000000000000` (present, file-backed) before pselect to
`8100000000000000` (present, anonymous) afterward; smaps anonymous memory
increased from 0 to 4 KiB. The first mprotect loop call overlapped pselect for
293 microseconds. Pselect returned 112 and the overlay words matched the
expected task, lock, and priority values. These results validate the
diagnostic COW/overlay observations only; the build stopped before CFI,
physical read/write, root, or KernelSU loading. RMG ended with its diagnostic
code 255. The handset stayed booted and ADB-responsive, with increasing
uptime, the expected firmware/kernel, and no `/dev/ksu` or `/proc/ksu` node.
The reported `ro.boot.bootreason=kernel_panic,bug` was already present from
earlier device history and was not caused by a reboot in this attempt.

Run evidence is in
`build/passive-cow-observer-256m-20261126/one-shot-run/`:

| File | SHA-256 |
| --- | --- |
| `rmg-run-log.txt` | `22B0718D28DD5D589CA85F1892D1BCBEB3D960269FA4351E59F887D05B0B2106` |
| `rmg-result.xml` | `9F7385F284A4338046B750BDB8E7160C5F06CBF169DAE0588D03D560C8B6852A` |
| `rmg-result.png` | `0248D10FEDF880FC4A0491C5BECDF5D0DADB368D39B3A49FCB5F9933635D28DD` |
| `device-post-run.txt` | `62F67F71C33CDAD75867744340C2701F6A7FB1F4712B63FF75D1B96B5E2B34D5` |

### 2026-11-26 deferred-wchan observer refinement

The next COW-only build records the D-state, monotonic timestamp, gate state,
and pselect-active state in memory before doing any optional procfs read. It
checks pselect activity immediately after that state capture. The waiter
`wchan` read is deferred until after the monitor and mmap-helper threads have
joined, and is explicitly labeled as a post-monitor snapshot. Durable
classification now uses the active flags captured around the state sample,
not the later `wchan` result. This removes the identified delay from the
monitor's timing-sensitive path while preserving the optional wait-channel
evidence.

Built with Android NDK r27b for target `r9q-G990BXXSLIZH2`, API 30, and the
same 256-MiB COW-only/passive-observer/durable-log flags as the prior build.
The artifact is
`build/passive-cow-observer-256m-20261126-deferred-wchan/cve-2026-43499-app.so`
(163,152 bytes; SHA-256
`21A7915D0C41683A1F5A215B43AA6E04875786B9F9A4429F67DAEEB20107846B`). ELF
inspection identified it as an AArch64 Android 30 shared object; string
inspection confirmed the new deferred-wchan and COW-only diagnostics. The
build completed with only the existing unused-function warnings. The
artifact was subsequently staged and run once; see the following section.

### 2026-11-26 deferred-wchan observer run

The refined waiter-wchan candidate was staged as
`/sdcard/Download/rmg-deferred-wchan-cow-observer-256m-20261126.so`. Its
SHA-256 (`21A7915D0C41683A1F5A215B43AA6E04875786B9F9A4429F67DAEEB20107846B`)
matched both the local build and RMG's app-private
`files/local-payload/local-exploit.so`. RMG selected support profile
`r9q-G990BXXSLIZH2`, Shizuku transport, and this local payload. The device was
on Android 16 / kernel `5.4.289-qgki-32192773-abG990BXXSLIZH2`, with power
saving off. RMG confirmed `diagnostic=cow-only` and
`cfi_write_pipe_physrw_root=disabled`.

This run's CPU placement differed from the preceding attempt. The payload's
initial requests for CPUs 7 and 6 returned `EINVAL`; the waiter fell back to
CPU 0. The monitor inherited mask `0x1`, expanded only to the process-allowed
mask `0x3f`, and pinned itself to CPU 1. It predicted and pinned the mprotect
helper to CPU 5. Its affinity setup succeeded. Seventeen periodic waiter
samples passed the strict gate-and-pselect-active checks; all were `R` on CPU
1, from monotonic time 66,362,479,010 through 66,362,479,947 microseconds.
No waiter D-state sample occurred, so the deferred waiter-wchan capture path
was not exercised in this run.

The monitor instead observed the mprotect helper in D state (`state=68`) at
time 66,362,480,022 microseconds, with gate and pselect-active set. Pselect
returned 34 microseconds later, at 66,362,480,056. The helper's
`/proc/.../wchan` read returned the string `0` with no read error. The
mprotect helper was still being read synchronously in this candidate; because
it was also the thread that entered D in this run, the next diagnostic
revision defers both waiter and helper wchan reads until after their threads
have joined. The helper snapshot is evidence of a D-state observation during
the active bracket, but its `wchan=0` does not identify the wait site.

Pagemap again changed from `a100000000000000` (present, file-backed) before
pselect to `8100000000000000` (present, anonymous) afterward. Smaps anonymous
memory changed from 0 to 4 KiB. The two mprotect calls covered the full
1,087-microsecond pselect interval; the helper D sample occurred during the
second call. The pselect result was 124, RMG printed `overlay=1`, and the
recorded task, lock, and priority words matched their expected values. The
positive preflight `select()` result was also 124. Its logged `errno=22` is
stale: the source read `errno` even after a positive return, and the earlier
CPU-affinity request had left `EINVAL` there. The follow-up source sets errno
to zero and reports it only for a failed `select()`.

As designed, this COW-only run stopped at `readiness-attempt-status a=33 b=0`
and `readiness-cow-only-stop-before-cfi`; it did not perform CFI writes,
physical read/write, root installation, or KernelSU loading. RMG showed code
255. The canary read covered 16,420,480 of 16,777,216 bytes and reported no
changed chunks or task/PI mutation. The handset stayed booted and ADB
responsive; `sys.boot_completed=1`, low-power mode remained off, and both
`/dev/ksu` and `/proc/ksu` were absent. `ro.boot.bootreason=kernel_panic,bug`
was unchanged from prior device history and no reboot occurred in this run.

The complete capture is in
`build/passive-cow-observer-256m-20261126-deferred-wchan/one-shot-run/`:

| File | SHA-256 |
| --- | --- |
| `rmg-run-log.txt` | `BDD8B89692198AA97545E8CED15B7472AC64E8BE923E0C502E68F5D7A7F471E2` |
| `rmg-result.xml` | `465481226F3CE2DD3AE0855A51F9B55281B9881D17F1842366B2ABA1D2E9B5EF` |
| `rmg-result.png` | `38BB93388D1B5A1F31F393A19480DA70A13092F2A986758DE6FFCEBA4E76A808` |
| `device-post-run.txt` | `5894EA250DAA9A3CAC2760282B0F14E9C52311726E73956E7B70DBA9605A37AC` |
| `rmg-pipe-oracle-checkpoint.log` | `980AAE3157532FED7BAF52C8CC54D006ADDD870B2B3CA4DB2288AF2766F091DC` |
| `payload-hashes.txt` | `B46F1F7A261F80384D8D7B01C90B4757D58D2DB07D4E27C535BFE270C6F87122` |

### 2026-11-26 defer both observer wchan reads

The helper D-state observation exposed a second synchronous procfs read in
the monitor loop: `readiness_record_locker_block_sample()` read the
mprotect-helper wchan before the monitor returned to waiter polling. The next
source revision records both task-state snapshots in memory and defers both
wchan reads until after the monitor and mmap-helper threads join. It also
clears errno before the preflight `select()` and records errno only if that
call fails, avoiding stale `EINVAL` in a successful positive-return record.

Built with Android NDK r27b for target `r9q-G990BXXSLIZH2`, API 30, with the
same 256-MiB COW-only/passive-observer/durable-log flags. The artifact is
`build/passive-cow-observer-256m-20261126-deferred-wchan-all/cve-2026-43499-app.so`
(163,608 bytes; SHA-256
`73C088105D2239F47D1A83A98FEBCCF261C018811584FB0157A3A7C7521709A4`). ELF
and diagnostic-string inspection succeeded; compilation emitted only the
existing unused-function warnings. This build has not been staged or run on
the phone. It was subsequently staged and run once; see the next section.

### 2026-11-26 deferred-both-wchan observer run

The 163,608-byte candidate was staged as
`/sdcard/Download/rmg-deferred-both-wchan-cow-observer-256m-20261126.so`.
SHA-256 `73C088105D2239F47D1A83A98FEBCCF261C018811584FB0157A3A7C7521709A4`
matched the local file, the device download, and RMG's app-private
`files/local-payload/local-exploit.so`. RMG selected profile
`r9q-G990BXXSLIZH2` over Shizuku. The handset remained on Android 16 and
kernel `5.4.289-qgki-32192773-abG990BXXSLIZH2`, with low-power mode off.
RMG confirmed the local payload, `diagnostic=cow-only`, and
`cfi_write_pipe_physrw_root=disabled`.

The observer pinned to CPU 0 with inherited mask `0x40`, allowed mask `0x6f`,
and `affinity_errno=0`. Separate route pin requests logged `EINVAL` and a
fallback to CPU 5; the monitor reported waiter CPU 6 and predicted/pinned
helper CPU 5. The pselect-active interval was 298 microseconds. Five periodic waiter
samples qualified inside the gate and active bracket, from 67,728,936,562
through 67,728,936,796 microseconds; `COWPASSIVE` reported R for both first
and last waiter samples. The state-transition trace recorded only the initial
R states for the waiter and helper and one waiter R state during the active
bracket. No D state was recorded for either thread, so neither deferred wchan
capture path ran. This run therefore does not validate the post-monitor wchan
behavior; it shows the monitor completed without observing either target
blocked.

The COW transition was reproduced again: pagemap changed from
`a100000000000000` (present, file-backed) to `8100000000000000` (present,
anonymous), and smaps anonymous memory increased from 0 to 4 KiB. The mprotect
calls overlapped the pselect bracket; total mprotect time was 622 microseconds.
Pselect returned 112, `overlay=1`, and the logged task, lock, and priority
words matched their expected values. The preflight select also returned 112
with `errno=0`, consistent with the corrected stale-errno reporting.

RMG stopped at diagnostic step 33 / `readiness-cow-only-stop-before-cfi` and
returned code 255. CFI, physical read/write, root, and KernelSU loading were
not attempted. The canary read remained incomplete at 16,420,480 of
16,777,216 bytes, with no changed chunks or task/PI mutation. The handset
stayed booted and ADB-responsive; post-run `sys.boot_completed=1`, uptime was
advancing, and `/dev/ksu` and `/proc/ksu` remained absent. The existing
`ro.boot.bootreason=kernel_panic,bug` value persisted; no reboot occurred in
this attempt.

Run evidence is in
`build/passive-cow-observer-256m-20261126-deferred-wchan-all/one-shot-run/`:

| File | SHA-256 |
| --- | --- |
| `rmg-run-log.txt` | `B182558456A5F594C454FEB4728B8E8CD8BF22BD53E27478E980F7D46330A465` |
| `rmg-result.xml` | `394A1F6666A7B8AFB49507B0CB642AA5A390C72A4789A92E551855C68877A6A7` |
| `rmg-result.png` | `10B7AE5DAC22D82068319E21EABEF844AF4797A5F9C7EB2262EF3F892BF3508D` |
| `device-post-run.txt` | `808FB1BC53E05EEE0D4B0D68A8633F88491F0BBF77AF45B34CDF27FC98381E94` |
| `rmg-pipe-oracle-checkpoint.log` | `36AC4FE43D66609E55764815EBE283264D47F64A38D40CBE7DEEA411C50DED85` |
| `payload-hashes.txt` | `A9972992BDE5B8A3AA39BB8E4D32282D249E1029F460508CC4C5FA88ED0C9823` |

### 2026-11-26 CPU 6/7 affinity preflight candidate

The recent runs show a CPU-placement correlation worth controlling, though
three attempts do not establish causality. In the first passive-observer run,
the allowed mask was `0xef`, so the mprotect helper selected CPU 7; the waiter
on CPU 6 was sampled in D state during pselect. In the next two attempts the
allowed masks (`0x3f` and `0x6f`) excluded CPU 7, so the helper selected CPU
5. Those runs observed respectively a D-state mprotect helper with an R-state
waiter, and no D-state transition for either thread. The COW transition and
overlay check passed in all three.

Added `RMG_DIAG_COW_REQUIRE_CPUS_67=1`, which is compile-time restricted to a
`RMG_DIAG_COW_ONLY=1` build. Before core pinning, KASLR-slide discovery, page
reclaim, or the vulnerable route, the startup check temporarily queries the
current thread's full process-allowed affinity mask, then restores and
verifies its inherited mask. It proceeds only if CPU 6, CPU 7, and at least
one separate observer CPU are all allowed. Any query/restore failure or
missing CPU logs a clear skip and returns before entering the exploit route;
the check does not try to override Android's cpuset restrictions.

Built with Android NDK r27b for `r9q-G990BXXSLIZH2`, API 30, with the
256-MiB passive COW-observer and durable-log flags plus the new affinity
guard. The artifact is
`build/passive-cow-observer-256m-20261126-cpu67-preflight/cve-2026-43499-app.so`
(165,112 bytes; SHA-256
`DB52AD4F2FAF4869A04FD6E7DEF17BA119905FC22AD9255BD677BD29EC515CDD`). It
compiled with only the existing unused-function warnings and was verified as
an AArch64 Android 30 shared object with the expected COW-only and preflight
strings. It was subsequently staged and run once; see the next section.

### 2026-11-26 CPU 6/7 affinity preflight run

The candidate was imported into RMG Next as
`rmg-cow-cpu67-preflight-20261126.so`. The SHA-256 of the build artifact,
`/sdcard/Download/rmg-cow-cpu67-preflight-20261126.so`, and RMG's active
`files/local-payload/local-exploit.so` matched:
`DB52AD4F2FAF4869A04FD6E7DEF17BA119905FC22AD9255BD677BD29EC515CDD`. RMG
selected support profile `r9q-G990BXXSLIZH2` over Shizuku, with permission
granted. The run record is
`files/install-history/0a2c2a93-9a82-44fe-907e-9e1ece0d2a86.json` and names
source commit `3095e6f2a0659570b5adc54a4cb6b121f96d2fa2`.

The one child attempt logged
`RMG_COW_CPU_PREFLIGHT inherited=000000000000003f allowed=000000000000003f`
with `allowed_errno=0`, `cpu6=0`, `cpu7=0`, and `observer_cpu=1`. Affinity
restoration and verification both succeeded (`restore=0/0 verify=0/0`,
`restored=000000000000003f`, `match=1`). Because the effective mask lacked
both required CPUs, the guard logged that it was skipping and returned status
42 before core pinning, KASLR discovery, page reclaim, or the vulnerable COW
route. RMG translated that diagnostic return to its generic payload code 255
at the “Running kernel exploit” stage. This is a successful preflight skip,
not a failed exploit attempt, and it provides no new COW timing result.

RMG verified and prepared its KernelSU service before starting the payload,
but the payload did not reach root or KernelSU loading. The phone remained
ADB-responsive and booted on the expected kernel; post-run
`sys.boot_completed=1`, uptime was advancing, battery saver was off, and ADB
still reported shell UID 2000. `/dev/ksu` and `/proc/ksu` were absent. The
pre-existing `ro.boot.bootreason=kernel_panic,bug` value persisted; there was
no evidence of a reboot during this attempt.

Run evidence is in
`build/passive-cow-observer-256m-20261126-cpu67-preflight/one-shot-run/`:

| File | SHA-256 |
| --- | --- |
| `rmg-result.json` | `5C0645D1DA7977C24D208BDB057C295083EFDA628D405949E9C68AA2140B588F` |
| `rmg-run-log.txt` | `7D3AFA1EE16C62B57458679FE606828CDBB958C41FCC625B08CD9ADA38E854FE` |
| `device-post-run.txt` | `8CC38EF258F13E5B6C8A9A42B155765079CA55F964207D8025D7D3685E4EEC82` |
| `payload-hashes.txt` | `98A0AB6AC593670E141BE02269DA82E94F332B3F55D05315FDA223167651141D` |

A read-only cpuset snapshot at 14:02 local time showed that ADB and the
foreground RMG process were allowed CPUs `0-7`; `shizuku_plus_server` (UID
2000) was also allowed `0-7`. The separate Shizuku manager app process was in
`/background` with allowed list `0-2`. Android's configured cpusets reported
`top-app=0-7`, `foreground=0-2,4-7`, `foreground_window=0-7`, and
`background=0-2`. Battery saver remained off. Thus the current system and
visible server/RMG process masks are wider than the payload's `0x3f` result;
the background Shizuku app is restricted, but that alone does not explain the
payload mask because its server process retained `0-7`. The exploit child had
already exited, so this snapshot cannot identify which launcher/parent state
limited that historical child. The cause remains unknown; do not assume the
phone-wide cpuset or battery-saver state produced `0x3f`.

Before another attempt, use the process-context candidate below. Keep the
skip guard enabled, and do not retry the vulnerable route until the actual
run process is allowed CPUs 6 and 7. The post-run snapshot is
`build/passive-cow-observer-256m-20261126-cpu67-preflight/one-shot-run/cpu-affinity-post-run.txt`
(SHA-256 `BA5A7ACA19E2468248D7D876E6619F1EA5379B47A93D7D4DD908B3B9F0DBA95F`).

### 2026-11-26 CPU preflight process-context logging candidate

Extended the COW-only CPU preflight to log its PID, PPID, TID, UID, and GID;
the payload's full `/proc/self/status` and `/proc/self/cgroup`; and selected
identity, security, and affinity fields plus `/proc/<ppid>/cgroup` for its
parent. These records are emitted before the temporary affinity query, so
they can be compared directly with the existing `RMG_COW_CPU_PREFLIGHT`
inherited/allowed/restored masks. Failures to read procfs are logged and do
not change the preflight decision. The guard still restores the inherited
affinity and skips before the vulnerable route if CPUs 6 and 7 are missing.

Built with Android NDK `27.1.12297006` (r27b), API 30, for
`r9q-G990BXXSLIZH2`, using the same 256-MiB passive COW-observer, durable-log,
and CPU 6/7 guard flags as the preceding candidate. The artifact is
`build/passive-cow-observer-256m-20261126-cpu67-proc-context/cve-2026-43499-app.so`
(167,960 bytes; SHA-256
`8C7563D60160F895FB9A93CD1D4D589EEBD014F26D773B8CDEC3E12198A410EE`). It
compiled with only the existing unused-function warnings and was verified as
an AArch64 Android 30 shared object. The unique log markers include
`RMG_COW_CPU_CONTEXT`, `RMG_COW_CPU_STATUS_BEGIN/END`, and
`RMG_COW_CPU_CGROUP_BEGIN/END`. This candidate has not been staged or run on
the handset. It was subsequently staged and run once; see the next section.

### 2026-11-26 CPU preflight process-context run

The process-context candidate was imported into RMG Next as
`rmg-cow-cpu67-proc-context-20261126.so`; the build artifact, Downloads copy,
and RMG's active local payload all had SHA-256
`8C7563D60160F895FB9A93CD1D4D589EEBD014F26D773B8CDEC3E12198A410EE`. RMG
used profile `r9q-G990BXXSLIZH2` over Shizuku, with permission granted. The
run record is
`files/install-history/6895549d-3c55-4632-bb80-09f5f0464c2b.json`; it ran from
14:23:45.474 to 14:23:48.532 local time.

The new process log identifies payload PID 31349 (UID/GID 2000), parent PID
31346, and parent-of-parent PID 12631. Both the payload and its direct parent
reported `Cpus_allowed=ff`, `Cpus_allowed_list=0-7`, and cgroup
`/system/uid_0/pid_12565`. The payload's `sched_getaffinity()` mask was
`0x5f` both before and after requesting all CPUs; restoration verified the
same mask. That mask includes CPU 6 but excludes CPU 7, so the guard skipped
with status 42 before the vulnerable COW route. RMG reported its generic code
255 at “Running kernel exploit”; CFI, physical read/write, root, and
KernelSU loading were not reached.

After the run, `taskset -p 12631` reported the Shizuku server's current mask
as `0x5f`, matching the payload's effective affinity. The recorded parent
chain ends at that server PID. The system reported CPUs `0-7` online, so CPU
7 was online during the post-run check. These observations make affinity
inherited from the Shizuku server the leading explanation for the payload's
mask; the server mask was sampled after the child exited, so inheritance at
the exact launch instant is not directly proven. The phone remained booted
and ADB-responsive with battery saver off; `/dev/ksu` and `/proc/ksu` remained
absent, and the existing bootreason `kernel_panic,bug` persisted.

Run evidence is in
`build/passive-cow-observer-256m-20261126-cpu67-proc-context/one-shot-run/`:

| File | SHA-256 |
| --- | --- |
| `rmg-result.json` | `895F02D6D8D893F807D2B8A684A84945E564A6E7D53C73EB223DDE4C598AB9E3` |
| `rmg-run-log.txt` | `C22763857F1E4338DFAF8231890ADBC099399DFD1DAADDDF934397E039078557` |
| `device-post-run.txt` | `6B3112413D7BDE4085C8E8692D66E874180721BE8BDFB6BE64FF049060D45FAA` |
| `payload-hashes.txt` | `DDBA405B6641AE35890974F3825342295C5B5AF78843D29B3E2A8DB055653044` |
| `cpu-affinity-post-run.txt` | `072FACA1A6DEAA35CACBE75014EE85FB6E6B3B6A4A068171E7A9AFF966143692` |

The follow-up below queried `sched_getaffinity()` for the direct parent while
the child was alive. The parent's mask matched the child's `0x6f` mask. The
child's parent was Shizuku PID 12631, but this does not establish where the
supervisor's CPU mask originated. Keep the hard stop enabled while resolving
why CPU 7 is missing.

### 2026-11-26 parent-affinity hard-stop preflight

Extended the guarded process-context diagnostic to call
`sched_getaffinity(parent_pid, ...)` while the payload child is still running.
Added `RMG_DIAG_COW_PARENT_AFFINITY_ONLY=1`, which returns immediately after
the preflight even if CPUs 6 and 7 are available. This candidate cannot enter
the COW, KASLR, CFI, physical read/write, root, or KernelSU stages. It retains
the CPU 6/7 guard and the parent's mask query.

Built with Android NDK `27.1.12297006` (r27b), API 30, for
`r9q-G990BXXSLIZH2`, with COW-only, trace, passive observer (256 MiB), durable
logging, CPU 6/7 guard, and parent-affinity-only flags. The artifact is
`build/passive-cow-observer-256m-20261126-parent-affinity/cve-2026-43499-app.so`
(166,016 bytes; SHA-256
`CC7FA5D318EFB85CA01EDFA3B44DBB349D518510A1CA6D24FEE59159E04F5C6E`). The
compiler emitted only the existing unused-function warnings.

RMG Next imported it as `rmg-cow-parent-affinity-only-20261126.so`. The build
artifact, Downloads copy, and app-private active payload had the same SHA-256.
RMG used profile `r9q-G990BXXSLIZH2` over Shizuku. Run record
`cb0b4ee8-1e89-4b52-9882-1bef1b31424d` started at 14:47:38.992 and completed
at 14:47:42.092 local time on 2026-11-26.

The payload was PID 32615 and its direct parent was the `sh` supervisor PID
32612 (PPID 12631, the Shizuku server). The in-child parent query succeeded
with `sched_getaffinity(parent_pid)` mask `0x6f`. The payload's inherited mask
and its effective mask after requesting all CPUs were also `0x6f`; restoration
matched. CPU 6 was present and CPU 7 absent. The RMG preflight returned status
42 before the exploit route; RMG's generic code 255 is the expected wrapper
result for that guarded exit. No exploit route, COW operation, root, or
KernelSU load was reached.

Both `/proc/<pid>/status` records reported `Cpus_allowed=ff` and
`Cpus_allowed_list=0-7`; the process cgroup path was
`/system/uid_0/pid_12565`. Read-only checks found CPUs `0-7` online, the root
and top-app cpuset masks `0-7`, and foreground cpuset `0-2,4-7`. Thus the
available CPU and cpuset controls include CPU 7, while the actual task
affinity does not.

After the run, Shizuku PID 12631 and all of its observed threads reported
`0x5f`. A `taskset -ap ff 12631` request reported a new mask of `0x5f` for
each thread, and a second snapshot remained `0x5f`; this shell-level request
did not widen the service affinity. The supervisor's in-run `0x6f` differs
from the server's post-run `0x5f` in CPUs 4 and 5, but both exclude CPU 7.
The post-run server snapshot cannot establish its exact mask when the
supervisor was created. The remaining source of the task-level CPU 7
restriction is unresolved; the evidence does not support blaming offline
CPUs, battery saver, or the exposed cpuset masks.

The phone remained booted and ADB-responsive on the expected firmware and
kernel, with battery saver off. `/dev/ksu` and `/proc/ksu` remained absent.
The attempted affinity widening left Shizuku at its original `0x5f` mask.

Evidence is in
`build/passive-cow-observer-256m-20261126-parent-affinity/one-shot-run/`:

| File | SHA-256 |
| --- | --- |
| `rmg-result.json` | `9EDDFDD7725DAFCA85A331BB9F6D93FB4E7B4C20A1673A3340DD269A8D1959E3` |
| `rmg-run-log.txt` | `B41BB1DA445DA99279FBE986ADB994AC736E36943DE35EA842B8EBECBE4F32DB` |
| `device-post-run.txt` | `1EDB3F00F0D3E4B7ADFDEE7BDFD4041D0DDF0B91E1B06BC08DBC04B1DB3D1767` |
| `payload-hashes.txt` | `96A9DC7343B197A3913A54D078016F450721DBAE9C5052F8B909FD2B5B8D41CA` |
| `cpu-affinity-post-run.txt` | `EE97ABC1285D25ACDED038869B29425C4CDE257206C67B78331AC0D2C6850C9C` |
| `cpu-set-controls.txt` | `7B0C2D4B41D2150432A8A9FCB0EE1EFC26C8D9772A3288CF001CB385DA07553B` |
| `shizuku-affinity-widen-attempt.txt` | `8CFFD32AA283652BE8F7771DBC09E29D30E7924F417B84805D0D479A4F55E787` |
| `shizuku-thread-affinity-before-temp-widen.txt` | `702CC31C7F208C63DDECA0FF0C14C1051112EA81DFFD89DAADE4D0D9289C7B24` |
| `shizuku-thread-affinity-after-attempt.txt` | `702CC31C7F208C63DDECA0FF0C14C1051112EA81DFFD89DAADE4D0D9289C7B24` |

The initial source review found a CPU 5 fallback that a prior COW-only run
had exercised. It did not account for the caller's affinity being narrowed
before its worker threads were created. The 512-MiB run and source follow-up
below resolve that discrepancy.

### 2026-11-26 core-affinity fallback source review

The target sets `CORE=6` for the waiter, and `common.h` sets
`CONSUMER_CORE=(CORE + 1)`, so the consumer initially requests CPU 7. The
shared `pin_to_core_ex()` helper does not abort when that request fails: it
calls `sched_pin_highest_allowed(avoid_core)`, which scans the calling
thread's current affinity mask downward and skips the waiter's core. If that
mask is `0x6f`, the waiter can pin to CPU 6 and the consumer's CPU 7 request
can fall back to CPU 5. The passive observer's helper selection would also
predict CPU 5, leaving CPU 0 as a separate observer CPU. This is conditional
on CPU 5 being in the calling thread's mask; `pin_to_core_ex()` does not widen
affinity by itself.

The `deferred-both-wchan` COW-only run already exercised that fallback: it
logged the failed request for CPU 7 and fallback to CPU 5, then reported the
waiter on CPU 6 and helper on CPU 5. The COW page transition and overlay check
passed. That run did not observe either target thread blocked. Across the
three passive-observer runs, the COW transition and overlay check passed, but
waiter/helper state varied; a CPU 7 preflight would discard a supported
fallback path. The new `RMG_DIAG_COW_REQUIRE_CPUS_67` guard is therefore a
conservative preflight for the specific 6/7 arrangement, not a general
requirement of `pin_to_core_ex()`.

`RMG_DIAG_COW_ONLY=1` still performs the timing-sensitive pselect/COW work,
then forces `route_signal=0` and stops before CFI, physical read/write, root,
and KernelSU. It is useful for readiness measurements but does not establish
why `/dev/ksu` is absent; the underlying kernel race can still freeze or
reboot the phone.

The proposed 512-MiB COW-only passive-observer run is documented below. Its
per-thread masks differed from the earlier `0x6f` parent mask, so follow-up
work should resolve affinity inheritance and actual CPU placement rather than
assuming that the consumer can use the source-level CPU 5 fallback.

### 2026-11-26 512-MiB passive observer fallback run

Added a diagnostic-only compile-time cap so every `RMG_DIAG_COW_ONLY` build
sets `max_attempts=1`, regardless of the launcher environment. This prevents
the timing-sensitive route from retrying if RMG changes its `EXPLOIT_ATTEMPTS`
setting. The COW-only route also compiles to one readiness attempt when
`RMG_DIAG_COW_TRACE` is enabled and forces `route_signal=0` before CFI,
physical read/write, root, or KernelSU.

Built the `r9q-G990BXXSLIZH2` app payload with Android NDK r27c/API 30 and
`RMG_DIAG_COW_ONLY=1`, `RMG_DIAG_COW_TRACE=1`,
`RMG_DIAG_PASSIVE_COW_OBSERVER=1`, `RMG_DIAG_DURABLE_LOG=1`, and
`RMG_DIAG_COW_PTE_MB=512`. The AArch64 shared object is
`build/passive-cow-observer-512m-20261126-cpu-fallback/cve-2026-43499-app.so`
(163,112 bytes; SHA-256
`8406480744ED809FE3308CD565862B81988E2CE423310214D09B4A9938889181`). It
compiled with only the existing unused-function warnings. The staged
Downloads file had the same SHA-256 and was selected in RMG as
`rmg-cow-passive-observer-512m-fallback-once-20261126.so`; RMG's live log
confirmed that exact local-payload filename, firmware profile
`r9q-G990BXXSLIZH2`, `pte_mb=512`, `diagnostic=cow-only`, and
`cfi_write_pipe_physrw_root=disabled`. The KernelSU service was downloaded by
RMG as usual, but the payload did not reach the root or service-load stages.

The one supervisor attempt completed the controlled collector/reclaim setup:
34 target objects were collected, 251/256 socket-buffer messages were sent,
and the observed `mm_struct` slab count changed from 38 to 37. Only three
`io_uring` spectrum sizes (512, 1,024, and 2,048 entries) were created; larger
sizes failed with `ENOMEM`. Kernel-page preparation took 127,742 ms.

The route-specific CPU evidence does not match the earlier parent-mask-only
prediction. The consumer's request for CPU 7 failed with `EINVAL`, and
`pin_to_core_ex()` reported that it continued unpinned. The passive monitor
and mmap-locker each inherited mask `0x40` (CPU 6); when widened, their
effective allowed mask was `0x5f` (CPUs 0-4 and 6). The locker selected CPU 4,
and the observer pinned to CPU 0; the observer predicted CPU 4 for the helper.
Thus CPU 5 was not available to those widened threads, and the consumer's own
fallback did not select an alternate CPU. The source-level audit below
reconciles this with the thread-creation order.

The passive observer made nine samples. It recorded the waiter as `R` on CPU
0 in one sample while the gate and pselect-active flags were set; it recorded
no waiter or helper `D` state and `blocked_samples=0`. The mmap locker ran two
iterations for 1,225 us, while pselect returned 115 in 623 us. The COW page
transition was reproduced: pagemap changed from `a100000000000000` (present,
file-backed) to `8100000000000000` (present, anonymous), and smaps anonymous
memory increased from 0 to 4 KiB. Soft-dirty remained unset and did not
provide a useful signal. The overlay check passed (`overlay=1`), but the COW
walk reported zero calls/successes and its canary coverage was incomplete
(16,420,480 of 16,777,216 bytes scanned). The build's forced route signal
remained zero, so no CFI, physical read/write, root, or KernelSU stage ran.

RMG reported its generic code 255 at “Running kernel exploit”, as expected for
this guarded diagnostic; the payload exited after its one attempt. The phone
remained booted and ADB-responsive on Android 16 /
`G990BXXSLIZH2` / kernel `5.4.289-qgki-32192773-abG990BXXSLIZH2`, with battery
saver off. `/dev/ksu` and `/proc/ksu` remained absent. This run reproduced
COW/overlay behavior but did not resolve why the observer missed a blocked
thread or establish that KernelSU can load.

The source-level audit resolves the fallback discrepancy. In `main.c`,
`run_exploit()` pins its caller to `CORE` (CPU 6) before the normal route.
`run_main_route_threads()` then creates the waiter, owner, and consumer from
that caller. The waiter requests CPU 6; the consumer requests CPU 7 and asks
`sched_pin_highest_allowed()` for a fallback that avoids CPU 6. Since
`pthread_create()` gives these threads the caller's current CPU-6-only mask,
there is no other allowed CPU for that fallback to select. The helper leaves
the inherited mask in place; its message “continuing unpinned” means no new
affinity was installed, not that the thread became free to run on every CPU.

The readiness monitor and mmap locker are different: their diagnostic setup
explicitly requests all CPUs, reads back the kernel-intersected mask `0x5f`,
then pins the observer to CPU 0 and the locker to CPU 4. That explains their
`inherited=0x40` and `allowed=0x5f` logs. The earlier `0x6f` parent snapshot
was not the affinity inherited by threads after `run_exploit()` pinned its
caller. CPU 5 fallback was observed in the earlier deferred-wchan run, but it
does not describe the current route's thread-creation state.

The follow-up candidate that logs the consumer's inherited and effective
masks and widens only that thread is recorded below. It is diagnostic-only,
one attempt, and has not been staged or run. The normal route's affinity
policy remains unchanged pending comparison with this baseline.

Run evidence is in
`build/passive-cow-observer-512m-20261126-cpu-fallback/one-shot-run/`:

| File | SHA-256 |
| --- | --- |
| `rmg-run-log.txt` | `5A46D0610CAF8B08C8F3ED070054671CDA8535FF78415A52D24E507064E9E4EF` |
| `rmg-pipe-oracle-checkpoint.log` | `A82D23D2D3E544134B37061DB71AA37067EB25D3775511EA06DF6CBB07252C53` |
| `rmg-run-result.png` | `A07021B32F85E89A4927670FAB9F30561D1688B5F2235AB3A7992AF5A1F2E504` |
| `device-post-run.txt` | `3016E501CC8BB0FB67C244A10CD9B5A378C278BC919CAC7B29BD6E988BBEB175` |
| `payload-hashes.txt` | `1FFE9285F827A8E4A150DF59D1D54082BA595A639ED3A4112372585C386385B5` |

### 2026-11-26 consumer-affinity diagnostic built and run (COW-only)

Added `RMG_DIAG_COW_CONSUMER_AFFINITY`, a compile-time-only diagnostic that is
rejected unless COW-only, COW trace, and the passive observer are all enabled.
In `consumer_thread()`, it logs the inherited affinity mask and current CPU,
requests all CPUs so the kernel returns the process/cpuset-allowed mask, logs
that mask, calls the existing `pin_to_core_ex(CONSUMER_CORE, waiter_core)`,
then logs the selected core and effective mask. This tests the fallback after
affinity expansion without changing the normal route. The existing
`RMG_DIAG_COW_ONLY` compile-time cap keeps the supervisor to one attempt; the
trace keeps readiness to one route attempt and forces the route signal to zero
before CFI, physical read/write, root, or KernelSU.

Built for `r9q-G990BXXSLIZH2` with Android NDK r27c/API 30 and
`RMG_DIAG_COW_ONLY=1`, `RMG_DIAG_COW_TRACE=1`,
`RMG_DIAG_PASSIVE_COW_OBSERVER=1`, `RMG_DIAG_DURABLE_LOG=1`,
`RMG_DIAG_COW_PTE_MB=512`, and
`RMG_DIAG_COW_CONSUMER_AFFINITY=1`. The AArch64 shared object is
`build/passive-cow-observer-512m-consumer-affinity-20261126/cve-2026-43499-app.so`
(163,968 bytes; SHA-256
`2CB83D5121F004ADEF2193A2C77D2A0394BF231F8719A2ADE341A2C04F98492F`). ELF
inspection confirmed an AArch64 shared object, and string inspection found
the consumer-affinity markers plus the COW-only stop-before-CFI marker. The
compiler emitted only the existing unused-function warnings. This artifact
was staged after hash verification and run once; findings are recorded below.

#### One-shot run findings

The AArch64 artifact was copied to `/sdcard/Download/rmg-cow-consumer-affinity-512m-20261126.so`; its on-device SHA-256 matched the local artifact (`2CB83D5121F004ADEF2193A2C77D2A0394BF231F8719A2ADE341A2C04F98492F`). RMG Next selected the `r9q-G990BXXSLIZH2` profile and this local payload. The single diagnostic attempt completed without a device reboot. ADB still reported `sys.boot_completed=1`, battery saver was off, and uptime advanced from about 79,297.66 to 79,333.54 seconds.

RMG reported `Failed at: Running kernel exploit` and code 255. The payload's one attempt ended with status 1. This was not a root or KernelSU success: `/dev/ksu` and `/proc/ksu` were absent afterward. RMG verified/prepared its KernelSU service before the exploit, but the exploit did not gain root or load the driver. The payload recorded `root=0`, `read_ok=0`, and `write_ok=0`.

The affinity change worked as intended. The consumer inherited mask `0x1` (CPU 0); widening succeeded and exposed allowed mask `0x3f` (CPUs 0–5). Requested CPU 7 was unavailable (`EINVAL`), and the existing fallback selected CPU 5 with effective mask `0x20`, avoiding waiter CPU 0. The passive observer likewise widened from `0x1` to `0x3f`, selected CPU 1, and sampled on CPU 1.

The remaining trigger evidence does not line up: the pselect waiter was observed in state `R` while pselect was active (34 passive samples; first and last state both `R`), while the separate mprotect helper/locker monitor observed one `D`-state sample after 141 polls. The route-level waiter `blocked_samples` remained zero. The two mprotect calls returned successfully and took about 4,028 microseconds total. Thus this run shows the helper blocking, but not the waiter reaching the expected blocked COW waitpoint.

Pagemap changed from a present file-backed entry (`a100000000000000`) before pselect to a present anonymous entry (`8100000000000000`) afterward. The control mapping also had an anonymous entry (`8100000000000000`); the sampled entries reported no soft-dirty bit. The canary reported `changed_chunks=0`, `task_pi=0`, and `mutated=0`. The canary read covered 16,420,480 of 16,777,216 requested bytes (`order3_scanned=501/512`, `trailing=3712`) and was explicitly marked incomplete. The anonymous transition alone is not proof that the intended COW reclaim/trigger succeeded.

The compile-time COW-only guard forced the route signal to zero before the CFI stage. No successful physical read/write or root transition was recorded. The run artifacts are in `build/passive-cow-observer-512m-consumer-affinity-20261126/one-shot-run/`: the RMG screenshot, UI hierarchy, extracted full log, durable checkpoint, and post-run device state. The source trace below explains the helper/waiter timing and the incomplete canary stream. The current send and timing behavior should be corrected before another phone attempt.

### 2026-11-26 source trace: waiter/helper timing and canary short write

#### The observer sampled two different tasks

`readiness_block_monitor_thread()` opens `/proc/self/task/<waiter_tid>/stat` for
`monitor.tid`; the COW-only setup separately opens the helper thread's stat
file from `cow_ctx.tid`. `COWPASSIVE` samples only `monitor.tid` (the waiter).
The `mprotect-helper` state comes from the separate `locker_fd` sample. The
captured 34 passive `R` samples therefore describe the pselect waiter, while
the one `D` sample describes the helper. The IDs and sampling paths are
separate by design; this is not a TID-label mix-up.

The mapping path also matches the intended COW test: `readiness_map_cow_fdset()`
creates a `MAP_PRIVATE` mapping of a memfd and prefaults its file-backed page.
The pselect call uses that mapping as `writefds`. In the kernel tree's
`fs/select.c`, `core_sys_select()` reads the input fd set, runs `do_select()`,
then writes the result with `set_fd_set()` / `__copy_to_user()`. That output
copy is where the private mapping can COW. The run returned 109 ready
descriptors, and the pagemap entry changed from file-backed to anonymous,
consistent with a completed COW copyout. It did not show the waiter sleeping
in that fault.

#### Timing shows the helper may have waited on the waiter

The one-shot trace uses the same monotonic microsecond clock for the pselect
and mprotect markers:

| Operation | Enter | Return / sample | Result |
| --- | ---: | ---: | --- |
| `mprotect(PROT_NONE)` call 1 | 79,297,639,169 | 79,297,641,271 | 2,102 us |
| pselect | 79,297,639,215 marker | 79,297,641,323 | 2,106 us; returned 109 |
| `mprotect(PROT_READ)` call 2 | 79,297,641,272 | 79,297,643,191 | 1,919 us |
| helper state sample | — | 79,297,641,308 | `D`, gate open, pselect active |

The first mprotect call ended 52 us before pselect returned. The second began
51 us before that return; the helper was sampled in `D` 15 us before pselect
returned, and its mprotect call finished 1,868 us later. In the 5.4 source,
each `mprotect()` takes `mmap_sem` for write inside `do_mprotect_pkey()` and
releases it on the syscall's exit path. Since the pselect output copy can take
the mapping's fault/read side, the most coherent explanation is that the first
mprotect lock ended just before copyout, pselect completed COW, and the second
mprotect then waited behind that operation. That would make the helper's `D`
sample the reverse wait, not proof that it was holding the lock against the
waiter.

This is a timing-based inference, not a proven lock stack: the post-join wchan
read returned `ENOENT`, and the run did not capture lock ownership. The source
and timestamps do establish that the helper's `D` sample cannot be treated as
a waiter `D` sample. The pselect waiter remained `R` in the sampled interval;
the expected blocked-waiter trigger was not observed.

#### The 501/512 canary is a short socket stream

For this target, `ORDER3_SIZE` is 32,768 bytes, `SKB_SEND_SIZE` is two order-3
chunks (65,536 bytes), and `SKB_RECLAIM_SENDS` is 256. The canary therefore
expects 16,777,216 bytes. The controlled-reclaim sender uses a nonblocking
`SOCK_STREAM` socket, requests an 8 MiB send buffer, and loops over
`sendmsg(..., MSG_DONTWAIT)`. It increments `sent` for every positive return
without checking that the return equals `SKB_SEND_SIZE` or sending the
remaining bytes. The capture reports `sends=251/256` but does not record each
return length.

The canary's 16,420,480-byte peek equals 250 full 65,536-byte requests plus a
36,480-byte remainder. In order-3 units, that is 501 full 32,768-byte chunks
plus 3,712 bytes, exactly matching `order3_scanned=501/512 trailing=3712`.
At least one positive `sendmsg()` was short and its remainder was dropped;
the evidence does not identify which send call was short. The missing 356,736
bytes are a producer-side short-write issue, not a boundary error in the
canary scan. The warning that rejects an incomplete stream is correct.

Before another run, add per-call byte-count/errno logging and reject any
short logical send. Retrying a remainder blindly could change skb allocation
and reclaim layout, so it should not be folded into the exploit path without a
separate source review.

#### Source-version limit

The downloaded SLIZH2 kernel package's instructions say to start from
`G990BXXUGIYJ1` source and apply the SLIZH2 update; its update archive does not
contain `fs/select.c` or `mm/mprotect.c`. I cross-checked the generic control
flow against the referenced kernel repository's `vanilla` branch:
[`fs/select.c`](https://github.com/Ryujin9610/android_kernel_samsung_sm8350/blob/vanilla/fs/select.c)
and
[`mm/mprotect.c`](https://github.com/Ryujin9610/android_kernel_samsung_sm8350/blob/vanilla/mm/mprotect.c).
That branch's Makefile identifies it as 5.4.302, while the phone runs 5.4.289;
this confirms the generic sequence but is not an exact build-source match.

The preceding source trace made no payload edits and no phone attempt.

### 2026-11-26 diagnostic send accounting and COW timing candidate

Added `RMG_DIAG_COW_SEND_TRACE`, which is rejected at compile time unless the
build also enables `RMG_DIAG_COW_ONLY` and `RMG_DIAG_COW_TRACE`. It requests a
16 MiB `SO_SNDBUF` for the controlled reclaim stream and logs the requested
and effective socket buffer values. Each attempted `sendmsg()` return and
errno is saved in memory, then logged after the loop so per-call output does
not perturb the send timing. A positive short send ends the loop; its
remainder is not retried. The summary counts only exact 65,536-byte sends and
marks the stream incomplete unless all 256 sends are full length. An
incomplete stream cannot independently qualify the target page. The COW-only
route may still continue when the separate io_uring spectrum capture reported
a target, allowing the timing observation to proceed without treating partial
socket data as a complete spray.

Added `RMG_DIAG_COW_MPROTECT_DELAY_USEC`, also compile-time guarded to require
the passive COW-only observer. At 250 microseconds, the helper waits after its
release signal before entering the traced `mprotect()` loop. The main thread
does not wait for the helper's start marker in this profile, so pselect begins
first and the helper can contend during the fd-set output copy. Other builds
keep their prior synchronization and mprotect behavior. The region remains
512 MiB; the prior 1 GiB run did not overlap pselect, so increasing its size
alone was not supported by the timing evidence.

Built the AArch64 Android 30 candidate with the Windows-hosted Android NDK
`27.1.12297006` (clang 18.0.2), using `RMG_DIAG_COW_ONLY=1`,
`RMG_DIAG_COW_TRACE=1`, `RMG_DIAG_PASSIVE_COW_OBSERVER=1`,
`RMG_DIAG_DURABLE_LOG=1`, `RMG_DIAG_COW_PTE_MB=512`,
`RMG_DIAG_COW_CONSUMER_AFFINITY=1`, `RMG_DIAG_COW_SEND_TRACE=1`, and
`RMG_DIAG_COW_MPROTECT_DELAY_USEC=250`. The binary is
`build/passive-cow-sendtrace-delay250-20261126/cve-2026-43499-app.so`
(ELF64/AArch64, 165,112 bytes; SHA-256
`ccff706021202ebf44db5820c36d85154fd19b6854206cd150ca7557d4a4f13a`). The
build succeeded with only the existing unused-function warnings. ELF and
embedded-string inspection confirmed Android 30/AArch64 output, the
COW-only stop-before-CFI guard, and the new send trace messages. Before the
one-shot run below, this candidate had not been staged on the phone.

#### One-shot result

The local payload was imported into RMG Next and its app-private SHA-256
matched the build (`ccff706021202ebf44db5820c36d85154fd19b6854206cd150ca7557d4a4f13a`).
RMG selected the exact SM-G990B / kernel 5.4.289 profile and logged
`Using local payload: rmg-cow-sendtrace-delay250-20261126.so`. The phone was
awake with battery saver off. The single attempt completed without a reboot;
uptime advanced from 23:31 to 23:32 and `sys.boot_completed` remained `1`.

The send-buffer change fixed the short stream for this run. The requested
16 MiB `SO_SNDBUF` was accepted and `getsockopt()` reported 33,554,432 bytes.
All 256 calls returned exactly 65,536 bytes with errno 0, totaling
16,777,216 bytes. The canary peek covered all 512 order-3 chunks with no
trailing bytes. It still found no changed payload chunks (`mutated=0`).

The 250 microsecond helper delay missed the pselect window. In the shared
monotonic trace, pselect began at 84,658,668,618 us and returned 62 us later
with 115 ready descriptors. The helper's first mprotect began at
84,658,668,817 us, 137 us after pselect returned. That call took 1,299 us;
the restore took 549 us, for 1,849 us total. The passive observer collected
zero samples, and both waiter and helper blocked-sample counts were zero.
Pagemap changed from file-backed to anonymous after pselect, consistent with
the fd-set COW copyout, but the trace shows no overlap or blocked waiter.

The attempt ended at the expected COW-only stop before CFI, physical read/write,
root, or KernelSU. RMG reported failure code 255; `root=0`, read/write checks
were false, and `/dev/ksu` and `/proc/ksu` were absent. The complete RMG log,
durable checkpoint, and result screenshot are in
`build/passive-cow-sendtrace-delay250-20261126/one-shot-run/`.

For the next candidate, use the original helper-start synchronization
(`RMG_DIAG_COW_MPROTECT_DELAY_USEC` omitted or set to 0), preserving the 50 us
main-thread headstart and the corrected send-buffer accounting. The 250 us
setting allowed pselect to finish before mprotect began; the original
synchronization previously placed mprotect about 46 us before pselect. The
synchronized candidate and its one-shot result are recorded below.

#### Synchronized helper-start one-shot

Built without `RMG_DIAG_COW_MPROTECT_DELAY_USEC`, leaving the helper-start wait
and 50 us main-thread headstart in place. The Android NDK
`27.1.12297006` build is
`build/passive-cow-sendtrace-synchronized-20261126/cve-2026-43499-app.so`
(ELF64/AArch64, 165,048 bytes; SHA-256
`d1e453904bbafb731d64059099d544f1fc8eb84ce602781c48ff954ef9cdf155`). It used
the same COW-only/passive-observer/send-trace flags as the 250 us candidate,
with the delay flag omitted. The app-private payload hash matched before the
run, and RMG logged this local payload by name for the exact SM-G990B / kernel
5.4.289 profile.

The phone stayed on the same boot with battery saver off. The requested 16 MiB
socket send buffer again reported 32 MiB effective. All 256 sends were full
length (65,536 bytes, errno 0), totaling 16,777,216 bytes; the canary covered
all 512 chunks and found no mutations.

This timing reproduced the earlier overlap. The first helper mprotect entered
at 85,398,940,416 us, 48 us before pselect's marker at 85,398,940,464 us. It
returned at 85,398,941,405 us; pselect returned at 85,398,941,444 us with
result 106, about 39 us later. The first mprotect lasted 989 us. The helper
immediately entered a second mprotect, which returned at 85,398,942,302 us
(897 us duration). Its separate monitor sampled the helper in state `D`
(`68`) at 85,398,941,436 us while pselect was active. The waiter's passive
observer captured 16 samples, all state `R` (`82`); the route's waiter
`blocked_samples` remained zero. The helper's post-join wchan was unavailable,
so the D sample confirms temporal overlap but does not prove which kernel wait
caused it.

Pagemap again changed from file-backed before pselect to anonymous afterward,
consistent with the COW output copy. Unlike the 250 us run, this run shows the
helper in `D` during pselect and a roughly 1 ms pselect duration, but it still
does not show the waiter entering `D` or trigger the route signal. The
COW-only guard stopped before CFI, physical read/write, root, or KernelSU;
RMG returned code 255, `root=0`, and `/dev/ksu` plus `/proc/ksu` were absent.
The RMG log, durable checkpoint, and result screenshot are in
`build/passive-cow-sendtrace-synchronized-20261126/one-shot-run/`.

### 2026-11-26 exact Samsung 5.4.289 source trace: mmap semaphore hypothesis

Inspected the complete Samsung source package supplied for this firmware. The
outer `SM-G990B.zip` contains `SM-G990B_16_Opensource.zip`; that archive
contains `Kernel.tar.gz`. The kernel tree's top-level Makefile declares
`VERSION = 5`, `PATCHLEVEL = 4`, and `SUBLEVEL = 289`. Samsung's README names
`vendor/r9q_eur_openx_defconfig` as the build target, and that defconfig has
both `CONFIG_RWSEM_SPIN_ON_OWNER=y` and
`CONFIG_SPECULATIVE_PAGE_FAULT=y`. The archive includes the exact paths
`fs/select.c`, `mm/mprotect.c`, `arch/arm64/mm/fault.c`, `mm/memory.c`, and
`kernel/locking/rwsem.c`. This matches the phone's reported 5.4.289 release
and r9q Europe build target. The package and defconfig are a strong source
match; the exact `.config` and build provenance of the installed kernel were
not independently recovered.

The earlier source-version note refers to the SLIZH2 update-only archive. This
newly inspected full source package contains the corresponding Samsung
5.4.289 files, so the earlier 5.4.302 upstream comparison is no longer the
best source for this analysis.

The matching source gives a plausible explanation for the synchronized run:

* `fs/select.c`'s `core_sys_select()` copies the readiness result back to the
  user `fd_set` through `set_fd_set()`, which calls `__copy_to_user()` (around
  lines 399-404 and 691-694). That write is where the private file mapping
  takes its COW fault.
* On arm64, `do_page_fault()` first tries the speculative page-fault path. If
  it asks for a retry, the normal path tries `down_read_trylock()` and then
  calls `down_read(&mm->mmap_sem)` (around lines 532-550 of
  `arch/arm64/mm/fault.c`). For a private writable file VMA without an
  `anon_vma`, `__handle_speculative_fault()` explicitly returns
  `VM_FAULT_RETRY` (around lines 4460-4465 of `mm/memory.c`). The payload
  describes this as the mapping's first COW; the run's file-backed-to-anonymous
  pagemap transition is consistent with that, although the VMA's `anon_vma`
  state was not captured.
* `mprotect()` takes the same `mm->mmap_sem` for writing with
  `down_write_killable()` and releases it with `up_write()` (`mm/mprotect.c`,
  around lines 518-519 and 607-609). The read-side rwsem slow path tries
  `rwsem_optimistic_spin(sem, false)` before queuing and setting the task's
  sleep state (`kernel/locking/rwsem.c`, around lines 1015-1037 and
  1097-1114). With `CONFIG_RWSEM_SPIN_ON_OWNER=y`, a reader can remain
  runnable while spinning on a running writer; if it instead queues, the
  `down_read()` path uses `TASK_UNINTERRUPTIBLE`.

The run's ordering fits that source path: the first mprotect began 48 us
before pselect and returned 39 us before pselect returned. The waiter was
sampled as `R` throughout the observed pselect window, while the helper's
second mprotect was sampled `D` 31 us after entering and 8 us before pselect
returned. A plausible sequence is that the first mprotect held the write
semaphore while pselect's COW fault spun in the read-side slow path; after the
first mprotect released it, pselect acquired the read side and completed COW,
while the helper's second mprotect waited for write access. This is consistent
with the waiter's `R` samples and the helper's `D` sample; the monitors sampled
different TIDs.

This remains a hypothesis, not a captured kernel stack or lock-owner record.
The exact fault may take the speculative path in some conditions, and the
current logs do not show whether this VMA had an `anon_vma`. The helper's
`wchan` read is currently deferred until after the monitor and helper finish;
by then the helper TID is gone, which explains `wchan_errno=2` and yields no
wait-channel evidence. A useful next diagnostic is to read and retain the
helper's `wchan` at the first live `D` sample, then print it after the run.
That read may perturb timing and may return no symbol, so keep the
COW-only/stop-before-CFI guard and treat it as a diagnostic observation. No
new phone attempt was made during this source review.

### 2026-11-26 live mprotect-helper wchan COW-only run

Built `build/passive-cow-sendtrace-live-wchan-20261126/cve-2026-43499-app.so`
with the existing COW-only, passive-observer, durable-log, 512 MiB COW PTE,
consumer-affinity, and send-trace flags. The helper's `wchan` is now read and
retained at its first sampled `D` state; logging is deferred until after the
monitor. The build is ELF64/AArch64, 164,960 bytes, SHA-256
`18AE4B469C28D01032540ED630DC04A5BE629B672005EC9FC54D73D7B95A153B`.
The RMG app selected this local payload for `r9q-G990BXXSLIZH2`; the staged
and app-private copies had the same hash. The app also printed generic run
parameters `attempts=24 p0_cache=on shell=optional`, while the payload's own
runtime configuration reported one attempt and ended at `attempt=1/1`.

The phone remained on the same boot, awake, with battery saver off. The single
attempt returned RMG error 255 at “Running kernel exploit.” It did not gain
temporary root: the payload ended with `root=0`, and read-only ADB checks found
neither `/dev/ksu` nor `/proc/ksu`.

The COW trace gives stronger timing evidence for the mmap-lock hypothesis.
The active monitor sampled the pselect waiter (`tid=23003`) in state `D` at
polls 123 and 124 (`89359699185` and `89359699201` us) while the trigger gate
and pselect were active; it became `R` by poll 138 (`89359699417` us). Its
summary counted 15 `D` samples between first/last sampled states `R`. The
separate passive observer sampled the waiter 34 times and saw only `R`, so its
sampling interval missed the brief blocked window. Pagemap changed from
file-backed before pselect (`file=1`) to anonymous afterward (`file=0`),
consistent with the readiness copy causing COW. The first helper mprotect
spanned `89359697383`–`89359699420` us (2.037 ms); its second started at
`89359699420` us and lasted 2.035 ms. The mprotect helper (`tid=23007`) was
sampled once in state `D` at `89359699458` us, 38 us into the second call,
while pselect was still marked active.

This ordering fits a writer-to-reader-to-writer handoff on `mm->mmap_sem`:
the pselect waiter enters `D` while the first mprotect holds the write side,
returns to `R` as that syscall releases it, then the helper's second mprotect
waits after the COW fault has resumed. It strengthens the semaphore hypothesis
but does not prove the wait site, since neither live `wchan` yielded a symbol
and no kernel stack or lock-owner record was available.

The new live helper-wchan capture succeeded as a proc-file read (`errno=0`),
but its content was the literal `0`, not a wait-site symbol. The waiter's
separate wchan read remains post-monitor; it also returned `0` after
`pselect_active` had cleared. Thus this run does not identify the wait channel
or prove that the sampled `D` state was specifically an mmap-semaphore wait.

The COW route then reported `route pi sanitize lock=-1 errno=110`,
`unlock=-1 errno=1`, `readiness-cow walk ... calls=0`, and no canary changes.
The pipe physical-RW summary remained `read_ok=0 write_ok=0 rw64=0/0`.
The COW-only guard stopped before CFI, physical read/write, root, or KernelSU.
No flash or reboot occurred. The captured RMG UI log is saved at
`build/passive-cow-sendtrace-live-wchan-20261126/one-shot-run/rmg-ui-log.txt`.

For a follow-up, the missing observation is the waiter's `wchan` at its first
live `D` sample; its current read happens only after pselect exits. If another
attempt is authorized, add a single immediate waiter-wchan read beside the
existing helper capture, record the sampled state before and after that read,
and keep the same one-attempt COW-only stop-before-CFI guard. A returned `0`
would still be inconclusive; a kernel wait symbol would help distinguish the
page-fault wait from mmap-semaphore contention.

### 2026-11-26 waiter live-wchan diagnostic run

Extended the COW-only waiter monitor to read its `wchan` immediately after the
first sampled `D` state, then reread `/proc/self/task/<tid>/stat`. The saved
record includes the state before and after the wchan read, its elapsed time,
and pselect-active state. The post-monitor path now only prints the retained
sample, so it cannot overwrite the live result. The existing helper live-wchan
capture and stop-before-CFI guard remain enabled.

Built with Android NDK `27.1.12297006` / clang 18.0.2 using the COW-only,
COW-trace, passive-observer, durable-log, 512 MiB COW PTE,
consumer-affinity, and send-trace flags; no mprotect-delay flag was set. The
ELF64/AArch64 shared object is
`build/passive-cow-sendtrace-waiter-live-wchan-20261126/cve-2026-43499-app.so`
(165,384 bytes; SHA-256
`A21D3604BE4A037F5924F09E0751039600A8ED184949355A2E16A60FDF330B6E`). The
build emitted only the existing unused-function warnings. RMG selected this
local payload for the exact `r9q-G990BXXSLIZH2` profile and ran the payload's
single internal attempt.

The new waiter capture path did not execute: the waiter monitor and the
mprotect-helper monitor each made 46 polls and saw no `D` sample; the passive
observer recorded 10 waiter samples, all `R`. The COW state trace contained
only the initial and pselect-active runnable states. Therefore this run says
nothing about what the waiter's live `wchan` would return; no waiter-wchan
record was produced.

Pagemap still changed from file-backed before pselect to anonymous afterward,
consistent with COW. Pselect returned 106 and took 713 us. The first mprotect
call took 718 us (`91041284706`–`91041285424` us); the second took 663 us
(`91041285425`–`91041286088` us). These calls overlap the pselect window, but
the monitors did not observe either thread in `D`, unlike the prior run. This
shows the timing is variable and the suspected blocked-wait path was not
reproduced on this attempt.

The route again reported `route pi sanitize lock=-1 errno=110`,
`unlock=-1 errno=1`, and `readiness-cow walk ... calls=0`. The canary had no
changed chunks; physical read/write remained unavailable and the payload
ended with `root=0`. RMG returned code 255 at “Running kernel exploit.” The
phone stayed booted and ADB-responsive; `/dev/ksu` and `/proc/ksu` were absent.
No flash or reboot occurred. The full RMG UI log and compiler output are in
`build/passive-cow-sendtrace-waiter-live-wchan-20261126/one-shot-run/` and
`build/passive-cow-sendtrace-waiter-live-wchan-20261126/build.log`.

The waiter-wchan instrumentation is built and ready, but another attempt
needs to reproduce a live waiter `D` sample before it can provide the missing
wait-site evidence. This run did not explain the PI-lock timeout.

### 2026-11-26 waiter live-wchan retries

Repeated the same local COW-only candidate after RMG's recommended one-minute
cooldown. The first retry (`retries/attempt-01/rmg-ui-log.txt`) again had no
waiter or helper `D` sample: both monitors saw only `R`, and pselect returned
103 in 528 us. On the second retry
(`retries/attempt-02/rmg-ui-log.txt`), the new live waiter capture fired.
No third attempt was made after obtaining the target sample.

For the second retry, the waiter (`tid=21776`) was sampled `D` at polls 19 and
20 while pselect was active. The live capture reported
`state_before_wchan=68 state_after_wchan=68`, `pselect_active_state=1/1`,
`pselect_active_after_wchan=1`, and `wchan_errno=0 wchan=do_page_fault`.
The procfs read and immediate state recheck took 103 us. The following waiter
sample, about 121 us after the first `D` sample, was still `D`; the waiter was
back to `R` by poll 42. Thus the symbol was read while the waiter remained in
`D`, rather than after it had already returned to `R`.

This is the first live wait-site clue: the waiter was inside the kernel's
`do_page_fault` path during the pselect output-copy/COW window. Pagemap again
changed from file-backed before pselect to anonymous afterward. This supports
the page-fault/COW explanation and the mmap-semaphore contention hypothesis,
but `do_page_fault` does not identify the deeper wait function or prove which
lock caused the sleep.

The timing also matches the proposed write/read/write handoff. The first
mprotect ran from `92296457771` to `92296458530` us (759 us). The waiter was
sampled `D` during that call, then `R` at `92296458533` us as the call
returned. The helper's second mprotect began at `92296458530` us; its helper
monitor sampled it `D` at `92296458559` us. The helper's live `wchan` still
returned `0`, but the waiter's live `do_page_fault` symbol is now evidence
that the target thread was blocked in the fault path.

The route reported the sanitizer lock probe as
`lock=-1 errno=110`, `unlock=-1 errno=1`; this is expected in the current
design because `sanitize_holder_thread()` intentionally retains
`f_pi_sanitize` until the route exits. It does not establish the cause of the
exploit failure. The attempt performed zero readiness-COW walks and ended with
`root=0`, `read_ok=0`, and `write_ok=0`. RMG returned code 255. The phone
remained booted and responsive, and `/dev/ksu` and `/proc/ksu` remained absent.
No flash or reboot occurred.

### 2026-11-26 guarded PI-trigger probe

Built a fresh one-attempt diagnostic with NDK `27.1.12297006` for the exact
`r9q-G990BXXSLIZH2` target. Compile-time options were
`RMG_DIAG_PI_PROBE_ONLY=1`, `RMG_DIAG_UCLAMP_TRIGGER_ONLY=1`,
`RMG_DIAG_FOPS_READBACK_ONLY=1`, `RMG_DIAG_DURABLE_LOG=1`,
`RMG_DIAG_COW_TRACE=1`, and `RMG_DIAG_COW_PTE_MB=512`. The binary is
`build/pi-trigger-do-page-fault-20261126/cve-2026-43499-app.so` (149,280 bytes;
SHA-256 `D26D3585D577EB7CC392CB85A1EDA74DDA27A3E9F2853D0769ED45BAAB6C9AF5`).
The active app-private payload hash was checked after import and matched. The
build forces the route signal off unconditionally; even a successful gated
uclamp trigger can only permit read-only fops evidence collection. CFI, pipe
physical read/write, and Android root installation are compile-time disabled.

RMG `0.13+local.af081c2` selected the exact model/kernel profile and ran one
exploit attempt. The readiness pselect returned `112` in 656 us. The sampled
pagemap changed from file-backed before pselect (`file=1`) to anonymous after
it (`file=0`), consistent with the COW path. This run did not reproduce the
waiter's blocked state: the monitor made 4,753 polls, reported
`blocked_samples=0`, and recorded first and last state `82` (`'R'`). The
readiness scheduler summary was `0/0`; the gated uclamp setter was not called.
The earlier `sched-nochange-setattr-return=0` entry is the separate baseline
no-change call, not the gated trigger. The partial canary scan covered 501 of
512 order-3 pages and found zero changed chunks; it was incomplete and is not
a full negative scan. The trace then recorded `uclamp-trigger-canary=0`,
`uclamp-trigger-fops-skipped=0`, `uclamp-trigger-fops-gate=0`, and
`uclamp-trigger-stop-before-cfi=0`. No CFI, physical read/write, or root stage
was entered.

RMG returned code 255 with its generic “refused or unavailable privilege”
message, and KernelSU did not load. The sanitizer again printed
`lock=-1 errno=110`, `unlock=-1 errno=1`; as above, that is the expected
held-lock probe and is not evidence that it caused this run's failure. The
phone remained booted and ADB-responsive (`sys.boot_completed=1`); battery was
100% at 18.9 C. `/dev/ksu` and `/proc/ksu` were absent. No reboot or flash
occurred.

The full UI text, result XML and screenshot, durable checkpoint trace, and
post-run device state are saved under
`build/pi-trigger-do-page-fault-20261126/one-shot-run/`. The trace shows the
immediate blocker: this attempt never observed the readiness waiter in `D`,
so it never reached the uclamp trigger. The earlier COW-only attempt did
observe `do_page_fault` while the waiter was in `D`; the event is intermittent
under the current timing.

### 2026-11-26 guarded PI-trigger retry

After about 18 minutes of cooldown, used RMG's recommended wait-then-retry
option and ran the same candidate once more. The app-private payload hash
still matched the candidate. RMG again selected the exact profile and ran
one payload attempt. Pselect returned `103` in 751 us. The monitor made 4,718
polls, reported `blocked_samples=0`, and recorded first and last state `82`
(`'R'`); the scheduler summary was `0/0`. The gated uclamp setter still was not
called. The separate baseline `sched-nochange-setattr-return=0` call returned
success before the readiness route. Pagemap again changed from file-backed to
anonymous across pselect. The partial canary scan covered 501 of 512 order-3
pages and found zero changed chunks, but remained incomplete. The sanitizer
logged `lock=-1 errno=110`, `unlock=-1 errno=1`; source review shows this is the
expected held-lock probe, not an established failure cause.

RMG again returned code 255, with `root=0`, `read_ok=0`, and `write_ok=0`.
The phone remained booted and responsive (`sys.boot_completed=1`), battery
100% at 18.8 C; `/dev/ksu` and `/proc/ksu` were absent. No reboot or flash
occurred. Artifacts are in
`build/pi-trigger-do-page-fault-20261126/retries/attempt-01/`.

### 2026-11-26 passive observer enabled for guarded PI probe

The two PI-trigger runs both missed the waiter's `D` state, while the earlier
COW-only observer recorded `do_page_fault` during pselect. The existing
passive-observer implementation was compile-time restricted to COW-only mode,
which skips `sched_setattr`; simply enabling that mode would have prevented
the PI trigger from running. Generalized its state-trace storage and affinity
setup so the passive observer can also be built with the guarded PI/uclamp
profile. The monitor still pins only itself to a separate allowed CPU, records
the affinity and waiter state transitions, and retains the original trigger
conditions: it calls the uclamp setter only for a `D` sample after the gate,
after a post-gate runnable sample, and while pselect is active. An affinity
setup failure stops the route. The COW-only profile still skips the scheduler
setter.

Built the next candidate with NDK `27.1.12297006`, API 30, and
`RMG_DIAG_PI_PROBE_ONLY=1`, `RMG_DIAG_UCLAMP_TRIGGER_ONLY=1`,
`RMG_DIAG_FOPS_READBACK_ONLY=1`, `RMG_DIAG_PASSIVE_COW_OBSERVER=1`,
`RMG_DIAG_DURABLE_LOG=1`, `RMG_DIAG_COW_TRACE=1`, and
`RMG_DIAG_COW_PTE_MB=512`. The artifact is
`build/pi-passive-uclamp-trigger-20261126/cve-2026-43499-app.so` (152,504
bytes; SHA-256 `D140D8C29E34869D0A1127DC1D4128E407688A90AF72BC8A0A4452701D50210B`).
It compiled with only the existing unused-function warnings and is an
ELF64/AArch64 shared object. Binary string inspection confirmed the
`COWPASSIVE` and `COWSTATE` telemetry and explicit uclamp-trigger,
stop-before-CFI, physical read/write, and root guards. It was pushed to
`/sdcard/Download/rmg-pi-passive-uclamp-trigger-20261126.so`; the device-side
hash matched. The candidate was subsequently selected in RMG and run once;
the app-private hash matched the binary. This
profile keeps `route_signal=0` unconditionally;
even a successful trigger can only enable read-only fops evidence collection.
CFI, physical read/write, and root remain disabled.

The following section records its affinity and state-transition findings. The
uclamp trigger and root-disabled guards remained unchanged.

### 2026-11-26 passive observer PI run with consumer affinity diagnostics

The next candidate added `RMG_DIAG_COW_CONSUMER_AFFINITY=1` to the guarded PI
observer profile. It built with NDK `27.1.12297006`, API 30, to
`build/pi-passive-consumer-uclamp-20261126/cve-2026-43499-app.so` (153,376
bytes; SHA-256
`9C4ABB48E1A5F6CB3AB8482D02A838B9E8BECE83F4ED3CA83047AAB445EF7910`). The
binary is an ELF64/AArch64 shared object and contains the consumer-affinity
telemetry and explicit stop-before-CFI/root guards. It was pushed to
`/sdcard/Download/rmg-pi-passive-consumer-uclamp-20261126.so`, imported in RMG,
and verified by matching the app-private `files/local-payload/local-exploit.so`
hash. RMG selected the exact SM-G990B / kernel 5.4.289 profile. Its generic
runner line advertises a 24-attempt limit, but the payload itself recorded
`attempts=1` and `exploit attempt=1/1`.

The controlled-mm scan completed after 90 groups and selected one full group;
page preparation took 136,892 ms. The affinity trace identifies the remaining
setup bug. The consumer began with inherited mask `0x1`; widening succeeded
and the cpuset-allowed mask was `0x6f`, which excludes requested CPU7. Because
the consumer thread started before `waiter_thread` published
`route_waiter_core`, it passed `avoid=0`. Pinning CPU7 failed with `EINVAL`,
and the fallback selected CPU6, the waiter's CPU. The attempt therefore did
not test a consumer on a separate CPU.

The passive monitor was correctly isolated on CPU0. Its state trace contained
two samples, both state `82` (`R`): one before the gate and one while pselect
was active. The monitor polled 29 times, `blocked_samples=0`, and the passive
observer sampled six times with first and last state `R`. Pselect returned
`109` in 518 us. Both traced mprotect calls returned 0, but the pagemap
evidence showed only file-backed-to-anonymous transition, with no soft-dirty
or changed-canary evidence. The scan covered 501/512 pages and was incomplete.
The gated uclamp setter was not called; the `sched-nochange-setattr-return=0`
record is only the separate baseline call. The expected held-lock sanitizer
probe again logged `lock=-1 errno=110`, `unlock=-1 errno=1`.

The payload stopped before CFI, physical read/write, and root. RMG reported
code 255; `root=0`, `read_ok=0`, and `write_ok=0`. The phone stayed booted and
ADB-responsive on the same kernel, with shell uid 2000 and no `/dev/ksu` or
`/proc/ksu`; battery was 100% at 19.2 C. No new reboot or flash occurred. Run
artifacts, including the complete UI log, XML, screenshot, checkpoint trace,
and post-run state, are in
`build/pi-passive-consumer-uclamp-20261126/one-shot-run/`.

The follow-up fixes the race by waiting for `waiter_ready` before reading
`route_waiter_core`. Since CPU7 is not in the current cpuset but CPU5 is, the
existing highest-allowed fallback will then exclude CPU6 and select an allowed
separate CPU. Retain the one-attempt limit and root/CFI/physical-RW guards.

### 2026-11-26 PI helper state trace and gated uclamp run

Built the next exact-profile candidate with the prior passive observer,
consumer-affinity and CPU4 helper-selection diagnostics, plus
`RMG_DIAG_PI_HELPER_STATE_TRACE=1`. It reads the mprotect helper's
`/proc/self/task/<tid>/stat` alongside the waiter state trace. The candidate
was built with NDK `27.1.12297006` / clang 18.0.2 as
`build/pi-helper-state-trace-uclamp-20261126/cve-2026-43499-app.so`
(155,016 bytes; SHA-256
`83436D20D5D1FE11925BC13D6D37790D9BE9386CD2A3CC6E77707E1713C70576`).
Compile-time options also included `RMG_DIAG_PI_PROBE_ONLY=1`,
`RMG_DIAG_UCLAMP_TRIGGER_ONLY=1`,
`RMG_DIAG_FOPS_READBACK_ONLY=1`, `RMG_DIAG_PASSIVE_COW_OBSERVER=1`,
`RMG_DIAG_COW_CONSUMER_AFFINITY=1`,
`RMG_DIAG_PASSIVE_COW_HELPER_CORE=4`, `RMG_DIAG_DURABLE_LOG=1`,
`RMG_DIAG_COW_TRACE=1`, and `RMG_DIAG_COW_PTE_MB=512`. CFI,
physical read/write, and root remain disabled by the candidate's guards.

RMG `0.13+local.af081c2` selected the exact `r9q-G990BXXSLIZH2` profile and
ran one payload attempt. The actual affinity roles were separated: waiter CPU0
(the requested CPU6 was unavailable), consumer CPU5, passive monitor CPU1,
and mprotect helper CPU4. The monitor recorded the waiter in state `68` (`D`)
at poll 78 while the trigger gate and pselect were active. It sampled the
mprotect helper 78 times, with first and last state `82` (`R`) and zero `D`
samples. Thus this run reproduces the blocked waiter without observing the
helper blocked; the polling trace cannot exclude an unsampled brief helper
state transition.

The gated `sched_setattr` did run and returned success: the overlay summary
reports `blocked=1 sched=0/0`, and the route summary reports one call and one
successful call (`calls=1 success=1`). Pselect returned `118` in 2,858 us.
The pselect output page changed from file-backed to anonymous in pagemap, and
`smaps` reported anonymous memory increasing from 0 to 4 KiB. This is
consistent with the intended COW fault. The route's 22-us walk completed, but
the canary scan read only 16,420,480 of 16,777,216 requested bytes, covering
501/512 order-3 chunks (with 3,712 bytes left after the last full chunk). It
found zero changed chunks in the scanned prefix, but the scan is incomplete
and cannot establish that the missing tail was unchanged. The controlled
reclaim log also reports only 251/256 sends, which is consistent with the
short stream. No fops readback was attempted because the canary gate remained
false; the payload stopped before CFI, physical read/write, or root.

RMG returned code 255. The phone remained booted and ADB-responsive on the
same Android 16 fingerprint and kernel; battery was 100% at 19.5 C, and
`/dev/ksu` and `/proc/ksu` were absent. No reboot or flash occurred. The full
RMG UI log, checkpoint trace, screenshot, and post-run state are saved under
`build/pi-helper-state-trace-uclamp-20261126/one-shot-run/`.

The previous interpretation that the uclamp setter had not run was incorrect:
the successful `calls=1 success=1` record confirms that it ran once after the
waiter's `D` sample. The immediate measurement gap is now the incomplete
reclaim stream. The next diagnostic should enable the existing
`RMG_DIAG_COW_SEND_TRACE=1` path, which raises the socket send-buffer request
and records individual send results, then verify `256/256` sends and a full
512/512-chunk canary read before interpreting any negative scan. Keep it to
one attempt with the same pre-CFI, physical-RW-disabled, and root-disabled
guards.

### 2026-11-26 send-trace attempt blocked before payload entry; ashmem failure

The send-trace candidate was built as
`build/pi-helper-state-sendtrace-uclamp-20261126/cve-2026-43499-app.so`
(155,720 bytes; SHA-256
`9AAFD4A861384F11B9E551AA3E0A7021BBE593BD500C829079C510D17323B4B6`). It
added `RMG_DIAG_COW_SEND_TRACE=1` to the prior one-attempt PI/uclamp diagnostic
profile. CFI, physical read/write, and root remained disabled. The candidate
was imported into RMG, and its device/app-private payload hash matched the
local build before the run.

RMG logged `Run started` at 21:57:39.072. DropBox recorded a
`system_server_crash` at 21:57:39.102, only 30 ms later. The exception was
`SharedMemory_create failed: ENOTTY` while
`NotificationRankingUpdate.writeToParcel()` was sending a notification update
through `INotificationListener.onNotificationEnqueuedWithChannel()`. The app
log has no later transport or payload-stage record, and the candidate's
constructor resets its durable checkpoint at `preload.c` before recording
`constructor-entry`. No new checkpoint was captured. This is strong evidence
that the native payload did not enter, so this attempt produced no send trace
and is not an exploit result. Timing links the RMG start and framework crash,
but the stack does not identify which notification or listener caused it; do
not treat this correlation as proof that RMG or the candidate caused the
failure.

Afterward, DropBox recorded repeated zygote exits and cgroup process-file
errors. `vold` then failed under init's `reboot_on_failure` handling, and the
system rebooted with `rescueparty_by_bootchecker`. The captured evidence points
to a userspace/system-server recovery reboot; it does not show a kernel panic.
The phone recovered on the expected Android 16 fingerprint and kernel with
`sys.boot_completed=1`; `/dev/ksu` and `/proc/ksu` remained absent.

Read-only checks on the recovered boot report `sys.use_memfd=false` and boot
ID `29fb998d-b482-4364-a56e-f8acd670add1`. Both `/dev/ashmem` and
`/dev/ashmem29fb998d-b482-4364-a56e-f8acd670add1` exist as character device
10:119. Their SELinux labels are respectively `ashmem_device` and
`ashmem_libcutils_device`; `/sys/class/misc/ashmem` also exists. Although the
shell domain is denied reads of `/sys/class/misc/ashmem/dev` and `/proc/misc`,
`/sys/dev/char/10:119` resolves to `../../devices/virtual/misc/ashmem`,
confirming this device number is registered as ashmem. The first native probe
hard-coded `/dev/ashmem` and was denied with `EACCES` as shell and through
`run-as`; it never issued an ioctl.

The matching Samsung source package contains
`Kernel.tar.gz`; `arch/arm64/configs/vendor/r9q_eur_openx_defconfig` sets
`CONFIG_ASHMEM=y`. In `drivers/staging/android/ashmem.c`, `ashmem_ioctl()`
handles `ASHMEM_SET_NAME` and `ASHMEM_SET_SIZE` and initializes its return
value to `-ENOTTY` for unrecognized commands. This confirms that the published
source has the expected ashmem interface, but does not prove that the running
binary's handler or ioctl request is the same. Android 16 libcutils also sets
`ENOTTY` before issuing an ioctl if `fstat()` says the opened file is not a
character device or has a zero device number. Thus the framework exception
alone does not distinguish that validation failure from an ioctl rejected by
the ashmem handler. The observed node metadata makes the validation failure
less likely if libcutils opened this same boot-ID path, but does not prove
which binary/path system_server used. Android 16's configured
libcutils path uses the boot-ID ashmem node when memfd is disabled
([AOSP libcutils ashmem implementation](https://android.googlesource.com/platform/system/core/%2B/android16-release/libcutils/ashmem-dev.cpp));
the framework JNI propagates ashmem creation errors
([AOSP SharedMemory JNI](https://android.googlesource.com/platform/frameworks/base/%2B/refs/heads/main/core/jni/android_os_SharedMemory.cpp)).

The source package's UAPI header defines `ASHMEM_SET_NAME` as
`_IOW(0x77, 1, char[256])` and `ASHMEM_SET_SIZE` as
`_IOW(0x77, 3, size_t)`. The Android 16 libcutils source includes
`<linux/ashmem.h>` and issues those same two operations after opening and
validating the boot-ID path. This source-level comparison shows no obvious
request-number mismatch; it does not verify the system image's actual
libcutils binary or the live ashmem file operations.

Pulled the phone's read-only system libraries to
`build/ashmem-probe-20261126/`. The installed `libcutils.so` is AArch64 ELF64,
93,744 bytes, SHA-256
`6696C5ADA7E2F4656CFD601FB8374F0D928A3075BA73653B3991AF951C7F2E6A`, build ID
`a96da1adee6a28cfdd1d4bb1af5c7a35`. Its strings include `sys.use_memfd`,
`/proc/sys/kernel/random/boot_id`, and `/dev/ashmem`; `ashmem_create_region`
is an exported symbol. Disassembly shows the actual calls use ioctl requests
`0x41007701` and `0x40087703`, matching the kernel header's AArch64 encodings
for `ASHMEM_SET_NAME` and `ASHMEM_SET_SIZE`. `libandroid_runtime.so` also
imports `ashmem_create_region` and contains the `SharedMemory_create` JNI
error string. There is no evident libcutils/kernel-header request-number
mismatch.

The recovered system's enabled notification assistant is
`com.google.android.ext.services/android.ext.services.notification.Assistant`,
which is consistent with the crash stack's `NotificationAssistants` frame.
RMG also has a `run` notification channel. A foreground run notification is
therefore a plausible trigger for the callback, but the crash record does not
include the notification package or identify the listener call target, so
this remains unproven. The current boot's crash buffer was empty; DropBox
still contains the historical 21:57:39 entry.

The follow-up benign native probe uses the exact boot-ID path used by libcutils,
`/dev/ashmem29fb998d-b482-4364-a56e-f8acd670add1`, under
`run-as dev.rushiranpise.rmgnext`. The fd passed `fstat()` as a character
device with `rdev=10:119`; `ASHMEM_SET_NAME` returned 0,
`ASHMEM_SET_SIZE(4096)` returned 0, and `ASHMEM_GET_SIZE` returned 4096. The
8,296-byte probe is `build/ashmem-probe-20261126/ashmem_bootid_probe`; it
creates then closes one small ashmem region and does not touch the exploit
path. No system-server crash appeared in the current boot's crash buffer after
this probe.

The installed libcutils request numbers, node registration, `fstat()`, and
ashmem ioctls therefore work in this app sandbox on the recovered boot. This
does not exercise the `system_server` notification path or prove the precise
cause of its earlier exception. Taken together with the RMG run-start timing,
enabled Google notification assistant, and RMG's `run` notification channel,
the prior exception remains consistent with a transient system-server-side or
prior-kernel-state problem, not a permanent ioctl ABI mismatch. A single
send-trace attempt can now be resumed from the clean boot with the existing
pre-CFI, physical-read/write-disabled, and root-disabled guards. If the
system-server exception recurs before `constructor-entry`, stop there and
investigate the notification path instead of retrying again.

### 2026-11-26 send-trace retry: complete reclaim stream, trigger not reached

After the recovered boot passed the benign boot-ID ashmem probe, RMG ran one
attempt with the send-trace candidate. RMG `0.13+local.af081c2` selected the
exact `r9q-G990BXXSLIZH2` profile from
`Ryujin9610/Root-My-Galaxy-Payloads @ r9q-g990b-lizh2`. Shizuku was ready and
granted; KernelSU Manager v3.3.0 was already installed and the KernelSU
service was verified. The run started at 22:48:49 and failed at 22:51:21 at
“Running kernel exploit” with code 255.

The tested local payload was
`build/pi-helper-state-sendtrace-uclamp-20261126/cve-2026-43499-app.so`
(155,720 bytes; SHA-256
`9AAFD4A861384F11B9E551AA3E0A7021BBE593BD500C829079C510D17323B4B6`). It
used one exploit attempt with `PI_PROBE_ONLY`, `UCLAMP_TRIGGER_ONLY`,
`FOPS_READBACK_ONLY`, passive COW observation, helper-state tracing,
send-result tracing, durable logging, and a 512-MiB PTE region. CFI,
physical read/write, and root remained disabled. The RMG history JSON records
the payload source commit as `3095e6f2a0659570b5adc54a4cb6b121f96d2fa2`.

This attempt entered the native payload and completed controlled reclaim:
the order-3 seed reported 128/128 sends, and the reclaim stream reported
256/256 full 65,536-byte sends (16,777,216 bytes total). The kernel raised
the requested 16-MiB socket send buffer to 32 MiB. This resolves the earlier
short-stream uncertainty for this run.

The intended waitpoint did not occur. The monitor sampled the waiter 38 times
with first and last state `82` (`R`) and zero blocked samples. Its passive
samples were also `R`; the mprotect helper was sampled 37 times, first and
last state `82` with zero blocked samples. `pselect` returned 109 in 626 us,
while the readiness gate was open. The mprotect helper completed two calls
successfully in about 1.27 ms, but the guarded readiness walk reported
`calls=0 success=0`: the `sched_setattr` trigger was never invoked. The
separate `consumer baseline self-sched_setattr` timing is only a baseline
measurement, not evidence that the trigger ran.

The COW output mapping changed in pagemap from file-backed (`a100000000000000`)
to anonymous (`8100000000000000`), with `smaps` reporting 0 to 4 KiB of
anonymous memory. That establishes a mapping transition consistent with a
private COW fault; it does not establish that the exploit's target data was
altered. The complete 16-MiB canary scan covered all 512 order-3 chunks and
found `changed_chunks=0`, `lock_a=0`, `lock_b=0`, `task_pi=0`, and
`mutated=0`. No fops readback was attempted.

The payload then reported `readiness-cow cfi miss ... step=42 errno=0`.
Here, step 42 is the diagnostic's explicit stop-before-CFI sentinel set by
`RMG_DIAG_UCLAMP_TRIGGER_ONLY`; it is not a measured CFI failure. The route
was forced to return failure, and the final summary reported `done=0 root=0`,
so the RMG code-255 result is expected for this guarded diagnostic. No
KernelSU driver was loaded by this run.

Read-only post-run checks at 22:54:51 found the phone still on fingerprint
`samsung/r9qxeea/r9q:16/BP2A.250605.031.A3/G990BXXSLIZH2`, kernel
`5.4.289-qgki-32192773-abG990BXXSLIZH2`, boot ID
`29fb998d-b482-4364-a56e-f8acd670add1`, and `sys.boot_completed=1`.
`global.low_power=0`; `/dev/ksu` and `/proc/ksu` were absent, and the current
crash buffer was empty. The run caused no reboot or flash. Its RMG screenshot,
UI XML, complete app log, run-history JSON, checkpoint log, and post-run state
are saved under
`build/pi-helper-state-sendtrace-uclamp-20261126/one-shot-retry-01/`.

#### Comparison with the earlier waiter-D run

The retained RMG history JSON for the earlier run is now saved at
`build/pi-helper-state-trace-uclamp-20261126/one-shot-run/history.json`.
Both runs used the same firmware/kernel, 512-MiB PTE region, the same initial
roughly 50-us mprotect-to-pselect alignment, helper CPU4, and consumer CPU5.
The send-trace retry used the same guarded PI/uclamp route plus
`RMG_DIAG_COW_SEND_TRACE`; its complete stream differs from the earlier
251/256-send run, so this is not a perfectly controlled single-variable
comparison.

| Measurement | Earlier run that observed waiter `D` | Send-trace retry |
|---|---:|---:|
| Waiter / consumer / observer / mprotect-helper CPUs | 0 / 5 / 1 / 4 | 6 / 5 / 0 / 4 |
| Monitor inherited / allowed / effective masks | `0x1` / `0x3f` / `0x2` | `0x40` / `0xdf` / `0x1` |
| Consumer mask after widening | `0x3f` | `0x6f` |
| First mprotect entry relative to pselect start | −46 us | −49 us |
| First `PROT_NONE` mprotect return relative to pselect start | +2,044 us | +613 us |
| Waiter `D` observation relative to pselect start | +1,128 us (poll 78) | None observed in 38 polls |
| pselect result / duration | 118 / 2,858 us | 109 / 626 us |
| Total mprotect helper duration | 4,780 us | 1,269 us |
| Readiness scheduler calls / successes | 1 / 1 | 0 / 0 |
| Canary coverage | 501/512 chunks, incomplete | 512/512 chunks, complete |

These are within-run monotonic offsets; absolute timestamps are from different
boots and are not compared. In the earlier run the waiter entered `D` about
1.13 ms after pselect began, while the first mprotect was still holding the
write side for about another 0.92 ms. The first mprotect returned about
0.82 ms before pselect returned. In the retry, the first mprotect returned
about 13 us before pselect returned; the waiter stayed `R`, and the monitor
made no scheduler call. The passive observer sampled `R` 18 times in the
earlier run and missed the brief `D` observation, while the main monitor
caught it. This shows why the per-poll state trace matters.

Read-only sysfs inspection on the same SM8350 phone reports CPU capacities
325 for CPUs 0–3 (policy maximum 1.8048 GHz), 828 for CPUs 4–6 (2.4192 GHz),
and 1024 for CPU7 (2.8416 GHz). The earlier waiter ran on CPU0; the retry's
waiter ran on CPU6. Thus the waiter moved from the lower-capacity cluster to
the middle cluster, while helper and consumer remained on CPUs 4 and 5. The
per-thread affinity masks also changed: the earlier monitor widened from
`0x1` to `0x3f`, whereas the retry's monitor widened from `0x40` to `0xdf`;
the consumer's widened mask changed from `0x3f` to `0x6f`. These are the
effective masks logged for each thread, not one process-wide cpuset. The
phone reported battery saver off in both run records. The current read-only
topology snapshot is saved at
`build/pi-helper-state-sendtrace-uclamp-20261126/one-shot-retry-01/cpu-topology.txt`.

The shorter pselect and first-mprotect windows in the retry, plus the CPU
placement change, fit the hypothesis that the pselect COW path did not spend
long enough waiting on the mmap semaphore to queue in `TASK_UNINTERRUPTIBLE`.
It may have remained runnable while the rwsem owner completed, as allowed by
the kernel's optimistic-spin path. This is still an inference: CPU placement,
the completed versus incomplete reclaim stream, runtime frequency, and
memory-management timing all differ, and there is no kernel stack trace.
The retry's complete 512-chunk canary rules out a short stream as the reason
for its zero-mutation result; it does not identify why the waiter failed to
reach `D`.

The exact Samsung source analysis already documented above shows that the
read-side rwsem path can optimistically spin while a writer is active and
remain runnable. That makes an `R`-throughout interval plausible while the
pselect COW path waits for the mmap semaphore; it remains an inference because
no kernel stack or live wait-channel symbol was captured in this run. Earlier
COW-only delay experiments also showed that delaying mprotect by 250 us can
miss pselect entirely, while synchronized timing can block the helper without
blocking the waiter. Avoid another blind delay change. Before another device
attempt, if allowed CPUs make it possible, hold waiter and observer placement
constant; otherwise record the effective masks and placement explicitly. Then
choose one narrowly targeted observer or timing change. Retain the one-attempt
cap and the pre-CFI,
physical-read/write-disabled, and root-disabled guards.

### 2026-11-26 CPU-pinned send-trace comparison: waiter stayed runnable

To isolate waiter placement, one more attempt used the complete send-trace
candidate with the waiter requested on CPU0, the consumer on CPU5, and the
mprotect helper on CPU4. The candidate was
`build/pi-helper-state-sendtrace-cpu0-20261126/cve-2026-43499-app.so`
(155,832 bytes; SHA-256
`0317e77e91750cc44a04f97291bb65ac262c8c88aaf0ac4ab5978b4f8e95bd15`). It
kept the 512-MiB region, one payload attempt, complete send tracing, passive
COW observation, durable logging, and the pre-CFI, physical-read/write-disabled,
and root-disabled guards. The build included `RMG_DIAG_WAITER_CORE=0` and
`RMG_DIAG_CONSUMER_CORE=5`.

RMG selected the local candidate and the exact G990B SLIZH2 profile. Shizuku
was ready; the attempt started at 23:45:14 and ended at 23:47:31 with code
255 at “Running kernel exploit.” This is the expected diagnostic stop at
`readiness-cow cfi miss ... step=42`, not a CFI result or a root attempt. The
order-3 seed completed 128/128 sends and the reclaim stream completed 256/256
64-KiB sends. The 16-MiB canary covered 512/512 chunks and found no mutation.

The passive monitor reported waiter CPU0, observer CPU1, and predicted helper
CPU4. Its inherited mask was `0x1`, allowed mask `0x3f`, and effective observer
mask `0x2`. The main waiter monitor sampled 109 times, with zero blocked
samples and first/last state `R`. The passive observer also saw the waiter as
`R` in all 26 samples. The helper's separate state observer caught three brief
`D` samples near the end of the first mprotect; those samples belonged to the
mprotect helper, not the waiter. The consumer affinity log showed a request for
CPU7, followed by fallback selection of CPU5 and effective mask `0x20`. Thus
the effective CPU placement matches the intended `0 / 5 / 1 / 4` layout, but
the runtime did not confirm the compiled consumer request override. Resolve
that request/log discrepancy before relying on that override again.

| Measurement | Earlier waiter-`D` run | CPU-pinned send-trace run |
|---|---:|---:|
| Waiter / consumer / observer / helper CPUs | 0 / 5 / 1 / 4 | 0 / 5 / 1 / 4 |
| Observer inherited / allowed / effective masks | `0x1` / `0x3f` / `0x2` | `0x1` / `0x3f` / `0x2` |
| First mprotect entry relative to pselect start | −46 us | −46 us |
| First `PROT_NONE` mprotect return relative to pselect start | +2,044 us | +2,073 us |
| Waiter `D` observation | +1,128 us (poll 78) | None in 109 main-monitor polls |
| pselect result / duration | 118 / 2,858 us | 124 / 2,147 us |
| Total mprotect-helper elapsed | 4,780 us | 4,426 us |
| Scheduler trigger calls / successes | 1 / 1 | 0 / 0 |
| Reclaim stream / canary coverage | 251/256 sends / 501/512 chunks | 256/256 sends / 512/512 chunks |

This controls effective CPU placement: the waiter was on CPU0 and the observer,
consumer, and helper occupied the same CPUs as in the run that saw the waiter
in `D`. Placement alone did not reproduce that state. The first mprotect
write-side interval was also close in duration (2.119 ms here versus 2.090 ms
in the earlier run), but pselect returned about 711 us sooner here. The earlier
waiter's `D` sample occurred while that first mprotect was still active; this
run kept the waiter `R` despite a similarly long interval. The changed send
stream completeness, tracing overhead, and per-run scheduling remain possible
contributors, so this comparison rules out CPU placement as a sufficient
explanation but does not identify the cause of the intermittent wait state.

The overlay completed (`overlay=1`); the COW mapping changed from file-backed
to anonymous and `smaps` rose from 0 to 4 KiB anonymous memory, but there was no
target mutation, scheduler call, or KernelSU load. `/dev/ksu` and `/proc/ksu`
remained absent. The firmware, kernel, boot ID, and `sys.boot_completed` value
were unchanged after the run; battery saver remained off. The full RMG history,
app screenshots/UI dumps, build log, payload, and result screenshot are saved
under `build/pi-helper-state-sendtrace-cpu0-20261126/`.

### 2026-11-27 source trace of the CPU-pinned COW window

A read-only query of the running phone's `/proc/config.gz` returned
`CONFIG_IKCONFIG_PROC=y`, `CONFIG_SPECULATIVE_PAGE_FAULT=y`, and
`CONFIG_RWSEM_SPIN_ON_OWNER=y`. This confirms that both the speculative page
fault path and rwsem owner spinning are enabled in the installed kernel; the
same settings appear in Samsung's `vendor/r9q_eur_openx_defconfig`. No exploit
or state-changing command was run for this check.

The current candidate constructs `cow_out` by writing the fd set to a memfd,
mapping it `MAP_PRIVATE`, then reading one word to install a file-backed PTE
(`src/targets/r9q-G990BXXSLIZH2/port/fops.c`, around lines 1316-1341). It does
not write privately to that mapping before passing it as `pselect6`'s
`writefds` pointer. In Samsung's 5.4.289 source, `core_sys_select()` copies the
result bitmap back with `__copy_to_user()` (`fs/select.c`, around lines
399-404 and 691-694). That output copy is therefore the first intended private
write and triggers COW. Because a fresh private file VMA has no prepared
`anon_vma`, the speculative fault handler returns `VM_FAULT_RETRY`
(`mm/memory.c`, around lines 4460-4465); arm64 then enters the ordinary
read-side `mm->mmap_sem` path (`arch/arm64/mm/fault.c`, around lines 532-550).

The event timestamps for the CPU-pinned run fit that lock sequence:

| Event | Offset from pselect start |
|---|---:|
| First `PROT_NONE` mprotect enters | −46 us |
| pselect starts | 0 us |
| First mprotect returns | +2,073 us |
| Second `PROT_READ` mprotect enters | +2,074 us |
| Mprotect-helper observer first sees `D` | +2,101 us |
| pselect returns 124; helper observer sees `R` again | +2,148 us |

`mprotect()` takes the same mmap semaphore for writing and releases it at the
end of the call (`mm/mprotect.c`, around lines 518-519 and 607-609). The
waiter's rwsem read slow path tries `rwsem_optimistic_spin()` before it queues;
only after that spin fails does it set the task's uninterruptible state and
call `schedule()` (`kernel/locking/rwsem.c`, around lines 1015-1040 and
1097-1118). With the live config's `CONFIG_RWSEM_SPIN_ON_OWNER=y`, it is
therefore possible for the waiter to remain `R` while spinning on the first
mprotect writer. Once that writer releases the semaphore, the COW copy can
finish under the read lock. The helper's immediately following write-side
mprotect can then block behind that reader, matching its brief `D` interval
from +2,101 to +2,148 us.

This is now the best-supported explanation for why the CPU-pinned run showed
the helper in `D` while the waiter remained `R`; it also explains why matching
the earlier run's effective CPU placement did not reproduce waiter `D`. One
plausible difference is that the earlier waiter left the optimistic-spin path
and queued, whereas the current run stayed runnable long enough to acquire the
read lock after the writer released it. The observations do not capture a
stack or lock owner, so they cannot prove the waiter was specifically spinning
in `rwsem_down_read()`.
The observed `R` state also cannot distinguish running from runnable. The
shorter pselect duration, send-trace instrumentation, and per-run scheduling
remain possible influences.

The logged `readiness mmap locker elapsed_us` is the elapsed mprotect-helper
loop time, not a direct measurement of mmap-semaphore ownership duration. The
two per-call `MPTIME` intervals are the relevant syscall timings. The
following schedstat run adds per-thread scheduling/runtime counters. A live
wait channel still needs to be captured only when the waiter enters `D`; do
not infer a blocked waiter from the helper's state.

### 2026-11-27 guarded schedstat trace: waiter briefly entered `D`

Built one local candidate with `RMG_DIAG_SCHEDSTAT_TRACE=1`, retaining the
single-attempt limit and the pre-CFI, physical-read/write-disabled, and
root-disabled guards. The candidate was
`build/pi-helper-state-schedstat-20261127/cve-2026-43499-app.so` (158,040
bytes; SHA-256
`3f757baa3c7f3b2c59380e50b515d6315b4ad4a9ca024d26483e9a357304011c`). RMG
confirmed it was using the imported local file and selected the exact
`r9q-G990BXXSLIZH2` profile. The run log reported
`diagnostic=uclamp-trigger-only` and
`cfi_write_pipe_physrw_root=disabled`.

The effective placement was mostly the same as the earlier run that caught the
waiter blocked, but the mmap helper moved. The passive observer widened its
affinity and reported allowed mask `0x6f`; this mask excludes CPU4, so the
helper selector fell back from its preferred CPU4 to CPU6. The low-power
setting was off (`global.low_power=0`).

| Role or mask | Earlier waiter-`D` run | Schedstat run |
|---|---:|---:|
| Waiter | CPU0 | CPU0 |
| Consumer | CPU5 | CPU5 |
| State observer | CPU1, effective mask `0x2` | CPU1, effective mask `0x2` |
| mmap helper selected CPU | CPU4 | CPU6 (CPU4 unavailable in allowed mask `0x6f`) |
| Observer allowed mask | `0x3f` | `0x6f` |

`pselect` began at monotonic `11921270448 us`. The first mprotect entered
46 us earlier and returned 2,185 us after pselect began. The waiter was first
sampled in `D` at poll 40, 670 us after pselect began; the state-transition
trace marker followed at +673 us. This is about 458 us earlier than the
earlier waiter-`D` run's +1,128-us observation (poll 78). The first mprotect
was still in progress for about another 1.5 ms after the current `D` sample.
The current pselect returned `112` after 2,294 us; the earlier run returned
`118` after 2,858 us. The second mprotect returned at +4,358 us, and the
reported total helper-loop duration was 4,412 us versus 4,780 us previously.

The new per-thread counters cover the first 605 us of the active pselect
window. The waiter was sampled as `R` at polls 8 through 36 and as `D` at poll
40. Its cumulative runtime rose from 145,290,839 to 145,812,506 ns, runqueue
wait rose from 6,599,061 to 6,861,509 ns, and timeslices rose from 114 to
116. Those totals are cumulative snapshots, not a precise decomposition of
each 75-us polling interval. The mprotect helper was sampled as `R` at polls
8 through 40; its counters stayed at 1,159,062 ns runtime, 7,922,970 ns
runqueue wait, and four timeslices in all nine samples. Thus the trace confirms
that the waiter reached `D` during this run, but it does not reveal the exact
wait channel or prove whether an `R`-state thread was executing versus merely
runnable. The helper's unchanged snapshots also do not resolve its CPU-time
behavior.

The readiness route completed its overlay diagnostic and reported
`readiness-cow cfi miss ... step=42`; the 16-MiB canary scan covered 512/512
chunks with no changed chunks. RMG ended at “Running kernel exploit” with code
255, before KernelSU loading. The pipe-physrw summary had `done=0`, `root=0`,
and no successful read or write. Afterward the phone still reported firmware
`G990BXXSLIZH2` and kernel `5.4.289-qgki-32192773-abG990BXXSLIZH2`,
`sys.boot_completed=1`, and shell UID 2000; `/dev/ksu` and `/proc/ksu` were
absent. There was no reboot or flash.

The full RMG log and UI dump, result screenshot, build log, and the captured
logcat are saved locally under
`build/pi-helper-state-schedstat-20261127/` (`phone-run-ui.txt`,
`phone-run-ui.xml`, `phone-run-final.png`, `build.log`, and
`phone-logcat.txt`). The schedstat counters improve the state trace, but the
helper CPU fallback and one-sample `D` observation mean this was not a fully
controlled comparison. The following guarded run adds the requested live
wait-channel sample without enabling the downstream root stages.

### 2026-11-27 guarded live-D wait-channel trace: `do_page_fault`

Added optional `RMG_DIAG_LIVE_WCHAN_TRACE=1` support to the G990B port. Its
compile-time guard requires `PI_PROBE_ONLY`, `UCLAMP_TRIGGER_ONLY`, the
passive COW observer, COW trace, and durable logging, and rejects COW-only
builds. The monitor reads
`/proc/self/task/<waiter-tid>/wchan` once when the waiter is observed in
`D`, after the trigger gate and while the pselect call is active. It then
re-reads the task state and pselect-active flag. The read itself is
instrumented with an elapsed-time and errno record.

Built the AArch64 Android 30 candidate
`build/pi-helper-state-live-wchan-20261127/cve-2026-43499-app.so` (159,080
bytes; SHA-256
`706EBCB4AD49B48B84E56C5773702EEF450DC89A60657302F6C0B56BDB0E7229`) with
NDK r27c. Besides the new wait-channel flag, this retained the guarded
`PI_PROBE_ONLY` / `UCLAMP_TRIGGER_ONLY` profile, passive COW observation,
consumer affinity, durable and COW tracing, 512-MiB COW region, send trace,
schedstat trace, helper-state trace, waiter CPU0, and consumer CPU5. Binary
inspection confirmed the new logger and the
`cfi_write_pipe_physrw_root=disabled` marker. The build completed with only
the existing unused-function warnings.

RMG Settings showed this imported local payload selected, and the exact
`SM-G990B` / `5.4.289` profile was selected. RMG's generic parent log still
printed `attempts=24`, but the payload's own supervisor reported
`attempts=1`, followed by `exploit attempt=1/1`. The candidate logged
`diagnostic=uclamp-trigger-only` and
`cfi_write_pipe_physrw_root=disabled`; no retry was made.

The effective placement matched the earlier 2026-11-27 schedstat profile:

| Role or mask | Earlier schedstat run | Live-wchan run |
|---|---:|---:|
| Waiter | CPU0 | CPU0 |
| Consumer | CPU5 | CPU5 |
| State observer | CPU1, effective mask `0x2` | CPU1, effective mask `0x2` |
| mmap helper selected CPU | CPU6 | CPU6 |
| Observer allowed mask | `0x6f` | `0x6f` |

The live run started with low power off (`global.low_power=0`). CPU4 was
outside the observer's allowed mask, so the helper selector again chose CPU6.
This reproduces the previous run's effective placement, including its helper
CPU fallback.

The pselect-before trace was at monotonic `39716842672 us`. The first
mprotect entered 45 us earlier and returned at +2,095 us. The waiter was
sampled in `R` at polls 8, 12, 16, 20, and 24, then in `D` at poll 28,
about +440 us after pselect began. At that first active `D` sample, the new
read reported:

```text
state_before=D state_after=D polls=28
gate=1 runnable_after_gate=1
pselect_active_before=1 pselect_active_after=1
elapsed_us=71 wchan_errno=0 wchan=do_page_fault
```

The first mprotect was still in progress for about 1.65 ms after that waiter
sample. Pselect returned `118` at +2,172 us (the overlay summary rounds this
to `pselect_us=2170`). The second mprotect returned at +4,208 us; the
two-mprotect helper loop reported 4,260 us total.

The six schedstat snapshots show cumulative waiter runtime unchanged through
poll 20, then increasing from 150,535,620 to 150,989,422 ns at poll 24 and
151,019,891 ns at poll 28. Runqueue wait stayed at 7,377,764 ns through poll
24 and reached 7,403,493 ns at poll 28; timeslices increased from 130 to 131.
These are sparse cumulative snapshots, not precise per-poll execution times.
The mprotect helper remained in `R` at all six samples; its reported runtime
(183,178 ns), runqueue wait (7,663,594 ns), and timeslices (3) did not change.
As before, `R` alone does not distinguish a running thread from one waiting
on the run queue.

The COW walk completed (`success=1`, `walk_us=23`), but the 16-MiB canary
scan reported `512/512` chunks scanned and `changed_chunks=0`. The route
then reported `readiness-cow cfi miss ... step=42`; the overlay summary was
`blocked=1 sched=0/0`. Pipe-physrw ended with `done=0`, `root=0`, and no
successful read or write. RMG ended at “Running kernel exploit” with code
255, before KernelSU loading.

Afterward, the phone remained booted with the same firmware fingerprint and
kernel, shell UID 2000, low power off, and no reboot. `/dev/ksu` and
`/proc/ksu` were absent. The new live-D sample reproduces
`do_page_fault` under the guarded PI/uclamp profile and at the same effective
CPU placement as the previous schedstat run. Its overlap with the first
mprotect is consistent with a page-fault wait during the COW/mmap sequence,
but `wchan` is only a symbol snapshot: this does not provide a kernel stack,
identify a specific rwsem or lock owner, or prove the exact wait site.

#### Source-level correlation from the Samsung Android 16 kernel bundle

The downloaded `SM-G990B_16_Opensource.zip` contains a kernel tree whose
top-level Makefile reports Linux `5.4.289`; its kernel README names
`vendor/r9q_eur_openx_defconfig`, matching this device family. The phone also
allowed a read-only check of `/proc/config.gz`, which returned
`CONFIG_SMP=y` and `CONFIG_RWSEM_SPIN_ON_OWNER=y`.

The matching source provides a concrete path consistent with the observed
symbol. In `arch/arm64/mm/fault.c`, `do_page_fault()` first tries
`handle_speculative_fault()` (line 536). If that returns
`VM_FAULT_RETRY`, it tries `down_read_trylock(&mm->mmap_sem)` and falls back
to `down_read(&mm->mmap_sem)` if the read lock is contended (lines 545-549).
In `mm/mprotect.c`, `do_mprotect_pkey()` acquires the write side with
`down_write_killable(&current->mm->mmap_sem)` (line 518). In
`kernel/locking/rwsem.c`, `__down_read()` invokes the slow path with
`TASK_UNINTERRUPTIBLE` (lines 1363-1366); that path first attempts
optimistic spinning, then queues, sets the task state, and calls
`schedule()` (around lines 999-1113). The running kernel's
`RWSEM_SPIN_ON_OWNER` setting permits that spin-before-sleep transition.

The timing fits this path: the helper entered its first mprotect 45 us before
pselect began, the waiter reached `D` about 440 us into pselect, and the
mprotect did not return until +2,095 us. A page fault in `do_page_fault()`
waiting for the read side of the same process `mm->mmap_sem` while a
concurrent mprotect holds or is acquiring its write side is now the
best-supported explanation for the captured `do_page_fault` wait channel.
This remains a hypothesis: the trace has no stack or lock-owner pointer, and
the syscall interval does not prove when the write lock was acquired. The
helper's sampled `R` state and unchanged schedstat counters do not settle
that ownership question.

#### Runtime tracefs access

The current ADB shell (UID 2000) can read `/proc/config.gz`, which reports
`CONFIG_TRACEPOINTS=y`, `CONFIG_EVENT_TRACING=y`, and `CONFIG_FTRACE=y`.
However, it cannot read `/sys/kernel/tracing/available_events` or
`set_event` (both return permission denied), and
`/sys/kernel/debug/tracing` is absent. This does not establish which
individual lock events are built; write access to `set_event` was not tested.
The shell cannot inspect the tracepoint inventory, so no lock event could be
selected or verified through this read-only check. No tracefs controls were
changed.

The build log, complete RMG UI text and XML, before/run/final screenshots, and
profile-selection dumps are saved under
`build/pi-helper-state-live-wchan-20261127/` (`build.log`,
`phone-run-ui.txt`, `ui-running-80s.xml`, `phone-before.png`,
`phone-run-final.png`, `ui-profile.xml`, `phone-config-snippet.txt`,
and `phone-tracefs-access.txt`).

### 2026-11-27 guarded helper-D syscall snapshot

Built the app payload with `RMG_DIAG_LIVE_HELPER_SYSCALL_TRACE=1`, layered on
the live waiter `wchan` and helper-state diagnostics. The candidate is
`build/pi-helper-state-live-wchan-helperd-syscall-20261127/cve-2026-43499-app.so`
(160,888 bytes; SHA-256
`B426D0A44AA3D7154A0DF2FC0652E2B4D05A4E08602FC1B1772360F304711B96`). The
device-side staged file matched that hash. RMG used the exact
`r9q-G990BXXSLIZH2` profile and ran one attempt with
`diagnostic=uclamp-trigger-only` and
`cfi_write_pipe_physrw_root=disabled`.

Effective placement matched the latest waiter-`D` run that captured
`do_page_fault`: waiter CPU0, consumer CPU5, observer CPU1 (effective mask
`0x2`), and mmap helper CPU6 (allowed mask `0x6f`). This contrasts with the
older comparison run whose helper was on CPU4 with allowed mask `0x3f`; the
current run therefore matches the most recent confirmed trace's placement,
though not every historical run's helper placement.

The event timing differs from the waiter-`D` capture. Relative to pselect start,
the first mprotect entered at −46 us and returned at +2,043 us; the second
mprotect entered immediately at +2,043 us. The helper was first sampled in
`D` at +2,086 us, during the second mprotect and 4 us before pselect returned
115 at +2,090 us. The gated passive observer's 29 pselect-active samples all
reported the waiter as `R`; the route reported `blocked_samples=0`.

The one-shot `/proc/self/task/<helper-tid>/syscall` read began at +2,089 us and
took 95 us. It returned successfully (`errno=0`) but its data was the literal
`running`, not a syscall number or argument set. By the time the snapshot read
the task, the helper state was `R` both before and after and the pselect-active
flag was already clear. The snapshot also read the waiter as `R` before and
`D` afterward, with the flag clear both times. This is a race after the helper
`D` edge, not evidence that the waiter blocked during pselect; it does not
identify which syscall or wait site held up the helper. The helper's D sample
close to the second mprotect is compatible with mmap-semaphore contention, but
this run provides no helper `wchan`, stack, or lock-owner evidence to prove it.

For comparison, the immediately preceding waiter-`D`/live-wchan run used the
same effective CPU placement. It observed the waiter in `D` about 443 us after
pselect started, while the first mprotect was still active, and captured
`wchan=do_page_fault`; that mprotect returned at about +2,095 us. This run saw
the helper's brief `D` 43 us into the second mprotect, after the first mprotect
had returned, while its pselect-qualified waiter samples stayed `R`. Matching
CPU placement therefore did not reproduce the same event ordering.

The diagnostic stopped at the guarded pre-CFI miss (`step=42`): the COW walk
reported zero calls and zero successes, the canary reported no changed chunks,
and pipe physical read/write and root remained disabled (`done=0`, `root=0`).
RMG ended with code 255. Afterward the phone remained booted on the same
Android 16 fingerprint and 5.4.289 kernel, low-power mode remained off, shell
UID was 2000, battery was 100% at 19.4 C, and `/dev/ksu` and `/proc/ksu` were
absent. The full RMG log, current UI hierarchy, build log, payload, and device
state are saved under
`build/pi-helper-state-live-wchan-helperd-syscall-20261127/`.

### 2026-11-27 guarded helper-D wchan trace and PI-futex panic

Built the app payload with the compile-time guard
`RMG_DIAG_LIVE_HELPER_WCHAN_TRACE=1`, layered on the live waiter-wchan and
helper-state tracing. After two consecutive helper `D` samples with the
trigger gate open, this diagnostic reads
`/proc/self/task/<helper-tid>/wchan` and records helper/waiter states plus
gate and pselect snapshots. The candidate was
`build/pi-helper-state-live-wchan-helperd-wchan-20261127/cve-2026-43499-app.so`
(160,744 bytes; SHA-256
`7F5FE4381D4ECBF4BC37ABDD843DD422CC90887773AE24BE300319F3E06598E5`). The
device-side file
`/sdcard/Download/rmg-live-helper-wchan-uclamp-20261127.so` matched this
hash. The candidate retained
`cfi_write_pipe_physrw_root=disabled`.

RMG showed the exact `r9q-G990BXXSLIZH2` profile and Shizuku Running. One
diagnostic attempt was started. ADB disconnected during the attempt and
reconnected after the phone rebooted. RMG did not save a new history entry;
the history remained at 138 runs. No helper-wchan record or final payload
result was recovered. Do not repeat this image unchanged: the bug report
shows the kernel panicked during its PI-futex path.

The read-only bug report contains a pstore panic at boot uptime about 43,557
seconds. It reports `kernel BUG at kernel/locking/rtmutex_common.h:60!` on
CPU 4, PID/TID 29594, Comm: `libpoc64.so`, and PC
`rt_mutex_adjust_prio_chain+0x878/0x890`. The call chain starts at
`rt_mutex_adjust_prio_chain` and continues through `task_blocks_on_rt_mutex`,
`__rt_mutex_start_proxy_lock`, `futex_lock_pi`, `do_futex`, and
`__arm64_sys_futex`, followed by the arm64 syscall return path. The kernel
then panicked with `Fatal exception`. The full bug report and extracted
excerpt are in
`build/pi-helper-state-live-wchan-helperd-wchan-20261127/bugreport.zip` and
`build/pi-helper-state-live-wchan-helperd-wchan-20261127/kernel-panic-excerpt.txt`.

#### Exact Samsung source match and panic-site reconstruction

The downloaded `SM-G990B.zip` contains the Samsung Android 16 kernel base
archive `SM-G990B_16_Opensource.zip` and a firmware delta archive for
`G990BXXSLIZH2`. The delta README instructs unpacking the `G990BXXUGIYJ1`
base and then applying the SLIZH2 update. The base Makefile identifies Linux
5.4.289. The delta contains 35 files and does not replace
`kernel/locking/rtmutex_common.h`, `kernel/locking/rtmutex.c`,
`kernel/futex.c`, or `include/linux/rtmutex.h`; those files therefore remain
the corresponding 5.4.289 base files for this firmware source bundle.

In that base, `kernel/locking/rtmutex_common.h` has SHA-256
`c79353d84625e4d352be7d96193cf162fba7857a85171540996f499679ea3d08` and
line 60 is exactly `BUG_ON(w->lock != lock)`. `rt_mutex_top_waiter()` gets
the cached leftmost node from `lock->waiters`, treats it as an embedded
`rt_mutex_waiter`, and asserts that the waiter's `lock` pointer matches the
mutex. The panic's reported `rtmutex_common.h:60` is therefore an exact
source match, not an inference from a five-line offset. The corresponding
upstream 5.4.289 sources are also available for
[`rtmutex_common.h`](https://github.com/gregkh/linux/blob/v5.4.289/kernel/locking/rtmutex_common.h#L52-L62),
[`rtmutex.h`](https://github.com/gregkh/linux/blob/v5.4.289/include/linux/rtmutex.h#L30-L33),
[`futex_lock_pi()`](https://github.com/gregkh/linux/blob/v5.4.289/kernel/futex.c#L2914-L2932),
and [`futex_setup_timer()`](https://github.com/gregkh/linux/blob/v5.4.289/kernel/futex.c#L489-L503).

The saved register/disassembly analysis at the panic site records `x23` as
the rt-mutex being walked (`0xffffff80f5bca940`), `x0` as the cached
leftmost waiter pointer (`0xffffff80f5bca950`), and `x8` as zero when the
waiter's `lock` field is loaded. Since `x0 == x23 + 0x10`, and the ARM64 layout places
`waiters.rb_leftmost` at offset `0x10` in the `rt_mutex` structure, the cached
leftmost pointer points back to the address of its own field instead of to a
waiter node. The failing comparison then sees a null
waiter-lock value rather than `x23`; the firmware disassembly reaches
`brk #0x800` at `rt_mutex_adjust_prio_chain+0x878`. This establishes that
the waiter tree was malformed and self-referential at the panic. It does not
show whether that state was deliberately forged by the exploit path or arose
from a race or other corruption, and the crash record does not preserve the
user futex address that led there. No matching unstripped `vmlinux` or DWARF
line table was available; the source match comes from Samsung's base-plus-
delta package, and the register interpretation comes from the saved firmware
disassembly.

After reboot, ADB reported `sys.boot_completed=1`,
`ro.boot.bootreason=kernel_panic,bug`, the same Android 16 fingerprint and
kernel `5.4.289-qgki-32192773-abG990BXXSLIZH2`, low-power mode off, shell UID
2000, battery 100% at 21.5 C, and no `/dev/ksu` or `/proc/ksu`. The
post-reboot snapshot is saved as
`build/pi-helper-state-live-wchan-helperd-wchan-20261127/phone-state-after-reboot.txt`.
The intended helper `wchan` was not captured, so the source match does not
identify what blocked that helper.

#### Payload PI-futex call review

The stack's `futex_lock_pi()` frame narrows the immediate failure to the
plain `FUTEX_LOCK_PI` slow path. It does not identify which user-space futex
word was passed. In the 64-bit payload path, two direct contended lock calls
fit that stack:

- `owner_thread()` in `main.c:305` waits on `f_pi_chain`, after the waiter
  thread has locked that word at `main.c:168`.
- `route_pi_sanitize()` in `fops.c:546` waits on `f_pi_sanitize`, which is
  held by `sanitize_holder_thread()` at `main.c:105`. The sanitizer is called
  around the pselect route and its cleanup paths at `fops.c:895`, `3296`, and
  `3476`.

The initial `f_pi_target` lock and the sanitizer holder's initial lock are
uncontended in this setup, so they do not match the captured
`task_blocks_on_rt_mutex` frame. The earlier `FUTEX_WAIT_REQUEUE_PI` and
`FUTEX_CMP_REQUEUE_PI` operations create the exploit's PI state, but the
panic's `futex_lock_pi()` frame is the direct-lock handler. Without the
calling TID, syscall arguments, or a checkpoint naming the futex word, the
owner-chain call and sanitizer call cannot be distinguished.

There is a separate timeout-clock bug in `route_pi_sanitize()`. It builds an
absolute deadline by calling `clock_gettime(CLOCK_MONOTONIC)` and adding
50 ms (`fops.c:525-546`), then passes that deadline to plain
`FUTEX_LOCK_PI`. In the matching 5.4.289 source, `futex_lock_pi()` calls
`futex_setup_timer(..., FLAGS_CLOCKRT, ...)`, and `futex_setup_timer()`
selects `CLOCK_REALTIME` for that flag. The monotonic deadline is therefore
interpreted against realtime and is already in the past. An `ETIMEDOUT`
result from this call cannot be treated as evidence of a 50 ms wait. This
clock mismatch may change the cleanup timing, but it does not by itself
explain the self-referential waiter tree or prove that this call caused the
panic.

The source correction now uses `CLOCK_REALTIME` for the sanitizer deadline.
When `RMG_DIAG_PI_FUTEX_CALL_TRACE=1` is enabled, both contended call sites
log their TID and futex address; with durable logging enabled, they also
`fdatasync` a checkpoint immediately before the lock call. The two entry
stages are `pi-futex-owner-enter` and `pi-futex-sanitize-enter` in
`/data/local/tmp/rmg-pipe-oracle-checkpoint.log`.

The guarded PI-probe/uclamp diagnostic candidate built successfully with NDK
r27c at
`build/pi-futex-clockfix-calltrace-20261127/cve-2026-43499-app.so` (161,560
bytes; SHA-256
`85b22a502f5feb3500078f79bda25ae2036422848ed25070dc4aa91e26f320bd`). It
retains the prior diagnostic profile and adds the PI call trace.

#### Guarded clock-fix run — 27 November 2026

The candidate was imported into RMG Next `0.13+local.af081c2 (28218460)` and
run once against the exact `r9q-G990BXXSLIZH2` profile. RMG reported Shizuku
permission granted, selected the local file
`rmg-pi-clockfix-calltrace-20261127.so`, verified the profile's KernelSU
service, and logged `diagnostic=uclamp-trigger-only` with
`cfi_write_pipe_physrw_root=disabled`. Battery saver was off. The run reached
the exploit stage and performed one independent attempt.

The new call trace distinguishes both contended `FUTEX_LOCK_PI` calls:

- `route-sanitize` entered at uptime 6001.258 s and returned at 6001.309 s,
  about 51 ms later. The trace reports `CLOCK_REALTIME` and `timeout_ms=50`;
  the call returned `ETIMEDOUT` (110). Its subsequent unlock returned
  `EPERM` (1), consistent with the lock call timing out before acquiring the
  lock.
- `owner-chain` entered at uptime 6001.060 s and returned at 6009.060 s,
  about 8 seconds later, with return value 0 and errno 0.

This run therefore verifies that the sanitizer's corrected realtime deadline
behaves like a 50 ms timeout. It also shows the owner-chain caller can remain
inside `FUTEX_LOCK_PI` for several seconds and later return successfully. The
older panic capture still cannot be assigned to either call site because it
does not contain a caller TID or futex address.

The readiness observer's placement and timing differed from the 26 November
run at
`build/pi-helper-state-trace-uclamp-20261126/one-shot-run/checkpoint.log`:

| Observation | 26 November blocked-state run | 27 November clock-fix run |
| --- | --- | --- |
| Observer CPU | CPU 1 | CPU 0; effective mask `0x1`, allowed mask `0x5f`, affinity error 0 |
| Waiter samples | 78 polls; final sample `state=68` | 36 polls; samples remained `state=82`, `blocked_samples=0` |
| pselect duration | 2,858 µs | 582 µs |
| mprotect timing | 4,780 µs | 1,183 µs |

These are observations from separate runs. They do not establish that CPU
placement caused the different waiter state or durations. The current
readiness COW check saw a file-backed page before pselect and an anonymous page
after it, but did not establish dirty-page or control-page mutation; the
canary remained unchanged. This build's `RMG_DIAG_UCLAMP_TRIGGER_ONLY` guard
then stopped before CFI and reported step 42, the sentinel assigned by that
guard; this is the expected diagnostic stop, not a kernel fault. The CFI walk
made zero calls, so this run says nothing about the CFI or root stages. The
physical read/write summary reported `root=0`, `read_ok=0`, and `write_ok=0`,
as expected because those stages were disabled.

RMG displayed exit code 255 with its generic message about an unavailable
privileged spawn. That message is misleading for this run: the RMG log records
Shizuku permission granted and the full payload trace. The port's
`preload.c` logged `exploit attempt=1/1 failed status=1`, then calls `pr_error`
for the exhausted attempt, which exits with `-1` and is surfaced by the shell
as 255. The payload did run and reached the configured stop-before-CFI guard;
the unchanged canary kept the test from advancing to CFI.

The attempt did not reboot the phone: ADB remained connected,
`sys.boot_completed` stayed at 1, and uptime advanced from about 6001 to 6186
seconds. The existing `kernel_panic,bug` boot reason therefore predates this
run. Afterward `/dev/ksu` was absent and the RMG failure occurred before its
KernelSU load stage. The captured RMG log and durable checkpoint are in
`build/pi-futex-clockfix-calltrace-20261127/rmg-failure-log.txt` and
`build/pi-futex-clockfix-calltrace-20261127/post-attempt-checkpoint.log`.

#### Guarded FOPS readback candidate — 27 November 2026

The next candidate was built with NDK r27c at
`build/pi-fops-readback-only-20261127/cve-2026-43499-app.so` (159,032 bytes;
SHA-256
`248211d2375c8d257d63c1c490e4c87a422c6f1ea0ce024eeeccf929631b6697`). Its
`RMG_DIAG_FOPS_READBACK_ONLY` and `RMG_DIAG_CFI_ONLY` guards allow only the
ashmem FOPS readback after a successful readiness route. The CFI write,
physical read/write, and root-install paths are disabled. The build also
enables durable checkpoints, COW tracing, PI futex call tracing, scheduler
statistics, and a 512 MiB PTE cap.

The candidate was imported into RMG Next `0.13+local.af081c2 (28218460)` and
run once with Shizuku against the exact `r9q-G990BXXSLIZH2` profile. RMG
confirmed it used `rmg-pi-fops-readback-only-20261127.so`, verified the
KernelSU service, and prepared that service before starting the exploit. The
phone was boot-complete with battery saver off before and after the run.

The attempt leaked the kernel slide through tracefs, then stopped earlier in
the readiness route:

- The readiness COW check saw the page change from file-backed to anonymous,
  but `dirty=0`, `control_write=0`, and `blocked_samples=0`; the last waiter
  sample remained `state=82`.
- `route-sanitize` returned `ETIMEDOUT` (110), and its unlock returned
  `EPERM` (1). The readiness walk recorded `calls=0 success=0`.
- The canary scan read 16,420,480 of 16,777,216 requested bytes, scanning 501
  of 512 order-3 chunks. It reported an incomplete negative result,
  `changed_chunks=0`, and `mutated=0`.
- The route ended at step 33 with no waiter calls. Consequently,
  `try_cfi_stage()` was never entered and there is no FOPS readback result in
  this run. It does not establish whether the readback itself works.

The payload log reports `exploit attempt=1/1 failed status=1`; RMG rendered
the failure as its generic exit code 255. The `pipe-physrw-summary` reported
`done=0 root=0 read_ok=0 write_ok=0`. RMG did not reach its KernelSU load
stage, `/dev/ksu` remained absent, and the phone stayed online with
`sys.boot_completed=1`.

The run log is in
`build/pi-fops-readback-only-20261127/rmg-failure-log.txt`. The durable
checkpoint file is cumulative; the current run is identified by PID 32269 in
`build/pi-fops-readback-only-20261127/post-attempt-checkpoint.log`, with the
relevant entries also isolated in
`build/pi-fops-readback-only-20261127/post-attempt-checkpoint-excerpt.log`.
The next investigation should focus on why the readiness route produced no
waiter calls and why the canary scan stopped short of the full reclaim stream.

#### Source trace: readiness gate and short reclaim stream

The `calls` field in `readiness-cow walk` is not a count of pselect calls.
`readiness_pselect_asm()` did run once and returned 100. `consumer_calls` is
incremented only after the monitor samples the waiter in `D` state while the
trigger gate and pselect are active and the waiter has first been observed
runnable after the gate. That branch then invokes `sched_setattr_tid_nochange()`.
The route signal additionally requires `monitor.saw_block`, valid overlay
geometry, and a successful consumer call. In the current run there was no
monitor `D` sample, so `calls=0 success=0`, the route signal stayed false, and
`try_cfi_stage()` never ran. The later `route-sanitize` timeout is cleanup
after pselect and did not cause the missing consumer call.

The current COW trace has no post-gate-runnable or blocked event. Its monitor
first sampled the waiter in state `82` before pselect. Pselect reported
711 microseconds (713 microseconds across the trace brackets) and returned
100; the first mprotect pass lasted 746
microseconds, and the helper loop lasted 1,448 microseconds. By comparison,
the 26 November blocked-state run had a 2,858-microsecond pselect, a
4,780-microsecond helper interval, and a `D` sample at poll 78 while the
trigger gate and pselect were active. That sample was followed by one
successful scheduler call. The blocked run logged waiter CPU 0, observer CPU
1, helper CPU 4, and consumer CPU 5. The current candidate did not enable the
passive observer or log its CPU, so these runs do not isolate CPU placement as
the cause. The timing difference is consistent with an intermittent,
scheduling-sensitive wait window, but the userspace trace does not prove
whether or for how long the current COW fault waited on `mmap_sem`.

There is also a misleading field in the current output: `[OVERLAY] ...
sched=0/0` looks like a successful scheduler call, but the monitor struct is
zero-initialized and `sched_ret` / `sched_errno` are assigned only inside the
`D`-state branch. With `calls=0`, that `0/0` is the untouched default. A future
trace should add an explicit `sched_attempted` field or initialize the return
value to a non-success sentinel.

The short canary result has a separate, directly recorded cause. The current
run reported `sends=251/256`. Its build did not enable
`RMG_DIAG_COW_SEND_TRACE`, so the sender uses the 8-MiB send-buffer request and
the nonblocking loop counts any positive `sendmsg()` return as a sent chunk;
it does not verify that the return equals the requested 65,536 bytes or record
the byte count. Linux permits nonblocking sends to fail with `EAGAIN` when the
send buffer is full, and a successful send reports the number of bytes sent.
Thus 251 counted sends do not prove 251 full chunks.

The canary then performs one `recv(MSG_DONTWAIT | MSG_PEEK)` on the
`AF_UNIX/SOCK_STREAM` receiver. Stream reads ignore message boundaries and
return currently available data without waiting for the full request, so one
peek is not by itself a complete-stream check ([`recv(3p)`](https://www.man7.org/linux/man-pages/man3/recv.3p.html)).
This run returned 16,420,480 of 16,777,216 requested bytes, covering 501 full
order-3 chunks plus 3,712 trailing bytes. The stream was already short at the
sender (`251/256`), so the zero-change result cannot establish that the
unscanned tail was unchanged. The run also lacked per-send byte counts, so it
cannot distinguish a failed send from a positive short send or an additional
short peek.

The earlier `RMG_DIAG_COW_SEND_TRACE` run provides a useful control: its
16-MiB send-buffer request produced an effective 32-MiB buffer, all 256 sends
returned exactly 65,536 bytes, and the canary read all 16,777,216 bytes
(512/512 chunks). That run still had no blocked waiter because its delayed
mprotect began after pselect returned. Together, the runs show that the
reclaim-stream completeness and waiter-state gates are independent.

For the next diagnostic, combine the already-tested full-send trace with the
original synchronized mprotect start and the passive observer's separate
CPU placement, while retaining the COW-only stop before FOPS, physical
read/write, and root. Require 256/256 full sends and 512/512 canary coverage
before interpreting a negative scan. Do not treat the readback-only route as
tested until the readiness signal is observed.

#### COW-only send-trace and passive-observer run (27 November 2026)

A fresh one-attempt diagnostic combined the full reclaim send trace with the
passive COW observer and the synchronized mprotect start. It used
`RMG_DIAG_COW_ONLY=1`, `RMG_DIAG_COW_TRACE=1`,
`RMG_DIAG_COW_SEND_TRACE=1`, `RMG_DIAG_PASSIVE_COW_OBSERVER=1`,
`RMG_DIAG_PASSIVE_COW_HELPER_CORE=4`, `RMG_DIAG_DURABLE_LOG=1`, and
`RMG_DIAG_COW_PTE_MB=512`. No mprotect delay or fixed waiter override was
enabled. The build guard rejects fixed waiter affinity in COW-only mode; the
target default selected waiter CPU 0. The binary was built with NDK r27c as
`build/cow-only-sendtrace-passive-20261127/cve-2026-43499-app.so` (164,672
bytes; SHA-256
`6F21C26828031423A4E7C272774B0780EC8C78568EE0C1387297E5601BF79377`). The
phone's staged copy had the same hash and RMG confirmed it was using the local
payload. Its runtime guard reported `diagnostic=cow-only` and
`cfi_write_pipe_physrw_root=disabled`.
RMG's profile log showed its general route limit of 24, but the payload
supervisor reported `attempts=1` and `exploit attempt=1/1`; only one payload
attempt ran.

The reclaim stream was complete. The order-3 seed reported 128/128 sends;
the final reclaim requested a 16-MiB send buffer and got an effective 32-MiB
buffer. All 256 `sendmsg()` calls returned exactly 65,536 bytes, for
16,777,216/16,777,216 bytes and `complete=1`. The canary read the same full
16 MiB, scanned 512/512 order-3 chunks, and reported
`changed_chunks=0 ... mutated=0`. This makes the negative canary result
complete for this run.

CPU placement matched the earlier blocked-waiter run: waiter CPU 0, passive
observer CPU 1, and mprotect helper CPU 4. The observer inherited CPU 0's
affinity, widened to allowed mask `0x3f`, then pinned to CPU 1 (effective mask
`0x2`). It recorded 11 waiter samples; the waiter remained in state `82`
(`R`) at the sampled points, including poll 8 while the trigger gate and
pselect were active. No waiter `D` sample occurred. The mprotect helper,
however, was sampled in state `68` (`D`) at poll 48 with the gate and pselect
active, at the same trace timestamp as pselect return. Pselect returned 112
in 648 microseconds; the mprotect helper ran for 2,539 microseconds. The
pagemap changed from present/file/exclusive to present/non-file/exclusive,
and the smaps anonymous count changed from 0 to 4 KiB, consistent with a COW
copyout. This does not by itself prove which lock the helper waited on.

The route still reported `calls=0 success=0`, so the readiness signal stayed
false and `try_cfi_stage()` did not run. The COW-only guard forced the signal
off before CFI; physical read/write, root, and KernelSU loading were not
reached. RMG reported its generic failure code 255. `/dev/ksu` remained
absent. ADB stayed online and `/proc/uptime` was about 31,499 seconds after
the run, so the `ro.boot.bootreason=kernel_panic,bug` value observed then
predates this attempt and is not evidence that this run rebooted the phone.

This comparison rules out CPU placement alone as the explanation for the
missing waiter block: the prior blocked run and this run used the same waiter,
observer, and helper CPUs, yet only the prior run sampled the waiter in `D`.
The runs differ in their trigger configuration and timing: the prior uclamp
trigger run's pselect lasted 2,858 microseconds and recorded the waiter in `D`
at poll 78, while this COW-only run's pselect lasted 648 microseconds and
only the helper entered `D`. The next source review should compare the
pre-pselect scheduling and thread setup in those two builds, then focus any
follow-up on the helper's block and the waiter's state around pselect return.
Keep the complete send trace and COW-only stop for that comparison.

The first `PROT_NONE` mprotect interval gives a sharper timing contrast. In
this run it began about 49 microseconds before pselect and returned 615
microseconds after pselect began; pselect returned about 34 microseconds
later. In the earlier blocked run, the first mprotect returned 2,044
microseconds after pselect began, and the waiter was sampled in `D` at
1,128 microseconds while that mprotect was still active. This points to a
substantially shorter overlap in the current run, but it does not identify
why the mprotect interval changed.

The comparison is not a single-variable timing control. The earlier run used
the PI/uclamp trigger profile and did not enable send tracing; this run used
the COW-only profile with send tracing. The traced sender requested a 16-MiB
socket buffer and received 32 MiB, whereas the non-trace source path requests
8 MiB. The different reclaim stream and buffer can change the memory and
scheduling conditions before pselect. Matching waiter, observer, and helper
CPUs rules out placement alone, but does not isolate the cause of the shorter
mprotect overlap or the missing waiter `D` sample.

The RMG UI log, durable checkpoint, result screen, and build are in
`build/cow-only-sendtrace-passive-20261127/`; the checkpoint SHA-256 is
`AD6400155BE02565FC9271FD84597F4A9EF9C0CF674664D743D2800701DBFC7A`, and the
RMG UI log SHA-256 is
`44450B6BA396E011B7756365C5A0DADAF55DFDE1B6C89E7F5021E70DB0F34448`.

#### COW-only send-trace follow-up with consumer affinity (27 November 2026)

To resolve the unmeasured consumer placement, a fresh one-attempt payload
added `RMG_DIAG_COW_CONSUMER_AFFINITY=1` to the previous guarded COW-only,
send-trace profile. It retained the 512-MiB COW region, passive observer,
CPU-4 mprotect-helper preference, durable checkpoints, and the forced stop
before CFI, physical read/write, and root. The exact flags were
`RMG_DIAG_COW_ONLY=1`, `RMG_DIAG_COW_TRACE=1`,
`RMG_DIAG_COW_SEND_TRACE=1`, `RMG_DIAG_PASSIVE_COW_OBSERVER=1`,
`RMG_DIAG_COW_CONSUMER_AFFINITY=1`,
`RMG_DIAG_PASSIVE_COW_HELPER_CORE=4`, `RMG_DIAG_DURABLE_LOG=1`, and
`RMG_DIAG_COW_PTE_MB=512`. It was built for Android API 30 with NDK r27b
(`27.1.12297006`) as
`build/cow-only-sendtrace-consumer-affinity-20261127/cve-2026-43499-app.so`
(165,736 bytes; SHA-256
`E7B5DAABB3A4BDEA2E4B3CE1A77DE213486C91DD7E1124F1C2AA6508955683B6`). The
phone's staged file had the same SHA-256. RMG displayed the exact new local
payload name and reported `diagnostic=cow-only`,
`cfi_write_pipe_physrw_root=disabled`, and `attempt=1/1`.

The affinity trace confirms that the consumer started with inherited mask
`0x1`, widened to the cpuset-allowed mask `0x3f`, then fell back from requested
CPU 7 to CPU 5 (effective mask `0x20`). The waiter selected CPU 0, the passive
observer CPU 1 (effective mask `0x2`), and the mprotect helper CPU 4. This
matches all four measured CPU roles from the 26 November blocked-waiter run.
The controlled order-3 seed completed 128/128 sends; controlled reclaim
completed all 256 sends of 65,536 bytes each. The traced socket-buffer request
was 16 MiB and its effective size was 32 MiB. Kernel-page preparation took
55,220 ms.

The passive monitor sampled the waiter for 139 polls and recorded no `D`
state (`blocked_samples=0`); its 33 samples during pselect also began and
ended in `R`. Pselect returned 100 in 2,028 microseconds. The mprotect helper
was sampled in `D` at poll 139 while pselect was active, 19 microseconds before
the pselect-return trace timestamp; its captured wchan value was `0`. The
first `PROT_NONE` mprotect ran from trace time `34414180991` to `34414183009`
(2,018 microseconds). The pselect-before trace followed 46 microseconds after
that call started, so the call returned 1,972 microseconds into pselect, roughly
58 microseconds before pselect returned. Both helper calls completed, with
two loop iterations and 4,008 microseconds total helper time.

Pagemap changed from present/file/exclusive to present/non-file/exclusive,
and smaps anonymous memory rose from 0 to 4 KiB, consistent with COW copyout.
The full 16-MiB canary scan covered 512/512 order-3 chunks, found zero changed
chunks, and reported `mutated=0`. The readiness route reported `calls=0`;
the COW-only guard forced the route signal to zero and stopped before
`try_cfi_stage()`. The trailing physical-RW summary reported `root=0`,
`read_ok=0`, and `write_ok=0`; KernelSU was not loaded. RMG returned code 255
at “Running kernel exploit,” as expected when this guarded diagnostic found
no readiness mutation. The phone remained awake and ADB-responsive on the
same Android 16 fingerprint and kernel; the existing
`ro.boot.bootreason=kernel_panic,bug` value was not a reboot from this run.

Compared with the 26 November blocked-waiter run, consumer placement is now
explicitly confirmed to match, and the other three CPU roles match as well.
The first mprotect call returned about 1,972 microseconds after pselect began,
close to the earlier 2,044-microsecond offset. However, the earlier run
sampled the waiter in `D` at poll 78 (about 1,128 microseconds into pselect),
whereas this run sampled the waiter in `R` throughout all 139 monitor polls
and found the mprotect helper in `D` near pselect return. Pselect was 830
microseconds shorter than the earlier 2,858-microsecond run. Matching all
four CPUs and nearly matching the first-mprotect overlap show that consumer
placement alone does not reproduce the waiter block.

The runs still differ in reclaim and trigger behavior: the earlier PI/uclamp
profile completed 251/256 reclaim sends and took 133,561 ms to prepare the
kernel page; this COW-only send-trace run completed 256/256 and took 55,220
ms. The earlier profile also called the uclamp scheduler operation only after
observing the waiter in `D`; this COW-only build skips that operation. The
current evidence therefore leaves reclaim-stream completeness and the
PI/uclamp trigger as unresolved differences. The helper's `D` sample with
wchan `0` does not identify the lock it waited on.

The RMG UI log extraction, original result UI XML, result screenshot, and
build are in `build/cow-only-sendtrace-consumer-affinity-20261127/`. The
extracted UI-log SHA-256 is
`7E7BA11765428EA1485F5CD636399249453FE007DA3A743638CB7BECE20D1036`; the
original XML SHA-256 is
`0D03240FE47DBDB3B9B4973279AEE7C177B71B4920BF43BF3D858E041144611B`; and the
result screenshot SHA-256 is
`B7A5ADB995521458262F9A68E196FC6B5CBA147C44503C3EB9824B1210BCC272`.

### 2026-11-27 matched PI/uclamp, send-trace, affinity comparison

A fresh one-attempt diagnostic combined the PI/uclamp trigger with the complete
send trace, passive waiter/helper observer, and consumer-affinity tracing. The
build retained the 512-MiB COW region and the compile-time stop before CFI,
physical read/write, and root. Flags were `RMG_DIAG_PI_PROBE_ONLY=1`,
`RMG_DIAG_UCLAMP_TRIGGER_ONLY=1`, `RMG_DIAG_FOPS_READBACK_ONLY=1`,
`RMG_DIAG_PI_HELPER_STATE_TRACE=1`, `RMG_DIAG_COW_TRACE=1`,
`RMG_DIAG_COW_SEND_TRACE=1`, `RMG_DIAG_PASSIVE_COW_OBSERVER=1`,
`RMG_DIAG_COW_CONSUMER_AFFINITY=1`,
`RMG_DIAG_PASSIVE_COW_HELPER_CORE=4`, `RMG_DIAG_DURABLE_LOG=1`, and
`RMG_DIAG_COW_PTE_MB=512`. The Android API 30 / NDK r27b build is
`build/pi-uclamp-sendtrace-consumer-affinity-20261127/cve-2026-43499-app.so`
(155,944 bytes; SHA-256
`B21F34A78B50BC384F8DEAE4A3D8052C03D3507B8D3F7CD866F5C2EA46C3DDAE`). The
staged phone copy matched. RMG selected the local file, identified the
`r9q-G990BXXSLIZH2` profile, used Shizuku, and reported
`diagnostic=uclamp-trigger-only`,
`cfi_write_pipe_physrw_root=disabled`, and one exploit attempt.

The phone remained on Android 16, fingerprint
`samsung/r9qxeea/r9q:16/BP2A.250605.031.A3/G990BXXSLIZH2:user/release-keys`,
and kernel `5.4.289-qgki-32192773-abG990BXXSLIZH2`. Power saving was off and
battery was 100%.

The consumer began with affinity mask `0x1`, widened to `0x3f`, then fell
back from requested CPU 7 (`EINVAL`) to CPU 5 (effective mask `0x20`). The
waiter was pinned to CPU 0, the observer to CPU 1 (effective mask `0x2`), and
the mmap helper affinity selected CPU 4. These match the four CPU roles in
the 26 November blocked-waiter run and the preceding 27 November COW-only
send-trace run.

Controlled reclaim completed all 256 sends of 65,536 bytes (16 MiB total);
the 16-MiB socket-buffer request had an effective size of 32 MiB. Kernel-page
preparation took 59,681 ms. Pselect returned 118 in 643 microseconds. The
waiter was `R` at poll 8, then `D` at poll 24 while the trigger gate and
pselect were active. Its last passive sample was at monotonic time
`35730383070 us`, about 282 microseconds after pselect began. The helper
observer saw the mprotect helper in `R` across all 24 polls, with no `D`
samples.

The first `PROT_NONE` mprotect began 49 microseconds before pselect and
returned 587 microseconds after pselect began, 57 microseconds before
pselect returned. The second mprotect completed 1,241 microseconds after
pselect began. Both calls took 1,293 microseconds total.

The run reported one consumer scheduler call and one success, with a
19-microsecond call duration and scheduler return/errno `0/0`. The counters
are consistent with the source: this guarded path increments `consumer_calls`
before `sched_setattr_tid_uclamp_nochange()` and increments `consumer_success`
only when it returns 0. The durable checkpoint has no `sched-uclamp-*`
per-call records because logging is deliberately deferred around the timing
window. `rmg_diag_checkpoint_defer(1)` is set before the monitor is armed,
and `rmg_diag_checkpoint()` discards records while deferred rather than
buffering them; deferral is cleared after pselect. The target's clamp values
and flags are therefore not retained. Pagemap changed from file-backed to
anonymous/exclusive and smaps anonymous memory rose from 0 to 4 KiB, but the
complete 16-MiB canary scan found no changed data (`512/512` order-3 chunks;
`mutated=0`). The fops readback was skipped. `readiness-cow cfi miss
step=42` is the sentinel assigned by the diagnostic guard; CFI did not run.
The guard recorded
`uclamp-trigger-stop-before-cfi a=0 b=0`, forced the route signal to zero,
and stopped before CFI, physical-RW, and root. RMG returned code 255 at
“Running kernel exploit”; KernelSU was not loaded, and `/dev/ksu` and
`/proc/ksu` remain absent.

| Observation | 26 Nov blocked PI/uclamp | 27 Nov COW-only matched | 27 Nov PI/uclamp matched |
| --- | --- | --- | --- |
| Consumer / waiter / observer / helper CPUs | 5 / 0 / 1 / 4 | 5 / 0 / 1 / 4 | 5 / 0 / 1 / 4 |
| Waiter state | `D` at poll 78, about +1,128 us | no `D` in 139 polls; helper entered `D` | `D` at poll 24, about +282 us |
| Pselect result and duration | 118 / 2,858 us | 100 / 2,028 us | 118 / 643 us |
| First mprotect return after pselect start | +2,044 us | +615 us | +587 us |
| Helper interval | 4,780 us | 4,008 us | 1,293 us |
| Reclaim sends / page preparation | 251/256 / 133,561 ms | 256/256 / 55,220 ms | 256/256 / 59,681 ms |

This rules out CPU placement alone as the reason the waiter state differed:
the COW-only run and this PI/uclamp run used the same selected CPU roles and
completed all sends, yet only the PI/uclamp run sampled the waiter in `D`.
That is consistent with the trigger profile affecting the observed wait, but
does not prove causation. Timing varied substantially: the current `D` sample
came earlier in a much shorter pselect window, and its first mprotect ended
about 1.46 ms earlier than in the 26 November run. The current and earlier
PI/uclamp runs also differ in reclaim completeness and page-preparation time.

The RMG UI log, durable checkpoint, result UI XML, result screenshot, and
build are in `build/pi-uclamp-sendtrace-consumer-affinity-20261127/`. Their
SHA-256 values are respectively
`AC218D4BF2B6E9163411EFABA111F9D6DBAB04A24E7D0BFDBDC8C479375B468F`,
`E6EF07B69769CCDCC3AD0202F0B9EE9D445F3DE69CD79C40C0ADB0F2D8C2E521`,
`FE72A260629CC89B3B9A5F013DABCB5C17D2D21B214621621AB565ED6856307A67`, and
`498DB05BC821DA918FFB4898802DEE5CB1652FF2EEBB4D8862E2ACDCD1D5F829`; the
payload SHA-256 is recorded above. After the attempt the phone remained
booted and ADB-responsive with the same firmware and kernel. The existing
`ro.boot.bootreason=kernel_panic,bug` value did not change and is not evidence
of a reboot from this attempt.

### 2026-11-28 in-memory uclamp trace candidate

The uclamp helper now writes its per-call details into a fixed-size record
owned by the readiness monitor. It captures the waiter TID and poll, state,
trigger-gate and pselect state before the call, `sched_getattr()` result and
errno, policy/nice, clamp values, flags before and after adding the util-clamp
flags, whether `sched_setattr()` was attempted, its result/errno, and call
timestamps. The monitor prints the record and writes its checkpoint only
after pselect returns and deferred logging is restored; no checkpoint file
I/O was added inside the timing window.

The Android API 30 candidate was built with NDK r27b
(`27.1.12297006`):
`build/uclamp-memorytrace-20261128/cve-2026-43499-app.so` (156,872 bytes;
SHA-256 `B17071435ABA5BB7F60DA8668040FEAF88D8BC7C64EC7A21FE35045A3AFA76E8`).
It is AArch64 ELF64. Binary inspection found the `UCLAMPTRACE` and
`readiness-uclamp-call` records plus the `diagnostic=uclamp-trigger-only`,
`cfi_write_pipe_physrw_root=disabled`, and explicit CFI/physical-RW/root
refusal messages. Build flags are recorded in
`build/uclamp-memorytrace-20261128/build-flags.txt`.

### 2026-11-28 one-shot in-memory uclamp trace run

The candidate was staged in RMG Next as
`rmg-uclamp-memorytrace-20261128.so`; the phone-side SHA-256 matched the local
build. RMG selected the `r9q-G990BXXSLIZH2` profile, used Shizuku, verified the
KernelSU service, and reported `diagnostic=uclamp-trigger-only` with
`cfi_write_pipe_physrw_root=disabled`. The phone was on Android 16,
fingerprint
`samsung/r9qxeea/r9q:16/BP2A.250605.031.A3/G990BXXSLIZH2:user/release-keys`,
and kernel `5.4.289-qgki-32192773-abG990BXXSLIZH2`. Battery saver was off.
The payload supervisor and exploit child each ran one attempt.

KASLR discovery succeeded from tracefs. Controlled reclaim completed all 256
64-KiB sends (16 MiB), and the readiness path reached pselect. The call
returned 118 in 2,970 microseconds. The waiter was in state `R` before the
trigger gate and in state `D` at poll 32 while pselect was active; the
checkpoint records waiter CPU 6, observer CPU 0, and helper CPU 5.

The in-memory trace finally captures the complete scheduler request:

`tid=12274 poll=32 state=68 gate=1 runnable=1 active=1 recheck=1`
`getattr=0/0 policy=0 nice=0 flags=0->96 util=0/1024`
`setattr attempted=1 result=0 errno=0 call=19 us`.

Thus both `sched_getattr()` and the same-value `sched_setattr()` call succeeded
without an error on this kernel. The values stayed at the default clamp range
(`min=0`, `max=1024`), although the added flags were not a pure no-op. The
matching Samsung source package identifies the base kernel as 5.4.289 and
instructs applying the `G990BXXSLIZH2` update archive; that update does not
contain `kernel/sched/core.c`. In the base source, the UAPI defines the two
clamp flags as `0x20` and `0x40`, a same-policy request with those flags takes
the `change` path in `__sched_setscheduler()`, and `sched_setattr()` calls it
with PI enabled. The `change` path calls `rt_mutex_adjust_pi(p)`. That
function returns without walking the PI chain if `p->pi_blocked_on` is null or
points to the task's own waiter. This user-space trace cannot read that field,
so it confirms the scheduler request but not whether the PI-chain walk
occurred, whether the waiter's PI tree changed, or whether effective
utilization changed. The
512-MiB COW trace observed a blocked waiter and the overlay geometry check
passed, but the 16-MiB canary scan found no changed chunks (`mutated=0`,
`task_pi=0`). The fops readback was skipped. The diagnostic guard recorded
`uclamp-trigger-stop-before-cfi a=0 b=0`, forced the route signal to zero, and
set `readiness-cow cfi miss step=42` as its stop sentinel. CFI was not called.

RMG displayed the generic `Failed at: Running kernel exploit` result with code
255. Its final counters were `done=0 root=0`, `read_ok=0`, `write_ok=0`, and
`rw64=0/0`; no physical-RW or root operation succeeded, and KernelSU was not
loaded. `/dev/ksu` and `/proc/ksu` were absent, as expected for this guarded
run. The phone stayed booted and ADB-responsive with the same fingerprint and
kernel; `sys.boot_completed` remained `1`. This was the single approved run;
there was no retry.

Compared with the 27 November matched PI/uclamp run, this run captured the
actual scheduler attributes rather than only the return counters. Both runs
reported a successful scheduler call and a blocked waiter, but this run
observed the waiter at poll 32 and pselect lasted 2,970 microseconds, versus
poll 24 and 643 microseconds on 27 November. CPU placement also differed
(waiter/observer/helper 6/0/5 here versus 0/1/4 there), so the timing results
are not a controlled placement comparison. The key result is narrower: the
scheduler syscall succeeds, while the canary remains unchanged and the route
stops before CFI. Further work should inspect the canary/COW transition and
capture the waiter's PI-tree links around `rt_mutex_adjust_pi()` and compare
them with the canary scan. The current evidence does not justify changing the
request or enabling later exploit stages.

The captured UI log, durable checkpoint, result UI XML, and result screenshot
are in `build/uclamp-memorytrace-20261128/`. Their SHA-256 values are
`AE198E445734165FFC541C16D056B5DAB31127F36016DF95F5A584FBA8BC2BE7`,
`6296BD8FAA9ABA2EDD92F4E68980E15BA5ACA4422E648C5307D2836D462A33C4`,
`0205ED07F816A861C42DC8A73D7E12A7FD8A0EAC9212B0589CE66239CBF9A96B`, and
`C4A570BCC1D2044E8155A080BAEFB0E1D18090F16DDD8636A60CF0D337656F36`,
respectively.

### 2026-11-28 guarded nice-step candidate prepared

The matching `rtmutex.c` source shows that `rt_mutex_waiter_equal()` returns
true for non-deadline tasks when waiter priority equals the task's current
priority. `rt_mutex_adjust_pi()` returns immediately in that case, before the
PI-chain walk. `task_blocks_on_rt_mutex()` initializes the proxy waiter's
priority from the task's current priority. Since the previous diagnostic
preserved policy and nice, the successful scheduler call could still have
returned from `rt_mutex_adjust_pi()` before walking the stale PI chain. This
is consistent with the unchanged canary, but remains an inference: the
userspace trace does not expose effective `p->prio` or `p->pi_blocked_on`.

A follow-up candidate adds a compile-time guarded nice step of `0 -> 1` to the
same scheduler request and records both values. It retains the `0..1024`
clamp range and the compile-time stop before CFI, physical-RW, and root. The
build is `build/uclamp-priority-step-20261128/cve-2026-43499-app.so` (157,024
bytes; SHA-256
`2FABCFD3954E7A26E129713EA8E7BC6EED8F84BE2067891114D54827DF9A97B2`). Its
startup identity includes `nice_step=+1` and
`cfi_write_pipe_physrw_root=disabled`; build flags are recorded in the same
directory. It was imported into RMG Next as a local payload and run once on
the phone; see the result below.

Changing nice may make the stale PI waiter unequal to the task and drive the
chain walker into the corrupted waiter/lock state. This attempt did not
exercise the nice change because the trigger gate was not reached.

### 2026-11-28 one-shot guarded nice-step run

The candidate above was pushed to `/sdcard/Download/` and imported through
RMG Next's Local payload setting. The imported app-private copy had the same
SHA-256 as the local build. RMG selected the `r9q-G990BXXSLIZH2` profile,
received Shizuku permission, verified the KernelSU service, and reported the
local filename `rmg-uclamp-priority-step-20261128.so`. The run used the
guarded `diagnostic=uclamp-trigger-only`, `nice_step=+1` build with
`cfi_write_pipe_physrw_root=disabled`. Although RMG's generic route setting
listed 24 attempts, the payload supervisor and exploit child each logged a
limit of one; the trace confirms exactly `exploit attempt=1/1`.

Tracefs KASLR discovery succeeded. Controlled reclaim completed the selected
34-object group and all 256 64-KiB sends (16 MiB). The readiness path then
returned from pselect with result 106 in 611 microseconds. During the active
gate, the waiter was sampled at state 82, but `blocked_samples=0`; the passive
observer's first and last recorded states were also 82, with zero blocked
samples across its ten samples. The mprotect helper had zero blocked samples.
The 16-MiB canary scan covered 512/512 order-3 chunks and found no changed
chunks (`mutated=0`, `task_pi=0`); fops readback was skipped. The compile-time
guard stopped at the expected `readiness-cow cfi miss step=42`, before CFI,
physical read/write, or root.

Crucially, this run did **not** test the proposed priority-equality explanation:
the gated helper `sched_setattr_tid_uclamp_nochange()` was not called. The
durable checkpoint has no `sched-uclamp-*` records, the UI trace has no
`UCLAMPTRACE`, and the readiness scheduler summary is `0/0`. The earlier
`sched-nochange-*` records are from the consumer's baseline self-scheduling
call and do not show the new `nice +1` request. Since the waiter was never
observed blocked during the gate, the `0 -> 1` nice step was not applied. The
unchanged canary therefore does not confirm or refute whether a changed
priority would advance the PI chain.

RMG displayed `Failed at: Running kernel exploit` with code 255. The phone
remained ADB-responsive and booted on the same Android 16 fingerprint and
kernel; `sys.boot_completed` stayed `1`. `/dev/ksu` and `/proc/ksu` remained
absent, as expected because this guarded run never gained root or reached the
KernelSU load stage. No retry was made.

The local evidence is in
`build/uclamp-priority-step-20261128/`: `rmg-ui-log.txt` (SHA-256
`8AE6CD7F0A0065B5D159F2E6E2CD3F89C9514E4C31922B1900AD171482527E90`),
`post-attempt-checkpoint.log` (SHA-256
`3A167370D63D12FC0B4AB374793BFAC3467BF9F882166CE0EDBF07932EB81B5A`), and
`result-screen.png` (SHA-256
`CBFEF61EECE339108D19E7B803708DB0E822B35033E1ED08DB672F795305DD50`).

The next useful work is to explain the readiness timing difference that
prevented any blocked waiter sample, using the existing COW-only versus
PI/uclamp traces. Do not treat another nice-step run as informative until the
trigger path is shown to reach the gated setter.

### 2026-11-28 readiness timing comparison

The earlier `uclamp-memorytrace` run and the guarded nice-step run used the
same CPU placement: consumer on CPU 5, waiter on CPU 6, passive observer on
CPU 0, and mprotect helper on CPU 5. CPU affinity therefore does not explain
why only the earlier run observed a blocked waiter.

In the earlier run, `pselect()` lasted 2,970 microseconds. The waiter was
sampled in state `D` at poll 32, about 514 microseconds after `pselect()`
began. The first mprotect interval ran from about 48 microseconds before
`pselect()` to +1,660 microseconds; the second ran from +1,660 to +3,712
microseconds. Thus `pselect()` returned while the second mprotect call was
still in progress. In the nice-step run, `pselect()` returned in 611
microseconds. The waiter was sampled as `R` at +46 and +575 microseconds, and
the two mprotect intervals were approximately -48 to +602 and +602 to +1,172
microseconds. No blocked sample was observed, so the guarded nice setter did
not run. These user-space intervals bracket whole syscalls and do not reveal
the exact point at which mprotect held the memory-map semaphore.

The matching Samsung source gives a plausible lock path: `core_sys_select()`
copies its result sets to user memory through `set_fd_set()` and
`__copy_to_user()`; a write-protection fault in that COW output mapping can
enter the arm64 page-fault path, which takes `mmap_sem` for reading. Meanwhile,
`do_mprotect_pkey()` takes `mmap_sem` for writing. If the faulting copyout
arrives while that write lock is held, the waiter can block. The source review
and observed timing are consistent with this explanation, but do not prove
that this overlap caused the earlier `D` sample.

Evidence is in `build/uclamp-memorytrace-20261128/` and
`build/uclamp-priority-step-20261128/`; the matching extracted source review is
in `build/select-cow-source-review-20261128/`. Both runs completed the
readiness COW transition, neither changed the canary, and neither reached
CFI, physical read/write, or root. The latest run therefore provides no result
about the proposed nice-step/PI-chain hypothesis.

The next diagnostic should focus on making the copyout fault and the mprotect
write-lock window measurably overlap, and log each event independently. First
review whether the existing payload can bracket the COW output copy and each
mprotect call more precisely; if it cannot observe lock acquisition directly,
keep that limitation explicit and use controlled timing variations as a
correlation test. Re-run the nice-step candidate only after a blocked waiter
sample reaches its guarded scheduler hook. Keep the diagnostic stop before
CFI, physical read/write, and root, and use one attempt per run.

### Follow-up: timing controls review (2026-11-28)

The existing trace build already records the start and end of `pselect()` and
each `mprotect()` syscall. It cannot mark the precise `__copy_to_user()` fault
or the interval inside `mprotect()` when `mmap_sem` is held, so those remain
inferred from syscall timing and the waiter state.

The delayed COW-only run from 2026-11-26 used a 250-microsecond helper delay;
`pselect()` returned in 62 microseconds, and the helper began mprotect about
137 microseconds after that return. That delay missed the operation entirely.
In the 2026-11-28 no-delay run, the first mprotect syscall began about 48
microseconds before `pselect()` and returned at +602 microseconds, while
`pselect()` returned at +611 microseconds; no blocked state was sampled. The
earlier blocked run had a longer first mprotect interval, ending at +1,660
microseconds, and observed the waiter in `D` around +514 microseconds.

A useful next timing-only candidate is `RMG_DIAG_COW_MPROTECT_DELAY_USEC=75`
in the one-attempt COW-only build. The main thread has a 50-microsecond
head-start before entering `pselect()`, so this setting should start the
helper near the beginning of `pselect()` (roughly +15 to +25 microseconds,
subject to scheduling). That tests whether moving the helper window later
captures the COW copyout, without enabling the nice step, CFI, physical
read/write, or root stages. Treat the result as timing correlation, since the
kernel lock interval itself is not directly traced. Only after a `D` sample
reaches the guarded setter would a separate one-attempt nice-step run test the
priority hypothesis.

### 2026-11-28 one-shot 75-microsecond COW timing run

The COW-only build used `RMG_DIAG_COW_MPROTECT_DELAY_USEC=75`, a 512-MiB PTE
region, and the passive COW observer. It was built for Android API 30 with NDK
r27b. Its SHA-256 is
`99165DD918FDB0A3F78C70B5CE64B63E04E10ECE391F30B72DFFA0F718464D5C` (165,880
bytes). The build strings include the COW trace and
`readiness-cow-only-stop-before-cfi`; the build used no PI/uclamp or nice-step
flags. RMG's app-private `files/local-payload/local-exploit.so` matched the
candidate hash before execution.

RMG logged `exploit attempt=1/1`, used the local payload, and completed tracefs
KASLR discovery. It also verified and staged its KernelSU service before
starting the diagnostic. The readiness COW mapping changed from present,
file-backed to present, anonymous. The diagnostic stopped at
`readiness-cow cfi miss`; it did not obtain temporary root, load KernelSU, or
perform physical read/write. RMG reported code 255 at the kernel-exploit step.

The 75-microsecond delay moved the first mprotect syscall entry to about +27
microseconds after the `pselect()`-before trace marker. That call lasted 653
microseconds and returned around +680 microseconds. The pselect-after marker
was around +690 microseconds (reported `pselect_us=685`). A second mprotect
call started around +680 microseconds and ended around +1,243 microseconds.
This is a tighter timing alignment than the 250-microsecond-delay run, where
mprotect began after pselect had returned.

No waiter `D` state was sampled: the passive observer recorded 11 samples,
all `R` (ASCII 82), from about +47 to +639 microseconds; the helper observer
also recorded zero blocked samples. The COW-only monitor samples passively
once every four polls. Its final sample was about 51 microseconds before the
pselect-after marker, so a short blocked interval could have gone unsampled;
that is a possibility, not proof that the waiter blocked. The longer
685-microsecond pselect duration and the syscall timing are suggestive of
changed event alignment, but the run does not establish that `mmap_sem`
contention caused the delay.

The phone remained on the same Android 16 fingerprint and 5.4.289 kernel,
ADB-responsive with battery saver off. The ADB shell remained UID 2000, and
`kernelsu` was absent from `/proc/modules`; `/dev/ksu` and `/proc/ksu` were
absent. The staged KernelSU service was not loaded.

Evidence is in `build/cow-mprotect-delay75-20261128/`: `rmg-ui-log.txt`
(SHA-256
`136D6F632BE7F06D144D72B0407B843F5DFDC1164EB434428F49D214E2A398A6`),
`result-ui.xml` (SHA-256
`88585EBB5297421E07D05927DD72E2909A96C2FB6C5202EC9A2A3EDE6469DFF9`), and
`result-screen.png` (SHA-256
`4715D62AD2749ECA78F44DE9A69F0ADED7C657A52FD3E662C2DFBEE4F1293807`).

The next diagnostic should keep the 75-microsecond timing and sample the
waiter on every monitor poll instead of every fourth poll. This may catch a
brief `D` window; the added sampling load could also affect timing, so preserve
the trace markers and compare the syscall intervals again. Keep it one-shot
and COW-only. Do not enable the nice step unless a blocked sample reaches its
guarded setter.

### 2026-11-28 one-shot every-poll observer run

The follow-up build retained the 75-microsecond mprotect delay and enabled
`RMG_DIAG_PASSIVE_COW_OBSERVER_EVERY_POLL=1`. It remained a one-attempt
COW-only diagnostic and stopped before CFI, physical read/write, and root.
The 165,816-byte Android API 30 build has SHA-256
`F33CADDAEC9CEDB0B07161D42FEA400CF9B0416E4B7DCD0DFCE13B42F78F009A`.
RMG Next's app-private local-payload copy matched that hash before the run.

RMG logged `exploit attempt=1/1`, used the local payload, and completed tracefs
KASLR discovery. The readiness COW mapping again changed from present,
file-backed to present, anonymous. The waiter remained `R` in all 129
every-poll observer samples; `blocked_samples=0`. The samples span about +19
to +2,172 microseconds relative to the pselect-before marker. The reported
`pselect_us` was 2,203 microseconds.

The mprotect helper, rather than the waiter, was observed once in state `D`
while `pselect()` was still active: helper TID 10328, poll 134, about +2,185
microseconds from the pselect-before marker. The helper's wait channel was
reported as `0`, so the reason for that state is unknown. The first mprotect
syscall ran from about +29 to +2,150 microseconds; the second began at +2,151
and ended at +4,113 microseconds. The pselect-after marker was about +2,206
microseconds. This confirms a helper-side blocked sample during the pselect
window, but it does not show that the waiter blocked or explain why the helper
was waiting.

CPU placement also differed from the preceding 75-microsecond run. This run
reported waiter CPU 0, observer CPU 1, helper CPU 4, and allowed mask `0x3f`;
the preceding run reported waiter CPU 6, observer CPU 0, helper CPU 5, and
allowed mask `0x6f`. Therefore the two runs do not isolate sampling cadence
from CPU placement. A very short waiter-state change between polls also
cannot be ruled out, but the earlier every-fourth-poll gap is no longer a
good explanation for a sustained blocked interval.

The COW-only guard stopped at `readiness-cow cfi miss`; canary mutation was
zero, and the final summary reported `done=0`, `root=0`, and no physical
read/write. RMG returned code 255. The phone stayed on the same Android 16
fingerprint and 5.4.289 kernel, remained ADB-responsive as UID 2000, and had
no `kernelsu` module, `/dev/ksu`, or `/proc/ksu`. RMG verified and staged its
KernelSU service before the attempt but did not load it.

Evidence is in `build/cow-mprotect-delay75-every-poll-20261128/`:
`rmg-ui-log.txt` (SHA-256
`5A1E2922814DD05BCDC9F2F3D034840403207BC6597FE09D2FAD2A3DDD520691`),
`result-ui.xml` (SHA-256
`64D9610F21F83210B1FE31E963E7B7E4FC5BC95CFA30C41130D296613FB95300`), and
`result-screen.png` (SHA-256
`C039D339A95BB9536327DE802173BC3CEE6AF5D693341D2A9C9C382AFA0F9724`).

Before another phone attempt, review why the task's allowed CPU mask and
selected waiter/helper cores changed between these two runs. Then hold the
placement constant so a future timing comparison tests one variable at a
time. Keep the payload COW-only and one-shot until a waiter `D` sample is
observed.
