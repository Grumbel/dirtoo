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
          dirtools = pkgs.python3Packages.buildPythonPackage rec {
            name = "dirtools";
            src = self;
            nativeBuildInputs = [ pkgs.qt5.wrapQtAppsHook ];
	    doCheck = false;
            makeWrapperArgs = [
              "\${qtWrapperArgs[@]}"
              "--set" "DIRTOOLS_7ZIP" "${pkgs.p7zip}/bin/7z"
              "--set" "DIRTOOLS_FFPROBE" "${pkgs.ffmpeg}/bin/ffprobe"
              "--set" "DIRTOOLS_RAR" "${pkgs.rar}/bin/rar"

              "--set" "LIBGL_DRIVERS_PATH" "${pkgs.mesa.drivers}/lib/dri"
              "--prefix" "LD_LIBRARY_PATH" ":" "${pkgs.mesa.drivers}/lib"
            ];
            preCheck = ''
              export QT_QPA_PLATFORM_PLUGIN_PATH="${pkgs.qt5.qtbase.bin}/lib/qt-${pkgs.qt5.qtbase.version}/plugins";
              export DIRTOOLS_7ZIP='${pkgs.p7zip}/bin/7z'
              export DIRTOOLS_FFPROBE='${pkgs.ffmpeg}/bin/ffprobe'
              export DIRTOOLS_RAR='${pkgs.rar}/bin/rar'
            '';
            propagatedBuildInputs = [
              python-ngram
              bytefmt.defaultPackage.${system}

              pkgs.xorg.libxcb
              pkgs.p7zip
              pkgs.rar
              pkgs.ffmpeg
              pkgs.python3Packages.setuptools
              pkgs.python3Packages.pyparsing
              pkgs.python3Packages.pymediainfo
              pkgs.python3Packages.colorama
              pkgs.python3Packages.inotify-simple
              pkgs.python3Packages.libarchive-c
              pkgs.python3Packages.numpy
              pkgs.python3Packages.pypdf2
              pkgs.python3Packages.pyqt5
              pkgs.python3Packages.pyxdg
              pkgs.python3Packages.scipy
              pkgs.python3Packages.sortedcontainers
              pkgs.python3Packages.unidecode
            ];
          };
        };
        defaultPackage = packages.dirtools;
      });
}
