# MaterialX CUDA GPU validation release blocker

Status: blocked; do not report canonical GPU validation as passed.

## What is verified

* The current MaterialX source was configured in the isolated build directory
  `C:\tmp\blender-materialx-core-current-gpu-build` with CUDA 12.8,
  `WITH_CYCLES_DEVICE_CUDA=ON`, `WITH_CYCLES_CUDA_BINARIES=ON`, and `sm_89`.
* The current-source CUDA kernel was produced at
  `intern\cycles\kernel\device\cuda\kernel_sm_89.cubin.zst` (2,961,620 bytes).
* The current-source `blender.exe` was linked.
* The canonical CPU Blender test group passed 3/3: authority, ramp authority,
  and the MaterialX utility battery.
* The two install prerequisites that were initially absent have been built:
  `bf_intern_draco_bridge.dll` and `bf_intern_meshopt_bridge.dll`.

## Blocking behavior

The isolated current-source CUDA runtime does not provide a stable process in
which to execute the canonical GPU fixtures. A bounded
`blender.exe --background --factory-startup --version` probe is intermittent:
some probes exit silently before five seconds and others remain blocked for
more than ten seconds. This occurs before any MaterialX scene or CUDA render
is run.

The evidence rules out the original missing-DLL/SxS hypothesis:

* the executable, CRT, and shared-assembly manifests are byte-identical to a
  prior CUDA runtime that enumerates the device;
* module inspection of a blocked owned process shows that Blender's direct
  imports are loaded, including `blender_cpu_check`, MaterialX, USD, OSL,
  OpenImageIO, Embree, Python, TBB, and the Windows Common Controls assembly;
* blocked processes are low-CPU post-load waits, rather than loader failures.

The normal CMake install reaches its final optional Windows shell extension
(`BlendThumb.dll`). This does not explain the post-load hang. Use
`--component Blender` when installing the executable itself; the default
`Unspecified` component does not install it.

## Required next diagnostic

Capture a user-mode stack from a freshly launched, owned isolated runtime PID
while it is in the blocked state. The local machine has Visual Studio Build
Tools only; it has no CDB, WinDbg, ProcDump, Process Explorer, or Visual Studio
IDE debugger. The built-in `comsvcs.dll` MiniDump entry point did not emit a
dump artifact in this environment.

Provide one of the following without changing the source or CUDA settings:

* WinDbg/CDB: attach to the owned PID and run `~* kb`;
* ProcDump: capture a full dump of the owned PID;
* Visual Studio IDE: Attach to Process and capture all thread stacks.

Only after the hang is diagnosed and a current-source runtime starts normally
may the CUDA authority, ramp, and utility fixtures run. Acceptance requires
their expected non-decoy MaterialX pixels and the tampered-authority black
result; CUDA device enumeration alone is insufficient.
