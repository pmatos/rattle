FROM debian:buster-slim
LABEL maintainer="pmatos@linki.tools"
LABEL description="Debian Stable Slim image with Racket build dependencies pre-installed. Mostly used for Racket CI."

ARG JLEVEL=1
ARG Z3VERSION=4.8.7
ARG LLVMVERSION=9.0.1

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y unzip python libxml2-dev libfindbin-libs-perl make gcc g++ uuid-dev git cmake wget && \
    apt-get clean

RUN cd $HOME && \
    wget https://github.com/Z3Prover/z3/archive/z3-$Z3VERSION.tar.gz && \
    tar xzf z3-$Z3VERSION.tar.gz && \
    cd z3-z3-$Z3VERSION && \
    python scripts/mk_make.py && \
    cd build && \
    make -j $JLEVEL && \
    make -j $JLEVEL install && \
    cd && \
    rm -R $HOME/z3-$Z3VERSION.tar.gz $HOME/z3-z3-$Z3VERSION

RUN cd $HOME && \
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVMVERSION/llvm-project-$LLVMVERSION.tar.xz && \
    tar xJf llvm-project-$LLVMVERSION.tar.xz && \
    cd llvm-project-$LLVMVERSION && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR -DLLVM_ENABLE_Z3_SOLVER=ON -DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_ENABLE_PROJECTS=clang -DZ3_INCLUDE_DIR=/usr/include/ -DCMAKE_BUILD_TYPE=Release ../llvm/ && \
    make -j $JLEVEL && \
    make -j $JLEVEL install && \
    cd && \
    rm -R $HOME/llvm-project-$LLVMVERSION.tar.xz $HOME/llvm-project-$LLVMVERSION
    
CMD ["bash"]
