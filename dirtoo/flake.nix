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
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "dirtoo";
          version = "0.1.0";
          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            pkg-config
            qt6.wrapQtAppsHook
          ];

          buildInputs = with pkgs; [
            qt6.qtbase
            libarchive
          ];

          # Runtime helpers used by ArchiveManager / archive_index
          propagatedBuildInputs = with pkgs; [
            libarchive # bsdtar
            unzip
            gnutar
            p7zip
          ];

          cmakeFlags = [
            "-DDIRTOO_BUILD_TESTS=ON"
          ];

          doCheck = true;
          checkPhase = ''
            runHook preCheck
            ctest --output-on-failure
            runHook postCheck
          '';
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
            gdb
            clang-tools
          ];

          shellHook = ''
            echo "dirtoo C++ dev shell (C++23, Qt6, CMake/Ninja)"
            echo "Archive tools: bsdtar/tar/unzip/7z available for browsing"
          '';
        };
      });
}
