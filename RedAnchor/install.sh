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
    local word pct text bar filled width=30
    while read -r word pct text; do
        case "$word" in
            PROGRESS)
                [[ $pct =~ ^[0-9]+$ ]] || continue
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

# Каталог Qt, в котором есть всё нужное оболочке, и его версия
qt_prefix_version() {
    local prefix="$1" cfg
    for cfg in "$prefix/lib/cmake/Qt6/Qt6ConfigVersion.cmake" "$prefix/lib/x86_64-linux-gnu/cmake/Qt6/Qt6ConfigVersion.cmake"; do
        [[ -f $cfg ]] || continue
        local dir
        dir=$(dirname "$(dirname "$cfg")")
        local mod
        for mod in Qt6Quick Qt6QuickControls2 Qt6DBus Qt6Network Qt6Concurrent Qt6Multimedia Qt6WebEngineQuick Qt6ShaderTools; do
            [[ -d $dir/$mod ]] || return 1
        done
        sed -n 's/.*PACKAGE_VERSION "\([0-9.]*\)".*/\1/p' "$cfg" | head -n1
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
    local prefix ver best="" best_ver=""
    if [[ -n ${QT_PREFIX:-} ]]; then
        ver=$(qt_prefix_version "$QT_PREFIX") || fail "В QT_PREFIX=$QT_PREFIX нет нужных модулей Qt (Quick, Multimedia, WebEngineQuick, ShaderTools…)."
        version_ge "$ver" "$QT_MIN" || fail "В QT_PREFIX=$QT_PREFIX Qt $ver, нужен $QT_MIN или новее."
        QT_FOUND_PREFIX="$QT_PREFIX"
        ok "Qt $ver: $QT_PREFIX (из QT_PREFIX)"
        return 0
    fi
    # Qt из установщика или aqtinstall: ~/Qt/<версия>/gcc_64
    for prefix in "$QT_INSTALL_DIR"/*/gcc_64 "$HOME"/Qt/*/gcc_64 /opt/Qt/*/gcc_64 /opt/qt*/gcc_64; do
        [[ -d $prefix ]] || continue
        ver=$(qt_prefix_version "$prefix") || continue
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
    find_qt || fail "Qt $QT_VERSION установлен, но не найден в $QT_INSTALL_DIR/$QT_VERSION/gcc_64"
}

install_qt_aqt() {
    info "подготовка aqtinstall ($AQT_VENV)"
    if [[ ! -x $AQT_VENV/bin/python3 ]]; then
        python3 -m venv "$AQT_VENV" >>"$LOG" 2>&1 || fail "Не удалось создать окружение Python (нужен пакет python3-venv)."
    fi
    "$AQT_VENV/bin/python3" -m pip install -q -U pip aqtinstall >>"$LOG" 2>&1 || fail "Не удалось установить aqtinstall (pip)."
    local aqt="$AQT_VENV/bin/aqt"

    # Имя сборки: linux_gcc_64 (Qt 6.7+) или gcc_64 (старые)
    local arches arch=""
    arches=$("$aqt" list-qt linux desktop --arch "$QT_VERSION" 2>>"$LOG") ||
        fail "aqtinstall не знает Qt $QT_VERSION. Укажите другую версию: QT_VERSION=6.10.1 ./install.sh"
    for a in linux_gcc_64 gcc_64; do
        if [[ " $arches " == *" $a "* ]]; then arch=$a; break; fi
    done
    [[ -n $arch ]] || fail "Нет сборки Qt $QT_VERSION для linux x86_64 (есть: $arches)."

    # Нужные модули — только те, что есть у этой версии (часть уже входит в базовый пакет)
    local available wanted=(qtwebengine qtwebchannel qtpositioning qtmultimedia qtshadertools) mods=() m
    available=$("$aqt" list-qt linux desktop --modules "$QT_VERSION" "$arch" 2>>"$LOG") || available=""
    for m in "${wanted[@]}"; do
        if [[ " $available " == *" $m "* ]]; then mods+=("$m"); fi
    done
    info "загрузка Qt $QT_VERSION ($arch; модули: ${mods[*]:-базовые}) — это займёт время"
    mkdir -p "$QT_INSTALL_DIR"
    local cmd=("$aqt" install-qt linux desktop "$QT_VERSION" "$arch" -O "$QT_INSTALL_DIR")
    [[ ${#mods[@]} -gt 0 ]] && cmd+=(-m "${mods[@]}")
    "${cmd[@]}" >>"$LOG" 2>&1 || fail "aqtinstall не смог установить Qt $QT_VERSION."
    ok "Qt $QT_VERSION установлен в $QT_INSTALL_DIR/$QT_VERSION"
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
