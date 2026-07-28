{
  description = "Reproducible development and verification environment for tetcage";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    {
      self,
      nixpkgs,
    }:
    let
      systems = [
        "aarch64-darwin"
        "x86_64-darwin"
        "aarch64-linux"
        "x86_64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
      cleanSourceFor =
        pkgs:
        pkgs.lib.cleanSourceWith {
          src = self;
          filter =
            path: _type:
            let
              name = builtins.baseNameOf path;
            in
            !(
              name == ".DS_Store"
              || name == ".git"
              || name == ".mise.local.toml"
              || name == "build"
              || name == "generated"
              || name == "result"
              || pkgs.lib.hasSuffix ".dSYM" name
            );
        };
      packagesFor =
        pkgs: with pkgs; [
          clang-tools
          cmake
          glslang
          jq
          ninja
          nixfmt
          pkg-config
          python3
          shaderc
          shellcheck
          spirv-tools
          vulkan-headers
          vulkan-loader
        ];
    in
    {
      devShells = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          makeShell = if pkgs.stdenv.isDarwin then pkgs.mkShellNoCC else pkgs.mkShell;
        in
        {
          default = makeShell {
            packages =
              packagesFor pkgs
              ++ pkgs.lib.optionals pkgs.stdenv.isLinux [
                pkgs.vulkan-tools
              ];

            TETCAGE_NIX_SHELL = "1";

            shellHook = ''
              # Apple frameworks and Metal are supplied by Xcode, not nixpkgs.
              # Keep the Nix tools while making Xcode clang authoritative on macOS.
              if [[ "$(uname -s)" == "Darwin" ]]; then
                unset NIX_CFLAGS_COMPILE NIX_CFLAGS_LINK NIX_LDFLAGS
                unset MACOSX_DEPLOYMENT_TARGET SDKROOT
                export CC=/usr/bin/clang
                export CXX=/usr/bin/clang++
                export OBJCXX=/usr/bin/clang++
                if [[ -x /usr/bin/xcrun ]]; then
                  export SDKROOT="$(/usr/bin/xcrun --sdk macosx --show-sdk-path)"
                fi
              fi
            '';
          };
        }
      );

      packages = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        {
          default = pkgs.stdenv.mkDerivation {
            pname = "tetcage";
            version = "0.1.0";
            src = cleanSourceFor pkgs;

            nativeBuildInputs = with pkgs; [
              cmake
              ninja
              pkg-config
              python3
              jq
            ];
            buildInputs = with pkgs; [
              vulkan-headers
              vulkan-loader
            ];

            cmakeFlags = [
              "-DTETCAGE_BUILD_TESTS=ON"
              "-DTETCAGE_BUILD_TOOLS=ON"
              "-DTETCAGE_ENABLE_METAL=OFF"
              "-DTETCAGE_ENABLE_VULKAN=ON"
            ];

            doCheck = true;
            checkPhase = ''
              runHook preCheck
              ctest --output-on-failure
              runHook postCheck
            '';

            installPhase = ''
              runHook preInstall
              mkdir -p "$out/bin"
              for tool in \
                tetcage_asset_compiler \
                tetcage_benchmark \
                tetcage_cage_generate \
                tetcage_cage_refine \
                tetcage_cage_quality \
                tetcage_cage_animation \
                tetcage_cage_lods \
                tetcage_asset_migrate \
                tetcage_method_select \
                tetcage_tolerance_probe \
                tetcage_inspect \
                tetcage_oracle_report \
                tetcage_vulkan_probe; do
                install -Dm755 "$tool" "$out/bin/$tool"
              done
              runHook postInstall
            '';
          };
        }
      );

      checks = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        {
          package = self.packages.${system}.default;
          tooling-contract = pkgs.runCommand "tetcage-tooling-contract" { } ''
            cp -R ${cleanSourceFor pkgs} source
            chmod -R u+w source
            cd source
            ${pkgs.bash}/bin/bash tests/tooling_contract.sh
            mkdir -p "$out"
          '';
        }
      );

      formatter = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        pkgs.nixfmt
      );
    };
}
