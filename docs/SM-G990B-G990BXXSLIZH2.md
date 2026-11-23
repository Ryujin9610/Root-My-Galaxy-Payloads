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
