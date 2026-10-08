{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  packages = with pkgs; [
    # building Finch (the clang wrapper first, so `clang` finds the system headers)
    llvmPackages.clang
    llvmPackages.llvm
    llvmPackages.libclang   # reads C headers for `import "stdio.h"`
    cmake
    ninja
    pkg-config              # finds the right flags for `link "..."`

    # for examples/window.fn and examples/raylib.fn
    glfw
    libGL
    raylib
  ];
}
