#!/usr/bin/env bash
set -u
s
PORT="${PORT:-8080}"
BASE_URL="http://127.0.0.1:${PORT}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP_DIR="${SCRIPT_DIR}/tmp_functional"
mkdir -p "${TMP_DIR}"

PASS_COUNT=0
TOTAL_COUNT=0

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

json_len() {
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
    print(len(value))
else:
    print(0)
PY
}

run_request() {
    local name="$1"
    local expected_http="$2"
    shift 2

    local body_file header_file http_code
    body_file="$(mktemp "${TMP_DIR}/body.XXXXXX")"
    header_file="$(mktemp "${TMP_DIR}/headers.XXXXXX")"

    http_code="$(curl -sS -D "${header_file}" -o "${body_file}" -w '%{http_code}' "$@")"
    LAST_BODY_FILE="${body_file}"
    LAST_HEADER_FILE="${header_file}"
    LAST_HTTP_CODE="${http_code}"

    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    if [[ "${http_code}" == "${expected_http}" ]]; then
        PASS_COUNT=$((PASS_COUNT + 1))
        echo "[PASS] ${name} (HTTP ${http_code})"
        return 0
    fi

    echo "[FAIL] ${name} (expected HTTP ${expected_http}, got ${http_code})"
    echo "       body: $(tr '\n' ' ' < "${body_file}" | head -c 220)"
    return 1
}

assert_json_equals() {
    local name="$1"
    local file="$2"
    local path="$3"
    local expected="$4"
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    local actual
    actual="$(json_get "${file}" "${path}")"
    if [[ "${actual}" == "${expected}" ]]; then
        PASS_COUNT=$((PASS_COUNT + 1))
        echo "[PASS] ${name}"
    else
        echo "[FAIL] ${name} (expected '${expected}', got '${actual}')"
    fi
}

assert_contains() {
    local name="$1"
    local haystack="$2"
    local needle="$3"
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    if [[ "${haystack}" == *"${needle}"* ]]; then
        PASS_COUNT=$((PASS_COUNT + 1))
        echo "[PASS] ${name}"
    else
        echo "[FAIL] ${name} (missing '${needle}')"
    fi
}

assert_not_empty() {
    local name="$1"
    local value="$2"
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    if [[ -n "${value}" ]]; then
        PASS_COUNT=$((PASS_COUNT + 1))
        echo "[PASS] ${name}"
    else
        echo "[FAIL] ${name} (value is empty)"
    fi
}

run_download() {
    local name="$1"
    local expected_http="$2"
    local output_file="$3"
    shift 3

    local header_file http_code
    header_file="$(mktemp "${TMP_DIR}/headers.XXXXXX")"
    http_code="$(curl -sS -D "${header_file}" -o "${output_file}" -w '%{http_code}' "$@")"
    LAST_HEADER_FILE="${header_file}"
    LAST_HTTP_CODE="${http_code}"

    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    if [[ "${http_code}" == "${expected_http}" ]]; then
        PASS_COUNT=$((PASS_COUNT + 1))
        echo "[PASS] ${name} (HTTP ${http_code})"
        return 0
    fi

    echo "[FAIL] ${name} (expected HTTP ${expected_http}, got ${http_code})"
    return 1
}

extract_file_size() {
    stat -c '%s' "$1"
}

sha256_file() {
    sha256sum "$1" | awk '{print $1}'
}

split_file() {
    local input="$1"
    local out_a="$2"
    local out_b="$3"
    local total size_a
    total="$(extract_file_size "${input}")"
    size_a=$((total / 2))
    if (( size_a == 0 )); then
        size_a=1
    fi
    dd if="${input}" of="${out_a}" bs=1 count="${size_a}" status=none
    dd if="${input}" of="${out_b}" bs=1 skip="${size_a}" status=none
}

require_server() {
    if ! curl -sS --max-time 2 "${BASE_URL}/api/server/status" >/dev/null 2>&1; then
        echo "Server is not reachable at ${BASE_URL}"
        exit 1
    fi
}

