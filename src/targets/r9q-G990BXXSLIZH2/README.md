# SM-G990B G990BXXSLIZH2 source set

This directory contains the target-specific source for the
`SM-G990B` / `G990BXXSLIZH2` payload. The baseline pipe route was
hardware-proven. The live-offset cache-table change was exercised once but
did not reach physical read/write. The current page-reclaim snapshots are a
diagnostic candidate and have not been run on the phone.

Samsung's 5.4 kernel requires target-specific compiler stack geometry for the
legacy `rt_mutex_waiter` overlay. The proven route uses a pselect result-copy
COW fault and `overlay_asm.S`; newer shared-engine changes must not silently
alter those frames. For that reason the Makefile selects the frozen `port/`
source set only for this target. The app payload also fixes the runtime to the
phone-validated controlled reclaim and readiness-pause route, with one
900-second attempt per boot. After the FOPS swap, the app gets three chances
to take over the pipe page. The old drain arrays are released while the
captured AF_UNIX probe still pins the target; the probe is then drained last,
so reclaim growth reaches the target after reusing the old slabs. A failed
takeover keeps the fake FOPS page alive until reboot instead of releasing a
stale kernel function table.

Build with NDK r27b:

```sh
ANDROID_NDK_HOME=/path/to/android-ndk-r27b \
  make TARGET=r9q-G990BXXSLIZH2
```

The target automatically uses API 30 for the exploit and API 35 for the
generic Root My Galaxy helper. The current working-tree build, including the
experimental pipe-cache gate and staged page-metadata diagnostics, produces:

```text
415dbca2abb4e3df8f4f90b637a9a8b5d4992b1a6d956d9545d424f4d2239fa5  cve-2026-43499-app.so
fd193ddecbc5db5d13e72281cf54ce3ae054e1b3d8728c1dfc64ff1fc80d0bb8  cve-2026-43499-root
```

The full hardware record and KernelSU details are in
[`../../../docs/SM-G990B-G990BXXSLIZH2.md`](../../../docs/SM-G990B-G990BXXSLIZH2.md).
