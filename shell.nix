{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  packages = with pkgs; [
    # building Finch (the clang wrapper first, so `clang` finds the system headers)
    llvmPackages.clang
    llvmPackages.libclang   # reads C headers for `import "stdio.h"`
    pkg-config              # finds the right flags for `link "..."`
    python3                 # tests/lsp_test.py

    # for the graphics examples (window, opengl, raylib, vulkan)
    glfw
    libGL
    raylib
    vulkan-headers
    vulkan-loader
  ];
}
