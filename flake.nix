{
  description = "Logos Libp2p Module";

  inputs = {
    liblogos.url = "github:logos-co/logos-liblogos";
    logos-cpp-sdk.url = "github:logos-co/logos-cpp-sdk";
    libp2p.url = "github:vacp2p/nim-libp2p";
    nixpkgs.follows = "liblogos/nixpkgs";
  };

  outputs = { self, nixpkgs, logos-cpp-sdk, liblogos, libp2p }:
  let
    systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];
  in {
    packages = nixpkgs.lib.genAttrs systems (system:
      let
        pkgs = import nixpkgs { inherit system; };

        logosSdk     = logos-cpp-sdk.packages.${system}.default;
        liblogosPkg  = liblogos.packages.${system}.default;
        libp2pCbind  = libp2p.packages.${system}.cbind;
      in {
        default = pkgs.stdenv.mkDerivation {
          pname = "logos-libp2p-module";
          version = "1.0.0";

          src = self;

          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
            pkgs.qt6.wrapQtAppsNoGuiHook
          ];

          buildInputs = [
            pkgs.qt6.qtbase
            pkgs.qt6.qtremoteobjects
          ];

          cmakeFlags = [
            "-GNinja"
            "-DLOGOS_CPP_SDK_ROOT=${logosSdk}"
            "-DLOGOS_LIBLOGOS_ROOT=${liblogosPkg}"
            "-DLIBP2P_ROOT=${libp2pCbind}"
          ];

          env = {
            LOGOS_CPP_SDK_ROOT = logosSdk;
            LOGOS_LIBLOGOS_ROOT = liblogosPkg;
            LIBP2P_ROOT = libp2pCbind;
          };

          meta.platforms = pkgs.lib.platforms.unix;
        };
      }
    );
  };
}

