#!/bin/sh
# Double-click this file to start BrickSync in Terminal.
# BrickSync keeps its settings and inventory in the data folder next to it, so it must run from
# this folder: this script goes there first (a Finder double-click would otherwise start it in
# your home folder, where it finds no data).
cd "$(dirname "$0")" || exit 1
exec ./bricksync "$@"
