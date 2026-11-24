# Mutation runner: fast execution, unchanged failure gates

`python3 tools/mutate.py` runs **every registered mutation**. Optional `tests`
ownership in `tools/mutations.json` selects explicit unittest methods/classes;
it is not a cached result. Each mutation gets a fresh worker. Fail-fast stops
only that mutation's test invocation after detection, not the campaign.

## Commands

```bash
python3 tools/mutate.py --check                 # registry/schema/needle drift only
python3 tools/mutate.py --list                  # no source edits or tests
python3 tools/mutate.py --report /tmp/mutations.json
python3 tools/mutate.py --full-suite --no-failfast --report /tmp/mutations-full.json
python3 tools/mutate.py btg-29       # example: use an exact id from --list
python3 tools/check_all.py --no-cache           # all normal checks, no geometry-cache reuse
```

Use an id actually listed by `--list`; unknown ids/options fail before a sweep.
`--help`, `--list` and `--check` never start a mutation campaign. `--timeout`
bounds each invocation (default 120 seconds). The normal build/check/release
policy is unchanged: build and `check_all.py` before each commit, a full
registered mutation campaign for behavior changes, and a separate release marker.

## Detection and restoration contract

- The complete **mutation test suite** (Windows-app tests, Notes tests and runner
  safety/settings and focused Wave print-safeguard regressions) must pass on
  clean sources before and after a campaign. Wave requires the normal
  `requirements-dev.txt` numpy/Shapely dependencies (use the configured venv).
  This does not replace `check_all.py` or native/hardware acceptance.
- A missing route runs that whole suite. A passing targeted invocation must
  fall back to that whole suite. A bad selector, worker crash, timeout, skipped
  test, or infrastructure exception is an error, never a detection. Errors
  abort the remaining work and fail the campaign; survivors and stale entries
  also fail it. `--full-suite` bypasses all routing for independent audits.
- Actual test/subtest assertions and component errors record the loadable owner
  selector. Fixture failures can legitimately run zero test methods. Missing
  files/tools/imports, memory errors and subprocess exceptions are classified
  conservatively as infrastructure rather than mutation evidence.
- The applicability/needle check still validates every entry on clean sources.
  During a deliberate break it excludes only the active **source file** from
  its needle assertions; the runner's replacement cannot itself kill a mutant.
- A nonblocking checkout lock covers validation, byte snapshots, writes,
  restoration and postflight. Different campaigns cannot mutate one checkout
  concurrently. Do not edit source or run other source-writing tools during a
  campaign. Source bytes (including CRLF) are restored in `finally` blocks,
  verified against all snapshots, and failure is never reported as restored.
- SIGINT and normal POSIX SIGTERM unwind restoration. Timed-out workers and
  descendants are terminated as a process group on POSIX (`taskkill /T /F` on
  Windows). Forced SIGKILL/power loss cannot execute Python cleanup; inspect and
  restore the interrupted source before rerunning. No Windows cleanup claim
  substitutes for native testing.
- A fresh Python bytecode prefix plus `-B` prevents equal-size, same-timestamp
  source mutations from reading stale `.pyc` files. No mutation verdicts or
  compiled harnesses are cached. Existing Windows-build and normal geometry
  check caches remain separate and unchanged.

## Timing and diagnosis

By default, an atomic JSON report is written outside Git under
`~/.cache/tests_mutate/<checkout-hash>-latest.json`; `--report` overrides it.
It records registry/selected/executed counts, detection/survival/stale/error
counts, clean gates, source-restoration state, per-attempt scope/selectors,
return codes, failing owners, per-test and wall timings, registry and mutated-
source fingerprints, Python version, worker count and disabled-cache status.
The source fingerprint covers registered mutation source files, not every
repository dependency; it is diagnostic, **not** a compiled-artifact cache key.
Worker/infrastructure diagnostics are included on errors. `check_all.py` also
prints task elapsed times and its existing geometry-cache hit/miss indication.
Keep generated full reports, source copies and compiler caches outside Git.

When adding a mutation, retain its existing definition and add a relevant
explicit `tests` owner where known. The clean suite checks that registered
owners resolve; a full campaign proves they actually detect the deliberate
break. Do not delete a survivor or weaken fallback/normal checks to gain speed.
Any future compiled-artifact reuse needs complete source/harness/compiler/
flags/dependency keys and must still execute the tests. Parallel mutation work
requires isolated source copies and measured benefit; never parallelize writes
to the shared checkout.

## PLAN-02 benchmark (2026-11-16)

Host: 2 CPUs, about 3.9 GiB RAM, Python 3.11.2, Debian GCC 12.2.0.
App-source snapshot: `b40cbab10bc33654e3b8b1431f54ad2633348748`.
All 335 mutation definitions/ids are unchanged; the registry adds 78 explicit
owners. Both reference copies include the active-source applicability correction.
The reference uses the original 137 Windows/Notes tests, one serial worker,
no fail-fast and no routing. Its 1,196.516-second campaign includes postflight;
adding the separately measured 3.394-second preflight normalizes both gates.
The optimized comparison uses identical app sources/tests/toolchain, one worker,
fail-fast/routing, and no verdict or compiled-harness cache. Isolated copies use
Git history from the checkout via explicit GIT_DIR/GIT_WORK_TREE, not a different
branch. Copies must stay outside the repo to avoid recursive script discovery.

| Campaign | Wall seconds, including clean pre/post | Outcomes |
| --- | ---: | --- |
| Serial full-suite reference, original 137 tests | 1199.910 | 329 detected, 6 survived |
| Routed/fail-fast, identical 137-test scope | 147.385 | Same verdict for **every one of 335** ids |
| Final expanded suite, 169 tests at each clean gate | 132.622 | **335 detected, 0 survived/stale/errors**, bytes restored |

Matched-scope speedup: **8.14x / 87.72% less time**. The expanded campaign took
2m13s, below the initial five-minute goal on this host. These are measured command
wall times, not end-to-end response guarantees or cold/sandbox-reset promises.
A separate uncached normal `check_all.py` run also passed both dependency states.
Per-test/subprocess timings can overlap in normal concurrent checks; do not sum
them as response latency.

The six reference survivors are a correctness finding, not accepted production
survivors: the old runner counted its own changed-needle check as detection.
New property coverage catches `ntui-06` (Notes palette initialization), `wx-13`
(stale geocoding ticket), `emblem-00` (canonical BGR adapter), `wr-alert-02`
(stale forecast side effects), `trayhub-02` (Notes wait relay), and `set-13`
(Hub panel preference persistence/rollback). Helpers execute production C where
portable; call-order contracts remain static checks, not Windows acceptance.
The original 137 tests are retained, plus 6 property and 26 runner-safety tests.
All final 335 targeted invocations detected their mutants; passing-target
fallback, unmapped/full mode, bad configuration, timeout/descendant cleanup,
subtests, stale bytecode, lock ordering and interruption/restoration are also
covered by regression tests. No app code, EXE, version or native acceptance claim
is changed by this tooling task.

Harness caching and two/four isolated mutation workers are **deferred**, not
claimed implemented or benchmarked: the simpler one-worker first slice already
meets the goal. Revisit only with a measured remaining bottleneck, complete cache
keys and identical per-id outcomes. Existing build and geometry caches are not
replaced. Full machine-readable benchmark reports stay in the external cache;
this compact record preserves the evidence without committing generated data.
