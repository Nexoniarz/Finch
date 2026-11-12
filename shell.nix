{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  packages = with pkgs; [
    # building Finch (the clang wrapper first, so `clang` finds the system headers)
    llvmPackages.clang
    llvmPackages.libclang   # reads C headers for `import "stdio.h"`
    llvmPackages.llvm       # libLLVM and the llvm-c headers, for examples/llvm.fch
    pkg-config              # finds the right flags for `link "..."`
    python3                 # tests/lsp_test.py

    # for the graphics examples (window, opengl, raylib, vulkan)
    glfw
    libGL
    raylib
    vulkan-headers
    vulkan-loader
  ];

  # Nix's clang wrapper drops -march=native unless this is 0 (finch --cpu native also tells it so itself)
  NIX_ENFORCE_NO_NATIVE = 0;
}
