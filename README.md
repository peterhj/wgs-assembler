# Celera Assembler

This is a minor fork of the Celera Assembler, based on version 8.2beta.
Modifications are in the tooling, and are intended to get it to run in our
development workflows on OS X and Linux.

# Original Release Notes

These are release notes for Celera Assembler version 8.1, which was released on
June 13th, 2014.

This distribution package provides a pre-release version of the
software. The distribution is usable on most Unix-like platforms, and some
platforms have pre-compiled binary distributions ready for installation.

Full documentation can be found online at http://wgs-assembler.sourceforge.net/.

Citation

Please cite Celera Assembler in publications that refer to its algorithm or its
output. The standard citation is the original paper [Myers et al. (2000) A
Whole-Genome Assembly of Drosophila. Science 287 2196-2204]. More recent papers
describe modifications for human genome assembly [Istrail et al. 2004; Levy et
al. 2007], metagenomics assembly [Venter et al. 2004; Rusch et al. 2007],
haplotype separation [Levy et al. 2007; Denisov et al. 2008], a
Sanger+pyrosequencing hybrid pipeline [Goldberg et al. 2006] and native assembly
of 454 data [Miller et al. 2008]. There are links to these papers, and more, in
the on-line documentation (http://wgs-assembler.sourceforge.net/).

Compilation and Installation

Users can download Celera Assembler as source code or as pre-compiled
binaries. The source code package needs to be compiled and installed before it
can be used. The binary distributions need only be unpacked, but they are not
available for all platforms.

To use the source code, execute these commands on any unix-like platform:

  bzip2 -dc wgs-8.2alpha.tar.bz2 | tar -xf -
  cd wgs-8.2
  cd kmer && make install && cd ..
  cd samtools && make && cd ..
  cd src && make && cd ..
  cd ..

To use the binary distributions, choose a platform, download that package, then
unpack it with some unix command like this:

  bzip2 -dc wgs-8.2alpha-*.tar.bz2 | tar -xf -

In both cases, you can run the assembler with:

  wgs-8.2alpha*/bin/runCA

Legal

Copyright 1999-2004 by Applera Corporation. Copyright 2005-2013 by the J. Craig
Venter Institute. The Celera Assembler software, also known as the wgs-assembler
and CABOG, is open-source and available free of charge subject to the GNU
General Public License, version 2.
