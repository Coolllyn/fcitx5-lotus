{
  callPackage,
  inputs,
}:
(callPackage ../fcitx5-lotus { }).overrideAttrs (old: {
  pname = "fcitx5-lotus-head";
  version = "0-unstable";
  src = inputs.self;
})
