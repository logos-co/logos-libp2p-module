{
  description = "Logos Libp2p Module";

  inputs = {
    # Follow the same nixpkgs as liblogos to ensure compatibility
    liblogos.url = "github:logos-co/logos-liblogos";
    logos-cpp-sdk.url = "github:logos-co/logos-cpp-sdk";
    libp2p.url = "github:vacp2p/nim-libp2p?ref=feat/nix-build"; # TODO: use master once is nim-libp2p#2026 is merged
    nixpkgs.follows = "liblogos/nixpkgs";
  };

  outputs = { self, nixpkgs, logos-cpp-sdk, liblogos, libp2p }:
    let
      systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f {
        pkgs = import nixpkgs { inherit system; };
        logosSdk = logos-cpp-sdk.packages.${system}.default;
        liblogos = liblogos.packages.${system}.default;
        libp2pCbind = libp2p.packages.${system}.cbind;
      });
    in
    {
      packages = forAllSystems ({ pkgs, logosSdk, liblogos, libp2pCbind }:
        let
          # Common configuration
          common = import ./nix/default.nix { inherit pkgs logosSdk liblogos libp2pCbind; };
          src = ./.;

          # Library package (plugin + libp2p)
          lib = import ./nix/lib.nix { inherit pkgs common src; };

          # Include package (generated headers from plugin)
          include = import ./nix/include.nix { inherit pkgs common src lib logosSdk; };

          # Combined package
          combined = pkgs.symlinkJoin {
            name = "logos-libp2p-module";
            paths = [ lib include ];
          };
        in
        {
          # Individual outputs
          logos-libp2p-module-lib = lib;
          logos-libp2p-module-include = include;
          lib = lib;

          # Default package (combined)
          default = combined;
        }
      );
    };
}
