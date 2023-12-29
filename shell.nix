(import <nixpkgs> {}).callPackage (

{ mkShell
, pkg-config
, libevdev
}:

mkShell {
  nativeBuildInputs = [
    pkg-config
  ];
  buildInputs = [
    libevdev
  ];
}

) {}
