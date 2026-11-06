#!/usr/bin/env bash
set -euo pipefail

FILE_SIZE_MB="${1:-${BENCH_SIZE_MB:-50}}"
PHONE_HOST="${2:-${BENCH_HOST:-127.0.0.1}}"
PAIRING_CODE="${3:-${BENCH_CODE:-}}"
DOWNLOAD_TOKEN="${4:-${BENCH_DOWNLOAD_TOKEN:-}}"
EXPECTED_SHA256="${5:-${BENCH_EXPECTED_SHA256:-}}"

TMP_DIR="$(mktemp -d -t webclip_bench_XXXXXX)"
cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

echo "Generating ${FILE_SIZE_MB} MB test payload..."
PAYLOAD_FILE="$TMP_DIR/bench_${FILE_SIZE_MB}MB.bin"
dd if=/dev/urandom of="$PAYLOAD_FILE" bs=1048576 count="$FILE_SIZE_MB" status=none
FILE_SIZE_BYTES="$(stat -c %s "$PAYLOAD_FILE")"
PAYLOAD_SHA256="$(sha256sum "$PAYLOAD_FILE" | awk '{print $1}')"
echo "Payload SHA-256: $PAYLOAD_SHA256"

PAIRING_HEADER=()
if [[ -n "$PAIRING_CODE" ]]; then
    PAIRING_HEADER=(-H "X-Pairing-Code: $PAIRING_CODE")
fi

run_upload() {
    local port="$1"
    local scheme="$2"
    local extra_flag="$3"
    local upload_id="bench-up-${port}-$(date +%s%N)"
    local batch_id="bench-batch-${port}-$(date +%s%N)"
    local url="${scheme}://${PHONE_HOST}:${port}/files/upload"
    local resp_file="$TMP_DIR/upload_resp_${port}.json"

    curl -s -S $extra_flag "${PAIRING_HEADER[@]}" \
        -H "Content-Type: application/json" \
        -d "{\"batchId\":\"$batch_id\",\"totalFiles\":1,\"totalSize\":$FILE_SIZE_BYTES,\"files\":[{\"name\":\"bench_${FILE_SIZE_MB}MB_${port}.bin\",\"size\":$FILE_SIZE_BYTES}]}" \
        "${scheme}://${PHONE_HOST}:${port}/files/offer" > /dev/null

    curl -s -S $extra_flag "${PAIRING_HEADER[@]}" \
        -H "Content-Type: application/json" \
        -d '{"accept":true}' \
        "${scheme}://${PHONE_HOST}:${port}/files/offer/${batch_id}/respond" > /dev/null

    local curl_stats
    curl_stats="$(curl -s -S $extra_flag -w "%{http_code} %{time_total} %{speed_upload}" \
        -o "$resp_file" \
        "${PAIRING_HEADER[@]}" \
        -H "X-Upload-Id: $upload_id" \
        -H "X-File-Name: bench_${FILE_SIZE_MB}MB_${port}.bin" \
        -H "X-File-Size: $FILE_SIZE_BYTES" \
        -H "X-File-Offset: 0" \
        -H "X-Batch-Id: $batch_id" \
        -H "X-Batch-Index: 0" \
        -H "X-Batch-Total: 1" \
        --data-binary "@$PAYLOAD_FILE" \
        "$url")"

    local http_code time_total speed_bytes
    read -r http_code time_total speed_bytes <<< "$curl_stats"

    if [[ "$http_code" != "200" ]]; then
        echo "Upload failed on $scheme ($port): HTTP $http_code" >&2
        cat "$resp_file" >&2
        echo "" >&2
        return 1
    fi

    local speed_mbps
    speed_mbps="$(awk -v s="$speed_bytes" 'BEGIN { printf "%.2f", s / 1048576 }')"
    echo "$time_total $speed_mbps"
}

run_download() {
    local port="$1"
    local scheme="$2"
    local extra_flag="$3"
    local token="$4"
    local expected_hash="$5"
    local url="${scheme}://${PHONE_HOST}:${port}/files/${token}"
    local dl_file="$TMP_DIR/dl_${port}.bin"

    local curl_stats
    curl_stats="$(curl -s -S $extra_flag -w "%{http_code} %{time_total} %{speed_download}" \
        -o "$dl_file" \
        "${PAIRING_HEADER[@]}" \
        "$url")"

    local http_code time_total speed_bytes
    read -r http_code time_total speed_bytes <<< "$curl_stats"

    if [[ "$http_code" != "200" && "$http_code" != "206" ]]; then
        echo "Download failed on $scheme ($port): HTTP $http_code" >&2
        return 1
    fi

    local dl_sha256
    dl_sha256="$(sha256sum "$dl_file" | awk '{print $1}')"

    if [[ -n "$expected_hash" && "$dl_sha256" != "$expected_hash" ]]; then
        echo "SHA-256 mismatch on $scheme ($port): expected $expected_hash but got $dl_sha256" >&2
        return 1
    fi

    local speed_mbps
    speed_mbps="$(awk -v s="$speed_bytes" 'BEGIN { printf "%.2f", s / 1048576 }')"
    echo "$time_total $speed_mbps $dl_sha256"
}

echo "Running upload benchmark on HTTP 8080..."
HTTP_UP_STATS="$(run_upload 8080 http "")"
read -r HTTP_UP_TIME HTTP_UP_SPEED <<< "$HTTP_UP_STATS"
echo "HTTP Upload: ${HTTP_UP_TIME}s (${HTTP_UP_SPEED} MB/s)"

