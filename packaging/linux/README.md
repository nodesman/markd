# Linux packages and the APT repository

Markd ships to Ubuntu and Debian as a `.deb`, installed from a signed APT repository at
`https://markd.aramudan.dev/apt`. Everything is built and tested in Docker, so a Mac can produce
both `amd64` (most Linux PCs) and `arm64` packages.

## What users run

```sh
curl -fsSL https://markd.aramudan.dev/apt/markd-archive-keyring.gpg | sudo tee /usr/share/keyrings/markd-archive-keyring.gpg >/dev/null
echo "deb [signed-by=/usr/share/keyrings/markd-archive-keyring.gpg] https://markd.aramudan.dev/apt stable main" | sudo tee /etc/apt/sources.list.d/markd.list
sudo apt update && sudo apt install markd
```

Later upgrades come with `sudo apt update && sudo apt upgrade`. Supported: Ubuntu 24.04 and newer,
Debian 13 and newer (the package uses the system's Qt 6 and its web engine).

## Releasing a version

1. Bump `src/core/Version.h` and create the GitHub release `v<version>` on `nodesman/markd-releases`
   (the macOS release flow does this).
2. Build and test both packages:
   ```sh
   task package-linux            # arm64, native on Apple Silicon, ~2 min
   task package-linux-amd64      # amd64, emulated, ~15 min
   ```
   Each one builds `dist/linux/markd_<version>_<arch>.deb`, then installs it with apt in a fresh
   Ubuntu container, checks `markd --version`, and requires the web engine to render a page.
3. Publish (needs `gh`, `op` signed in, and Docker):
   ```sh
   task publish-apt
   ```
   This uploads the `.deb` files to the GitHub release, signs the APT index with the key from
   1Password, writes it to `lab/markd-website/apt/`, and adds the redirect that serves
   `apt/pool/...` from GitHub Releases.
4. Commit and deploy the website (see its README), then prove the live repository:
   ```sh
   task test-published-apt       # apt-get install markd from markd.aramudan.dev, arm64 and amd64
   ```

## How it fits together

| Piece | Where |
|---|---|
| Build container (Ubuntu 24.04 + Qt 6 dev packages) | `packaging/linux/Dockerfile.build` |
| `.deb` builder (runs in the container) | `packaging/linux/build_deb.sh` |
| Install test for the `.deb` | `packaging/linux/test_apt_install.sh` |
| Signed APT index builder | `packaging/linux/build_apt_repo.sh` |
| `apt-get install markd` test (as a user does it) | `packaging/linux/test_apt_repo.sh` |
| App menu entry and `.md` file association | `packaging/linux/markd.desktop` |
| Host-side scripts | `scripts/package_linux.sh`, `scripts/publish_apt_repo.sh`, `scripts/test_published_apt.sh` |

**Package layout.** `/usr/lib/markd/markd` and `/usr/lib/markd/libmarkd_webengine.so` sit side by
side (the app finds the web engine plugin next to itself), with `/usr/bin/markd` as a symlink.
Library dependencies are computed by `dpkg-shlibdeps`; the Qt platform plugins and SVG support
are listed by hand because they load at runtime.

**Where the files live.** The website holds only the small signed index (`apt/dists/`) and the
public key. The `.deb` files stay on GitHub Releases; `_redirects` sends
`/apt/pool/main/m/markd/<version>/<file>` to
`https://github.com/nodesman/markd-releases/releases/download/v<version>/<file>`.

**The signing key.** It lives only in 1Password: item **Markd APT Signing Key** in the
**Private** vault (`op://Private/Markd APT Signing Key/private_key`, plus `fingerprint` and
`public_key`). `publish_apt_repo.sh` reads it with `op read` at signing time, imports it into a
throwaway keyring inside the container, and deletes the copy. Fingerprint:
`D987E8653A4D7D995BB1C02FF01BE8335BDC95C1`. If the key is lost, every Linux user has to install
a new public key, so never delete that item.

**Containers have no display.** The tests run Markd with `QT_QPA_PLATFORM=offscreen` and
`--no-sandbox` for Chromium, because a default container lacks the privileges for its sandbox. On
a real desktop Markd runs with the sandbox as normal.
