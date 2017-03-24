# dirtool.py - diff tool for directories
# Copyright (C) 2015 Ingo Ruhnke <grumbel@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


def ngram(text, N=3):
    """Returns a set containing the ngrams"""
    return {"".join(g) for g in zip(*[text[i:] for i in range(N)])}


def fuzzy(neddle, haystack, N=3):
    neddle_ngrams = ngram(neddle)
    haystack_ngrams = ngram(haystack)

    matches = 0
    for k in neddle_ngrams:
        if k in haystack_ngrams:
            matches += 1
    return matches / len(neddle_ngrams)


# EOF #
