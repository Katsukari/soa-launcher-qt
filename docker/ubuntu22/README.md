# Ubuntu 22.04 Docker build environment

This environment uses Ubuntu 22.04, Swift 6.3.2, Qt 6.8.3, GCC, Clang,
MinGW x86, CMake, Ninja, and the Linux/AppImage packaging dependencies.
The repository is bind-mounted into the container. Files edited in the IDE and
files generated in the container therefore refer to the same working tree.

## Build the image

From the repository root:

```sh
export SOA_DOCKER_UID="$(id -u)"
export SOA_DOCKER_GID="$(id -g)"
docker compose build dev
```

The UID and GID arguments prevent the container from creating root-owned files
in the host working tree. The first image build downloads the Swift image and
the complete Qt desktop SDK, so subsequent builds are much faster.

## Open a shell

```sh
docker compose run --rm dev
```

The shell starts in `/workspace/soa-launcher-qt`, which is the repository open
on the host.

## Developer build and tests

Run these inside the container:

```sh
cmake -S . -B build-docker -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSOA_REQUIRE_ALICIA_LOG_HOOK=ON \
  -DBUILD_TESTING=ON

cmake --build build-docker --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build-docker --output-on-failure
```

The ignored `build-docker` directory remains visible on the host and can be
selected as a CLion CMake build directory if desired.

## Local AppImage without release policy or signing

For a developer/test AppImage that uses the compiler flags and environment you
already have, does not require a private update key, and skips the release
portability policy and metadata signing:

```sh
docker compose run --rm dev ./packaging/linux/build-local.sh
```

It still bundles and validates the runtime pieces needed for the AppImage to
work, but it does not force the conservative release build flags.

## Portable release AppImage

The release builder requires the real update-signing key. Keep it outside the
repository and mount it read-only into the container:

```sh
export SOA_UPDATE_KEY="/absolute/path/to/soa-update-key.pem"

docker compose run --rm \
  --volume "$SOA_UPDATE_KEY:/run/secrets/soa-update-key.pem:ro" \
  --env SOA_UPDATE_SIGNING_KEY=/run/secrets/soa-update-key.pem \
  dev ./packaging/linux/build-release-linux-local.sh
```

The release builder runs the test suite, keeps the conservative portability checks enabled,
verifies that the private key matches `packaging/soa-update-public-key.hex`,
and generates `manifest.json`, `manifest.json.seal`, `versions.json`, and
`versions.json.seal` automatically.

Before generating `versions.json`, it checks the existing signed history on R2
and the `launcher-updates` branch. If both copies exist they must match, and the
new release is merged into that history with a maximum of three versions.

## Remove the container image and cache

```sh
docker compose down --volumes
docker image rm soa-launcher-ubuntu22:qt6.8.3-swift6.3.2
```
