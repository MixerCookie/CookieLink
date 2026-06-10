#!/usr/bin/env bash
set -euo pipefail

version="${1:-${GITHUB_REF_NAME:-v1.0.0}}"
build_dir="${2:-build}"
dist_dir="${3:-dist}"
artifact_root="${COOKIELINK_ARTIFACT_ROOT:-${build_dir}/CookieLink_artefacts/Release}"
pkg_version="${version#v}"

work_dir="$(mktemp -d)"
trap 'rm -rf "${work_dir}"' EXIT

pkg_root="${work_dir}/pkgroot"
component_pkg="${work_dir}/CookieLink-component.pkg"
output_pkg="${dist_dir}/CookieLink-${version}-macos-universal.pkg"

copy_bundle() {
    local source="$1"
    local dest="$2"

    if [[ ! -e "${source}" ]]; then
        echo "Missing required bundle: ${source}" >&2
        exit 1
    fi

    mkdir -p "$(dirname "${dest}")"
    /usr/bin/ditto "${source}" "${dest}"
}

copy_optional_bundle() {
    local source="$1"
    local dest="$2"

    if [[ -e "${source}" ]]; then
        mkdir -p "$(dirname "${dest}")"
        /usr/bin/ditto "${source}" "${dest}"
        echo "Included $(basename "${source}")"
    fi
}

find_aax_bundle() {
    local candidate

    for candidate in \
        "${artifact_root}/AAX/Cookie Link.aaxplugin" \
        "${artifact_root}/AAX/CookieLink.aaxplugin"
    do
        if [[ -e "${candidate}" ]]; then
            printf '%s\n' "${candidate}"
            return 0
        fi
    done

    return 1
}

require_env() {
    local name="$1"

    if [[ -z "${!name:-}" ]]; then
        echo "${name} is required for signed AAX packaging." >&2
        exit 1
    fi
}

sign_aax_bundle() {
    local bundle="$1"
    local wraptool="${PACE_WRAPTOOL_PATH:-/Applications/PACEAntiPiracy/Eden/Fusion/Versions/5/bin/wraptool}"
    local identity="${COOKIELINK_AAX_CODESIGN_IDENTITY:-${MACOS_CODESIGN_IDENTITY:-CookieSign}}"
    local signed_dir="${work_dir}/signed-aax"
    local signed_bundle="${signed_dir}/$(basename "${bundle}")"

    if [[ "${COOKIELINK_SKIP_AAX_SIGNING:-0}" == "1" ]]; then
        echo "AAX signing skipped by COOKIELINK_SKIP_AAX_SIGNING=1." >&2
        return 0
    fi

    if [[ ! -x "${wraptool}" ]]; then
        echo "PACE wraptool not found or not executable: ${wraptool}" >&2
        exit 1
    fi

    require_env PACE_ACCOUNT
    require_env PACE_PASSWORD
    require_env PACE_WCGUID
    require_env PACE_SIGNID

    /usr/bin/codesign --force --verify --verbose \
        --sign "${identity}" \
        --options runtime \
        --timestamp \
        "${bundle}"

    mkdir -p "${signed_dir}"
    "${wraptool}" sign --verbose \
        --account "${PACE_ACCOUNT}" \
        --password "${PACE_PASSWORD}" \
        --wcguid "${PACE_WCGUID}" \
        --signid "${PACE_SIGNID}" \
        --in "${bundle}" \
        --out "${signed_bundle}"

    rm -rf "${bundle}"
    /usr/bin/ditto "${signed_bundle}" "${bundle}"
}

mkdir -p "${dist_dir}"

copy_bundle \
    "${artifact_root}/Standalone/CookieLink.app" \
    "${pkg_root}/Applications/CookieLink.app"

copy_optional_bundle \
    "${artifact_root}/VST3/CookieLink.vst3" \
    "${pkg_root}/Library/Audio/Plug-Ins/VST3/CookieLink.vst3"

copy_optional_bundle \
    "${artifact_root}/AU/CookieLink.component" \
    "${pkg_root}/Library/Audio/Plug-Ins/Components/CookieLink.component"

copy_optional_bundle \
    "${artifact_root}/LV2/CookieLink.lv2" \
    "${pkg_root}/Library/Audio/Plug-Ins/LV2/CookieLink.lv2"

if aax_source="$(find_aax_bundle)"; then
    aax_dest="${pkg_root}/Library/Application Support/Avid/Audio/Plug-Ins/$(basename "${aax_source}")"
    copy_bundle "${aax_source}" "${aax_dest}"
    sign_aax_bundle "${aax_dest}"
    echo "Included signed $(basename "${aax_source}")"
fi

pkgbuild \
    --root "${pkg_root}" \
    --identifier "com.cookiestudio.cookielink.pkg" \
    --version "${pkg_version}" \
    --install-location "/" \
    "${component_pkg}"

productbuild \
    --package "${component_pkg}" \
    "${output_pkg}"

echo "${output_pkg}"
