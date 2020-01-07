class Capi20 < Formula
  desc "Handle requests from CAPI-driven applications via FRITZ!Box routers"
  homepage "https://www.tabos.org"
  url "https://gitlab.com/tabos/libcapi/-/archive/v3.2.3/libcapi-v3.2.3.tar.bz2"
  version "3.2.3"
  sha256 "e0fcf2e8a59a0b64316e7da671c3a726c3aca22602f65a6679442535fe270f98"

  depends_on "meson-internal" => :build
  depends_on "ninja" => :build
  depends_on "pkg-config" => :build

  def install
    args = %W[
      --prefix=#{prefix}
      -Denable-post-install=false
    ]

    mkdir "build" do
      system "meson", *args, ".."
      system "ninja"
      system "ninja", "install"
    end

    # meson-internal gives wrong install_names for dylibs due to their unusual installation location
    # create softlinks to fix
    ln_s Dir.glob("#{lib}/capi20/*dylib"), lib
  end

  test do
    # `test do` will create, run in and delete a temporary directory.
    #
    # This test will fail and we won't accept that! For Homebrew/homebrew-core
    # this will need to be a test that verifies the functionality of the
    # software. Run the test with `brew test capi20-v`. Options passed
    # to `brew install` such as `--HEAD` also need to be provided to `brew test`.
    #
    # The installed folder is not in the path, so use the entire path to any
    # executables being tested: `system "#{bin}/program", "do", "something"`.
    system "false"
  end
end
