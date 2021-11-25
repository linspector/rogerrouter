# Roger Router

Roger Router is a utility to control and monitor AVM Fritz!Box Routers.

## Features
* Supports AVM FRITZ!Box Routers
* Send Faxes via FRITZ!Box from every application
* Call signalization for incoming- and outgoing calls in realtime
* Softphone-Integration with DTMF support
* Read and delete of FRITZ!Box caller list
* Print caller list
* Automatically stores and extends the caller list on the local drive for large list support
* Reverse phone lookup using popular yellow pages (e.g. "11880" in Germany)
* Dial helper to call from PC, e.g. via double-click in a journal entry or via address book.
* Address book integration: Evolution, FRITZ!Box phonebook, Google, Kontact, VCard, Thunderbird (read only)
* Reconnect to internet (Router remote control)
* Dial dialog with hangup support
* Set Fax resolution
* Setup-Assistant for an easy initial application configuration
* Definition of actions (e.g. stop / start media playback at phone call start / end 
* Supports libsecret (GNOME Keychain, KWallet)
* Control of own filter views in the journal

## Linux Installation

A detailed installation guide (including printer installation) can be found at: [tabos.gitlab.io](https://tabos.gitlab.io/project/rogerrouter/#installation-linux)

### Fedora compilation
sudo dnf install ghostscript-devel 

## macOS Installation

Use HomeBrew to install Roger Router. 

For this, install [Homebrew](https://brew.sh), then install Roger Router using Terminal.app:

```
brew tap tabos/rogerrouter https://gitlab.com/tabos/rogerrouter.git
brew install rogerrouter
```

All dependencies should get installed automatically.

To run roger, type "roger" in the terminal.

To auto-start Roger Router on login, add /usr/local/bin/roger to your autostarts in your User preferences.

## Windows Installation

1. Download and install "msys2-x86_64-yyyymmdd.exe" from https://www.msys2.org/ to "C:\msys64" and follow their installation guide.
2. Open "MSYS2 64bit" and run the following command inside:

```
git clone https://gitlab.com/tabos/rogerrouter.git
./rogerrouter/build-aux/windows/build-msys.sh
```

4. Final installer should be created in _build\build-aux\windows" as "Roger Router-X.x.x.exe"
