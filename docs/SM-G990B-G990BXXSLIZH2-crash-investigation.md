# SM-G990B G990BXXSLIZH2 follow-up crash investigation

This note records two runs of the locally instrumented RMG payload on the
Galaxy S21 FE 5G (`r9q`, `SM-G990B`). It supplements
[`SM-G990B-G990BXXSLIZH2.md`](SM-G990B-G990BXXSLIZH2.md), which documents a
separate payload build and earlier validation results. The first diagnostic
run below panicked; a later one-attempt rerun loaded the KernelSU module without
rebooting, but the `su` control command still failed. Do not treat this payload
as a fully validated root path until the missing control device is explained.

## Device and run

| Field | Value |
| --- | --- |
| Model | `SM-G990B` |
| Firmware fingerprint | `samsung/r9qxeea/r9q:16/BP2A.250605.031.A3/G990BXXSLIZH2:user/release-keys` |
| Android | 16 / SDK 36 |
| Kernel | `5.4.289-qgki-32192773-abG990BXXSLIZH2` |
| RMG run ID | `f25a8b44-6404-4a99-b564-e49a627edad6` |
| Run start | 2026-11-22 16:39 local time |
| Saved panic record | DropBox `SYSTEM_LAST_KMSG`, 2026-11-22 16:47:17 local time |
| Diagnostic artifact | `cve-2026-43499-app-pte2g-mmstats-v3.so` |
| Artifact SHA-256 | `E274FD6D27B420A9C18F80EE69F167F0BD5C95EED6D5F1760D13CA58221513DE` |
| Rerun start | Approximately 2026-11-22 18:00 local time |
| Rerun completion | 2026-11-22 18:08 local time; RMG run ID not captured |

## Outcome

The first attempt ended in a kernel panic and the phone rebooted. On reconnect,
the device reported `sys.boot_completed=1` and `sys.boot.reason=kernel_panic,null`.
The kernel version and firmware fingerprint were unchanged. The first run's
`su -c id` returned `/system/bin/sh: su: inaccessible or not found`.

The later rerun used the same candidate, with the RMG local-payload override
confirmed in Settings and the exact `r9q-G990BXXSLIZH2` profile selected. The
RMG Install screen reported **KernelSU active** and marked support check,
download, kernel exploit, and KernelSU load complete. The phone did not reboot;
`/proc/uptime` continued increasing during the run, and `sys.boot_completed`
remained `1`. Battery Saver was off (`settings get global low_power` returned
`0`; PowerManager reported `mSettingBatterySaverEnabled=false`). The on-screen
completion capture is [`images/SM-G990B-mmstats-v3-KernelSU-active-2026-11-22.png`](images/SM-G990B-mmstats-v3-KernelSU-active-2026-11-22.png).

The module is present in `/proc/modules` as `kernelsu 163840 ... Live`, and
`/data/adb/ksu/bin/su` exists as a symlink to `/data/adb/ksud`. However,
`/dev/ksu` is absent and invoking the staged `su` returned:

```text
Error: grant root through KernelSU control device
Caused by:
    Bad file descriptor (os error 9)
```

The app can report the module loaded from the module-list proof alone; this
does not establish that the KernelSU control channel or an interactive root
shell works. Therefore the rerun is a successful module-load result, but not
yet a verified usable-root result. No reboot or firmware partition write was
performed after completion.

### KernelSU Manager status after the rerun

KernelSU Manager was opened on the same boot. Its status card says “Not
supported” and explains that KernelSU currently supports only GKI kernels,
with image patching offered for GKI devices. It reports manager v3.3.0
(`32601-2`), kernel `5.4.289-qgki-32192773-abG990BXXSLIZH2`, the expected
G990B Android 16 fingerprint, SELinux permissive, and seccomp filter. The
screen also offers manager version `32680`; no manager update was started.
Screenshot: [`images/SM-G990B-KernelSU-manager-status-2026-11-22.png`](images/SM-G990B-KernelSU-manager-status-2026-11-22.png).

This Manager warning is separate from the RMG app's module-load reading:
`kernelsu` remains present in `/proc/modules`, while `/dev/ksu` is absent and
the staged `su` cannot obtain a root grant. The Manager's “Not supported” card
does not by itself prove the module is unloaded; the missing control device
and failed root grant remain the unresolved usability issue.

