{ self
, lib
, buildPythonPackage

, PyQt6-stubs
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
, pyqt6
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

  format = "pyproject";

  doCheck = doCheck;

  preCheck = ''
    export DIRTOO_7ZIP='${_7zz}/bin/7zz'
    export DIRTOO_FFPROBE='${ffmpeg}/bin/ffprobe'
  '' +
  lib.optionalString useRar ''
    export DIRTOO_RAR='${rar}/bin/rar'
  '';

  checkPhase = ''
    runHook preCheck
    flake8 --max-line-length 120
    pyright dirtoo tests
    MYPYPATH=src mypy -p dirtoo -p tests
    pylint dirtoo tests
    python3 -m unittest discover -v -s tests/
    runHook postCheck
  '';

  makeWrapperArgs = [
    "\${qtWrapperArgs[@]}"
    "--set" "DIRTOO_7ZIP" "${_7zz}/bin/7zz"
    "--set" "DIRTOO_FFPROBE" "${ffmpeg}/bin/ffprobe"
  ] ++
  lib.optional useRar [ "--set" "DIRTOO_RAR" "${rar}/bin/rar" ];

  nativeBuildInputs = [
    wrapQtAppsHook
    pip
  ];

  buildInputs = [
    qtbase
  ];

  propagatedBuildInputs = [
    setuptools
    pyparsing
    pymediainfo
    colorama
    inotify-simple
    libarchive-c
    numpy
    pypdf2
    pyqt6
    scipy
    sortedcontainers
    unidecode
    pyxdg
    python-ngram
    bytefmt
  ];

  checkInputs = [
    PyQt6-stubs
  ];

  nativeCheckInputs = [
    flake8
    mypy
    pylint
    pyright
    types-setuptools
  ];
}
