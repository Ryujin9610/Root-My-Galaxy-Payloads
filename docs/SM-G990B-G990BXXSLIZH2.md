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
call site without repeating allocator reclaim on the same boot. No further
phone attempt has been made.

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
