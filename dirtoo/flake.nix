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
        version = "${versionBase}+g${gitRev}";
        versionFlag = "-DPROJECT_VERSION_FULL=${version}";

        # Repo root must be available so libs can read ../../VERSION and tools paths.
        src = ./.;

        dirops = pkgs.stdenv.mkDerivation {
          pname = "dirops";
          inherit version src;
          nativeBuildInputs = with pkgs; [ cmake ninja ];
          cmakeFlags = [
            versionFlag
            # Installs dt-copy/move/rename/mkdir/mkfile/rm/symlink/swap from tools/
            "-DDIROPS_BUILD_TOOLS=ON"
          ];
          postUnpack = ''sourceRoot+=/libs/dirops'';
        };

        dirtoo-fs = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-fs";
          inherit version src;
          nativeBuildInputs = with pkgs; [ cmake ninja ];
          postUnpack = ''sourceRoot+=/libs/dirtoo-fs'';
          cmakeFlags = [ versionFlag ];
        };

        dirtoo-filter = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-filter";
          inherit version src;
          nativeBuildInputs = with pkgs; [ cmake ninja ];
          buildInputs = with pkgs; [ sqlite ];
          postUnpack = ''sourceRoot+=/libs/dirtoo-filter'';
          # dt-filter
          cmakeFlags = [ versionFlag "-DDIRTOO_FILTER_BUILD_TOOLS=ON" ];
        };

        dirtoo-collection = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-collection";
          inherit version src;
          nativeBuildInputs = with pkgs; [ cmake ninja ];
          buildInputs = [ dirtoo-fs dirtoo-filter pkgs.sqlite ];
          postUnpack = ''sourceRoot+=/libs/dirtoo-collection'';
          cmakeFlags = [ versionFlag ];
        };

        dirtoo-watcher = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-watcher";
          inherit version src;
          nativeBuildInputs = with pkgs; [ cmake ninja qt6.wrapQtAppsHook ];
          buildInputs = [ dirtoo-fs pkgs.qt6.qtbase ];
          postUnpack = ''sourceRoot+=/libs/dirtoo-watcher'';
          cmakeFlags = [ versionFlag ];
        };

        dirtoo-thumbnail = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-thumbnail";
          inherit version src;
          nativeBuildInputs = with pkgs; [ cmake ninja qt6.wrapQtAppsHook ];
          buildInputs = [ dirtoo-fs pkgs.qt6.qtbase ];
          postUnpack = ''sourceRoot+=/libs/dirtoo-thumbnail'';
          cmakeFlags = [ versionFlag ];
        };

        dirtoo-archive = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-archive";
          inherit version src;
          nativeBuildInputs = with pkgs; [ cmake ninja qt6.wrapQtAppsHook ];
          buildInputs = [ dirtoo-fs pkgs.qt6.qtbase ];
          propagatedBuildInputs = with pkgs; [ libarchive unzip gnutar p7zip ];
          postUnpack = ''sourceRoot+=/libs/dirtoo-archive'';
          cmakeFlags = [ versionFlag ];
        };

        # GUI + tools that need filter/archive (dt-rmdir, dt-mediainfo, dt-archiveinfo).
        # dirops tools and dt-filter already ship from their packages; building tools
        # here again is fine (same sources) and ensures a single bin/ with everything
        # when installing only .#dirtoo.
        dirtoo = pkgs.stdenv.mkDerivation {
          pname = "dirtoo";
          inherit version src;
          nativeBuildInputs = with pkgs; [ cmake ninja pkg-config qt6.wrapQtAppsHook ];
          buildInputs = [
            dirops
            dirtoo-fs
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
          # Runtime helpers used by mediainfo / archive tools / GUI.
          propagatedBuildInputs = with pkgs; [
            libarchive
            unzip
            gnutar
            p7zip
            ffmpeg # ffprobe for dt-mediainfo / media meta
          ];
          cmakeFlags = [
            versionFlag
            "-DDIRTOO_BUILD_APP=ON"
            "-DDIRTOO_BUILD_TESTS=ON"
            "-DDIRTOO_BUILD_TOOLS=ON"
          ];
          doCheck = true;
          checkPhase = ''
            runHook preCheck
            ctest --output-on-failure
            runHook postCheck
          '';
        };

        # All dt-* CLIs without pulling the full GUI (where packages already provide them).
        dirtoo-tools = pkgs.symlinkJoin {
          name = "dirtoo-tools-${version}";
          paths = [
            dirops          # dt-copy move rename mkdir mkfile rm symlink swap
            dirtoo-filter   # dt-filter
            dirtoo          # dt-rmdir mediainfo archiveinfo (+ same dirops tools)
          ];
          meta = {
            description = "dirtoo CLI tools (dt-copy, dt-filter, dt-mediainfo, …)";
          };
        };
      in
      {
        packages = {
          inherit
            dirops
            dirtoo-fs
            dirtoo-filter
            dirtoo-collection
            dirtoo-watcher
            dirtoo-thumbnail
            dirtoo-archive
            dirtoo
            dirtoo-tools;
          default = dirtoo;
          all-libs = pkgs.symlinkJoin {
            name = "dirtoo-all-libs-${version}";
            paths = [
              dirops
              dirtoo-fs
              dirtoo-filter
              dirtoo-collection
              dirtoo-watcher
              dirtoo-thumbnail
              dirtoo-archive
            ];
          };
        };

        apps = {
          dirtoo = {
            type = "app";
            program = "${dirtoo}/bin/dirtoo";
          };
          dt-filter = {
            type = "app";
            program = "${dirtoo-filter}/bin/dt-filter";
          };
          dt-copy = {
            type = "app";
            program = "${dirops}/bin/dt-copy";
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
            echo "dirtoo — libraries under libs/; tools under tools/"
            echo "  nix build .#dirtoo          # GUI + all dt-* tools"
            echo "  nix build .#dirtoo-tools    # CLI aggregate"
            echo "  nix build .#dirops          # dt-copy/move/rename/…"
            echo "  nix build .#dirtoo-filter   # dt-filter"
            echo "  nix run .#dirtoo"
          '';
        };
      });
}
