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
        gitRev = self.shortRev or self.dirtyShortRev or "dirty";
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
          buildInputs = [ pkgs.sqlite pkgs.openssl dirtoo-hash dirtoo-tags ];
          # Config.cmake find_dependency(dirtoo-hash/tags) needs these on the
          # dependent's cmake prefix path (e.g. dirtoo-collection).
          propagatedBuildInputs = [ dirtoo-hash dirtoo-tags ];
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
          buildInputs = [ dirtoo-fs dirtoo-filter dirtoo-hash dirtoo-tags pkgs.sqlite pkgs.openssl ];
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
          propagatedBuildInputs = with pkgs; [
            libarchive
            unzip
            gnutar
            p7zip
            ffmpeg
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
          nativeBuildInputs = with pkgs; [ cmake ninja pkg-config ];
          buildInputs = with pkgs; [ zlib freetype fontconfig ];
          # Compact mono defaults available to Fontconfig at runtime.
          propagatedUserEnvPkgs = with pkgs; [
            jetbrains-mono
            ibm-plex
            dejavu_fonts
          ];
          postUnpack = ''sourceRoot+=/thumbnailers/text'';
          cmakeFlags = [ versionFlag ];
          meta = {
            description = "Text layout thumbnailer (FreeType; 1–3 column start/mid/end)";
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
      in
      {
        packages = {
          inherit
            dirops
            dirtoo-fs
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

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake ninja pkg-config gcc
            qt6.qtbase qt6.qtsvg qt6.qttools
            libarchive unzip gnutar p7zip catch2_3
            sqlite ffmpeg
            gdb clang-tools
          ];
          inputsFrom = [ dirtoo ];
          shellHook = ''
            echo "dirtoo devShell"
            echo "  version:    ${version}"
            echo "  system:     ${system}"
            echo "  rev:        ${gitRev}"
            echo "  revCount:   ${revCount}"
            echo "  build type: ${cmakeBuildType} (dontStrip on packages)"
            echo ""
            echo "Independent flake packages (scoped sources — edit one lib, rebuild only it + dependents):"
            echo "  nix build .#dirops"
            echo "  nix build .#dirtoo-fs"
            echo "  nix build .#dirtoo-hash"
            echo "  nix build .#dirtoo-tags"
            echo "  nix build .#dirtoo-filter"
            echo "  nix build .#dirtoo-collection"
            echo "  nix build .#dirtoo-watcher"
            echo "  nix build .#dirtoo-thumbnail"
            echo "  nix build .#dirtoo-archive"
            echo "  nix build .#dirtoo            # GUI (does not rehash lib sources)"
            echo "  nix build .#dirtoo-tools"
            echo "  nix build .#hilbert-thumbnailer  # standalone binary map thumbs"
            echo "  nix build .#text-thumbnailer     # text layout thumbs"
            echo "  nix build .#dirtoo-full           # GUI + libs + optional tools"
            echo "  nix build .#all-libs"
            echo "  nix run .#dirtoo"
          '';
        };
      });
}
