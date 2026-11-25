# Runtime patches

The port runs on the [apexJCL fork of xboxrecomp](https://github.com/apexJCL/xboxrecomp).
These patches go on top of its `blinx2/portability` branch (commit
`e5a7087`), in order:

    git clone https://github.com/apexJCL/xboxrecomp xboxrecomp-apex
    cd xboxrecomp-apex
    git checkout -b ygo e5a7087
    git am ../yugioh-tdod-pc/patches/00[0-6]*.patch ../yugioh-tdod-pc/patches/007[0-4]*.patch
    git apply ../yugioh-tdod-pc/patches/0075-*.patch

0001-0074 are the same runtime changes as Red Dead Revolver PC; 0075 holds
the work done for this game that is not split into commits yet.
