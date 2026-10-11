#!/usr/bin/env bash
# install.sh — установка RedAnchor Shell и всех зависимостей одной командой (Ubuntu 24.04+, Debian 12+).
#
#   ./install.sh              всё: пакеты, Steam, драйверы, Qt ≥ 6.10, сборка и установка оболочки
#   ./install.sh --deps-only  только системные пакеты (без Qt из aqtinstall и без сборки)
#   ./install.sh --no-build   пакеты и Qt, но без сборки и установки оболочки
#   ./install.sh --yes        не задавать вопросов (на всё отвечать «да»)
#
# Запускать от обычного пользователя: права root берутся через sudo только там, где нужны.
# Повторный запуск безопасен — уже установленное пропускается.
#
# Переменные окружения:
#   QT_VERSION=6.10.1     версия Qt для установки через aqtinstall
#   QT_PREFIX=/путь       готовый Qt (каталог с lib/cmake/Qt6), без поиска и установки
#   QT_INSTALL_DIR=~/Qt   куда ставить Qt через aqtinstall
#   BUILD_DIR=build-release  каталог сборки, BUILD_TYPE=Release, PREFIX=/usr/local

# Журнал принадлежит пользователю: перенаправления после sudo — намеренно
# shellcheck disable=SC2024
set -euo pipefail

SRC_DIR="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"
HELPER="$SRC_DIR/data/redanchor-components"
QT_VERSION="${QT_VERSION:-6.10.1}"
QT_MIN=6.10
QT_INSTALL_DIR="${QT_INSTALL_DIR:-$HOME/Qt}"
AQT_VENV="${AQT_VENV:-$HOME/.local/share/redanchor-aqt}"
BUILD_DIR="${BUILD_DIR:-$SRC_DIR/build-release}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
PREFIX="${PREFIX:-/usr/local}"
LOG_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/redanchor"
LOG="$LOG_DIR/install.log"

DEPS_ONLY=0
NO_BUILD=0
ASSUME_YES=0

# ---------- Оформление ----------

if [[ -t 1 ]]; then
    C_RESET=$'\e[0m'; C_BOLD=$'\e[1m'; C_DIM=$'\e[2m'
    C_RED=$'\e[31m'; C_GREEN=$'\e[32m'; C_YELLOW=$'\e[33m'; C_BLUE=$'\e[34m'; C_CYAN=$'\e[36m'
    INTERACTIVE_OUT=1
else
    C_RESET=; C_BOLD=; C_DIM=; C_RED=; C_GREEN=; C_YELLOW=; C_BLUE=; C_CYAN=
    INTERACTIVE_OUT=0
fi

STEP=0
STEPS_TOTAL=0

step() {
    STEP=$((STEP + 1))
    printf '\n%s[%d/%d]%s %s%s%s\n' "$C_BLUE$C_BOLD" "$STEP" "$STEPS_TOTAL" "$C_RESET" "$C_BOLD" "$*" "$C_RESET"
    echo "==== [$STEP/$STEPS_TOTAL] $* ($(date '+%F %T'))" >>"$LOG"
}
info() { printf '      %s\n' "$*"; }
ok() { printf '      %s✓%s %s\n' "$C_GREEN" "$C_RESET" "$*"; }
warn() { printf '      %s!%s %s\n' "$C_YELLOW" "$C_RESET" "$*"; echo "WARN: $*" >>"$LOG"; }
fail() {
    printf '\n%s✗ %s%s\n' "$C_RED$C_BOLD" "$*" "$C_RESET" >&2
    echo "FAIL: $*" >>"$LOG"
    if [[ -s $LOG ]]; then
        printf '%sПоследние строки журнала (%s):%s\n' "$C_DIM" "$LOG" "$C_RESET" >&2
        tail -n 25 "$LOG" | sed 's/^/    /' >&2
    fi
    exit 1
}

# Вопрос да/нет; с --yes — ответ по умолчанию «да»
ask() {
    local answer
    if [[ $ASSUME_YES -eq 1 ]]; then
        return 0
    fi
    if [[ ! -t 0 ]]; then
        return 0
    fi
    read -r -p "      $1 [Д/н] " answer || true
    case "$answer" in
        ""|Д|д|да|Да|ДА|y|Y|yes|Yes|YES) return 0 ;;
        *) return 1 ;;
    esac
}

usage() {
    # Комментарий в начале файла
    awk 'NR > 1 && /^#/ { sub(/^# ?/, ""); print; next } NR > 1 { exit }' "${BASH_SOURCE[0]}"
}

