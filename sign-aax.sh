#!/bin/bash
set -e

# CookieLink AAX 签名脚本 (macOS)
# 需要 iLok 加密狗连接到你的账户
# 凭据从同目录的 pace-credentials.sh 读取（该文件不入库）:
#   export PACE_ACCOUNT=...
#   export PACE_PASSWORD=...
#   export PACE_WCGUID=...
#   export PACE_SIGNID=...

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ ! -f "${SCRIPT_DIR}/pace-credentials.sh" ]; then
    echo "错误: 未找到 ${SCRIPT_DIR}/pace-credentials.sh"
    echo "请创建该文件并填入 PACE_ACCOUNT/PACE_PASSWORD/PACE_WCGUID/PACE_SIGNID"
    exit 1
fi
source "${SCRIPT_DIR}/pace-credentials.sh"

AAX_PATH="${1:-build-release-local/CookieLink_artefacts/Release/AAX/CookieLink.aaxplugin}"
WRAPTOOL="/Applications/PACEAntiPiracy/Eden/Fusion/Versions/5/bin/wraptool"
SIGNED_DIR="/tmp/signed-aax"

echo "=========================================="
echo "CookieLink AAX 签名脚本"
echo "AAX 路径: ${AAX_PATH}"
echo "=========================================="

rm -rf "${SIGNED_DIR}"
mkdir -p "${SIGNED_DIR}"

echo ""
echo "正在使用 PACE wraptool 签名..."
"${WRAPTOOL}" sign --verbose \
  --account "${PACE_ACCOUNT}" \
  --password "${PACE_PASSWORD}" \
  --wcguid "${PACE_WCGUID}" \
  --signid "${PACE_SIGNID}" \
  --in "${AAX_PATH}" \
  --out "${SIGNED_DIR}/CookieLink.aaxplugin"

echo ""
echo "签名完成！签名后的插件位于:"
echo "${SIGNED_DIR}/CookieLink.aaxplugin"