The user supplied a comparison screenshot from their Galaxy S25. Its Manager
reports **“Working [Jailbreak model]”** and **“LKM”**, with version v3.3.1
(`32680-5`, marked “Aangepast” / customized). The G990B's installed Manager is
v3.3.0 (`32601-2`) and reports “Not supported.” This confirms the expected
working/LKM presentation on the user's other device, but the S25 screenshot
comes from a different, customized Manager build and does not establish what
the G990B Manager should display. The version/build difference alone is not
enough to explain the status mismatch. Keep the findings distinct: RMG and
`/proc/modules` indicate that the G990B LKM loaded, whereas the missing
`/dev/ksu` and failed `su` grant show that its root control path is not usable.
The reason the S21 Manager does not show a working LKM state remains open.
Comparison image: [`images/S25-KernelSU-LKM-working-2026-11-22.jpg`](images/S25-KernelSU-LKM-working-2026-11-22.jpg).

### Driver-fd failure traced from the RMG run log

The completed RMG run log contains the direct late-load result:

```text
[+] Bootstrap root acquired
[+] KernelSU staged for the next boot
late-load: KernelSU driver fd unavailable
```

The final status also says root was obtained but the requested userspace
restart was rejected because Shizuku did not have root. This explains the
split result: the exploit obtained temporary bootstrap root, and RMG accepted
the loaded module-list entry as proof that `kernelsu` was present, but the
KernelSU helper never acquired a usable driver fd. The earlier “KernelSU
active” screen therefore did not establish a working `su` path.

On the live phone, `/proc/modules` lists `kernelsu`, but all of
`/sys/class/misc/ksu`, `/sys/devices/virtual/misc/ksu`, `/sys/module/kernelsu`,
and `/dev/ksu` are absent. The exact installed `/data/adb/ksud` SHA-256 matches
the repository helper (`bafc40f0166418ec7564578d9f181083b33db6ae9815095a3cf67eba7ba551e9`).
The corresponding module object imports both `misc_register` and
`misc_deregister`, so its binary contains the legacy misc-device registration
path. `CONFIG_DEVTMPFS` is disabled in the target kernel config, but the missing
sysfs misc entry shows this is not merely ueventd failing to create the
`/dev/ksu` node or set its permissions: the misc device was not registered (or
was removed) in this boot.

The current accessible kernel ring buffer did not retain a matching
`legacy /dev/ksu registration failed` or `control device registered` message.
The remaining unresolved point is why the module's registration call did not
leave a misc entry: an init-time registration error, a module init path that
did not reach `ksu_supercalls_init()`, or a different runtime module than the
one represented by the local helper are still possible. Do not rerun the
exploit just to collect that message. Prefer to capture the registration log
from a controlled future load or inspect the embedded module offline before
another device run.

### Follow-up diagnosis and local fixes

A later read-only check on the connected phone reconfirmed `kernelsu` in
`/proc/modules`, an existing but empty `/sys/class/misc`, no `ksu` line in
`/proc/misc`, and no `/sys/class/misc/ksu`, `/sys/devices/virtual/misc/ksu`,
or `/dev/ksu`. Filtering the accessible kernel log and Android logcat for the
registration messages returned no result. This confirms the current failure
is at or before misc-device registration, rather than only ueventd failing to
create the `/dev` node.

Source inspection found why RMG could display a misleading success: the
Samsung patch logs the return from `misc_register()` but does not propagate a
failure from `ksu_supercalls_init()` to `kernelsu_init()`. The module can
therefore remain listed as loaded when its control device was not registered.
The app's previous verdict treated that module-list entry alone as proof of a
working KernelSU control channel. In the local `work/rmg-next-source` checkout,
the verdict now treats the module list as load evidence only and reports
“module present, control unavailable” unless a live control probe succeeds.
The target helper source also now includes the `open()` errno in its
“driver fd unavailable” diagnostic so a future controlled load can distinguish
`ENOENT`, `EACCES`, and other failures. These are source changes only; neither
was built, pushed to GitHub, or installed on the phone.

The registration errno itself remains unknown because the current kernel log
does not retain it. A candidate fallback patch is now recorded as
`kernelsu/patches/KernelSU-v3.2.5-r9q-5.4-proc-fallback.patch`. If
`misc_register()` fails, it registers the same ioctl file operations at
`/proc/ksu`; KernelSU userspace and the RMG verification helper then try that
path after `/dev/ksu`. This avoids the reboot kprobe route that the Samsung
5.4 port intentionally disables because of reset protection. The exact target
`Module.symvers` exports `proc_create` and `proc_remove`, so those calls are
available in the recovered target symbol list.

