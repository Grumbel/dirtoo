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

;; https://guix.gnu.org/manual/en/html_node/Build-Systems.html

;; Install with:
;; guix package --install-from-file=dirtools.scm

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
             )

(define-public python-inotify_simple
  (package
    (name "python-inotify_simple")
    (version "1.1.8")
    (source
     (origin
       (method url-fetch)
       (uri (pypi-uri "inotify_simple" version))
       (sha256
        (base32
         "1pfqvnynwh318cakldhg7535kbs02asjsgv6s0ki12i7fgfi0b7w"))))
    (build-system python-build-system)
    (arguments
     `(#:tests? #f))
    (home-page "https://pypi.org/project/inotify_simple/")
    (synopsis "A simple Python wrapper around inotify. No fancy bells and whistles.")
    (description "inotify_init() is wrapped as a class that does
little more than hold the resulting inotify file descriptor. A read()
method is provided which reads available data from the file descriptor
and returns events as namedtuple objects after unpacking them with the
struct module. inotify_add_watch() and inotify_rm_watch() are wrapped
with no changes at all, taking and returning watch descriptor integers
that calling code is expected to keep track of itself, just as one
would use inotify from C.")
    (license bsd-3)))

(define-public python-ngram
  (package
    (name "python-ngram")
    (version "3.3.2")
    (source
     (origin
       (method url-fetch)
       (uri (pypi-uri "ngram" version))
       (sha256
        (base32
         "1y243y0xl8m5zc22slx690j0hmdzzvgf2m7qlajp4gvg2bhzbxzn"))))
    (build-system python-build-system)
    (arguments
     `(#:tests? #f))
    (home-page "https://github.com/gpoulter/python-ngram")
    (synopsis "Set that supports searching by ngram similarity")
    (description "The NGram class extends the Python 'set' class with
efficient fuzzy search for members by means of an N-gram similarity
measure. It also has static methods to compare a pair of strings.

The N-grams are character based not word-based, and the class does not
implement a language model, merely searching for members by string
similarity.")
    (license lgpl3+)))

(define-public python-bytefmt
  (package
    (name "python-bytefmt")
    (version "0.1.0")
    (source
     (origin
       (method url-fetch)
       (uri (pypi-uri "bytefmt" version))
       (sha256
        (base32
         "1dfq6qlk2zdyab953hk82lzvfln4nx315lxf60c4fx1yahdizzmc"))))
    (build-system python-build-system)
    (arguments
     `(#:tests? #f))
    (home-page "")
    (synopsis "")
    (description "")
    (license gpl3+)))

(define %source-dir (dirname (current-filename)))

(define current-commit
  (with-directory-excursion %source-dir
    (let* ((port   (open-input-pipe "git describe --tags"))
           (output (read-line port)))
      (close-pipe port)
      (string-trim-right output #\newline))))

(package
  (name "dirtools")
  (version current-commit)
  (source (local-file %source-dir
                      #:recursive? #t
                      #:select? (git-predicate %source-dir)))
;; (source (origin
  ;;           (method git-fetch)
  ;;           (uri (git-reference
  ;;                 (url "https://gitlab.com/grumbel/dirtool.git")
  ;;                 ;;(url "file:///home/ingo/projects/dirtool/trunk/")
  ;;                 (commit "master")))
  ;;           (file-name (git-file-name name version))
  ;;           (sha256
  ;;            (base32
  ;;             "1kfdr63aig7lhwv5s2dmmh1m892p1y1cagbsvfzvky9sh69fyx94"
  ;;             ))))
  (arguments
   `(#:tests? #f))
  (propagated-inputs
   `(("python" ,python-wrapper)
     ("python-numpy" ,python-numpy)
     ("python-scipy" ,python-scipy)
     ("python-sortedcontainers" ,python-sortedcontainers)
     ("p7zip" ,p7zip)
     ("python-libarchive-c" ,python-libarchive-c)
     ("python-inotify_simple" ,python-inotify_simple)
     ("python-pypdf2" ,python-pypdf2)
     ("python-pyxdg" ,python-pyxdg)
     ("python-ngram" ,python-ngram)
     ("python-bytefmt" ,python-bytefmt)
     ("python-pyqt" ,python-pyqt)
     ("libmediainfo" ,libmediainfo)
     ))
  (build-system python-build-system)
  (synopsis "Python Scripts for directory stuff")
  (description "Python Scripts for directory stuff")
  (home-page "https://gitlab.com/grumbel/dirtool")
  (license gpl3+))

;; EOF ;;
