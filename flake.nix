{
  description = "Python Scripts for directory stuff";

  inputs = {
    # nixpkgs.url = "github:NixOS/nixpkgs/nixos-22.05";
    flake-utils.url = "github:numtide/flake-utils";

    bytefmt.url = "github:grumbel/python-bytefmt";
    bytefmt.inputs.nix.follows = "nix";
    bytefmt.inputs.nixpkgs.follows = "nixpkgs";
    bytefmt.inputs.flake-utils.follows = "flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils, bytefmt }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config = { allowUnfree = true; };
        };
      in rec {
        packages = flake-utils.lib.flattenTree rec {
          python-ngram = pkgs.python3Packages.buildPythonPackage rec {
            pname = "ngram";
            version = "4.0.3";
            src = pkgs.python3Packages.fetchPypi {
              inherit pname version;
              sha256 = "BtGAnuL+3dztYGXc0ZgmxhMYeH1Hv08QscAReD1BmqY=";
            };
          };
          dirtoo = pkgs.python3Packages.buildPythonPackage rec {
            name = "dirtoo";
            src = self;
            nativeBuildInputs = [ pkgs.qt5.wrapQtAppsHook ];
            makeWrapperArgs = [
              "\${qtWrapperArgs[@]}"
              "--set" "DIRTOO_7ZIP" "${pkgs.p7zip}/bin/7z"
              "--set" "DIRTOO_FFPROBE" "${pkgs.ffmpeg}/bin/ffprobe"
              "--set" "DIRTOO_RAR" "${pkgs.rar}/bin/rar"

              "--set" "LIBGL_DRIVERS_PATH" "${pkgs.mesa.drivers}/lib/dri"
              "--prefix" "LD_LIBRARY_PATH" ":" "${pkgs.mesa.drivers}/lib"
            ];
            preCheck = ''
              export QT_QPA_PLATFORM_PLUGIN_PATH="${pkgs.qt5.qtbase.bin}/lib/qt-${pkgs.qt5.qtbase.version}/plugins";
              export DIRTOO_7ZIP='${pkgs.p7zip}/bin/7z'
              export DIRTOO_FFPROBE='${pkgs.ffmpeg}/bin/ffprobe'
              export DIRTOO_RAR='${pkgs.rar}/bin/rar'
            '';
            propagatedBuildInputs = with pkgs; [
              python3Packages.setuptools
              python3Packages.pyparsing
              python3Packages.pymediainfo
              python3Packages.colorama
              python3Packages.inotify-simple
              python3Packages.libarchive-c
              python3Packages.numpy
              python3Packages.pypdf2
              python3Packages.pyqt5
              python3Packages.pyxdg
              python3Packages.scipy
              python3Packages.sortedcontainers
              python3Packages.unidecode
            ] ++ [
              python-ngram
              bytefmt.defaultPackage.${system}
            ];
            shellHook = ''
              eval $preCheck
            '';
          };
        };
        defaultPackage = packages.dirtoo;
      }
    );
}
