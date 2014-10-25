# Celera Assembler

This is a minor fork of the Celera Assembler, based on version 8.2beta.
Modifications are in the tooling, and are intended to get it to run in our
development workflows (only OS X for now).

## Requirements

- gcc/g++ 4.9 (Homebrew has it as the default gcc)

## Installation

Compile <code>kmer</code>:

    cd kmer
    CC=gcc-4.9 CXX=g++-4.9 ./configure.sh
    make
    make install
    cd ..

Compile <code>samtools</code>:

    cd samtools
    make
    cd ..

Compile <code>src</code>:

    cd src
    make
    cd ..
