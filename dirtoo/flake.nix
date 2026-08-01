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

        # Shared src / version for all derivations.
        common = {
          inherit version;
          src = ./.;
          nativeBuildInputs = with pkgs; [ cmake ninja pkg-config ];
        };

        # ---------------------------------------------------------------------------
        # Qt-free libraries (and tools that only need dirops)
        # ---------------------------------------------------------------------------
        dirops = pkgs.stdenv.mkDerivation (common // {
          pname = "dirops";
          cmakeFlags = [
            "-DPROJECT_VERSION_FULL=${version}"
            "-DDIRTOO_WITH_QT=OFF"
            "-DDIRTOO_BUILD_APP=OFF"
            "-DDIRTOO_BUILD_TESTS=OFF"
            "-DDIRTOO_BUILD_TOOLS=ON"
            "-DDIRTOO_BUILD_DIROPS=ON"
            "-DDIRTOO_BUILD_FS=OFF"
            "-DDIRTOO_BUILD_COLLECTION=OFF"
            "-DDIRTOO_BUILD_WATCHER=OFF"
            "-DDIRTOO_BUILD_THUMBNAIL=OFF"
            "-DDIRTOO_BUILD_ARCHIVE=OFF"
          ];
        });

        dirtoo-fs = pkgs.stdenv.mkDerivation (common // {
          pname = "dirtoo-fs";
          cmakeFlags = [
            "-DPROJECT_VERSION_FULL=${version}"
            "-DDIRTOO_WITH_QT=OFF"
            "-DDIRTOO_BUILD_APP=OFF"
            "-DDIRTOO_BUILD_TESTS=OFF"
            "-DDIRTOO_BUILD_TOOLS=OFF"
            "-DDIRTOO_BUILD_DIROPS=OFF"
            "-DDIRTOO_BUILD_FS=ON"
            "-DDIRTOO_BUILD_COLLECTION=OFF"
            "-DDIRTOO_BUILD_WATCHER=OFF"
            "-DDIRTOO_BUILD_THUMBNAIL=OFF"
            "-DDIRTOO_BUILD_ARCHIVE=OFF"
          ];
        });

        dirtoo-collection = pkgs.stdenv.mkDerivation (common // {
          pname = "dirtoo-collection";
          cmakeFlags = [
            "-DPROJECT_VERSION_FULL=${version}"
            "-DDIRTOO_WITH_QT=OFF"
            "-DDIRTOO_BUILD_APP=OFF"
            "-DDIRTOO_BUILD_TESTS=OFF"
            "-DDIRTOO_BUILD_TOOLS=OFF"
            "-DDIRTOO_BUILD_DIROPS=OFF"
            "-DDIRTOO_BUILD_FS=ON"
            "-DDIRTOO_BUILD_COLLECTION=ON"
            "-DDIRTOO_BUILD_WATCHER=OFF"
            "-DDIRTOO_BUILD_THUMBNAIL=OFF"
            "-DDIRTOO_BUILD_ARCHIVE=OFF"
          ];
        });

        # ---------------------------------------------------------------------------
        # Qt-based libraries
        # ---------------------------------------------------------------------------
        qtNative = with pkgs; [ cmake ninja pkg-config qt6.wrapQtAppsHook ];
        qtBuild = with pkgs; [ qt6.qtbase ];

        dirtoo-watcher = pkgs.stdenv.mkDerivation (common // {
          pname = "dirtoo-watcher";
          nativeBuildInputs = qtNative;
          buildInputs = qtBuild;
          cmakeFlags = [
            "-DPROJECT_VERSION_FULL=${version}"
            "-DDIRTOO_WITH_QT=ON"
            "-DDIRTOO_BUILD_APP=OFF"
            "-DDIRTOO_BUILD_TESTS=OFF"
            "-DDIRTOO_BUILD_TOOLS=OFF"
            "-DDIRTOO_BUILD_DIROPS=OFF"
            "-DDIRTOO_BUILD_FS=ON"
            "-DDIRTOO_BUILD_COLLECTION=OFF"
            "-DDIRTOO_BUILD_WATCHER=ON"
            "-DDIRTOO_BUILD_THUMBNAIL=OFF"
            "-DDIRTOO_BUILD_ARCHIVE=OFF"
          ];
        });

        dirtoo-thumbnail = pkgs.stdenv.mkDerivation (common // {
          pname = "dirtoo-thumbnail";
          nativeBuildInputs = qtNative;
          buildInputs = qtBuild;
          cmakeFlags = [
            "-DPROJECT_VERSION_FULL=${version}"
            "-DDIRTOO_WITH_QT=ON"
            "-DDIRTOO_BUILD_APP=OFF"
            "-DDIRTOO_BUILD_TESTS=OFF"
            "-DDIRTOO_BUILD_TOOLS=OFF"
            "-DDIRTOO_BUILD_DIROPS=OFF"
            "-DDIRTOO_BUILD_FS=ON"
            "-DDIRTOO_BUILD_COLLECTION=OFF"
            "-DDIRTOO_BUILD_WATCHER=OFF"
            "-DDIRTOO_BUILD_THUMBNAIL=ON"
            "-DDIRTOO_BUILD_ARCHIVE=OFF"
          ];
        });

        dirtoo-archive = pkgs.stdenv.mkDerivation (common // {
          pname = "dirtoo-archive";
          nativeBuildInputs = qtNative;
          buildInputs = qtBuild ++ (with pkgs; [ libarchive ]);
          propagatedBuildInputs = with pkgs; [ libarchive unzip gnutar p7zip ];
          cmakeFlags = [
            "-DPROJECT_VERSION_FULL=${version}"
            "-DDIRTOO_WITH_QT=ON"
            "-DDIRTOO_BUILD_APP=OFF"
            "-DDIRTOO_BUILD_TESTS=OFF"
            "-DDIRTOO_BUILD_TOOLS=OFF"
            "-DDIRTOO_BUILD_DIROPS=OFF"
            "-DDIRTOO_BUILD_FS=ON"
            "-DDIRTOO_BUILD_COLLECTION=OFF"
            "-DDIRTOO_BUILD_WATCHER=OFF"
            "-DDIRTOO_BUILD_THUMBNAIL=OFF"
            "-DDIRTOO_BUILD_ARCHIVE=ON"
          ];
        });

        # ---------------------------------------------------------------------------
        # Full application (default)
        # ---------------------------------------------------------------------------
        dirtoo = pkgs.stdenv.mkDerivation (common // {
          pname = "dirtoo";
          nativeBuildInputs = qtNative;
          buildInputs = qtBuild ++ (with pkgs; [ libarchive catch2_3 ]);
          propagatedBuildInputs = with pkgs; [ libarchive unzip gnutar p7zip ];
          cmakeFlags = [
            "-DPROJECT_VERSION_FULL=${version}"
            "-DDIRTOO_WITH_QT=ON"
            "-DDIRTOO_BUILD_APP=ON"
            "-DDIRTOO_BUILD_TESTS=ON"
            "-DDIRTOO_BUILD_TOOLS=ON"
            "-DDIRTOO_BUILD_DIROPS=ON"
            "-DDIRTOO_BUILD_FS=ON"
            "-DDIRTOO_BUILD_COLLECTION=ON"
            "-DDIRTOO_BUILD_WATCHER=ON"
            "-DDIRTOO_BUILD_THUMBNAIL=ON"
            "-DDIRTOO_BUILD_ARCHIVE=ON"
          ];
          doCheck = true;
          checkPhase = ''
            runHook preCheck
            ctest --output-on-failure
            runHook postCheck
          '';
        });
      in
      {
        packages = {
          inherit
            dirops
            dirtoo-fs
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
              dirtoo-collection
              dirtoo-watcher
              dirtoo-thumbnail
              dirtoo-archive
            ];
          };
        };

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            ninja
            pkg-config
            gcc
            qt6.qtbase
            qt6.qttools
            libarchive
            unzip
            gnutar
            p7zip
            catch2_3
            gdb
            clang-tools
          ];
          shellHook = ''
            echo "dirtoo C++ dev shell — VERSION $(cat VERSION 2>/dev/null || echo '?')"
            echo "Flake packages: dirops dirtoo-fs dirtoo-collection dirtoo-watcher"
            echo "                dirtoo-thumbnail dirtoo-archive dirtoo (default) all-libs"
          '';
        };
      });
}
