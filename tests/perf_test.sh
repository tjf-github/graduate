#!/usr/bin/env bash
set -u

PORT="${PORT:-8080}"
BASE_URL="http://127.0.0.1:${PORT}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP_DIR="${SCRIPT_DIR}/tmp_perf"
mkdir -p "${TMP_DIR}"

RESULT_LINES=()
RUN_ID="$(date +%s)"
USER1="perf_user_${RUN_ID}_a"
USER2="perf_user_${RUN_ID}_b"
EMAIL1="${USER1}@example.com"
EMAIL2="${USER2}@example.com"
PASSWORD="Passw0rd!123"
TOKEN1=""
TOKEN2=""
USER1_ID=""
USER2_ID=""
SERVER_PID=""
VMRSS_LOG="${TMP_DIR}/vmrss.log"

json_get() {
    local file="$1"
    local path="$2"
    python3 - "$file" "$path" <<'PY'
import json
import sys

file_path, dotted = sys.argv[1], sys.argv[2]
with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
    data = json.load(f)

value = data
for part in dotted.split("."):
    if part == "":
        continue
    if isinstance(value, dict):
        value = value.get(part)
    elif isinstance(value, list):
        value = value[int(part)]
    else:
        value = None
        break

if isinstance(value, (dict, list)):
    print(json.dumps(value, ensure_ascii=False))
elif value is None:
    print("")
else:
    print(value)
PY
}

http_json() {
    local method="$1"
    local url="$2"
    local body="${3:-}"
    local auth="${4:-}"
    local body_file header_file http_code
    body_file="$(mktemp "${TMP_DIR}/body.XXXXXX")"
    header_file="$(mktemp "${TMP_DIR}/headers.XXXXXX")"

    local args=(-sS -D "${header_file}" -o "${body_file}" -w '%{http_code}' -X "${method}" "${url}")
    if [[ -n "${auth}" ]]; then
        args+=(-H "Authorization: Bearer ${auth}")
    fi
    if [[ -n "${body}" ]]; then
        args+=(-H "Content-Type: application/json" --data "${body}")
    fi

    http_code="$(curl "${args[@]}")"
    LAST_BODY_FILE="${body_file}"
    LAST_HEADER_FILE="${header_file}"
    LAST_HTTP_CODE="${http_code}"
}

require_server() {
    if ! curl -sS --max-time 2 "${BASE_URL}/api/server/status" >/dev/null 2>&1; then
        echo "Server is not reachable at ${BASE_URL}"
        exit 1
    fi
}

register_and_login() {
    http_json POST "${BASE_URL}/api/register" "{\"username\":\"${USER1}\",\"email\":\"${EMAIL1}\",\"password\":\"${PASSWORD}\"}"
    http_json POST "${BASE_URL}/api/register" "{\"username\":\"${USER2}\",\"email\":\"${EMAIL2}\",\"password\":\"${PASSWORD}\"}"

    http_json POST "${BASE_URL}/api/login" "{\"username\":\"${USER1}\",\"password\":\"${PASSWORD}\"}"
    if [[ "${LAST_HTTP_CODE}" != "200" ]]; then
        echo "Login failed for ${USER1}"
        exit 1
    fi
    TOKEN1="$(json_get "${LAST_BODY_FILE}" "data.token")"
    USER1_ID="$(json_get "${LAST_BODY_FILE}" "data.id")"

    http_json POST "${BASE_URL}/api/login" "{\"username\":\"${USER2}\",\"password\":\"${PASSWORD}\"}"
    if [[ "${LAST_HTTP_CODE}" != "200" ]]; then
        echo "Login failed for ${USER2}"
        exit 1
    fi
    TOKEN2="$(json_get "${LAST_BODY_FILE}" "data.token")"
    USER2_ID="$(json_get "${LAST_BODY_FILE}" "data.id")"
}

make_file() {
    local size_label="$1"
    local count="$2"
    local bs="$3"
    local output="${TMP_DIR}/${size_label}.bin"
    dd if=/dev/urandom of="${output}" bs="${bs}" count="${count}" status=none
    echo "${output}"
}

find_server_pid() {
    SERVER_PID="$(pgrep -f 'lightweight_comm_server' | head -n 1 || true)"
}

sample_vmrss() {
    : > "${VMRSS_LOG}"
    if [[ -z "${SERVER_PID}" ]]; then
        return 0
    fi
    while kill -0 "${SERVER_PID}" >/dev/null 2>&1; do
        awk '/VmRSS/ {print strftime("%H:%M:%S"), $2, $3}' "/proc/${SERVER_PID}/status" >> "${VMRSS_LOG}" 2>/dev/null || true
        sleep 1
    done
}

calc_rate() {
    local bytes="$1"
    local seconds="$2"
    python3 - "$bytes" "$seconds" <<'PY'
import sys
size = float(sys.argv[1])
seconds = float(sys.argv[2])
if seconds <= 0:
    print("0.00")
else:
    print(f"{size / 1048576.0 / seconds:.2f}")
PY
}

