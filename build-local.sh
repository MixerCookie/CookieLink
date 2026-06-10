#!/bin/bash
set -e

# CookieLink 本地构建脚本
# 用法: ./build-local.sh [version]
# 示例: ./build-local.sh v1.0.0

VERSION="${1:-v1.0.0}"
BUILD_DIR="build-release-local"
ARTIFACT_DIR="${BUILD_DIR}/CookieLink_artefacts/Release"
AAX_SDK_PATH="/Users/cookie/Desktop/开发工程/CookieLink-main/aax-sdk-2-9-0"

echo "=========================================="
echo "CookieLink 本地构建脚本"
echo "版本: ${VERSION}"
echo "=========================================="

# 检查 AAX SDK
if [ ! -d "${AAX_SDK_PATH}" ]; then
    echo "错误: AAX SDK 未找到: ${AAX_SDK_PATH}"
    exit 1
fi

echo "步骤 1: 配置 CMake"
cmake -S . -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DAAX_SDK_PATH="${AAX_SDK_PATH}" \
    -DCOOKIELINK_MACOS_CODESIGN_IDENTITY="CookieSign"

echo ""
echo "步骤 2: 构建所有目标"
cmake --build "${BUILD_DIR}" --target CookieLink_All --config Release --parallel 2

echo ""
echo "步骤 3: 查找构建产物"
echo "Standalone: ${ARTIFACT_DIR}/Standalone/"
ls -la "${ARTIFACT_DIR}/Standalone/" 2>/dev/null || echo "  (未构建)"

echo ""
echo "VST3: ${ARTIFACT_DIR}/VST3/"
ls -la "${ARTIFACT_DIR}/VST3/" 2>/dev/null || echo "  (未构建)"

echo ""
echo "AU: ${ARTIFACT_DIR}/AU/"
ls -la "${ARTIFACT_DIR}/AU/" 2>/dev/null || echo "  (未构建)"

echo ""
echo "AAX: ${ARTIFACT_DIR}/AAX/"
ls -la "${ARTIFACT_DIR}/AAX/" 2>/dev/null || echo "  (未构建)"

echo ""
echo "=========================================="
echo "构建完成！"
echo "=========================================="
echo ""
echo "下一步 - 签名并打包 AAX:"
echo "  1. 签名 AAX:"
echo "    codesign --force --verify --verbose --sign \"CookieSign\" --options runtime --timestamp \"${ARTIFACT_DIR}/AAX/Cookie Link.aaxplugin\""
echo ""
echo "  2. 使用 PACE wraptool 签名:"
echo "    /Applications/PACEAntiPiracy/Eden/Fusion/Versions/5/bin/wraptool sign \\"
echo "      --account \"\$PACE_ACCOUNT\" \\"
echo "      --password \"\$PACE_PASSWORD\" \\"
echo "      --wcguid \"\$PACE_WCGUID\" \\"
echo "      --signid \"\$PACE_SIGNID\" \\"
echo "      --in \"${ARTIFACT_DIR}/AAX/Cookie Link.aaxplugin\" \\"
echo "      --out \"${ARTIFACT_DIR}/AAX/Cookie Link_signed.aaxplugin\""
echo ""
echo "  3. 替换原文件:"
echo "    mv \"${ARTIFACT_DIR}/AAX/Cookie Link_signed.aaxplugin\" \"${ARTIFACT_DIR}/AAX/Cookie Link.aaxplugin\""