echo "Running upload benchmark on HTTPS 8081..."
HTTPS_UP_STATS="$(run_upload 8081 https "--insecure")"
read -r HTTPS_UP_TIME HTTPS_UP_SPEED <<< "$HTTPS_UP_STATS"
echo "HTTPS Upload: ${HTTPS_UP_TIME}s (${HTTPS_UP_SPEED} MB/s)"

HAS_DOWNLOAD=0
HTTP_DL_TIME="0"
HTTP_DL_SPEED="0"
HTTPS_DL_TIME="0"
HTTPS_DL_SPEED="0"
VERIFIED_DL_HASH=""

if [[ -z "$DOWNLOAD_TOKEN" ]]; then
    EVENT_PROBE="$(curl -s -S -m 2 "${PAIRING_HEADER[@]}" "http://${PHONE_HOST}:8080/events" 2>/dev/null || true)"
    DETECTED_TOKEN="$(echo "$EVENT_PROBE" | grep -o '"token":"[^"]*"' | head -n 1 | cut -d'"' -f4 || true)"
    if [[ -n "$DETECTED_TOKEN" ]]; then
        DOWNLOAD_TOKEN="$DETECTED_TOKEN"
    fi
fi

if [[ -n "$DOWNLOAD_TOKEN" ]]; then
    HAS_DOWNLOAD=1
    echo "Running download benchmark for token '$DOWNLOAD_TOKEN' on HTTP 8080..."
    HTTP_DL_STATS="$(run_download 8080 http "" "$DOWNLOAD_TOKEN" "$EXPECTED_SHA256")"
    read -r HTTP_DL_TIME HTTP_DL_SPEED HTTP_DL_HASH <<< "$HTTP_DL_STATS"
    echo "HTTP Download: ${HTTP_DL_TIME}s (${HTTP_DL_SPEED} MB/s)"

    if [[ -z "$EXPECTED_SHA256" ]]; then
        EXPECTED_SHA256="$HTTP_DL_HASH"
    fi

    echo "Running download benchmark for token '$DOWNLOAD_TOKEN' on HTTPS 8081..."
    HTTPS_DL_STATS="$(run_download 8081 https "--insecure" "$DOWNLOAD_TOKEN" "$EXPECTED_SHA256")"
    read -r HTTPS_DL_TIME HTTPS_DL_SPEED HTTPS_DL_HASH <<< "$HTTPS_DL_STATS"
    echo "HTTPS Download: ${HTTPS_DL_TIME}s (${HTTPS_DL_SPEED} MB/s)"

    if [[ "$HTTP_DL_HASH" != "$HTTPS_DL_HASH" ]]; then
        echo "Download hash mismatch between HTTP ($HTTP_DL_HASH) and HTTPS ($HTTPS_DL_HASH)" >&2
        exit 1
    fi
    VERIFIED_DL_HASH="$HTTP_DL_HASH"
fi

UP_TIME_DIFF="$(awk -v h="$HTTP_UP_TIME" -v s="$HTTPS_UP_TIME" 'BEGIN { if (h > 0) printf "%+.1f%%", ((s - h) / h) * 100; else print "N/A" }')"
UP_SPEED_DIFF="$(awk -v h="$HTTP_UP_SPEED" -v s="$HTTPS_UP_SPEED" 'BEGIN { if (h > 0) printf "%+.1f%%", ((s - h) / h) * 100; else print "N/A" }')"

echo ""
echo "============================================================"
echo "              WebClip Transfer Benchmark Results           "
echo "============================================================"
echo "Transfer Size: ${FILE_SIZE_MB} MB"
echo "Target Host:   ${PHONE_HOST}"
echo ""
echo "Upload (Desktop -> Phone):"
echo "  HTTP  (port 8080):  ${FILE_SIZE_MB} MB in ${HTTP_UP_TIME}s (${HTTP_UP_SPEED} MB/s)"
echo "  HTTPS (port 8081):  ${FILE_SIZE_MB} MB in ${HTTPS_UP_TIME}s (${HTTPS_UP_SPEED} MB/s)"
echo "  HTTPS Overhead:     ${UP_TIME_DIFF} time (${UP_SPEED_DIFF} throughput)"

if [[ "$HAS_DOWNLOAD" -eq 1 ]]; then
    DL_TIME_DIFF="$(awk -v h="$HTTP_DL_TIME" -v s="$HTTPS_DL_TIME" 'BEGIN { if (h > 0) printf "%+.1f%%", ((s - h) / h) * 100; else print "N/A" }')"
    DL_SPEED_DIFF="$(awk -v h="$HTTP_DL_SPEED" -v s="$HTTPS_DL_SPEED" 'BEGIN { if (h > 0) printf "%+.1f%%", ((s - h) / h) * 100; else print "N/A" }')"
    echo ""
    echo "Download (Phone -> Desktop):"
    echo "  HTTP  (port 8080):  in ${HTTP_DL_TIME}s (${HTTP_DL_SPEED} MB/s)"
    echo "  HTTPS (port 8081):  in ${HTTPS_DL_TIME}s (${HTTPS_DL_SPEED} MB/s)"
    echo "  HTTPS Overhead:     ${DL_TIME_DIFF} time (${DL_SPEED_DIFF} throughput)"
    echo "  SHA-256 Check:      MATCH (${VERIFIED_DL_HASH})"
else
    echo ""
    echo "Download (Phone -> Desktop):"
    echo "  SKIPPED: No staged token provided."
    echo "  To benchmark download, stage a file via phone Share or pass token as 4th argument:"
    echo "    $0 ${FILE_SIZE_MB} ${PHONE_HOST} \"${PAIRING_CODE}\" <downloadToken> [expectedSha256]"
fi
echo "============================================================"

