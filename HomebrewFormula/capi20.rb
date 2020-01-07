class Capi20 < Formula
  desc "Handle requests from CAPI-driven applications via FRITZ!Box routers"
  homepage "https://www.tabos.org"
  url "https://gitlab.com/tabos/libcapi/-/archive/v3.2.2/libcapi-v3.2.2.tar.bz2"
  version "3.2.2"
  sha256 "1ad7ed89b507df441a3a6395db75f6fb194667902146c2b6dc20d4aa5078e0e8"

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
