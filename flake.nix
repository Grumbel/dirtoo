{
  description = "Python Scripts for directory stuff";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-22.05";
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
          config = { allowUnfree = true; };  # for 'rar'
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

          pyxdg = pkgs.python3Packages.buildPythonPackage rec {
            pname = "pyxdg";
            version = "0.28";
            src = pkgs.python3Packages.fetchPypi {
              inherit pname version;
              sha256 = "sha256-Mme7MHTpNN8gKvLuCGhXVIQQhYHm88sAavHaNTleiLQ=";
            };
          };

          dirtoo = pkgs.python3Packages.buildPythonPackage rec {
            name = "dirtoo";
            src = ./.;
            meta = {
              mainProgram = "dt-fileview";
            };
            nativeBuildInputs = [ pkgs.qt5.wrapQtAppsHook ];
            makeWrapperArgs = [
              "\${qtWrapperArgs[@]}"
              "--set" "DIRTOO_7ZIP" "${pkgs._7zz}/bin/7zz"
              "--set" "DIRTOO_FFPROBE" "${pkgs.ffmpeg}/bin/ffprobe"
              "--set" "DIRTOO_RAR" "${pkgs.rar}/bin/rar"

              "--set" "LIBGL_DRIVERS_PATH" "${pkgs.mesa.drivers}/lib/dri"
              "--prefix" "LD_LIBRARY_PATH" ":" "${pkgs.mesa.drivers}/lib"
            ];
            doCheck = false;
            preCheck = ''
              export QT_QPA_PLATFORM_PLUGIN_PATH="${pkgs.qt5.qtbase.bin}/lib/qt-${pkgs.qt5.qtbase.version}/plugins";
              export DIRTOO_7ZIP='${pkgs._7zz}/bin/7zz'
              export DIRTOO_FFPROBE='${pkgs.ffmpeg}/bin/ffprobe'
              export DIRTOO_RAR='${pkgs.rar}/bin/rar'
            '';
            checkPhase = ''
              runHook preCheck
              make
              runHook postCheck
            '';
            propagatedBuildInputs = with pkgs.python3Packages; [
              setuptools
              pyparsing
              pymediainfo
              colorama
              inotify-simple
              libarchive-c
              numpy
              pypdf2
              pyqt5
              scipy
              sortedcontainers
              unidecode
            ] ++ [
              pyxdg
              python-ngram
              bytefmt.defaultPackage.${system}
            ];
            checkInputs = with pkgs; [
              pylint
              python3Packages.flake8
              python3Packages.mypy
              python3Packages.types-setuptools
            ];
            shellHook = ''
              eval $preCheck
            '';
          };

          dirtoo-check = dirtoo.override {
            doCheck = true;
          };

          default = dirtoo;
        };

        apps = rec {
          dirtoo = flake-utils.lib.mkApp {
            drv = packages.dirtoo;
          };
          default = dirtoo;
        };

        devShells = rec {
          dirtoo = pkgs.mkShell {
            inputsFrom = [ packages.dirtoo-check ];
          };
          default = dirtoo;
        };
      }
    );
}
