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
        pythonPackages = pkgs.python310Packages;
      in rec {
        packages = flake-utils.lib.flattenTree rec {

          python-ngram = pythonPackages.buildPythonPackage rec {
            pname = "ngram";
            version = "4.0.3";
            src = pythonPackages.fetchPypi {
              inherit pname version;
              sha256 = "BtGAnuL+3dztYGXc0ZgmxhMYeH1Hv08QscAReD1BmqY=";
            };
          };

          pyxdg = pythonPackages.buildPythonPackage rec {
            pname = "pyxdg";
            version = "0.28";
            src = pythonPackages.fetchPypi {
              inherit pname version;
              sha256 = "sha256-Mme7MHTpNN8gKvLuCGhXVIQQhYHm88sAavHaNTleiLQ=";
            };
          };

          dirtoo = pythonPackages.buildPythonPackage rec {
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
              flake8
              pyright dirtoo tests
              mypy -p dirtoo -p tests
              python3 -m unittest discover -v -s tests/
              runHook postCheck
            '';
            propagatedBuildInputs = with pythonPackages; [
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
              (bytefmt.lib.bytefmtWithPythonPackages pythonPackages)
            ];
            checkInputs = (with pkgs; [
              pyright
            ]) ++ (with pythonPackages; [
              flake8
              mypy
              pylint
              types-setuptools
            ]);
          };

          dirtoo-check = dirtoo.override {
            doCheck = true;
          };

          default = dirtoo;
        };

        apps = rec {
          dirtoo = flake-utils.lib.mkApp {
            drv = packages.dirtoo;
            exePath = "/bin/dt-fileview";
          };
          default = dirtoo;
        };

        devShells = rec {
          dirtoo = pkgs.mkShell {
            inputsFrom = [ packages.dirtoo-check ];
            shellHook = packages.dirtoo-check.preCheck + ''
              export QT_QPA_PLATFORM_PLUGIN_PATH="${pkgs.qt5.qtbase.bin}/lib/qt-${pkgs.qt5.qtbase.version}/plugins";
              runHook setuptoolsShellHook
            '';
          };
          dirtoo-check = pkgs.mkShell {
            inputsFrom = [ packages.dirtoo-check ];
            shellHook = packages.dirtoo-check.preCheck + ''
              export QT_QPA_PLATFORM_PLUGIN_PATH="${pkgs.qt5.qtbase.bin}/lib/qt-${pkgs.qt5.qtbase.version}/plugins";
            '';
          };
          default = dirtoo;
        };
      }
    );
}