Before the Samsung source archive became available, no candidate had been
compiled or loaded. The public
`Ryujin9610/android_kernel_samsung_sm8350` `ksu` branch currently points to
`99873ef94c646d591b73446be9d2ad61a859694b`; its top-level Makefile sets Linux
`5.4.302`, while this phone runs
`5.4.289-qgki-32192773-abG990BXXSLIZH2`. The repository also has
`permissive` (`698aba15903f89c68337bfca53ee2fca5521c53c`) and `twrp-12.1`
(`fc8a3a77dcbb8b72873aaf0f51d6f40173ae51f3`) branches whose Makefiles say
`5.4.289`, but their latest commits are from November 2025 and do not establish
that they are the source for this 2026 firmware. The permissive branch's R9Q
defconfig is a custom permissive build and differs from the recovered live
config in security and vendor options. Neither branch is the exact source for
this firmware; the matching Samsung OSRC release archive was later obtained
and used for the candidate build below.

### Exact-target offline build

The downloaded `SM-G990B.zip` contains both the generic source archive
`SM-G990B_16_Opensource.zip` and the release update
`SM-G990B_16_Opensource_G990BXXSLIZH2_G990B2XXSKIZH2.zip`. The generic archive
provides `Kernel.tar.gz`; the release archive provides
`G990BXXSLIZH2_kernel.tar.gz`. Samsung's release note says to unpack the
`G990BXXUGIYJ1` base kernel source first, then overlay the `G990BXXSLIZH2`
update. This was done in WSL at `/home/mick/src/sm-g990b-lizh2-osrc`; its
Makefile reports Linux `5.4.289`.

The candidate used the saved target `.config` and exact
`G990BXXSLIZH2.Module.symvers.gz`, plus the official Android Clang
`r383902b1` release whose reported compiler string matches the config's
`clang version 11.0.2` and compiler commit. The prepared tree computes
`5.4.289-qgki-32192773-abG990BXXSLIZH2`, exactly matching the phone's reported
kernel release.

The first compile exposed Samsung SELinux policy structures that differ from
the newer KernelSU policy mutation code. Building with
`CONFIG_KSU_LEGACY_SELINUX=y` selects the port's legacy SELinux stub and lets
the module compile. That stub explicitly disables KernelSU SELinux policy
mutation and says permissive mode is required; this is a material runtime
limitation and was not tested on the phone. Samsung KDP support was enabled.

The kernel object built after setting `KBUILD_MODPOST_WARN=1`. It has 23
undefined imports absent from the target `Module.symvers`, versus 21 in the
older local module. This matches the custom loading path in `ksuinit`: it reads
`/proc/kallsyms`, relocates undefined symbols in the ELF, and then calls
`init_module`. The loader temporarily writes `1` to
`/proc/sys/kernel/kptr_restrict` while reading symbols, then attempts to
restore its prior value. All 23 of the candidate's otherwise-unexported
symbols appear in the recovered exact-firmware kernel ELF, and the target
config has `CONFIG_KALLSYMS_ALL=y`. This is static evidence that the custom
resolver has names to look up; live `/proc/kallsyms` visibility and successful
runtime relocation remain unverified. A normal `insmod` load is not
established. The candidate's module `vermagic` matches the exact firmware
release.

The paired `ksud` was built for `aarch64-linux-android` with Android NDK r27c,
API 21. Its `android16-5.4_kernelsu.ko` asset is the candidate module above,
and its userspace includes the `/proc/ksu` fallback path. The first userspace
build did not yet embed the module; the recorded artifact is from the clean
rebuild after adding the correctly named KMI asset.

The patched `ksud` checks whether any KernelSU module is already loaded and
skips loading its embedded candidate if so. The last recorded phone state had
the previous `kernelsu` module present, so a future candidate attempt would
need a fresh boot or a separately validated unload. The phone state has not
been rechecked, and neither action was taken here.

Artifacts are saved as candidates in `kernelsu/`:

- `android16-5.4_kernelsu-r9q-G990BXXSLIZH2-kdp-proc-fallback-candidate.ko`
  SHA-256: `e42b03a7403741a586c66efb3b4a25c0e63852ad911ba54afb7778425c158fd3`
- `ksud-r9q-G990BXXSLIZH2-kdp-proc-fallback-candidate`
  SHA-256: `1eae042ffbd70848af8ec3cad8b138b70cce14f8176b88dcb435224b4256ec77`

