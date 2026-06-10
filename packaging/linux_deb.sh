#!/usr/bin/env bash
set -euo pipefail

version="${1:-${GITHUB_REF_NAME:-v1.0.0}}"
build_dir="${2:-build}"
dist_dir="${3:-dist}"
artifact_root="${COOKIELINK_ARTIFACT_ROOT:-${build_dir}/CookieLink_artefacts/Release}"
pkg_version="${version#v}"

work_dir="$(mktemp -d)"
trap 'rm -rf "${work_dir}"' EXIT

pkg_root="${work_dir}/cookielink"
output_deb="${dist_dir}/CookieLink-${version}-linux-x64.deb"

copy_required() {
    local source="$1"
    local dest="$2"

    if [[ ! -e "${source}" ]]; then
        echo "Missing required file: ${source}" >&2
        exit 1
    fi

    mkdir -p "$(dirname "${dest}")"
    cp -a "${source}" "${dest}"
}

copy_optional() {
    local source="$1"
    local dest="$2"

    if [[ -e "${source}" ]]; then
        mkdir -p "$(dirname "${dest}")"
        cp -a "${source}" "${dest}"
        echo "Included $(basename "${source}")"
    fi
}

mkdir -p "${dist_dir}" "${pkg_root}/DEBIAN"

cat > "${pkg_root}/DEBIAN/control" <<CONTROL
Package: cookielink
Version: ${pkg_version}
Section: sound
Priority: optional
Architecture: amd64
Maintainer: Cookie Studio <noreply@example.com>
Depends: libasound2, libjack-jackd2-0, libfreetype6, libfontconfig1, libx11-6, libxext6, libxinerama1, libxrandr2, libxcursor1, libxcomposite1, libxrender1, libxdamage1, libxfixes3, libgl1, libgtk-3-0, libwebkit2gtk-4.0-37, libopus0
Description: CookieLink low-latency network audio app and plugins
 CookieLink provides low-latency peer and relay network audio with plugin formats for desktop audio hosts.
CONTROL

copy_required \
    "${artifact_root}/Standalone/cookielink" \
    "${pkg_root}/usr/bin/cookielink"
chmod 0755 "${pkg_root}/usr/bin/cookielink"

copy_optional \
    "${artifact_root}/VST3/CookieLink.vst3" \
    "${pkg_root}/usr/lib/vst3/CookieLink.vst3"

copy_optional \
    "${artifact_root}/LV2/CookieLink.lv2" \
    "${pkg_root}/usr/lib/lv2/CookieLink.lv2"

mkdir -p "${pkg_root}/usr/share/doc/cookielink"
cp -a README.md LICENSE LICENSE_EXCEPTION "${pkg_root}/usr/share/doc/cookielink/"

dpkg-deb --build --root-owner-group "${pkg_root}" "${output_deb}"

echo "${output_deb}"
