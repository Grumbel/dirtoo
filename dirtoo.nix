{ self
, lib
, buildPythonPackage

, PyQt5-stubs
, _7zz
, bytefmt
, colorama
, ffmpeg
, flake8
, inotify-simple
, libarchive-c
, mypy
, numpy
, pip
, pylint
, pymediainfo
, pyparsing
, pypdf2
, pyqt5
, pyright
, python-ngram
, pyxdg
, qtbase
, rar
, scipy
, setuptools
, sortedcontainers
, types-setuptools
, unidecode
, wrapQtAppsHook

, useRar ? false
, doCheck ? false
}:
let
in
buildPythonPackage {
  pname = "dirtoo";
  version = "0.1.0";

  src = ./.;

  doCheck = doCheck;

  preCheck = ''
    export QT_QPA_PLATFORM_PLUGIN_PATH="${qtbase.bin}/lib/qt-${qtbase.version}/plugins";
    export DIRTOO_7ZIP='${_7zz}/bin/7zz'
    export DIRTOO_FFPROBE='${ffmpeg}/bin/ffprobe'
  '' +
  lib.optionalString useRar ''
    export DIRTOO_RAR='${rar}/bin/rar'
  '';

  checkPhase = ''
    runHook preCheck
    flake8
    pyright dirtoo tests
    mypy -p dirtoo -p tests
    pylint dirtoo tests
    python3 -m unittest discover -v -s tests/
    runHook postCheck
  '';

  makeWrapperArgs = [
    "\${qtWrapperArgs[@]}"
    "--set" "DIRTOO_7ZIP" "${_7zz}/bin/7zz"
    "--set" "DIRTOO_FFPROBE" "${ffmpeg}/bin/ffprobe"

    # "--set" "LIBGL_DRIVERS_PATH" "${mesa.drivers}/lib/dri"
    # "--prefix" "LD_LIBRARY_PATH" ":" "${mesa.drivers}/lib"
  ] ++
  lib.optional useRar [ "--set" "DIRTOO_RAR" "${rar}/bin/rar" ];

  nativeBuildInputs = [ wrapQtAppsHook ];

  propagatedBuildInputs = [
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
    pyxdg
    python-ngram
    bytefmt
  ];

  nativeCheckInputs = [
    flake8
    mypy
    pip
    pylint
    pyright
    types-setuptools
  ];
}