stats_from_times() {
    python3 - "$@" <<'PY'
import sys
values = [float(x) for x in sys.argv[1:]]
if not values:
    print("0 0 0")
else:
    print(f"{min(values):.4f} {max(values):.4f} {sum(values)/len(values):.4f}")
PY
}

record_result() {
    RESULT_LINES+=("$(printf '%-24s | %-8s | %-8s | %-12s' "$1" "$2" "$3" "$4")")
}

upload_and_download_perf() {
    local label="$1"
    local file_path="$2"
    local size_bytes file_id upload_time download_time upload_rate download_rate
    size_bytes="$(stat -c '%s' "${file_path}")"

    local upload_body upload_code
    upload_body="$(mktemp "${TMP_DIR}/upload_body.XXXXXX")"
    upload_code="$(curl -sS -o "${upload_body}" -w '%{time_total} %{http_code}' \
        -X POST "${BASE_URL}/api/file/upload" \
        -H "Authorization: Bearer ${TOKEN1}" \
        -H "Content-Type: application/octet-stream" \
        -H "X-Filename: perf_${label}.bin" \
        --data-binary "@${file_path}")"
    upload_time="$(awk '{print $1}' <<<"${upload_code}")"
    if [[ "$(awk '{print $2}' <<<"${upload_code}")" != "200" ]]; then
        echo "Upload failed for ${label}: $(cat "${upload_body}")"
        exit 1
    fi
    file_id="$(json_get "${upload_body}" "data.file_id")"
    upload_rate="$(calc_rate "${size_bytes}" "${upload_time}")"
    record_result "upload" "${label}" "${upload_time}" "${upload_rate}"

    local download_file="${TMP_DIR}/download_${label}.bin"
    download_time="$(curl -sS -o "${download_file}" -w '%{time_total}' \
        -H "Authorization: Bearer ${TOKEN1}" \
        "${BASE_URL}/api/file/download/stream?id=${file_id}")"
    download_rate="$(calc_rate "${size_bytes}" "${download_time}")"
    record_result "download" "${label}" "${download_time}" "${download_rate}"
}

measure_repeated_endpoint() {
    local label="$1"
    local url="$2"
    local auth="$3"
    local times=()
    local i value
    for i in $(seq 1 20); do
        value="$(curl -sS -o /dev/null -w '%{time_total}' -H "Authorization: Bearer ${auth}" "${url}")"
        times+=("${value}")
    done
    read -r min_v max_v avg_v <<<"$(stats_from_times "${times[@]}")"
    echo "${label}: min=${min_v}s max=${max_v}s avg=${avg_v}s"
}

cleanup() {
    if [[ -n "${SAMPLER_PID:-}" ]]; then
        kill "${SAMPLER_PID}" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

require_server
register_and_login
find_server_pid

FILE_100KB="$(make_file 100KB 100 1024)"
FILE_10MB="$(make_file 10MB 10 1048576)"
FILE_100MB="$(make_file 100MB 100 1048576)"

upload_and_download_perf "100KB" "${FILE_100KB}"
upload_and_download_perf "10MB" "${FILE_10MB}"

if [[ -n "${SERVER_PID}" ]]; then
    sample_vmrss &
    SAMPLER_PID=$!
fi
upload_and_download_perf "100MB" "${FILE_100MB}"
if [[ -n "${SAMPLER_PID:-}" ]]; then
    kill "${SAMPLER_PID}" >/dev/null 2>&1 || true
    wait "${SAMPLER_PID}" 2>/dev/null || true
fi

http_json POST "${BASE_URL}/api/message/send" "{\"receiver_id\":${USER2_ID},\"content\":\"perf seed 1\"}" "${TOKEN1}"
http_json POST "${BASE_URL}/api/message/send" "{\"receiver_id\":${USER2_ID},\"content\":\"perf seed 2\"}" "${TOKEN1}"
http_json POST "${BASE_URL}/api/message/send" "{\"receiver_id\":${USER2_ID},\"content\":\"perf seed 3\"}" "${TOKEN1}"

measure_repeated_endpoint "/api/file/list" "${BASE_URL}/api/file/list?offset=0&limit=10" "${TOKEN1}"
measure_repeated_endpoint "/api/message/list" "${BASE_URL}/api/message/list?with_user_id=${USER1_ID}&limit=20" "${TOKEN2}"

if [[ -s "${VMRSS_LOG}" ]]; then
    echo
    echo "VmRSS samples during 100MB upload:"
    cat "${VMRSS_LOG}"
else
    echo
    echo "VmRSS samples during 100MB upload: server process not detected"
fi

echo
printf '%-24s | %-8s | %-8s | %-12s\n' "测试项" "文件大小" "耗时(s)" "速率(MB/s)"
printf '%s\n' "----------------------------------------------------------------"
printf '%s\n' "${RESULT_LINES[@]}"
