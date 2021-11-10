{
  description = "Python Scripts for directory stuff";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs";
    nix.inputs.nixpkgs.follows = "nixpkgs";
    flake-utils.url = "github:numtide/flake-utils";

    bytefmt.url = "git+file:///home/ingo/projects/bytefmt/trunk";
    # bytefmt.url = "github:grumbel/python-bytefmt";
    bytefmt.inputs.nix.follows = "nix";
    bytefmt.inputs.nixpkgs.follows = "nixpkgs";
    bytefmt.inputs.flake-utils.follows = "flake-utils";
  };

  outputs = { self, nix, nixpkgs, flake-utils, bytefmt }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config = { allowUnfree = true; };
        };
        # pkgs = nixpkgs.legacyPackages.${system};
        # pkgs.{ config = { allowUnfree = true; } };
        # import self.inputs.nixpkgs { config = { ... }; }
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

          # python-mediainfo = pkgs.python3Packages.buildPythonPackage rec {
          #   pname = "mediainfodll";
          #   version = "0.7.95";
          #   src = pkgs.fetchurl {
          #     url = ("https://mediaarea.net/download/source/libmediainfo/${version}" +
          #            "/libmediainfo_${version}.tar.bz2");
          #     sha256 = "1kchh6285b07z5nixv619hc9gml2ysdayicdiv30frrlqiyxqw4b";
          #   };
          # };

          # QT_QPA_PLATFORM_PLUGIN_PATH="${qt5.qtbase.bin}/lib/qt-${qt5.qtbase.version}/plugins";
          # export QT_QPA_PLATFORM_PLUGIN_PATH=/nix/store/6hqj2rhdx7vnwwgsmxivm9m0pcdhx9g7-qtbase-5.15.2-bin/lib/qt-5.15.2/plugins/
          dirtools = pkgs.python3Packages.buildPythonPackage rec {
            name = "dirtools";
            src = self;
            # QT_QPA_PLATFORM_PLUGIN_PATH="${pkgs.qt5.qtbase.bin}/lib/qt-${pkgs.qt5.qtbase.version}/plugins";
            # shellHook = ''
            #   # Some tests will fail without this
            #   export QT_QPA_PLATFORM_PLUGIN_PATH="${pkgs.qt5.qtbase.bin}/lib/qt-${pkgs.qt5.qtbase.version}/plugins";
            # '';
            doCheck = false;
            nativeBuildInputs = [ pkgs.qt5.wrapQtAppsHook ];
            makeWrapperArgs = [
              "\${qtWrapperArgs[@]}"
            ];
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
