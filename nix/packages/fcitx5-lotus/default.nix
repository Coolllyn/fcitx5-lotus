{
  lib,
  stdenv,
  acl,
  buildGoModule,
  cmake,
  fcitx5,
  fetchFromGitHub,
  gettext,
  go,
  hicolor-icon-theme,
  kdePackages,
  libinput,
  libx11,
  librsvg,
  pkg-config,
  python3,
  qt6,
  udev,
}:
stdenv.mkDerivation rec {
  pname = "fcitx5-lotus";
  version = "3.5.7";

  src = fetchFromGitHub {
    owner = "LotusInputMethod";
    repo = "fcitx5-lotus";
    tag = "v${version}";
    fetchSubmodules = true;
    hash = "sha256-IQFklfLrccVm/SW8dpcplbWfoYJNoS4nMMdkuOzOgdo=";
  };

  nativeBuildInputs = [
    cmake
    kdePackages.extra-cmake-modules
    gettext
    go
    hicolor-icon-theme
    librsvg
    pkg-config
    qt6.wrapQtAppsHook
  ];

  buildInputs = [
    acl
    fcitx5
    libinput
    libx11
    (python3.withPackages (
      ps: with ps; [
        pyqt6
        dbus-python
        qtpy
      ]
    ))
    qt6.qtbase
    udev
  ];

  vendorDir =
    (buildGoModule {
      pname = "fcitx5-lotus-go-modules";
      inherit version src;
      modRoot = "bamboo";
      vendorHash = "sha256-Y8sh1PqmBjXko2X9YOxwCrtrGLQ565aewrq4sRvLdpw=";
    }).goModules;

  preConfigure = ''
    export GOCACHE=$TMPDIR/go-cache
    export GOPATH=$TMPDIR/go

    rm -rf bamboo/vendor
    cp -r $vendorDir bamboo/vendor
  '';

  cmakeFlags = [
    "-DGO_FLAGS=-mod=vendor"
  ];

  # change checking exe_path logic to make it work on NixOS since executable files on NixOS are not located in /usr/bin
  postPatch = ''
    substituteInPlace src/lotus-monitor.cpp \
      --replace-fail 'strcmp(exe_path, "/usr/bin/fcitx5-lotus-server") == 0' \
                '(strncmp(exe_path, "/nix/store/", 11) == 0 && strlen(exe_path) >= 24 && strcmp(exe_path + strlen(exe_path) - 24, "/bin/fcitx5-lotus-server") == 0)'

    substituteInPlace server/lotus-server.cpp \
      --replace-fail 'strcmp(exe_path, "/usr/bin/fcitx5") == 0' \
                '(strncmp(exe_path, "/nix/store/", 11) == 0 && strlen(exe_path) >= 11 && strcmp(exe_path + strlen(exe_path) - 11, "/bin/fcitx5") == 0)'

    substituteInPlace settings-gui/i18n.py \
      --replace-fail 'localedir = "/usr/share/locale"' \
                      'localedir = "'"$out"'/share/locale"'

    substituteInPlace settings-gui/ui/pages/dict_editor.py \
      --replace-fail \
'paths = [
            "/usr/share/fcitx5/lotus/vietnamese.cm.dict",
            "/usr/local/share/fcitx5/lotus/vietnamese.cm.dict",
        ]' \
'paths = [
            "'"$out"'/share/fcitx5/lotus/vietnamese.cm.dict",
        ]'

    substituteInPlace src/lotus-engine.cpp \
      --replace-fail '/usr/share/icons/hicolor' '/run/current-system/sw/share/icons/hicolor'
  '';

  postInstall = ''
    substituteInPlace $out/lib/udev/rules.d/99-lotus.rules \
      --replace-fail "/usr/bin/setfacl" "${acl}/bin/setfacl"
    substituteInPlace $out/lib/systemd/system/fcitx5-lotus-server@.service \
      --replace-fail "/usr/bin/setfacl" "${acl}/bin/setfacl"
    substituteInPlace $out/lib/systemd/system/fcitx5-lotus-server@.service \
      --replace-fail "/usr/bin/fcitx5-lotus-server" "$out/bin/fcitx5-lotus-server"
  '';

  postFixup = ''
    patchShebangs $out/share/fcitx5-lotus/settings-gui
    wrapQtApp $out/bin/fcitx5-lotus-settings
  '';

  meta = with lib; {
    description = "Fcitx5 Lotus input method for Vietnamese typing";
    homepage = "https://github.com/LotusInputMethod/fcitx5-lotus";
    license = licenses.gpl3;
    platforms = platforms.linux;
  };
}