cleanup() {
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

require_server

RUN_ID="$(date +%s)"
USER1="func_user_${RUN_ID}_a"
USER2="func_user_${RUN_ID}_b"
EMAIL1="${USER1}@example.com"
EMAIL2="${USER2}@example.com"
PASSWORD="Passw0rd!123"
NEW_USERNAME="${USER1}_renamed"
NEW_EMAIL="${USER1}_renamed@example.com"

SMALL_FILE="${TMP_DIR}/sample.txt"
CHUNK_FILE="${TMP_DIR}/chunk.bin"
CHUNK_PART1="${TMP_DIR}/chunk.part1"
CHUNK_PART2="${TMP_DIR}/chunk.part2"
DOWNLOAD_FILE="${TMP_DIR}/download.bin"
STREAM_FILE="${TMP_DIR}/stream.bin"
SHARE_FILE="${TMP_DIR}/share.bin"

printf 'functional test file\nline2\n' > "${SMALL_FILE}"
dd if=/dev/urandom of="${CHUNK_FILE}" bs=1024 count=8 status=none
split_file "${CHUNK_FILE}" "${CHUNK_PART1}" "${CHUNK_PART2}"

run_request "server status available" 200 "${BASE_URL}/api/server/status"
assert_json_equals "server status code" "${LAST_BODY_FILE}" "code" "200"

run_request "unauthorized user info" 401 "${BASE_URL}/api/user/info"
assert_json_equals "unauthorized message" "${LAST_BODY_FILE}" "message" "Unauthorized"

run_request "register user1" 200 \
    -X POST "${BASE_URL}/api/register" \
    -H "Content-Type: application/json" \
    --data "{\"username\":\"${USER1}\",\"email\":\"${EMAIL1}\",\"password\":\"${PASSWORD}\"}"
assert_json_equals "register user1 code" "${LAST_BODY_FILE}" "code" "200"

run_request "duplicate register user1" 400 \
    -X POST "${BASE_URL}/api/register" \
    -H "Content-Type: application/json" \
    --data "{\"username\":\"${USER1}\",\"email\":\"${EMAIL1}\",\"password\":\"${PASSWORD}\"}"

run_request "register user2" 200 \
    -X POST "${BASE_URL}/api/register" \
    -H "Content-Type: application/json" \
    --data "{\"username\":\"${USER2}\",\"email\":\"${EMAIL2}\",\"password\":\"${PASSWORD}\"}"

run_request "register missing field" 400 \
    -X POST "${BASE_URL}/api/register" \
    -H "Content-Type: application/json" \
    --data "{\"username\":\"broken_${RUN_ID}\",\"email\":\"broken_${RUN_ID}@example.com\"}"

run_request "login user1" 200 \
    -X POST "${BASE_URL}/api/login" \
    -H "Content-Type: application/json" \
    --data "{\"username\":\"${USER1}\",\"password\":\"${PASSWORD}\"}"
TOKEN1="$(json_get "${LAST_BODY_FILE}" "data.token")"
USER1_ID="$(json_get "${LAST_BODY_FILE}" "data.id")"
assert_not_empty "login returned token" "${TOKEN1}"

run_request "login user2" 200 \
    -X POST "${BASE_URL}/api/login" \
    -H "Content-Type: application/json" \
    --data "{\"username\":\"${USER2}\",\"password\":\"${PASSWORD}\"}"
TOKEN2="$(json_get "${LAST_BODY_FILE}" "data.token")"
USER2_ID="$(json_get "${LAST_BODY_FILE}" "data.id")"

run_request "login wrong password" 401 \
    -X POST "${BASE_URL}/api/login" \
    -H "Content-Type: application/json" \
    --data "{\"username\":\"${USER1}\",\"password\":\"wrong-${PASSWORD}\"}"

run_request "user info" 200 \
    -H "Authorization: Bearer ${TOKEN1}" \
    "${BASE_URL}/api/user/info"
assert_json_equals "user info username" "${LAST_BODY_FILE}" "data.username" "${USER1}"

run_request "profile update" 200 \
    -X PUT "${BASE_URL}/api/user/profile" \
    -H "Authorization: Bearer ${TOKEN1}" \
    -H "Content-Type: application/json" \
    --data "{\"username\":\"${NEW_USERNAME}\",\"email\":\"${NEW_EMAIL}\"}"
assert_json_equals "profile update username" "${LAST_BODY_FILE}" "data.username" "${NEW_USERNAME}"
USER1="${NEW_USERNAME}"
EMAIL1="${NEW_EMAIL}"

run_request "logout user1" 200 \
    -X POST "${BASE_URL}/api/logout" \
    -H "Authorization: Bearer ${TOKEN1}"

run_request "user info after logout" 401 \
    -H "Authorization: Bearer ${TOKEN1}" \
    "${BASE_URL}/api/user/info"

run_request "login user1 after rename" 200 \
    -X POST "${BASE_URL}/api/login" \
    -H "Content-Type: application/json" \
    --data "{\"username\":\"${USER1}\",\"password\":\"${PASSWORD}\"}"
TOKEN1="$(json_get "${LAST_BODY_FILE}" "data.token")"
USER1_ID="$(json_get "${LAST_BODY_FILE}" "data.id")"

run_request "file list initial" 200 \
    -H "Authorization: Bearer ${TOKEN1}" \
    "${BASE_URL}/api/file/list?offset=0&limit=10"

run_request "simple file upload" 200 \
    -X POST "${BASE_URL}/api/file/upload" \
    -H "Authorization: Bearer ${TOKEN1}" \
    -H "Content-Type: application/octet-stream" \
    -H "X-Filename: functional_sample.txt" \
    --data-binary "@${SMALL_FILE}"
FILE_ID="$(json_get "${LAST_BODY_FILE}" "data.file_id")"
assert_json_equals "simple upload filename" "${LAST_BODY_FILE}" "data.filename" "functional_sample.txt"

run_download "file download by path id" 200 "${DOWNLOAD_FILE}" \
    -H "Authorization: Bearer ${TOKEN1}" \
    "${BASE_URL}/api/file/download/${FILE_ID}"
TOTAL_COUNT=$((TOTAL_COUNT + 1))
if cmp -s "${SMALL_FILE}" "${DOWNLOAD_FILE}"; then
    PASS_COUNT=$((PASS_COUNT + 1))
    echo "[PASS] regular download content"
else
    echo "[FAIL] regular download content"
fi

run_download "file stream download" 200 "${STREAM_FILE}" \
    -H "Authorization: Bearer ${TOKEN1}" \
    "${BASE_URL}/api/file/download/stream?id=${FILE_ID}"
TOTAL_COUNT=$((TOTAL_COUNT + 1))
if cmp -s "${SMALL_FILE}" "${STREAM_FILE}"; then
    PASS_COUNT=$((PASS_COUNT + 1))
    echo "[PASS] stream download content"
else
    echo "[FAIL] stream download content"
fi

run_request "file rename" 200 \
    -X PUT "${BASE_URL}/api/file/rename" \
    -H "Authorization: Bearer ${TOKEN1}" \
    -H "Content-Type: application/json" \
    --data "{\"id\":${FILE_ID},\"new_name\":\"renamed_functional.txt\"}"

run_request "file search" 200 \
    -H "Authorization: Bearer ${TOKEN1}" \
    "${BASE_URL}/api/file/search?keyword=renamed_functional"
assert_contains "file search contains renamed file" "$(cat "${LAST_BODY_FILE}")" "renamed_functional.txt"

run_request "share create" 200 \
    -X POST "${BASE_URL}/api/share/create" \
    -H "Authorization: Bearer ${TOKEN1}" \
    -H "Content-Type: application/json" \
    --data "{\"file_id\":${FILE_ID}}"
SHARE_CODE="$(json_get "${LAST_BODY_FILE}" "data.share_code")"
assert_contains "share url includes code" "$(json_get "${LAST_BODY_FILE}" "data.share_url")" "${SHARE_CODE}"

run_download "share download" 200 "${SHARE_FILE}" \
    "${BASE_URL}/api/share/download?code=${SHARE_CODE}"
TOTAL_COUNT=$((TOTAL_COUNT + 1))
if cmp -s "${SMALL_FILE}" "${SHARE_FILE}"; then
    PASS_COUNT=$((PASS_COUNT + 1))
    echo "[PASS] share download content"
else
    echo "[FAIL] share download content"
fi

CHUNK_SIZE="$(extract_file_size "${CHUNK_FILE}")"
run_request "upload init" 200 \
    -X POST "${BASE_URL}/api/file/upload/init" \
    -H "Authorization: Bearer ${TOKEN1}" \
    -H "Content-Type: application/json" \
    --data "{\"filename\":\"chunked.bin\",\"total_size\":${CHUNK_SIZE},\"total_chunks\":2,\"mime_type\":\"application/octet-stream\"}"
UPLOAD_ID="$(json_get "${LAST_BODY_FILE}" "data.upload_id")"

run_request "upload init invalid payload" 400 \
    -X POST "${BASE_URL}/api/file/upload/init" \
    -H "Authorization: Bearer ${TOKEN1}" \
    -H "Content-Type: application/json" \
    --data "{\"filename\":\"\",\"total_size\":0,\"total_chunks\":0}"

HASH1="$(sha256_file "${CHUNK_PART1}")"
HASH2="$(sha256_file "${CHUNK_PART2}")"

run_request "upload chunk 0" 200 \
    -X POST "${BASE_URL}/api/file/upload/chunk?upload_id=${UPLOAD_ID}&chunk_index=0&chunk_hash=${HASH1}" \
    -H "Authorization: Bearer ${TOKEN1}" \
    -H "Content-Type: application/octet-stream" \
    --data-binary "@${CHUNK_PART1}"

run_request "upload chunk invalid hash" 400 \
    -X POST "${BASE_URL}/api/file/upload/chunk?upload_id=${UPLOAD_ID}&chunk_index=0&chunk_hash=badbad" \
    -H "Authorization: Bearer ${TOKEN1}" \
    -H "Content-Type: application/octet-stream" \
    --data-binary "@${CHUNK_PART1}"

run_request "upload chunk 1" 200 \
    -X POST "${BASE_URL}/api/file/upload/chunk?upload_id=${UPLOAD_ID}&chunk_index=1&chunk_hash=${HASH2}" \
    -H "Authorization: Bearer ${TOKEN1}" \
    -H "Content-Type: application/octet-stream" \
    --data-binary "@${CHUNK_PART2}"

run_request "upload progress" 200 \
    -H "Authorization: Bearer ${TOKEN1}" \
    "${BASE_URL}/api/file/upload/progress?upload_id=${UPLOAD_ID}"
assert_json_equals "upload progress completed chunks" "${LAST_BODY_FILE}" "data.completed_chunks" "2"

run_request "upload complete" 200 \
    -X POST "${BASE_URL}/api/file/upload/complete?upload_id=${UPLOAD_ID}" \
    -H "Authorization: Bearer ${TOKEN1}"
CHUNK_FILE_ID="$(json_get "${LAST_BODY_FILE}" "data.file_id")"

run_request "upload complete missing upload_id" 400 \
    -X POST "${BASE_URL}/api/file/upload/complete" \
    -H "Authorization: Bearer ${TOKEN1}"

run_request "upload init for cancel" 200 \
    -X POST "${BASE_URL}/api/file/upload/init" \
    -H "Authorization: Bearer ${TOKEN1}" \
    -H "Content-Type: application/json" \
    --data "{\"filename\":\"cancel.bin\",\"total_size\":2048,\"total_chunks\":2,\"mime_type\":\"application/octet-stream\"}"
CANCEL_UPLOAD_ID="$(json_get "${LAST_BODY_FILE}" "data.upload_id")"

run_request "upload cancel" 200 \
    -X POST "${BASE_URL}/api/file/upload/cancel?upload_id=${CANCEL_UPLOAD_ID}" \
    -H "Authorization: Bearer ${TOKEN1}"

run_request "message send" 200 \
    -X POST "${BASE_URL}/api/message/send" \
    -H "Authorization: Bearer ${TOKEN1}" \
    -H "Content-Type: application/json" \
    --data "{\"receiver_id\":${USER2_ID},\"content\":\"hello from functional test\"}"

run_request "message send to self" 400 \
    -X POST "${BASE_URL}/api/message/send" \
    -H "Authorization: Bearer ${TOKEN1}" \
    -H "Content-Type: application/json" \
    --data "{\"receiver_id\":${USER1_ID},\"content\":\"self message\"}"

run_request "message list" 200 \
    -H "Authorization: Bearer ${TOKEN2}" \
    "${BASE_URL}/api/message/list?with_user_id=${USER1_ID}&limit=10"
assert_contains "message list contains sent text" "$(cat "${LAST_BODY_FILE}")" "hello from functional test"

run_request "message list missing with_user_id" 400 \
    -H "Authorization: Bearer ${TOKEN2}" \
    "${BASE_URL}/api/message/list"

run_request "file delete simple upload" 200 \
    -X DELETE "${BASE_URL}/api/file/delete" \
    -H "Authorization: Bearer ${TOKEN1}" \
    -H "Content-Type: application/json" \
    --data "{\"id\":${FILE_ID}}"

run_request "file delete chunk upload" 200 \
    -X DELETE "${BASE_URL}/api/file/delete" \
    -H "Authorization: Bearer ${TOKEN1}" \
    -H "Content-Type: application/json" \
    --data "{\"id\":${CHUNK_FILE_ID}}"

run_request "logout user1 final" 200 \
    -X POST "${BASE_URL}/api/logout" \
    -H "Authorization: Bearer ${TOKEN1}"

run_request "logout user2 final" 200 \
    -X POST "${BASE_URL}/api/logout" \
    -H "Authorization: Bearer ${TOKEN2}"

echo
echo "Summary: ${PASS_COUNT}/${TOTAL_COUNT} passed"
if [[ "${PASS_COUNT}" -ne "${TOTAL_COUNT}" ]]; then
    exit 1
fi