These are compile outputs only. They have not been installed, loaded, or run
on the phone, and no exploit, reboot, or firmware write was performed during
this follow-up.

No firmware partitions were flashed. The panic was investigated from the
saved Android DropBox kernel log. Access to `/sys/fs/pstore` and
`/proc/last_kmsg` from the unprivileged shell had been denied, but the saved
DropBox `SYSTEM_LAST_KMSG` entry was readable through `dumpsys dropbox`.

## Panic evidence

The saved kernel log reported:

```text
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000001
Internal error: Oops: 96000005 [#1] PREEMPT SMP
CPU: 1 PID: 4063 Comm: sh
pc : _raw_spin_trylock+0x1c/0xac
lr : rt_mutex_adjust_prio_chain+0x108/0x890
```

The relevant call trace was:

```text
_raw_spin_trylock+0x1c/0xac
rt_mutex_adjust_prio_chain+0x108/0x890
remove_waiter+0x234/0x270
rt_mutex_cleanup_proxy_lock+0x50/0x9c
futex_lock_pi+0x5d0/0x7b0
do_futex+0xb4/0x1ac
__arm64_sys_futex+0xc8/0x16c
```

The fault address was `0x1`; the register dump also showed `x0=0x1` at the
spinlock attempt. This establishes that the failure occurred in the kernel's
priority-inheritance futex cleanup path while it tried to use an invalid
spinlock address. The instruction-level analysis below identifies the
corrupted waiter field; the saved log alone does not identify which userspace
`FUTEX_LOCK_PI` call led there.

### Exact kernel instruction analysis

The Desktop AP archive was found at
`C:\Users\admin\Desktop\SAMFW.COM_SM-G990B_EUX_G990BXXSLIZH2_fac` (the
directory name has no separator between `SAMFW` and `COM`). Its
`boot.img.lz4` was extracted and decompressed locally. The resulting kernel
Image SHA-256 is
`65345CD2969F22A10749F4B5B31ED631954A749C3CAD53292C5ED7D116484C40`, matching
the existing `work/firmware-analysis/kernel.Image`. The recovered ELF in
`work/firmware-analysis/kernel-recovered.elf` contains the relevant symbols
and can be disassembled at the exact fault addresses.

The ELF confirms the bad value came from the PI-blocked waiter of the task
being walked by `rt_mutex_adjust_prio_chain()`:

```text
rt_mutex_adjust_prio_chain:
  ldr x25, [x19, #0x900]     // task argument's pi_blocked_on
  ldr x8,  [x25, #0x38]     // pi_blocked_on->lock
  cmp x23, x8
  ...
  mov x0, x23
  bl  _raw_spin_trylock

_raw_spin_trylock+0x1c:
  ldr w8, [x0]
```

The task offset `0x900` and waiter lock offset `0x38` match this target's
profile. The crash's `x0=1` and fault address `1` therefore identify the
specific invalid value: at the time of the fault,
`task_argument->pi_blocked_on->lock == 1`. The kernel tried to read the
spinlock word at address `0x1`. The `task` argument is not necessarily
`current`: in the `remove_waiter()` path, the first argument to
`rt_mutex_adjust_prio_chain()` is the PI lock's owner task. The `Comm: sh`
line names the thread that faulted, not necessarily the owner whose PI chain
contained the invalid waiter. The panic register dump gives the chain-walk
task argument as `x19=0xffffff8082dfaf40`; without a vmcore or memory snapshot,
that address cannot be mapped back to a task name or TID.

The panic registers place `SP` at `0xffffffc03f96bb40` and the loaded
`pi_blocked_on` waiter at `0xffffffc03f963cd8`, a difference of `0x7e68`.
The exact firmware config has `CONFIG_VMAP_STACK=y`, 4 KiB pages, and no
KASAN. The repo's [arm64 `memory.h`](https://github.com/Ryujin9610/android_kernel_samsung_sm8350/blob/vanilla/arch/arm64/include/asm/memory.h)
defines 16 KiB task stacks and 32 KiB VMAP stack alignment for that config.
The two addresses fall in separate aligned stack regions.
Because the chain walker follows the lock owner's waiter, that separation is
consistent with the owner being a different task from the faulting thread; it
does not by itself prove the pointer was stale or outside a valid stack.

The backtrace's `futex_lock_pi+0x5d0` return address also matches the
disassembly: `futex_lock_pi` calls `rt_mutex_cleanup_proxy_lock` at `+0x5cc`,
then resumes at `+0x5d0`. This is the cleanup path taken after a failed or
timed-out PI lock. It is consistent with the sanitizer hypothesis below.

