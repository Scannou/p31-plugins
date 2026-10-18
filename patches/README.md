# Submodule patches

Fixes to third-party submodules that are not upstream yet. `scripts/bootstrap.sh`
applies every `patches/<submodule>/*.patch` to `libs/<submodule>` once (a patch
that already reverses cleanly is skipped). `.gitmodules` sets `ignore = dirty`
for patched submodules so the applied patch does not show as a change.

To update a patch: edit the files in `libs/<submodule>`, then
`git -C libs/<submodule> diff > patches/<submodule>/<name>.patch`.
When updating the submodule itself, re-run bootstrap and drop patches that
upstream has taken.

## clap-wrapper

### 0001-auv2-stage-parameter-changes-from-other-threads

Crash (EXC_BAD_ACCESS / bus error) in AUv2 builds of any plugin with parameters.
Hosts call `AudioUnitSetParameter` from any thread; `WrapAsAUV2::SetParameter`
appended straight to `ProcessAdapter::_events`, a `std::vector` the audio
thread sorts, hands to the plugin and clears in `process()` without a lock.
When the vector grew, `process()` read freed memory. pluginval's "Parameter
thread safety" test (strictness 10) hits it in roughly one run out of four.

The patch stages parameter changes from `addParameterEvent` in a preallocated,
per-parameter-coalesced list under a spin lock that writers hold only for a
short copy; `process()` moves them into `_events` with a try-lock, so the audio
thread never waits (busy lock: they move next cycle) and never reallocates.

Upstream: free-audio/clap-wrapper, affects v0.16.0 (latest at the time).

### 0002-auv2-guard-the-process-adapter-against-setparameter

Crash (EXC_BAD_ACCESS, `this == nullptr` in `addParameterEvent`) in AUv2 builds:
`WrapAsAUV2::SetParameter` checks `_processAdapter` and then calls into it, while
`deactivateCLAP()` (AudioUnitUninitialize, often on another thread) deletes it
in between. pluginval's "Parameter thread safety" test hits it in roughly one
run out of twenty, when queued `SetParameter` calls land after the plugin is
uninitialized.

The patch guards `_processAdapter`'s creation, deletion and use in
`SetParameter` with a spin lock held only around the pointer check and a short
copy. Rendering never takes it, and Uninitialize never overlaps a render.
