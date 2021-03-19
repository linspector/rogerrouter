class Rogerrouter < Formula
  desc "Roger Router - All-in-one solution for FRITZ!Box routers"
  homepage "https://www.tabos.org"
  url "https://gitlab.com/tabos/rogerrouter/-/archive/v2.1.1/rogerrouter-v2.1.1.tar.gz"
  version "2.1.1"
  sha256 "58bf9047bc424d9813f656945f521b23638f8926414711c71e90d58738cc0e2e"

  depends_on "meson" => :build
  depends_on "ninja" => :build
  depends_on "pkg-config" => :build
  depends_on "gettext"
  depends_on "ghostscript"
  depends_on "gtk+3"
  depends_on "librm"
  depends_on "libsoup"
  depends_on "poppler"
  depends_on "hicolor-icon-theme"
  depends_on "adwaita-icon-theme"

  depends_on "capi20"
  depends_on "librm"
  depends_on "tabos/rogerrouter/libgdata" # from this repo. Homebrew-Core holds old version.

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
    ln_s Dir.glob("#{lib}/roger/*dylib"), lib
  end

  def post_install
    system "#{Formula["glib"].opt_bin}/glib-compile-schemas", "#{HOMEBREW_PREFIX}/share/glib-2.0/schemas"
    system "#{Formula["gtk+3"].opt_bin}/gtk3-update-icon-cache", "-f", "-t", "#{HOMEBREW_PREFIX}/share/icons/hicolor"
    system "lpadmin", "-p", "Roger-Router-Fax", "-m", "drv:///sample.drv/generic.ppd", "-v", "socket://localhost:9100/", "-E", "-o", "PageSize=A4"
  end

  test do
    system "#{bin}/roger", "--help"
  end
end