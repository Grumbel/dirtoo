;; dirtools - Python Scripts for directory stuff
;; Copyright (C) 2019 Ingo Ruhnke <grumbel@gmail.com>
;;
;; This program is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <http://www.gnu.org/licenses/>.

(set! %load-path
      (cons* "/ipfs/QmetP3eCAM9q3VPCj9BvjFdWkTA7voycebnXjyfc2zacFE/guix-cocfree_v0.0.0-45-g0fe3c86"
             %load-path))

(use-modules (ice-9 popen)
             (ice-9 rdelim)
             (guix packages)
             (guix gexp)
             (guix git-download)
             (guix download)
             (guix build utils)
             (guix build-system python)
             (guix licenses)
             (gnu packages backup)
             (gnu packages compression)
             (gnu packages freedesktop)
             (gnu packages pdf)
             (gnu packages python)
             (gnu packages python-xyz)
             (gnu packages qt)
             (gnu packages video)
             (guix-cocfree packages python-bytefmt)
             (guix-cocfree packages python-mediainfodll)
             (guix-cocfree packages python-inotify_simple)
             (guix-cocfree packages python-ngram)
             (guix-cocfree utils))

(define %source-dir (dirname (current-filename)))

(define-public dirtools
  (package
   (name "dirtools")
   (version (version-from-source %source-dir))
   (source (source-from-source %source-dir))
   (arguments
    `(#:tests? #f))
   (propagated-inputs
    `(("p7zip" ,p7zip)))
   (inputs
    `(("python-mediainfodll" ,python-mediainfodll)
      ("python" ,python-wrapper)
      ("python-bytefmt" ,python-bytefmt)
      ("python-colorama" ,python-colorama)
      ("python-inotify_simple" ,python-inotify_simple)
      ("python-libarchive-c" ,python-libarchive-c)
      ("python-ngram" ,python-ngram)
      ("python-numpy" ,python-numpy)
      ("python-pypdf2" ,python-pypdf2)
      ("python-pyqt" ,python-pyqt)
      ("python-pyxdg" ,python-pyxdg)
      ("python-scipy" ,python-scipy)
      ("python-sortedcontainers" ,python-sortedcontainers)
      ("python-unidecode" ,python-unidecode)
      ))
   (build-system python-build-system)
   (synopsis "Python Scripts for directory stuff")
   (description "Python Scripts for directory stuff")
   (home-page "https://gitlab.com/grumbel/dirtool")
   (license gpl3+)))

dirtools

;; EOF ;;