# Сравнение версий: version_ge 6.10.1 6.10 → истина
version_ge() {
    [[ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -n1)" == "$2" ]]
}

# ---------- apt ----------

APT_OPTS=(-y -q -o DPkg::Lock::Timeout=600 -o Dpkg::Options::=--force-confdef -o Dpkg::Options::=--force-confold)

pkg_available() {
    apt-cache show --no-all-versions "$1" 2>/dev/null | grep -q '^Package:'
}

pkg_installed() {
    [[ $(dpkg-query -W -f='${db:Status-Abbrev}' "$1" 2>/dev/null) == ii* ]]
}

# Ставит те пакеты из списка, что есть в репозиториях и ещё не установлены
apt_install() {
    local required="$1"
    shift
    local pkgs=() missing=() pkg
    for pkg in "$@"; do
        pkg_installed "$pkg" && continue
        if pkg_available "$pkg"; then
            pkgs+=("$pkg")
        else
            missing+=("$pkg")
        fi
    done
    if [[ ${#missing[@]} -gt 0 ]]; then
        if [[ $required == required ]]; then
            fail "Нет в репозиториях: ${missing[*]}"
        fi
        info "${C_DIM}нет в репозиториях (пропущено): ${missing[*]}${C_RESET}"
    fi
    if [[ ${#pkgs[@]} -eq 0 ]]; then
        ok "уже установлено"
        return 0
    fi
    info "устанавливаются: ${pkgs[*]}"
    if ! sudo env DEBIAN_FRONTEND=noninteractive apt-get install "${APT_OPTS[@]}" "${pkgs[@]}" >>"$LOG" 2>&1; then
        if [[ $required == required ]]; then
            fail "Не удалось установить пакеты: ${pkgs[*]}"
        fi
        # Необязательные — по одному, чтобы один сломанный пакет не мешал остальным
        for pkg in "${pkgs[@]}"; do
            sudo env DEBIAN_FRONTEND=noninteractive apt-get install "${APT_OPTS[@]}" "$pkg" >>"$LOG" 2>&1 ||
                warn "не удалось установить $pkg"
        done
    fi
    ok "готово"
}

# Цель помощника redanchor-components (та же логика, что в «Настройки → Обновления»)
# с полосой прогресса из строк «PROGRESS <0..100> <текст>»
run_helper() {
    local target="$1" status=0
    sudo bash "$HELPER" "$target" 2>>"$LOG" | render_progress || status=$?
    [[ $INTERACTIVE_OUT -eq 1 ]] && printf '\r\e[K'
    if [[ $status -ne 0 ]]; then
        return "$status"
    fi
    ok "готово"
}

render_progress() {
    local word pct text bar filled width=30 rc last_pct=0 last_text="" quiet=0 activity
    while :; do
        # Помощник долго молчит (apt ждёт блокировку dpkg, качает или настраивает пакеты) —
        # показываем последнюю строку журнала, чтобы было видно, что установка идёт
        if read -r -t 5 word pct text; then
            quiet=0
        else
            rc=$?
            [[ $rc -le 128 ]] && break   # конец вывода
            quiet=$((quiet + 5))
            if [[ $INTERACTIVE_OUT -eq 1 ]]; then
                activity=$(tail -n 40 "$LOG" 2>/dev/null | grep -v '^  [0-9]*% ' | tail -n 1 | tr -d '\r' | cut -c1-70)
                printf '\r\e[K      %s%3d%%%s  %s %s(%d с)%s' "$C_CYAN" "$last_pct" "$C_RESET" \
                    "${activity:-$last_text}" "$C_DIM" "$quiet" "$C_RESET"
            fi
            continue
        fi
        case "$word" in
            PROGRESS)
                [[ $pct =~ ^[0-9]+$ ]] || continue
                last_pct=$pct
                last_text=$text
                echo "  $pct% $text" >>"$LOG"
                if [[ $INTERACTIVE_OUT -eq 1 ]]; then
                    filled=$((pct * width / 100))
                    printf -v bar '%*s' "$filled" ''
                    bar=${bar// /█}
                    printf '\r\e[K      %s%-*s%s %3d%%  %s' "$C_CYAN" "$width" "$bar" "$C_RESET" "$pct" "${text:0:60}"
                fi
                ;;
            REBOOT)
                REBOOT_NEEDED=1
                ;;
        esac
    done
    if [[ ${REBOOT_NEEDED:-0} -eq 1 ]]; then
        : >"$LOG_DIR/.reboot-needed"
    fi
}

# ---------- Шаги ----------

check_system() {
    step "Проверка системы"
    [[ $(id -u) -ne 0 ]] || fail "Запустите от обычного пользователя (без sudo): права root скрипт попросит сам."
    command -v apt-get >/dev/null 2>&1 || fail "Нужна система на базе Ubuntu или Debian (apt)."
    command -v sudo >/dev/null 2>&1 || fail "Не найден sudo. Установите его: su -c 'apt install sudo' и добавьте себя в группу sudo."
    [[ -f $HELPER ]] || fail "Не найден $HELPER — запускайте install.sh из каталога исходников."
    [[ $(uname -m) == x86_64 ]] || warn "Архитектура $(uname -m): Steam и Proton работают только на x86_64."

    # shellcheck disable=SC1091
    . /etc/os-release
    info "система: ${PRETTY_NAME:-неизвестно}"
    case "${ID:-}" in
        ubuntu)
            version_ge "${VERSION_ID:-0}" 24.04 || warn "Проверено на Ubuntu 24.04 и новее, у вас ${VERSION_ID:-?}." ;;
        debian)
            version_ge "${VERSION_ID:-0}" 12 || warn "Проверено на Debian 12 и новее, у вас ${VERSION_ID:-?}." ;;
        *)
            [[ ${ID_LIKE:-} == *debian* || ${ID_LIKE:-} == *ubuntu* ]] ||
                warn "Система не похожа на Ubuntu или Debian — возможны ошибки." ;;
    esac

    info "для установки пакетов нужен пароль sudo"
    sudo -v || fail "Не удалось получить права sudo."
    # Не даём sudo забыть пароль во время долгой установки
    ( while kill -0 "$$" 2>/dev/null; do sudo -n true 2>/dev/null; sleep 50; done ) &
    SUDO_KEEPALIVE=$!
    ok "Ubuntu/Debian, sudo доступен; журнал: $LOG"
}

prepare_apt() {
    step "Подготовка apt: 32-битные пакеты, прерванные установки"
    if ! dpkg --print-foreign-architectures | grep -qx i386; then
        info "включаем архитектуру i386 (нужна Steam и Proton)"
        sudo dpkg --add-architecture i386
    fi
    sudo dpkg --configure -a >>"$LOG" 2>&1 || warn "dpkg --configure -a завершился с ошибкой"
    info "обновление списка пакетов"
    sudo apt-get update -q -o DPkg::Lock::Timeout=600 >>"$LOG" 2>&1 || warn "список пакетов обновлён не полностью"
    ok "готово"
}

install_build_deps() {
    step "Инструменты сборки"
    apt_install required build-essential cmake pkg-config git curl ca-certificates \
        python3 python3-venv
    # Заголовки, которые ищет CMake-пакет Qt6Gui/ZLIB (для Qt из aqtinstall и из системы)
    apt_install optional python3-pip ninja-build libgl-dev libegl-dev libglx-dev libopengl-dev \
        libvulkan-dev libxkbcommon-dev libfontconfig-dev libfreetype-dev libdbus-1-dev \
        libgl1-mesa-dev
}

install_qt_runtime_libs() {
    step "Библиотеки для Qt (xcb внутри gamescope, Wayland, WebEngine, звук)"
    local asound=libasound2t64
    pkg_available "$asound" || asound=libasound2
    apt_install optional libxcb-cursor0 libxkbcommon0 libxkbcommon-x11-0 libxcb-icccm4 libxcb-image0 \
        libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-xinerama0 libxcb-xkb1 \
        libxcb-xinput0 libxcb-xfixes0 libxcb-sync1 libx11-xcb1 libwayland-client0 libwayland-cursor0 \
        libwayland-egl1 libegl1 libgl1 libopengl0 libgbm1 libdrm2 libfontconfig1 libfreetype6 \
        libdbus-1-3 libglib2.0-0t64 libglib2.0-0 libnss3 libnspr4 libxcomposite1 libxdamage1 \
        libxrandr2 libxtst6 libxkbfile1 libxshmfence1 libxss1 \
        "$asound" libpulse0 libva2 libva-drm2 libgstreamer1.0-0 fontconfig
}

helper_step() {
    local target="$1" title="$2" required="${3:-optional}"
    step "$title"
    if ! run_helper "$target"; then
        if [[ $required == required ]]; then
            fail "Не удалось: $title"
        fi
        warn "не удалось: $title — подробности в журнале, повторить можно из «Настройки → Обновления»"
    fi
}

# Модули Qt, без которых оболочка не соберётся, и пакеты aqtinstall, в которых они лежат
QT_REQUIRED_MODULES=(Qt6Quick Qt6QuickControls2 Qt6DBus Qt6Network Qt6Concurrent Qt6Multimedia Qt6WebEngineQuick Qt6ShaderTools)
AQT_MODULES=(qtwebengine qtwebchannel qtpositioning qtmultimedia qtshadertools)
# Каких модулей не хватило в последнем проверенном каталоге Qt
QT_MISSING=""

# Каталог Qt, в котором есть всё нужное оболочке, и его версия
qt_prefix_version() {
    local prefix="$1" cfg
    QT_MISSING=""
    for cfg in "$prefix/lib/cmake/Qt6/Qt6ConfigVersion.cmake" "$prefix/lib/x86_64-linux-gnu/cmake/Qt6/Qt6ConfigVersion.cmake"; do
        [[ -f $cfg ]] || continue
        local dir
        dir=$(dirname "$(dirname "$cfg")")
        local mod
        for mod in "${QT_REQUIRED_MODULES[@]}"; do
            [[ -d $dir/$mod ]] || QT_MISSING+="${QT_MISSING:+ }$mod"
        done
        [[ -z $QT_MISSING ]] || return 1
        # Версия — в Qt6ConfigVersionImpl.cmake (Qt6ConfigVersion.cmake только подключает его);
        # на крайний случай — из имени каталога (~/Qt/6.10.1/gcc_64)
        local ver
        ver=$(sed -n 's/.*PACKAGE_VERSION "\([0-9][0-9.]*\)".*/\1/p' "$dir/Qt6/Qt6ConfigVersionImpl.cmake" "$cfg" 2>/dev/null | head -n1)
        if [[ -z $ver ]]; then
            ver=$(basename "$(dirname "$prefix")")
            [[ $ver =~ ^[0-9]+\.[0-9]+(\.[0-9]+)?$ ]] || ver=""
        fi
        [[ -n $ver ]] || { QT_MISSING="(не удалось узнать версию Qt)"; return 1; }
        echo "$ver"
        return 0
    done
    return 1
}

# Пакеты системного Qt (если в репозиториях Qt ≥ 6.10, например Ubuntu 26.04)
SYSTEM_QT_PKGS=(qt6-base-dev qt6-base-dev-tools qt6-declarative-dev qt6-declarative-dev-tools
    qt6-multimedia-dev qt6-webengine-dev qt6-webengine-dev-tools qt6-shadertools-dev
    qt6-qpa-plugins qt6-wayland
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-templates
    qml6-module-qtquick-window qml6-module-qtquick-shapes qml6-module-qtquick-effects
    qml6-module-qtquick-layouts qml6-module-qtqml-workerscript qml6-module-qtcore
    qml6-module-qtmultimedia qml6-module-qtwebengine qml6-module-qtwebchannel
    qml6-module-qtqml-models)

QT_FOUND_PREFIX=""

find_qt() {
    local prefix ver best="" best_ver="" seen=""
    if [[ -n ${QT_PREFIX:-} ]]; then
        if ! ver=$(qt_prefix_version "$QT_PREFIX"); then
            qt_prefix_version "$QT_PREFIX" >/dev/null || true
            fail "В QT_PREFIX=$QT_PREFIX не хватает модулей Qt: ${QT_MISSING:-нет Qt6ConfigVersion.cmake}."
        fi
        version_ge "$ver" "$QT_MIN" || fail "В QT_PREFIX=$QT_PREFIX Qt $ver, нужен $QT_MIN или новее."
        QT_FOUND_PREFIX="$QT_PREFIX"
        ok "Qt $ver: $QT_PREFIX (из QT_PREFIX)"
        return 0
    fi
    # Qt из установщика или aqtinstall: ~/Qt/<версия>/gcc_64 (или linux_gcc_64)
    QT_INCOMPLETE=""
    for prefix in "$QT_INSTALL_DIR"/*/gcc_64 "$QT_INSTALL_DIR"/*/linux_gcc_64 "$HOME"/Qt/*/gcc_64 \
                  "$HOME"/Qt/*/linux_gcc_64 /opt/Qt/*/gcc_64 /opt/qt*/gcc_64; do
        [[ -d $prefix ]] || continue
        prefix=$(readlink -f "$prefix")
        [[ " $seen " == *" $prefix "* ]] && continue
        seen+=" $prefix"
        if ! ver=$(qt_prefix_version "$prefix"); then
            # Qt есть, но не со всеми модулями — запомним, чтобы доставить их
            qt_prefix_version "$prefix" >/dev/null || true
            if [[ -n $QT_MISSING ]]; then
                warn "в $prefix не хватает модулей: $QT_MISSING"
                QT_INCOMPLETE="$prefix"
            fi
            continue
        fi
        version_ge "$ver" "$QT_MIN" || continue
        if [[ -z $best_ver ]] || version_ge "$ver" "$best_ver"; then
            best="$prefix"; best_ver="$ver"
        fi
    done
    if [[ -n $best ]]; then
        QT_FOUND_PREFIX="$best"
        ok "Qt $best_ver: $best"
        return 0
    fi
    # Системный Qt
    if ver=$(qt_prefix_version /usr) && version_ge "$ver" "$QT_MIN"; then
        QT_FOUND_PREFIX=/usr
        ok "системный Qt $ver"
        return 0
    fi
    return 1
}

ensure_qt() {
    step "Qt $QT_MIN или новее"
    find_qt && return 0

    # Qt уже стоит, но без части модулей (чаще всего WebEngine) — доставляем их
    if [[ -n ${QT_INCOMPLETE:-} && $DEPS_ONLY -eq 0 ]]; then
        local incomplete_ver
        incomplete_ver=$(basename "$(dirname "$QT_INCOMPLETE")")
        if ask "Доставить недостающие модули в Qt $incomplete_ver через aqtinstall?"; then
            QT_VERSION="$incomplete_ver"
            QT_INSTALL_DIR="$(dirname "$(dirname "$QT_INCOMPLETE")")"
            install_qt_aqt
            find_qt && return 0
            qt_prefix_version "$QT_INCOMPLETE" >/dev/null || true
            fail "В $QT_INCOMPLETE всё ещё не хватает: $QT_MISSING. Подробности — в $LOG (пришлите последние строки)."
        fi
    fi

    # В репозиториях дистрибутива уже есть подходящий Qt?
    local candidate
    candidate=$(apt-cache policy qt6-base-dev 2>/dev/null | awk '/Candidate:/ {print $2}')
    candidate=${candidate#*:}
    candidate=${candidate%%[+~-]*}
    if [[ -n $candidate && $candidate != "(none)" ]] && version_ge "$candidate" "$QT_MIN"; then
        info "в репозиториях Qt $candidate — ставим системные пакеты"
        apt_install optional "${SYSTEM_QT_PKGS[@]}"
        find_qt && return 0
        warn "системных пакетов Qt оказалось недостаточно"
    else
        info "в репозиториях Qt ${candidate:-нет} — этого мало (нужен $QT_MIN+)"
    fi

    if [[ $DEPS_ONLY -eq 1 ]]; then
        warn "Qt $QT_MIN+ не найден. Без --deps-only скрипт поставит его через aqtinstall."
        return 0
    fi
    if ! ask "Установить Qt $QT_VERSION через aqtinstall в $QT_INSTALL_DIR (около 2 ГБ)?"; then
        fail "Нужен Qt $QT_MIN+. Укажите готовый: QT_PREFIX=/путь/к/Qt/6.x/gcc_64 ./install.sh"
    fi
    install_qt_aqt
    find_qt && return 0
    qt_prefix_version "$QT_INSTALL_DIR/$QT_VERSION/gcc_64" >/dev/null || true
    fail "Qt $QT_VERSION установлен в $QT_INSTALL_DIR/$QT_VERSION, но в нём не хватает модулей: ${QT_MISSING:-неизвестно}. Подробности — в $LOG"
}

# Зеркала Qt. download.qt.io из некоторых стран (в том числе из России) недоступен, поэтому
# первыми идут европейские зеркала с полной копией репозитория
read -r -a AQT_MIRRORS <<<"${AQT_MIRRORS:-https://ftp.fau.de/qtproject https://mirror.netcologne.de/qtproject https://mirrors.dotsrc.org/qtproject https://www.nic.funet.fi/pub/mirrors/download.qt-project.org https://download.qt.io https://mirrors.ocf.berkeley.edu/qt}"
AQT_ERROR=""
AQT_NO_HASH=0

# aqt_attempt <зеркало> <алгоритм> <сборка> [аргументы…] — одна попытка aqt install-qt.
# Пока aqt работает, показывает время, объём скачанного и последнюю строку aqt.
# При неудаче в AQT_ERROR — последняя строка ERROR из вывода aqt
aqt_attempt() {
    local base=$1 algo=$2 arch=$3 out pid rc start line mb dest alog="$LOG_DIR/aqt-current.log"
    shift 3
    local extra=()
    [[ $AQT_NO_HASH -eq 1 ]] && extra=(--UNSAFE-ignore-hash)
    dest=$(realpath -m "$QT_INSTALL_DIR")
    echo "aqt install-qt $QT_VERSION $arch $* (зеркало $base, $algo${extra[*]:+, без проверки})" >>"$LOG"
    : >"$alog"
    # Контрольные суммы aqt по умолчанию берёт только с download.qt.io — если он недоступен,
    # ни одно зеркало не помогает («Failed to download checksum»). Берём их с того же зеркала
    printf '[aqt]\nbaseurl : %s\n\n[requests]\nhash_algorithm : %s\n\n[mirrors]\ntrusted_mirrors :\n    %s\n' \
        "$base" "$algo" "$base" >"$LOG_DIR/aqt-settings.ini"
    # Запускаем из каталога журналов: aqt кладёт туда свой aqtinstall.log и скачанные архивы
    (cd "$LOG_DIR" && exec "$AQT_VENV/bin/aqt" -c aqt-settings.ini install-qt linux desktop "$QT_VERSION" "$arch" \
        -O "$dest" -b "$base" --timeout 30 "${extra[@]}" "$@") >"$alog" 2>&1 &
    pid=$!
    start=$SECONDS
    while kill -0 "$pid" 2>/dev/null; do
        if [[ $INTERACTIVE_OUT -eq 1 ]]; then
            line=$(grep -E 'INFO|ERROR|WARN' "$alog" 2>/dev/null | tail -n 1 | tr -d '\r' |
                   sed -E 's/^(INFO|WARNING|ERROR) *: *//' | cut -c1-60 || true)
            # aqt пишет о модуле, только когда он скачан и распакован, а большие модули
            # качаются минутами — показываем, сколько уже скачано и распаковано
            mb=$(du -scm "$LOG_DIR"/*.7z "$dest/$QT_VERSION" 2>/dev/null | tail -n 1 | cut -f1 || true)
            printf '\r\e[K      %s%d:%02d%s  %s МБ  %s' "$C_CYAN" $(((SECONDS - start) / 60)) \
                $(((SECONDS - start) % 60)) "$C_RESET" "${mb:-0}" "${line:-соединение с $base…}"
        fi
        sleep 1
    done
    rc=0
    wait "$pid" || rc=$?
    [[ $INTERACTIVE_OUT -eq 1 ]] && printf '\r\e[K'
    out=$(cat "$alog")
    echo "$out" >>"$LOG"
    [[ $rc -eq 0 ]] && return 0
    AQT_ERROR=$(grep -E 'ERROR' <<<"$out" | tail -1 | sed -E 's/^ERROR *: *//' || true)
    [[ -n $AQT_ERROR ]] || AQT_ERROR=$(tail -1 <<<"$out")
    return 1
}

# aqt_install <сборка> [аргументы…] — ставит Qt $QT_VERSION, пробуя зеркала по очереди.
# На зеркалах может не быть файлов .sha256 (их надёжно хранит только download.qt.io) — тогда
# проверяем по .sha1, которые лежат рядом с архивами для официального установщика Qt
aqt_install() {
    local base algo had_checksum=0
    for base in "${AQT_MIRRORS[@]}"; do
        for algo in sha256 sha1; do
            aqt_attempt "$base" "$algo" "$@" && return 0
            # WebEngine и PDF с Qt 6.8 лежат в «расширениях». Если у их списка пакетов на зеркале
            # нет контрольной суммы, aqt молча пропускает расширение и пишет «not found» — это
            # не «модуля нет», а та же нехватка контрольных сумм
            if [[ " $* " == *" qtwebengine "* || " $* " == *" qtpdf "* ]] &&
               grep -qi 'not found while parsing' <<<"$AQT_ERROR"; then
                had_checksum=1
                continue
            fi
            # Нет такой версии или модуля — другие зеркала не помогут
            if grep -qiE 'not (a valid|found|available)|invalid version|no qt version' <<<"$AQT_ERROR"; then
                return 1
            fi
            if grep -qi 'checksum' <<<"$AQT_ERROR"; then
                had_checksum=1
                [[ $algo == sha256 ]] && info "на зеркале $base нет SHA-256 — проверяю по SHA-1"
                continue
            fi
            break
        done
        warn "зеркало $base не подошло: $AQT_ERROR"
    done
    # Архивы на зеркалах есть, но контрольные суммы не скачиваются — с согласия
    # пользователя ставим без проверки (файлы всё равно идут по HTTPS с зеркала Qt)
    if [[ $had_checksum -eq 1 && $AQT_NO_HASH -eq 0 && $ASSUME_YES -eq 0 && -t 0 ]] &&
       ask "Контрольные суммы Qt ($*) недоступны на зеркалах. Установить без их проверки (файлы всё равно идут по HTTPS)?"; then
        AQT_NO_HASH=1
        for base in "${AQT_MIRRORS[@]}"; do
            aqt_attempt "$base" sha256 "$@" && return 0
            warn "зеркало $base не подошло: $AQT_ERROR"
        done
    fi
    return 1
}

# qt_extension_direct <расширение> — ставит расширение Qt (qtwebengine) напрямую из репозитория
# Qt, без aqt: aqt 3.3 для Qt 6.10 расширение не находит, если у его списка пакетов на зеркале
# нет контрольной суммы. Список пакетов (Updates.xml) берём с зеркал по очереди, архивы
# проверяем по .sha256/.sha1 рядом с ними; если сумм нет нигде — только с согласия пользователя
qt_extension_direct() {
    local ext=$1 rc=0 allow=$AQT_NO_HASH
    while :; do
        rc=0
        "$AQT_VENV/bin/python3" -I - "$ext" "$QT_VERSION" "$(realpath -m "$QT_INSTALL_DIR")" "$allow" \
            "$LOG_DIR" "${AQT_MIRRORS[@]}" 2>>"$LOG" <<'PY' || rc=$?
import hashlib, os, sys, xml.etree.ElementTree as ET
import requests, py7zr

ext, version, dest, allow, workdir, mirrors = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4] == "1", sys.argv[5], sys.argv[6:]
vstr = version.replace(".", "")
session = requests.Session()

def say(*a):
    print(*a, flush=True)

def get(url, stream=False):
    r = session.get(url, timeout=(15, 60), stream=stream)
    r.raise_for_status()
    return r

def checksum(url):
    for algo in ("sha256", "sha1"):
        try:
            text = get(f"{url}.{algo}").text.split()
            if text and len(text[0]) == {"sha256": 64, "sha1": 40}[algo]:
                return algo, text[0].lower()
        except Exception:
            pass
    return None, None

tried = []
nohash = False
for mirror in mirrors:
    for archdir in ("x86_64", "linux_gcc_64", "gcc_64"):
        folder = f"{mirror.rstrip('/')}/online/qtsdkrepository/linux_x64/extensions/{ext}/{vstr}/{archdir}"
        try:
            xml = get(f"{folder}/Updates.xml").text
        except Exception as e:
            tried.append(f"{folder}: {e}")
            continue
        root = ET.fromstring(xml)
        pkgs = []
        for pu in root.iter("PackageUpdate"):
            name = (pu.findtext("Name") or "").strip()
            if ext not in name or "gcc_64" not in name or "debug" in name:
                continue
            ver = (pu.findtext("Version") or "").strip()
            archives, paths = [], []
            ops = pu.findall("Operations/Operation[@name='Extract']/Argument")
            if ops:
                for i in range(0, len(ops) - 1, 2):
                    paths.append(ops[i].text.replace("@TargetDir@/", "", 1).replace("@TargetDir@", ""))
                    archives.append(ops[i + 1].text.strip())
            else:
                archives = [a.strip() for a in (pu.findtext("DownloadableArchives") or "").split(",") if a.strip()]
                paths = [""] * len(archives)
            pkgs.append((name, ver, archives, paths))
        if not pkgs:
            names = [(pu.findtext("Name") or "") for pu in root.iter("PackageUpdate")]
            tried.append(f"{folder}: нет пакета для gcc_64 (есть: {', '.join(names)})")
            continue
        try:
            for name, ver, archives, paths in pkgs:
                for archive, path in zip(archives, paths):
                    url = f"{folder}/{name}/{ver}{archive}"
                    algo, expected = checksum(url)
                    if algo is None and not allow:
                        raise LookupError(f"нет контрольной суммы для {archive}")
                    say(f"      загрузка {archive}")
                    local = os.path.join(workdir, archive)
                    h = hashlib.new(algo) if algo else None
                    with get(url, stream=True) as r, open(local, "wb") as f:
                        done = 0
                        for chunk in r.iter_content(1 << 20):
                            f.write(chunk)
                            done += len(chunk)
                            if sys.stdout.isatty():
                                print(f"\r\033[K      {archive[:40]}: {done >> 20} МБ", end="", flush=True)
                            if h:
                                h.update(chunk)
                    if sys.stdout.isatty():
                        print("\r\033[K", end="", flush=True)
                    if h and h.hexdigest() != expected:
                        os.unlink(local)
                        raise RuntimeError(f"контрольная сумма {archive} не совпала ({algo})")
                    target = os.path.join(dest, path) if path else dest
                    with py7zr.SevenZipFile(local, "r") as z:
                        z.extractall(path=target)
                    os.unlink(local)
            say(f"      {ext} установлен с {mirror}")
            sys.exit(0)
        except SystemExit:
            raise
        except LookupError as e:
            nohash = True
            tried.append(f"{folder}: {e}")
            break
        except Exception as e:
            tried.append(f"{folder}: {e}")
            break
for t in tried:
    print(t, file=sys.stderr)
if nohash:
    say(f"      у архивов {ext} на зеркалах нет контрольных сумм")
    sys.exit(3)
say(f"      {ext} {version} не найден ни на одном зеркале — подробности в журнале")
sys.exit(1)
PY
        [[ $rc -eq 3 && $allow -eq 0 ]] || break
        if [[ $ASSUME_YES -eq 0 && -t 0 ]] &&
           ask "У архивов $ext на зеркалах нет контрольных сумм. Установить без проверки (файлы всё равно идут по HTTPS)?"; then
            allow=1
            continue
        fi
        break
    done
    return "$rc"
}

install_qt_aqt() {
    info "подготовка aqtinstall ($AQT_VENV)"
    if [[ ! -x $AQT_VENV/bin/python3 ]]; then
        python3 -m venv "$AQT_VENV" >>"$LOG" 2>&1 || fail "Не удалось создать окружение Python (нужен пакет python3-venv)."
    fi
    "$AQT_VENV/bin/python3" -m pip install -q -U pip aqtinstall >>"$LOG" 2>&1 || fail "Не удалось установить aqtinstall (pip)."
    [[ $QT_VERSION =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] ||
        fail "QT_VERSION должна быть полной версией вида 6.10.1 (сейчас: «$QT_VERSION»)."

    # Имя сборки берём по версии, без «aqt list-qt»: тот не умеет другие зеркала и при любом
    # сбое сети выглядел как «версия неизвестна». Qt 6.7+ — linux_gcc_64, старые — gcc_64
    local arch=gcc_64 minor
    minor=$(cut -d. -f2 <<<"$QT_VERSION")
    if [[ ${QT_VERSION%%.*} -gt 6 || ( ${QT_VERSION%%.*} -eq 6 && $minor -ge 7 ) ]]; then
        arch=linux_gcc_64
    fi

    mkdir -p "$QT_INSTALL_DIR"
    # Базовый Qt (если его ещё нет) — повторная установка поверх просто ничего не меняет
    if [[ ! -f $QT_INSTALL_DIR/$QT_VERSION/gcc_64/lib/cmake/Qt6/Qt6ConfigVersion.cmake &&
          ! -f $QT_INSTALL_DIR/$QT_VERSION/linux_gcc_64/lib/cmake/Qt6/Qt6ConfigVersion.cmake ]]; then
        info "загрузка Qt $QT_VERSION ($arch) — это займёт время"
        aqt_install "$arch" ||
            fail "aqtinstall не смог установить Qt $QT_VERSION: $AQT_ERROR. Подробности — в $LOG"
    fi
    # Модули — каждый отдельно: WebEngine с Qt 6.8 лежит среди «расширений» и в списке
    # модулей aqt может не показываться, но ставится так же. Один неудачный модуль
    # не мешает остальным
    local m failed=()
    for m in "${AQT_MODULES[@]}"; do
        info "модуль $m"
        aqt_install "$arch" -m "$m" && continue
        # WebEngine — расширение: если aqt его не нашёл, качаем напрямую
        if [[ $m == qtwebengine ]]; then
            info "aqt не нашёл $m — загружаю напрямую из репозитория Qt"
            qt_extension_direct "$m" && continue
        fi
        failed+=("$m ($AQT_ERROR)")
    done
    if [[ ${#failed[@]} -gt 0 ]]; then
        warn "aqtinstall не смог поставить: ${failed[*]}"
    fi
    ok "Qt $QT_VERSION: $QT_INSTALL_DIR/$QT_VERSION"
}

build_shell() {
    step "Сборка RedAnchor Shell"
    [[ -n $QT_FOUND_PREFIX ]] || fail "Не найден Qt $QT_MIN+."
    local args=(-S "$SRC_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_INSTALL_PREFIX="$PREFIX")
    [[ $QT_FOUND_PREFIX != /usr ]] && args+=(-DCMAKE_PREFIX_PATH="$QT_FOUND_PREFIX")
    # Новый каталог сборки — Ninja, если есть (в существующем остаётся прежний генератор)
    if [[ ! -f $BUILD_DIR/CMakeCache.txt ]] && command -v ninja >/dev/null 2>&1; then
        args+=(-G Ninja)
    fi
    info "настройка (cmake, каталог $BUILD_DIR)"
    cmake "${args[@]}" >>"$LOG" 2>&1 || fail "cmake не смог настроить сборку."
    info "компиляция ($(nproc) потоков)"
    cmake --build "$BUILD_DIR" -j"$(nproc)" >>"$LOG" 2>&1 || fail "Ошибка сборки."
    ok "собрано"
}

install_shell() {
    step "Установка оболочки (sudo cmake --install)"
    sudo cmake --install "$BUILD_DIR" >>"$LOG" 2>&1 || fail "Не удалось установить."
    ok "redanchor-shell → $PREFIX/bin, сессия → /usr/share/wayland-sessions/redanchor.desktop"
}

setup_permissions() {
    step "Геймпады: правила udev и доступ к устройствам"
    if command -v udevadm >/dev/null 2>&1; then
        sudo udevadm control --reload >>"$LOG" 2>&1 || true
        sudo udevadm trigger >>"$LOG" 2>&1 || true
        ok "правила udev перечитаны"
    fi
    # uinput (виртуальный контроллер Xbox) должен быть загружен
    if [[ ! -e /dev/uinput ]]; then
        sudo modprobe uinput >>"$LOG" 2>&1 || warn "не удалось загрузить модуль uinput"
    fi
    if [[ ! -f /etc/modules-load.d/redanchor-uinput.conf ]]; then
        echo uinput | sudo tee /etc/modules-load.d/redanchor-uinput.conf >/dev/null
    fi
    if getent group input >/dev/null; then
        if id -nG "$USER" | tr ' ' '\n' | grep -qx input; then
            ok "$USER уже в группе input"
        else
            sudo usermod -aG input "$USER"
            ok "$USER добавлен в группу input (заработает после повторного входа)"
            RELOGIN_NEEDED=1
        fi
    fi
}

summary() {
    printf '\n%s%s✓ Готово.%s\n' "$C_GREEN" "$C_BOLD" "$C_RESET"
    if [[ $DEPS_ONLY -eq 0 && $NO_BUILD -eq 0 ]]; then
        cat <<EOF

  Дальше:
    1. Выйдите из системы и на экране входа выберите сеанс «RedAnchor» (шестерёнка внизу).
    2. В мастере настройки на шаге «Проверка обновлений» оболочка докачает Proton и gogdl.
    3. Steam: войдите один раз в Big Picture — дальше клиент обновляется сам.

  Журнал установки: $LOG
  Журнал сессии:    ~/.local/state/redanchor/session.log
EOF
    elif [[ -n $QT_FOUND_PREFIX ]]; then
        printf '\n  Сборка вручную:\n    cmake -B build -DCMAKE_PREFIX_PATH=%s && cmake --build build -j%s && sudo cmake --install build\n' \
            "$QT_FOUND_PREFIX" "$(nproc)"
    fi
    if [[ -f $LOG_DIR/.reboot-needed ]]; then
        rm -f "$LOG_DIR/.reboot-needed"
        printf '\n  %s!%s Установлен драйвер NVIDIA — %sперезагрузите компьютер%s.\n' "$C_YELLOW" "$C_RESET" "$C_BOLD" "$C_RESET"
    elif [[ ${RELOGIN_NEEDED:-0} -eq 1 ]]; then
        printf '\n  %s!%s Выйдите из системы и войдите снова, чтобы применились права на геймпады.\n' "$C_YELLOW" "$C_RESET"
    fi
}

cleanup() {
    [[ -n ${SUDO_KEEPALIVE:-} ]] && kill "$SUDO_KEEPALIVE" 2>/dev/null
    return 0
}

# ---------- Разбор аргументов ----------

for arg in "$@"; do
    case "$arg" in
        --deps-only) DEPS_ONLY=1 ;;
        --no-build) NO_BUILD=1 ;;
        -y|--yes) ASSUME_YES=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Неизвестный параметр: $arg (см. --help)" >&2; exit 2 ;;
    esac
done

mkdir -p "$LOG_DIR"
echo "==== install.sh $(date '+%F %T') $*" >>"$LOG"
trap cleanup EXIT

# Шаги: система, apt, сборка, библиотеки Qt, 4 цели помощника, Qt, [сборка, установка], права
STEPS_TOTAL=10
if [[ $DEPS_ONLY -eq 0 && $NO_BUILD -eq 0 ]]; then
    STEPS_TOTAL=12
fi

printf '%sRedAnchor Shell — установка%s\n' "$C_BOLD" "$C_RESET"
printf '%sисходники: %s%s\n' "$C_DIM" "$SRC_DIR" "$C_RESET"

check_system
prepare_apt
install_build_deps
install_qt_runtime_libs
helper_step system "Компоненты сессии: gamescope, Xwayland, Python 3, PipeWire, шрифт Inter" required
helper_step drivers "Графический драйвер (NVIDIA или Mesa, 64 и 32 бита)"
helper_step vulkan "Vulkan (64 и 32 бита)"
helper_step steam "Steam — официальный клиент, steam-devices, 32-битные библиотеки"
ensure_qt
if [[ $DEPS_ONLY -eq 0 && $NO_BUILD -eq 0 ]]; then
    build_shell
    install_shell
fi
setup_permissions
summary
