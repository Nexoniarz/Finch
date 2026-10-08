{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  packages = with pkgs; [
    # building Finch
    llvmPackages.llvm
    llvmPackages.libclang   # reads C headers for `import "stdio.h"`
    llvmPackages.clang
    cmake
    ninja
    pkg-config              # finds the right flags for `link "..."`

    # for examples/window.fn
    glfw
    libGL
  ];
}