## Payload progress before the panic

The run log showed that the corrected static physical image base was used:
`kernel_phys_load=0xa0080000`, a `0x20080000` delta from the earlier
`0x80080000` value. KASLR setup completed and the log reached the controlled
order-3 allocation/reclaim phase:

```text
controlled order-3 UNMOVABLE seed sends=128/128
controlled mm group attempt=32 base=ffffff852b518000 slot=20 count=1/34
```

The device reset before the collection returned, so the added scan/leak/group
counters were not printed. The RMG run record remained marked `Running` because
the device reset interrupted the app-side run record.

## Source correlation

The target profile defines `rt_mutex_waiter` fields including
`FAKE_WAITER_LOCK_OFF=0x38`, and documents assumptions about the reclaimed
waiter location and readiness-bitmap spray in
[`target.h`](../src/targets/r9q-G990BXXSLIZH2/target.h). The route implementation
and register/stack overlay are in
[`fops.c`](../src/targets/r9q-G990BXXSLIZH2/port/fops.c). The observed fault is
therefore confirmed to reach PI-chain cleanup with a waiter lock value of 1.
The payload source's precise runtime state at the crash remains unconfirmed.

`route_pi_sanitize()` remains a possible call site because it uses a timed
`FUTEX_LOCK_PI`, and the trace is in the timeout cleanup path. However, the
owner-walk semantics weaken the earlier hypothesis that it directly exposed
the faulting thread's own waiter:

1. `run_main_route_threads()` starts `sanitize_holder_thread()`, which acquires
   `f_pi_sanitize` and holds it until the route is marked done.
2. Saved same-profile RMG logs show `readiness_pause=on`; in that mode the
   `SIGUSR1` handler runs `do_readiness_pause_route()`.
3. After the handler returns, `waiter_thread()` sets `route_done` and then
   calls `route_pi_sanitize()`. The controller releases the helper when it
   observes `route_done`; other run logs show the sanitizer lock timing out
   with a 50 ms deadline.
4. A timed-out PI-futex waiter is consistent with the saved
   `futex_lock_pi -> rt_mutex_cleanup_proxy_lock -> remove_waiter` stack.
   `remove_waiter()` clears the faulting thread's own `pi_blocked_on`, then
   adjusts the PI lock owner. The faulting chain therefore requires the owner
   task's `pi_blocked_on->lock` to be `1` (or an equivalent chain-walk state
   that reads `1`).

This explains why the last visible `controlled mm group` line is not
necessarily the crashing operation: the controller thread handles the pipe
preparation request and can log the reclaim scan concurrently with the signal
handler and other PI-futex activity. The kernel evidence confirms an owner
task's `pi_blocked_on->lock` value was `1`. It does not establish that
`route_pi_sanitize()` was the userspace caller, or identify which route wrote
`1` into the owner waiter's lock field. The sanitizer helper normally holds
its own PI futex without blocking on another lock, so its owner would ordinarily
have no PI chain; reproducing this exact chain through the sanitizer would
require additional state corruption that has not been demonstrated.

Other saved same-profile RMG runs show the sanitizer call returning
`lock=-1 errno=110` (timeout), followed by `unlock=-1 errno=1` (not owner).
That establishes the timed-out path is expected in this route; timeout by
itself is not enough to explain the panic. The distinguishing condition in
this run is the invalid owner waiter lock observed by the kernel.

There is also a source/config discrepancy to resolve: the readiness overlay
comments in `fops.c` describe the relevant 5.4 path as having no `VMAP_STACK`,
while the exact firmware config enables it. VMAP stack alignment does not by
itself change a function's frame size, but it changes stack allocation and
interrupt-stack assumptions. Revalidate the overlay/IRQ rationale against the
exact kernel before relying on those comments.

## Current limits and next offline checks

- The exact `FUTEX_LOCK_PI` caller is not recoverable from the saved kernel
  trace alone. The complete RMG stdout/stderr for run
  `f25a8b44-6404-4a99-b564-e49a627edad6` would help establish which stage was
  active immediately before the reset.
- The owner task is identified only by a register-held `task_struct` address;
  the saved log has no memory snapshot of that task or its waiter. It therefore
  cannot show which overlay, spray phase, or other PI operation wrote `1`.
