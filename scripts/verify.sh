#!/usr/bin/env bash
# The one verification command for this repo, mirroring CI's "Build Tests" and "Run Tests" steps.
#
#   bash scripts/verify.sh           full run: static checks, configure, build DistortionTests, run it
#   bash scripts/verify.sh --quick   SessionStart subset: static checks only, silent when clean
#
# Exit 0 when everything passes, non-zero otherwise.
# BUILD_TYPE (default Debug) and BUILD_DIR (default build) override the CMake settings.
# On Windows the script finds MSVC itself (vcvars64.bat under Visual Studio); nothing to source first.

set -u
cd "$(dirname "$0")/.." || exit 1

BUILD_TYPE="${BUILD_TYPE:-Debug}"
BUILD_DIR="${BUILD_DIR:-build}"
fail=0
quick=0
[ "${1:-}" = "--quick" ] && quick=1

bad() { echo "FAIL: $*" >&2; fail=1; }

# Static checks, a few seconds, no network.
for f in scripts/*.sh installer/macos/*.sh; do
  [ -f "$f" ] && { bash -n "$f" 2>/dev/null || bad "$f has a shell syntax error"; }
done
py=""
# Windows ships a python3 stub that exits non-zero, so test that the interpreter runs.
for c in python3 python; do
  if command -v "$c" >/dev/null 2>&1 && "$c" -c "" >/dev/null 2>&1; then py="$c"; break; fi
done
if [ -n "$py" ]; then
  for f in scripts/*.py; do
    [ -f "$f" ] && { "$py" -m py_compile "$f" 2>/dev/null || bad "$f does not compile"; }
  done
  find scripts -name __pycache__ -type d -prune -exec rm -rf {} + 2>/dev/null
fi
# CI derives the dev version from this line; if it stops matching, every build job fails.
sed -n 's/^project(Distortion VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt | grep -q . \
  || bad "CMakeLists.txt: no 'project(Distortion VERSION x.y.z' line (CI parses it)"
for f in LICENSE THIRD-PARTY-NOTICES.md; do
  [ -f "$f" ] || bad "$f is missing (packaging steps fail without it)"
done

if [ "$quick" = 1 ]; then exit "$fail"; fi
[ "$fail" = 0 ] || exit 1

# Full run.
run_build() {
  cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" || return 1
  cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --target DistortionTests -j || return 1
}

exe="$BUILD_DIR/DistortionTests_artefacts/$BUILD_TYPE/DistortionTests"
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    exe="$exe.exe"
    if command -v cl >/dev/null 2>&1; then
      run_build || { bad "build failed"; exit 1; }
    else
      vcvars=$(ls -d "/c/Program Files (x86)/Microsoft Visual Studio"/*/*/VC/Auxiliary/Build/vcvars64.bat \
                      "/c/Program Files/Microsoft Visual Studio"/*/*/VC/Auxiliary/Build/vcvars64.bat 2>/dev/null | tail -1)
      [ -n "$vcvars" ] || { bad "no MSVC found: install Visual Studio Build Tools (C++ workload)"; exit 1; }
      # A batch file sidesteps the quoting cmd //c needs for a path with spaces.
      bat="$(mktemp -u).bat"
      {
        printf '@echo off\r\n'
        printf 'call "%s" >nul || exit /b 1\r\n' "$(cygpath -w "$vcvars")"
        printf 'cmake -B %s -DCMAKE_BUILD_TYPE=%s || exit /b 1\r\n' "$BUILD_DIR" "$BUILD_TYPE"
        printf 'cmake --build %s --config %s --target DistortionTests -j\r\n' "$BUILD_DIR" "$BUILD_TYPE"
      } > "$bat"
      cmd //c "$(cygpath -w "$bat")" || { rm -f "$bat"; bad "build failed"; exit 1; }
      rm -f "$bat"
    fi
    ;;
  Linux)
    # CI runs the tests under xvfb; see .github/workflows/build.yml for the apt packages.
    run_build || { bad "build failed"; exit 1; }
    if [ -z "${DISPLAY:-}" ] && command -v xvfb-run >/dev/null 2>&1; then
      xvfb-run -a "$exe" || { bad "DistortionTests failed"; exit 1; }
      exit "$fail"
    fi
    ;;
  *)
    run_build || { bad "build failed"; exit 1; }
    ;;
esac

[ -x "$exe" ] || { bad "$exe was not built"; exit 1; }
"$exe" || { bad "DistortionTests failed"; exit 1; }
exit "$fail"
