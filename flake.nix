# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
{
  description = "dirtoo — modular Qt file manager (C++23)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        lib = pkgs.lib;
        versionBase = lib.strings.removeSuffix "\n" (builtins.readFile ./VERSION);
        # Flake source attrs (Nix):
        #   shortRev       — clean tree only
        #   dirtyShortRev  — dirty tree (short hash, often with -dirty suffix)
        #   revCount       — commit count to HEAD (no separate dirtyRevCount)
        # Prefer dirtyShortRev first so uncommitted work never falls through to
        # the literal "dirty" token (which became the useless +gdirty label).
        gitRev = self.dirtyShortRev or self.shortRev or "unknown";
        # SemVer-ish: 0.2.0-dev.1509+g2fdf60f  (VERSION + .revCount + +g shortRev)
        revCount = toString (self.revCount or 0);
        version = "${versionBase}.${revCount}+g${gitRev}";
        versionFlag = "-DPROJECT_VERSION_FULL=${version}";

        # RelWithDebInfo: optimised but keeps symbols for gdb/backtraces.
        cmakeBuildType = "RelWithDebInfo";

        # --- Scoped sources -------------------------------------------------
        # Each package only includes the paths it needs. Changing apps/dirtoo
        # must NOT rebuild dirops / dirtoo-fs / … (same for other libs).
        # Layout under the source root stays the same (VERSION, libs/<name>/, …)
        # so existing postUnpack sourceRoot+=/libs/… keeps working.
        fs = lib.fileset;

        srcFor = filesets:
          fs.toSource {
            root = ./.;
            fileset = fs.unions ([ ./VERSION ] ++ filesets);
          };

        # Shared CLI tool sources used by dirops (and optionally the app).
        toolsFs = ./tools;

        dirops = pkgs.stdenv.mkDerivation {
          pname = "dirops";
          inherit version cmakeBuildType;
          src = srcFor [ ./libs/dirops toolsFs ];
          nativeBuildInputs = with pkgs; [ cmake ninja ];
          dontStrip = true;
          cmakeFlags = [
            versionFlag
            "-DDIROPS_BUILD_TOOLS=ON"
          ];
          postUnpack = ''sourceRoot+=/libs/dirops'';
          preConfigure = ''
            echo "dirops: version=''${version} cmakeBuildType=''${cmakeBuildType:-}"
          '';
          meta = {
            description = "dirtoo filesystem mutation library + dt-copy/move/… tools";
          };
        };

        dirtoo-fs = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-fs";
          inherit version cmakeBuildType;
          src = srcFor [ ./libs/dirtoo-fs ];
          dontStrip = true;
          nativeBuildInputs = with pkgs; [ cmake ninja ];
          postUnpack = ''sourceRoot+=/libs/dirtoo-fs'';
          cmakeFlags = [ versionFlag ];
          meta.description = "dirtoo Location / FileInfo library";
        };
        dirtoo-tree = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-tree";
          inherit version cmakeBuildType;
          src = srcFor [ ./libs/dirtoo-tree ];
          dontStrip = true;
          nativeBuildInputs = with pkgs; [ cmake ninja pkg-config ];
          buildInputs = with pkgs; [ sqlite ];
          propagatedBuildInputs = with pkgs; [ sqlite ];
          postUnpack = ''sourceRoot+=/libs/dirtoo-tree'';
          cmakeFlags = [ versionFlag ];
          meta.description = "dirtoo hierarchical filesystem tree cache (scan + sizes)";
        };


        dirtoo-hash = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-hash";
          inherit version cmakeBuildType;
          src = srcFor [ ./libs/dirtoo-hash ];
          dontStrip = true;
          nativeBuildInputs = with pkgs; [ cmake ninja pkg-config ];
          buildInputs = with pkgs; [ openssl sqlite ];
          # So dependents' find_dependency(OpenSSL/SQLite3) and link work.
          propagatedBuildInputs = with pkgs; [ openssl sqlite ];
          postUnpack = ''sourceRoot+=/libs/dirtoo-hash'';
          cmakeFlags = [ versionFlag ];
          meta.description = "dirtoo multi-algo file digests + checksum SQLite cache";
        };

        dirtoo-tags = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-tags";
          inherit version cmakeBuildType;
          src = srcFor [ ./libs/dirtoo-tags ];
          dontStrip = true;
          nativeBuildInputs = with pkgs; [ cmake ninja pkg-config ];
          buildInputs = [ dirtoo-hash pkgs.sqlite pkgs.openssl ];
          propagatedBuildInputs = [ dirtoo-hash ];
          postUnpack = ''sourceRoot+=/libs/dirtoo-tags'';
          cmakeFlags = [ versionFlag ];
          meta.description = "dirtoo file tags (SHA-256 identity via checksum cache)";
        };

        dirtoo-filter = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-filter";
          inherit version cmakeBuildType;
          src = srcFor [ ./libs/dirtoo-filter ];
          dontStrip = true;
          nativeBuildInputs = with pkgs; [ cmake ninja pkg-config ];
          buildInputs = [ pkgs.sqlite pkgs.openssl pkgs.libarchive dirtoo-hash dirtoo-tags ];
          # Config.cmake find_dependency(dirtoo-hash/tags) needs these on the
          # dependent's cmake prefix path (e.g. dirtoo-collection).
          propagatedBuildInputs = [ dirtoo-hash dirtoo-tags pkgs.libarchive ];
          postUnpack = ''sourceRoot+=/libs/dirtoo-filter'';
          cmakeFlags = [ versionFlag "-DDIRTOO_FILTER_BUILD_TOOLS=ON" ];
          meta.description = "dirtoo filter DSL, predicates, media meta cache + dt-filter";
        };

        dirtoo-collection = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-collection";
          inherit version cmakeBuildType;
          src = srcFor [ ./libs/dirtoo-collection ];
          dontStrip = true;
          nativeBuildInputs = with pkgs; [ cmake ninja ];
          buildInputs = [ dirtoo-fs dirtoo-filter dirtoo-hash dirtoo-tags pkgs.sqlite pkgs.openssl pkgs.libarchive ];
          postUnpack = ''sourceRoot+=/libs/dirtoo-collection'';
          cmakeFlags = [ versionFlag ];
          meta.description = "dirtoo FileCollection / sorter / grouper";
        };

        dirtoo-watcher = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-watcher";
          inherit version cmakeBuildType;
          src = srcFor [ ./libs/dirtoo-watcher ];
          dontStrip = true;
          nativeBuildInputs = with pkgs; [ cmake ninja qt6.wrapQtAppsHook ];
          buildInputs = [ dirtoo-fs pkgs.qt6.qtbase ];
          postUnpack = ''sourceRoot+=/libs/dirtoo-watcher'';
          cmakeFlags = [ versionFlag ];
          meta.description = "dirtoo directory watcher (inotify)";
        };

        dirtoo-thumbnail = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-thumbnail";
          inherit version cmakeBuildType;
          src = srcFor [ ./libs/dirtoo-thumbnail ];
          dontStrip = true;
          nativeBuildInputs = with pkgs; [ cmake ninja qt6.wrapQtAppsHook ];
          buildInputs = [ dirtoo-fs pkgs.qt6.qtbase ];
          postUnpack = ''sourceRoot+=/libs/dirtoo-thumbnail'';
          cmakeFlags = [ versionFlag ];
          meta.description = "dirtoo freedesktop Thumbnailer1 client";
        };

        dirtoo-archive = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-archive";
          inherit version cmakeBuildType;
          src = srcFor [ ./libs/dirtoo-archive ];
          dontStrip = true;
          nativeBuildInputs = with pkgs; [ cmake ninja qt6.wrapQtAppsHook ];
          buildInputs = [ dirtoo-fs pkgs.qt6.qtbase pkgs.libarchive ];
          propagatedBuildInputs = with pkgs; [ libarchive ];
          postUnpack = ''sourceRoot+=/libs/dirtoo-archive'';
          cmakeFlags = [ versionFlag ];
          meta.description = "dirtoo read-only archive TOC/extract (libarchive)";
        };

        # GUI + tests + extra tools (dt-rmdir, dt-mediainfo, dt-archiveinfo).
        # Source set excludes libs/ — those come from the packages above via
        # find_package, so editing a single library does not rebuild the GUI
        # derivation's *inputs hash for that lib's sources* (only the lib output).
        dirtoo = pkgs.stdenv.mkDerivation {
          pname = "dirtoo";
          inherit version cmakeBuildType;
          src = srcFor [
            ./CMakeLists.txt
            ./apps
            ./tools
            ./tests
            ./resources
            ./man
          ];
          dontStrip = true;
          nativeBuildInputs = with pkgs; [ cmake ninja pkg-config qt6.wrapQtAppsHook ];
          buildInputs = [
            dirops
            dirtoo-fs
            dirtoo-tree
            dirtoo-hash
            dirtoo-tags
            dirtoo-filter
            pkgs.sqlite
            dirtoo-collection
            dirtoo-watcher
            dirtoo-thumbnail
            dirtoo-archive
            pkgs.qt6.qtbase
            pkgs.qt6.qtsvg
            pkgs.catch2_3
          ];
          # Runtime helpers the media probe shells out to: ffprobe (ffmpeg)
          # and pdfinfo (poppler-utils); bsdtar comes with libarchive. Archive
          # browsing itself is libarchive-only (no unzip/tar/7z).
          propagatedBuildInputs = with pkgs; [
            libarchive
            ffmpeg
            poppler-utils
          ];
          cmakeFlags = [
            versionFlag
            "-DDIRTOO_BUILD_APP=ON"
            "-DDIRTOO_BUILD_TESTS=ON"
            "-DDIRTOO_BUILD_TOOLS=ON"
          ];
          preConfigure = ''
            echo "dirtoo: version=$version"
            echo "dirtoo: cmakeBuildType=$cmakeBuildType"
          '';
          # Tests are built and installed to $out/libexec/dirtoo/dirtoo-tests but
          # not run here — see checks.dirtoo-tests so `nix build` stays build-only
          # and `nix flake check` reuses the package store path without recompiling.
          doCheck = false;
          meta.description = "dirtoo GUI file manager";
        };

        # Run unit tests against the already-built dirtoo package (no rebuild).
        dirtoo-tests-check = pkgs.runCommand "dirtoo-tests-check" {
          nativeBuildInputs = [ dirtoo ];
          meta.description = "Run dirtoo-tests from the built package";
        } ''
          set -eu
          echo "dirtoo-tests-check: running ${dirtoo}/libexec/dirtoo/dirtoo-tests"
          ${dirtoo}/libexec/dirtoo/dirtoo-tests --reporter console
          touch "$out"
        '';

        hilbert-thumbnailer = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-hilbert-thumb";
          inherit version cmakeBuildType;
          src = srcFor [ ./thumbnailers/hilbert ];
          dontStrip = true;
          nativeBuildInputs = with pkgs; [ cmake ninja ];
          buildInputs = with pkgs; [ zlib ];
          postUnpack = ''sourceRoot+=/thumbnailers/hilbert'';
          cmakeFlags = [ versionFlag ];
          meta = {
            description = "Hilbert-curve binary map thumbnailer (XDG Thumbnailer1 example)";
            mainProgram = "dirtoo-hilbert-thumb";
          };
        };

        text-thumbnailer = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-text-thumb";
          inherit version cmakeBuildType;
          src = srcFor [ ./thumbnailers/text ];
          dontStrip = true;
          nativeBuildInputs = with pkgs; [ cmake ninja pkg-config qt6.wrapQtAppsHook ];
          buildInputs = with pkgs; [ qt6.qtbase ];
          # Compact mono defaults for QFont resolution at runtime.
          propagatedUserEnvPkgs = with pkgs; [
            jetbrains-mono
            ibm-plex
            dejavu_fonts
          ];
          postUnpack = ''sourceRoot+=/thumbnailers/text'';
          cmakeFlags = [ versionFlag ];
          meta = {
            description = "Text layout thumbnailer (QPainter; 1–3 column start/mid/end)";
            mainProgram = "dirtoo-text-thumb";
          };
        };

        dirtoo-tools = pkgs.symlinkJoin {
          name = "dirtoo-tools-${version}";
          paths = [
            dirops
            dirtoo-hash
            dirtoo-tags
            dirtoo-filter
            dirtoo
          ];
          meta.description = "dirtoo CLI tools (dt-copy, dt-filter, dt-mediainfo, …)";
        };

        # Everything installable: GUI, all libs, CLI tools, optional thumbnailers.
        dirtoo-full = pkgs.symlinkJoin {
          name = "dirtoo-full-${version}";
          paths = [
            dirops
            dirtoo-fs
            dirtoo-tree
            dirtoo-hash
            dirtoo-tags
            dirtoo-filter
            dirtoo-collection
            dirtoo-watcher
            dirtoo-thumbnail
            dirtoo-archive
            dirtoo
            hilbert-thumbnailer
            text-thumbnailer
          ];
          meta = {
            description = "dirtoo meta-package: GUI + all libraries + optional tools (thumbnailers, …)";
            mainProgram = "dirtoo";
          };
        };

        # --- Development shell ----------------------------------------------
        # Deliberately NOT `inputsFrom = [ dirtoo ]`: that pulls in every
        # library derivation, so entering the shell with a dirty tree rebuilt
        # all of libs/ in the Nix store. The shell only provides toolchain +
        # third-party deps; the dirtoo-* scripts build the whole checkout
        # (libs via add_subdirectory, DIRTOO_IN_TREE_LIBS=ON) incrementally in
        # an out-of-tree build dir. The per-library packages above stay for
        # `nix build` / other consumers.
        devDeps = with pkgs; [
          cmake ninja pkg-config
          qt6.qtbase qt6.qtsvg qt6.qttools
          libarchive sqlite openssl catch2_3
        ];
        # Runtime tools the packaged GUI propagates (archive/media helpers).
        devRuntime = with pkgs; [ ffmpeg poppler-utils ];
        devTools = with pkgs; [ gdb clang-tools ];

        # Same plugin path the wrapQtAppsHook wrapper gives `nix run .#dirtoo`;
        # the unwrapped build-tree binary needs it for platform/svg plugins.
        devQtPluginPath = lib.concatMapStringsSep ":"
          (p: "${p}/${pkgs.qt6.qtbase.qtPluginPrefix}")
          [ pkgs.qt6.qtbase pkgs.qt6.qtsvg ];

        # Changes when any toolchain/dependency store path changes (nixpkgs
        # bump, GC'd compiler, …). The scripts compare it with the build dir's
        # stamp and re-run CMake from a fresh cache instead of building against
        # stale /nix/store paths baked into CMakeCache.txt.
        devEnvFingerprint = builtins.hashString "sha256"
          (lib.concatMapStringsSep "\n" toString
            (devDeps ++ [ pkgs.stdenv.cc ]));

        devPreamble = ''
          set -euo pipefail
          if [ -z "''${DIRTOO_SOURCE:-}" ]; then
            echo "error: DIRTOO_SOURCE is not set — run this inside 'nix develop' in a dirtoo checkout" >&2
            exit 1
          fi
          if [ ! -f "$DIRTOO_SOURCE/CMakeLists.txt" ] || [ ! -d "$DIRTOO_SOURCE/libs" ]; then
            echo "error: DIRTOO_SOURCE=$DIRTOO_SOURCE is not a dirtoo checkout" >&2
            exit 1
          fi
          build_type="''${DIRTOO_BUILD_TYPE:-Debug}"
          build_dir="''${DIRTOO_BUILD_DIR:?DIRTOO_BUILD_DIR is not set}"
          stamp="$build_dir/.dirtoo-dev-env"
          fingerprint="${devEnvFingerprint}"

          dirtoo_configure() {
            mkdir -p "$build_dir"
            # Fresh cache when the Nix env changed: cached compiler/Qt paths
            # would otherwise point at an old store generation.
            if [ -f "$build_dir/CMakeCache.txt" ] \
               && [ "$(cat "$stamp" 2>/dev/null || true)" != "$fingerprint" ]; then
              echo "dirtoo: dev environment changed — reconfiguring from a fresh CMake cache"
              rm -rf "$build_dir/CMakeCache.txt" "$build_dir/CMakeFiles"
            fi
            cmake -S "$DIRTOO_SOURCE" -B "$build_dir" -G Ninja \
              -DCMAKE_BUILD_TYPE="$build_type" \
              -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
              -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$build_dir/bin" \
              -DDIRTOO_IN_TREE_LIBS=ON \
              -DDIRTOO_BUILD_APP=ON \
              -DDIRTOO_BUILD_TOOLS=ON \
              -DDIRTOO_BUILD_TESTS=ON \
              "$@"
            echo "$fingerprint" > "$stamp"
          }

          dirtoo_ensure_configured() {
            # Reconfigure when never configured, configured for another
            # checkout, or the Nix environment changed since.
            local cached_src=""
            if [ -f "$build_dir/CMakeCache.txt" ]; then
              cached_src="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$build_dir/CMakeCache.txt")"
            fi
            if [ ! -f "$build_dir/build.ninja" ] \
               || [ "$cached_src" != "$DIRTOO_SOURCE" ] \
               || [ "$(cat "$stamp" 2>/dev/null || true)" != "$fingerprint" ]; then
              if [ -n "$cached_src" ] && [ "$cached_src" != "$DIRTOO_SOURCE" ]; then
                echo "dirtoo: build dir belonged to $cached_src — starting fresh"
                rm -rf "$build_dir/CMakeCache.txt" "$build_dir/CMakeFiles"
              fi
              dirtoo_configure
            fi
          }

          dirtoo_build() {
            dirtoo_ensure_configured
            cmake --build "$build_dir" "$@"
          }
        '';

        devScript = name: body: pkgs.writeShellScriptBin name ''
          ${devPreamble}
          ${body}
        '';

        devScripts = [
          (devScript "dirtoo-configure" ''
            # Extra args go to cmake, e.g. dirtoo-configure -DCMAKE_CXX_FLAGS=-O1
            dirtoo_configure "$@"
          '')
          (devScript "dirtoo-build" ''
            # Extra args go to `cmake --build`, e.g. dirtoo-build --target dt-filter
            dirtoo_build "$@"
          '')
          (devScript "dirtoo-run" ''
            # Build, then run the GUI from the build tree with any args.
            dirtoo_build
            exec "$build_dir/bin/dirtoo" "$@"
          '')
          (devScript "dirtoo-run-gdb" ''
            # Build, then run under gdb: starts immediately, quits on a clean
            # exit, stays in the session on a crash or non-zero exit.
            dirtoo_build
            gdb_script="$(mktemp)"
            trap 'rm -f "$gdb_script"' EXIT
            cat > "$gdb_script" <<'GDB'
          set pagination off
          set confirm off
          run
          if !$_isvoid($_exitcode) && $_exitcode == 0
            quit
          end
          GDB
            gdb -q -x "$gdb_script" --args "$build_dir/bin/dirtoo" "$@"
          '')
          (devScript "dirtoo-test" ''
            # Build, then run ctest; extra args go to ctest (e.g. -R FileInfo).
            dirtoo_build
            QT_QPA_PLATFORM="''${QT_QPA_PLATFORM:-offscreen}" \
              ctest --test-dir "$build_dir" --output-on-failure -j"$(nproc)" "$@"
          '')
        ];

        devShell = pkgs.mkShell {
          packages = devDeps ++ devRuntime ++ devTools ++ devScripts;
          shellHook = ''
            if [ -z "''${DIRTOO_SOURCE:-}" ]; then
              # Walk up to the checkout root (works from any subdirectory).
              _d="$PWD"
              while [ "$_d" != "/" ] && [ ! -f "$_d/libs/dirtoo-fs/CMakeLists.txt" ]; do
                _d="$(dirname "$_d")"
              done
              [ -f "$_d/libs/dirtoo-fs/CMakeLists.txt" ] && export DIRTOO_SOURCE="$_d"
              unset _d
            fi
            export DIRTOO_BUILD_TYPE="''${DIRTOO_BUILD_TYPE:-Debug}"
            export DIRTOO_BUILD_DIR="''${DIRTOO_BUILD_DIR:-''${XDG_CACHE_HOME:-$HOME/.cache}/dirtoo/build-''${DIRTOO_BUILD_TYPE,,}}"
            export QT_PLUGIN_PATH="${devQtPluginPath}''${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
            # dt-* tools and the GUI from the build tree.
            export PATH="$DIRTOO_BUILD_DIR/bin:$PATH"

            echo "dirtoo devShell (''${DIRTOO_BUILD_TYPE}, build dir: $DIRTOO_BUILD_DIR)"
            echo "  dirtoo-configure | dirtoo-build | dirtoo-run | dirtoo-run-gdb | dirtoo-test"
            echo "  packages: nix build .#dirtoo / .#dirtoo-full / .#<lib>  (see flake.nix)"
          '';
        };
      in
      {
        packages = {
          inherit
            dirops
            dirtoo-fs
            dirtoo-tree
            dirtoo-hash
            dirtoo-tags
            dirtoo-filter
            dirtoo-collection
            dirtoo-watcher
            dirtoo-thumbnail
            dirtoo-archive
            dirtoo
            dirtoo-tools
            dirtoo-full
            hilbert-thumbnailer
            text-thumbnailer;
          default = dirtoo;
          all-libs = pkgs.symlinkJoin {
            name = "dirtoo-all-libs-${version}";
            paths = [
              dirops
              dirtoo-fs
              dirtoo-tree
              dirtoo-hash
              dirtoo-tags
              dirtoo-filter
              dirtoo-collection
              dirtoo-watcher
              dirtoo-thumbnail
              dirtoo-archive
            ];
          };
        };


        checks = {
          # `nix flake check` → run unit tests using the built package output.
          # Does not recompile if `packages.dirtoo` is already in the store.
          dirtoo-tests = dirtoo-tests-check;
        };

        apps = {
          hilbert-thumb = {
            type = "app";
            program = "${hilbert-thumbnailer}/bin/dirtoo-hilbert-thumb";
            meta.description = "Hilbert-curve binary thumbnailer";
          };
          dirtoo = {
            type = "app";
            program = "${dirtoo}/bin/dirtoo";
            meta = {
              description = "dirtoo GUI file manager";
            };
          };
          dt-filter = {
            type = "app";
            program = "${dirtoo-filter}/bin/dt-filter";
            meta = {
              description = "dirtoo filter DSL CLI (dt-filter)";
            };
          };
          dt-copy = {
            type = "app";
            program = "${dirops}/bin/dt-copy";
            meta = {
              description = "dirtoo copy tool (dt-copy)";
            };
          };
        };

        devShells.default = devShell;
      });
}