- Next analysis should identify which task owns the futex being cleaned up,
  correlate the exact run's final messages with that owner's PI chain, and
  revalidate stack/IRQ geometry with `CONFIG_VMAP_STACK=y`. Do not treat the
  current diagnostic artifact as safe to repeat based on this diagnosis.

## 2026-11-22: candidate run on RMG Extended

After the phone's swipe-only keyguard was dismissed through Android's normal
`wm dismiss-keyguard` path, the separate RMG Extended app was used (package
`dev.rushiranpise.rmgnext`, version `0.13+local.af081c2`, code `28063401`).
Its payload-source editor already had
`Ryujin9610/Root-My-Galaxy-Payloads @ r9q-g990b-lizh2` enabled. The app verified
that source at commit `2976ceb3e5a1e351c448b6948522d2e7f7440685` and reported
`Matches this device: r9q-G990BXXSLIZH2`. The selected catalog entry was
`CVE-2026-43499`, shown as `Galaxy S21 FE 5G (SM-G990B) | Kernel 5.4.289`,
`Exact kernel match`, with the source label above. This resolves the earlier
confusion with the other installed RMG package (`dev.busung.s25uroot`), whose
settings screen did not expose the payload-source manager.

RMG Extended honored its imported-exploit override and staged the phone's local
`cve-2026-43499-app-pte2g-mmstats-v3.so`. The staged file and
`files/local-payload/local-exploit.so` had the same SHA-256:
`e274fd6d27b420a9c18f80ee69f167f0bd5c95eed6d5f1760d13ca58221513de` (140,592
bytes). The exploit listed in the manifest at source commit `2976ceb` was not
the file run: it was 134,072 bytes with SHA-256
`f0a2b39c3d3a0e05beccd839f3e2af2417ca7f60e849b4e5e6bee898688b437b`. The
modified artifact in the local repository worktree was also different: 135,312
bytes with SHA-256
`daf8a8380b31f9d4eBC81F7A713D8E3A6EFEA9C4CAD99B879F7ACD7099A6DC87`.
Therefore this run tested the phone's imported v3 build, not the branch's
manifest artifact or the current worktree artifact. The history records
`KernelSU service verified` and `Payload download complete`; the KernelSU
service was downloaded and verified. RMG then executed one independent
exploit attempt (`attempts=1/1`) using Shizuku. The startup context was
ordinary shell (`uid=euid=2000`, SELinux `u:r:shell:s0`, enforcing), and the
build label was `r9q-G990BXXSLIZH2-diag-pte2g-once`. KASLR discovery succeeded
through tracefs with slide `0x1e8000` and the corrected physical image delta
`0x20080000`.

The controlled reclaim scan completed with
`scans=192 leak_miss=0 accepted=43 invalid=0 overflow=0 duplicate=0`,
`groups=2`, and `best=34/34`. Spectrum setup succeeded at 512 and 4096
entries but failed at 1024, 2048, 8192, 16384, and 32768 with `errno=12`
(`ENOMEM`); the exploit continued with two spectrum entries. The first
readiness-cow path reported a CFI miss at step 33, then its overlay attempt
reported `ret=118`, `mprotect=0/0`, `blocked=0`, and `overlay=1`. The PI
readiness canary had `gate=1`, but the subsequent live snapshot had
`live_root=0` and no detected root transition.

The fallback pipe physical-read/write path did not find a matching reclaimed
pipe page. It logged `pipe physrw attempt=1/1 failed`, followed by
`pipe-physrw-summary ... done=0 root=0 kaslr=1`, `read_ok=0`, `write_ok=0`,
and `uid=4294967295->4294967295`. The install-history record
`c5ca4609-22c3-4d5c-b407-4daa1682a3ea.json` ends with:

```text
[-] stability keeper pid=27025 retaining dirty fops page; reboot required
[-] exploit attempt=1/1 failed status=1
[!] exploit failed after 1 independent attempts
[-] The payload failed with code 255 — the command could not be run:
    a refused or unavailable privilege, or nothing to execute
[*] Failed at: Running kernel exploit
failureStage: Exploit
```

The phone remained ADB-accessible and completed a reboot after this result.
After reboot it reported the same Android 16 firmware and
`5.4.289-qgki-32192773-abG990BXXSLIZH2`; the shell still had uid 2000 and
`/dev/ksu` was absent. No KernelSU load was attempted after the failed exploit.
Do not retry this candidate without first resolving the pipe physical-read/
write discovery failure and reviewing the new readiness/overlay trace. The
`errno=12` spectrum allocation failures are additional evidence to account for
in any next diagnostic build.

