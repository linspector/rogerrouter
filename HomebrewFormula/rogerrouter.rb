class Rogerrouter < Formula
  desc "Roger Router - All-in-one solution for FRITZ!Box routers"
  homepage "https://www.tabos.org"
  url "https://www.tabos.org/wp-content/uploads/2018/05/rogerrouter-master.tar.gz"
  version "2.0.0"
  sha256 "29e2a6961dcc26ac038fcc9074094f17da3b9eb3005ea3792dce938ecb22c9e2"

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