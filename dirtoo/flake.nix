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
            "-DDIROPS_BUILD_TOOLS=ON"
          ];
          # cmake -S is not in cmakeFlags the same way on all stdenv versions;
          # use sourceRoot / configurePhase instead if needed.
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
          postUnpack = ''sourceRoot+=/libs/dirtoo-filter'';
          cmakeFlags = [ versionFlag "-DDIRTOO_FILTER_BUILD_TOOLS=ON" ];
        };

        dirtoo-collection = pkgs.stdenv.mkDerivation {
          pname = "dirtoo-collection";
          inherit version src;
          nativeBuildInputs = with pkgs; [ cmake ninja ];
          buildInputs = [ dirtoo-fs dirtoo-filter ];
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

        dirtoo = pkgs.stdenv.mkDerivation {
          pname = "dirtoo";
          inherit version src;
          nativeBuildInputs = with pkgs; [ cmake ninja pkg-config qt6.wrapQtAppsHook ];
          buildInputs = [
            dirops
            dirtoo-fs
            dirtoo-filter
            dirtoo-collection
            dirtoo-watcher
            dirtoo-thumbnail
            dirtoo-archive
            pkgs.qt6.qtbase
            pkgs.catch2_3
          ];
          propagatedBuildInputs = with pkgs; [ libarchive unzip gnutar p7zip ];
          cmakeFlags = [
            versionFlag
            "-DDIRTOO_BUILD_APP=ON"
            "-DDIRTOO_BUILD_TESTS=ON"
            "-DDIRTOO_BUILD_TOOLS=OFF"
          ];
          doCheck = true;
          checkPhase = ''
            runHook preCheck
            ctest --output-on-failure
            runHook postCheck
          '';
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
            dirtoo;
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

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake ninja pkg-config gcc
            qt6.qtbase qt6.qttools
            libarchive unzip gnutar p7zip catch2_3
            gdb clang-tools
          ];
          shellHook = ''
            echo "dirtoo — libraries are independent CMake packages under libs/"
            echo "nix build .#dirops .#dirtoo-fs .#dirtoo"
          '';
        };
      });
}
