{
  description = "Python Scripts for directory stuff";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-23.05";
    flake-utils.url = "github:numtide/flake-utils";

    bytefmt.url = "github:grumbel/python-bytefmt";
    bytefmt.inputs.nixpkgs.follows = "nixpkgs";
    bytefmt.inputs.flake-utils.follows = "flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils, bytefmt }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        pkgs-nonfree = import nixpkgs {
          inherit system;
          config = { allowUnfree = true; };
        };
        lib = pkgs.lib;
        pythonPackages = pkgs.python3Packages;
      in rec {
        packages = rec {
          default = dirtoo;

          PyQt5-stubs = pythonPackages.buildPythonPackage rec {
            pname = "PyQt5-stubs";
            version = "5.15.6.0";
            src = pythonPackages.fetchPypi {
              inherit pname version;
              sha256 = "sha256-kScKwj6/OKHcBM2XqoUs0Ir4Lcg5EA5Tla8UR+Pplwc=";
            };
          };

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

          dirtoo = pkgs.callPackage ./dirtoo.nix {
            inherit self;

            doCheck = false;
            bytefmt = bytefmt.lib.bytefmtWithPythonPackages pythonPackages;
            inherit pyxdg;
            inherit python-ngram;
            inherit PyQt5-stubs;

            inherit (pkgs) pyright;
            inherit (pythonPackages) buildPythonPackage;
            inherit (pkgs.qt5)
              qtbase
              wrapQtAppsHook;
            inherit (pythonPackages)
              colorama
              flake8
              inotify-simple
              libarchive-c
              numpy
              pymediainfo
              pyparsing
              pypdf2
              pyqt5
              scipy
              setuptools
              sortedcontainers
              unidecode
              mypy
              pylint
              types-setuptools
              pip;
          };

          dirtoo-nonfree = dirtoo.override {
            inherit (pkgs-nonfree) rar;
            useRar = true;
          };

          dirtoo-check = dirtoo.override {
            doCheck = true;
          };
        };

        apps = rec {
          default = dirtoo;

          dirtoo = flake-utils.lib.mkApp {
            drv = packages.dirtoo;
            exePath = "/bin/dirtoo";
          };

          dirtoo-nonfree = flake-utils.lib.mkApp {
            drv = packages.dirtoo-nonfree;
            exePath = "/bin/dirtoo";
          };
        };

        devShells = rec {
          default = dirtoo;

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
        };
      }
    );
}
