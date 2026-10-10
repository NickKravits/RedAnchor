// redanchor-pair-controller — сопряжение DualSense через кабель USB, как на консоли.
//
// Запускается от root через pkexec (правило polkit: data/org.redanchor.shell.policy).
//   1. Находит DualSense, подключённый по USB (hidraw), и его MAC.
//   2. Берёт ключ связи из хранилища BlueZ, если контроллер уже сопрягали с этим
//      адаптером, иначе создаёт новый.
//   3. Записывает в контроллер адрес адаптера и ключ (feature-отчёт 0x0A).
//   4. Если ключ новый — сохраняет его в /var/lib/bluetooth и перезапускает bluetoothd,
//      чтобы тот его подхватил (у BlueZ нет D-Bus API для загрузки ключа).
// После этого контроллер при нажатии PS сам подключается к консоли по Bluetooth.
//
// Использование: redanchor-pair-controller <MAC адаптера>
// Вывод при успехе: "paired <MAC контроллера> new|existing".
// Коды выхода: 0 — успех, 1 — неверные аргументы, 2 — контроллер не найден,
//              3 — ошибка записи в контроллер, 4 — ошибка хранилища BlueZ.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

constexpr uint16_t SonyVendor = 0x054c;
constexpr uint16_t DualSenseProduct = 0x0ce6;
constexpr uint16_t DualSenseEdgeProduct = 0x0df2;

constexpr uint8_t ReportPairingInfo = 0x09; // чтение: MAC контроллера и адрес сопряжённого хоста
constexpr uint8_t ReportSetPairing = 0x0a;  // запись: адрес хоста и ключ связи
constexpr size_t DefaultPairingInfoSize = 20;
constexpr size_t DefaultSetPairingSize = 27;

const fs::path BluezStorage = "/var/lib/bluetooth";

using Address = std::array<uint8_t, 6>; // в обычном порядке: AA:BB:... -> {0xAA, 0xBB, ...}
using LinkKey = std::array<uint8_t, 16>;

std::string upper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
    return s;
}

std::optional<Address> parseAddress(const std::string &text)
{
    Address addr{};
    unsigned v[6];
    char tail;
    if (std::sscanf(text.c_str(), "%2x:%2x:%2x:%2x:%2x:%2x%c", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &tail) != 6)
        return std::nullopt;
    for (int i = 0; i < 6; ++i)
        addr[i] = uint8_t(v[i]);
    return addr;
}

std::string formatAddress(const Address &addr)
{
    char buf[18];
    std::snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1], addr[2], addr[3], addr[4],
                  addr[5]);
    return buf;
}

std::string toHex(const uint8_t *data, size_t size)
{
    std::string out;
    char buf[3];
    for (size_t i = 0; i < size; ++i) {
        std::snprintf(buf, sizeof(buf), "%02X", data[i]);
        out += buf;
    }
    return out;
}

std::optional<LinkKey> parseKey(const std::string &hex)
{
    if (hex.size() != 32)
        return std::nullopt;
    LinkKey key{};
    for (size_t i = 0; i < 16; ++i) {
        unsigned v;
        if (std::sscanf(hex.c_str() + i * 2, "%2x", &v) != 1)
            return std::nullopt;
        key[i] = uint8_t(v);
    }
    return key;
}

std::string readFile(const fs::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool writeFile(const fs::path &path, const std::string &content)
{
    const fs::path tmp = path.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out)
            return false;
        out << content;
        if (!out.flush())
            return false;
    }
    ::chmod(tmp.c_str(), 0600);
    std::error_code ec;
    fs::rename(tmp, path, ec);
    return !ec;
}

// Простейший разбор ini-файлов BlueZ: секция -> ключ -> значение.
using Ini = std::map<std::string, std::map<std::string, std::string>>;

Ini parseIni(const std::string &text)
{
    Ini ini;
    std::string section;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.size() > 2 && line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            ini[section];
            continue;
        }
        const auto eq = line.find('=');
        if (eq != std::string::npos && !section.empty())
            ini[section][line.substr(0, eq)] = line.substr(eq + 1);
    }
    return ini;
}

// Размеры feature-отчётов (в байтах, с байтом Report ID) по дескриптору HID.
std::map<int, size_t> featureReportSizes(const std::string &desc)
{
    std::map<int, size_t> bits;
    uint32_t reportSize = 0, reportCount = 0, reportId = 0;
    std::vector<std::array<uint32_t, 3>> stack;
    for (size_t i = 0; i < desc.size();) {
        const uint8_t prefix = uint8_t(desc[i]);
        if (prefix == 0xfe) { // длинный элемент
            if (i + 1 >= desc.size())
                break;
            i += 3 + uint8_t(desc[i + 1]);
            continue;
        }
        size_t size = prefix & 3;
        if (size == 3)
            size = 4;
        const int type = (prefix >> 2) & 3;
        const int tag = prefix >> 4;
        uint32_t value = 0;
        for (size_t b = 0; b < size && i + 1 + b < desc.size(); ++b)
            value |= uint32_t(uint8_t(desc[i + 1 + b])) << (8 * b);
        i += 1 + size;

        if (type == 1) { // Global
            switch (tag) {
            case 7: reportSize = value; break;
            case 8: reportId = value; break;
            case 9: reportCount = value; break;
            case 10: stack.push_back({reportSize, reportCount, reportId}); break;
            case 11:
                if (!stack.empty()) {
                    reportSize = stack.back()[0];
                    reportCount = stack.back()[1];
                    reportId = stack.back()[2];
                    stack.pop_back();
                }
                break;
            default: break;
            }
        } else if (type == 0 && tag == 0xb) { // Main: Feature
            bits[int(reportId)] += size_t(reportSize) * reportCount;
        }
    }
    std::map<int, size_t> bytes;
    for (const auto &[id, b] : bits)
        bytes[id] = (b + 7) / 8 + (id ? 1 : 0);
    return bytes;
}

struct Controller {
    fs::path node;   // /dev/hidrawN
    fs::path sysfs;  // /sys/class/hidraw/hidrawN/device
    std::string uniq;
    uint16_t product = 0;
};

std::optional<Controller> findUsbDualSense()
{
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator("/sys/class/hidraw", ec)) {
        const fs::path device = entry.path() / "device";
        std::istringstream uevent(readFile(device / "uevent"));
        unsigned bus = 0, vendor = 0, product = 0;
        std::string uniq;
        for (std::string line; std::getline(uevent, line);) {
            if (line.rfind("HID_ID=", 0) == 0)
                std::sscanf(line.c_str() + 7, "%x:%x:%x", &bus, &vendor, &product);
            else if (line.rfind("HID_UNIQ=", 0) == 0)
                uniq = upper(line.substr(9));
        }
        if (bus == 0x03 && vendor == SonyVendor && (product == DualSenseProduct || product == DualSenseEdgeProduct))
            return Controller{fs::path("/dev") / entry.path().filename(), device, uniq, uint16_t(product)};
    }
    return std::nullopt;
}

bool getFeature(int fd, std::vector<uint8_t> &buf)
{
    return ::ioctl(fd, HIDIOCGFEATURE(buf.size()), buf.data()) >= 0;
}

bool setFeature(int fd, const std::vector<uint8_t> &buf)
{
    return ::ioctl(fd, HIDIOCSFEATURE(buf.size()), buf.data()) >= 0;
}

bool randomKey(LinkKey &key)
{
    std::ifstream in("/dev/urandom", std::ios::binary);
    return bool(in.read(reinterpret_cast<char *>(key.data()), key.size()));
}

// Ищет в кэше BlueZ записи SDP другого DualSense: у всех контроллеров они одинаковые,
// а с ними BlueZ сразу примет подключение, не опрашивая контроллер.
std::string donorServiceRecords(const fs::path &adapterDir, const std::string &deviceMac)
{
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(adapterDir / "cache", ec)) {
        if (upper(entry.path().filename().string()) == deviceMac)
            continue;
        const std::string text = readFile(entry.path());
        const Ini ini = parseIni(text);
        const auto general = ini.find("General");
        if (general == ini.end() || general->second.count("Name") == 0
            || general->second.at("Name").find("DualSense") == std::string::npos)
            continue;
        const auto pos = text.find("[ServiceRecords]");
        if (pos == std::string::npos)
            continue;
        const auto end = text.find("\n[", pos + 1);
        return text.substr(pos, end == std::string::npos ? std::string::npos : end + 1 - pos);
    }
    return {};
}

int fail(int code, const char *message)
{
    std::fprintf(stderr, "redanchor-pair-controller: %s\n", message);
    return code;
}

} // namespace

int main(int argc, char *argv[])
{
    if (argc != 2)
        return fail(1, "usage: redanchor-pair-controller <adapter MAC>");
    const auto adapter = parseAddress(argv[1]);
    if (!adapter)
        return fail(1, "invalid adapter address");
    const std::string adapterMac = formatAddress(*adapter);

    const auto controller = findUsbDualSense();
    if (!controller)
        return fail(2, "no DualSense connected over USB");

    const int fd = ::open(controller->node.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0)
        return fail(3, "cannot open hidraw device");

    const auto sizes = featureReportSizes(readFile(controller->sysfs / "report_descriptor"));
    const auto sizeOf = [&](int id, size_t fallback) {
        const auto it = sizes.find(id);
        return it != sizes.end() && it->second > 1 ? it->second : fallback;
    };

    // MAC контроллера: из ядра (hid-playstation) или из отчёта 0x09 (байты 1..6, в обратном порядке).
    std::vector<uint8_t> info(sizeOf(ReportPairingInfo, DefaultPairingInfoSize), 0);
    info[0] = ReportPairingInfo;
    const bool haveInfo = getFeature(fd, info) && info.size() >= 16;
    std::string deviceMac = controller->uniq;
    if (!parseAddress(deviceMac)) {
        if (!haveInfo) {
            ::close(fd);
            return fail(3, "cannot read controller address");
        }
        Address dev{};
        for (int i = 0; i < 6; ++i)
            dev[i] = info[6 - i];
        deviceMac = formatAddress(dev);
    }

    // Ключ: уже сохранённый BlueZ для этой пары или новый.
    const fs::path adapterDir = BluezStorage / adapterMac;
    const fs::path deviceDir = adapterDir / deviceMac;
    const Ini stored = parseIni(readFile(deviceDir / "info"));
    std::optional<LinkKey> key;
    if (const auto it = stored.find("LinkKey"); it != stored.end() && it->second.count("Key"))
        key = parseKey(it->second.at("Key"));
    const bool newKey = !key;
    if (newKey) {
        key.emplace();
        if (!randomKey(*key)) {
            ::close(fd);
            return fail(4, "cannot generate link key");
        }
    }

    // Отчёт 0x0A: адрес хоста (младший байт первым) и ключ связи.
    std::vector<uint8_t> pairing(sizeOf(ReportSetPairing, DefaultSetPairingSize), 0);
    pairing[0] = ReportSetPairing;
    for (int i = 0; i < 6; ++i)
        pairing[1 + i] = (*adapter)[5 - i];
    std::copy(key->begin(), key->end(), pairing.begin() + 7);
    if (!setFeature(fd, pairing)) {
        ::close(fd);
        return fail(3, "controller rejected pairing report");
    }

    // Проверка: контроллер должен вернуть адрес нашего адаптера (байты 10..15).
    std::fill(info.begin() + 1, info.end(), 0);
    if (getFeature(fd, info) && info.size() >= 16) {
        Address host{};
        for (int i = 0; i < 6; ++i)
            host[i] = info[15 - i];
        if (host != *adapter)
            std::fprintf(stderr, "redanchor-pair-controller: warning: controller reports host %s\n",
                         formatAddress(host).c_str());
    }
    ::close(fd);

    if (!newKey) {
        std::printf("paired %s existing\n", deviceMac.c_str());
        return 0;
    }

    // bluetoothd читает хранилище только при запуске: останавливаем, пишем, запускаем.
    if (std::system("systemctl stop bluetooth.service") != 0)
        std::fprintf(stderr, "redanchor-pair-controller: warning: failed to stop bluetooth.service\n");

    std::error_code ec;
    fs::create_directories(deviceDir, ec);
    fs::create_directories(adapterDir / "cache", ec);
    ::chmod(deviceDir.c_str(), 0700);

    const std::string records = donorServiceRecords(adapterDir, deviceMac);
    std::ostringstream infoText;
    infoText << "[General]\n"
             << "Name=DualSense Wireless Controller\n"
             << "Class=0x002508\n"
             << "SupportedTechnologies=BR/EDR;\n"
             << "Trusted=true\n"
             << "Blocked=false\n"
             << "WakeAllowed=true\n";
    // Без записей SDP список сервисов не указываем: BlueZ сам опросит контроллер
    // при первом подключении (BluetoothManager вызывает для этого Device1.Connect).
    if (!records.empty())
        infoText << "Services=00001124-0000-1000-8000-00805f9b34fb;00001200-0000-1000-8000-00805f9b34fb;\n";
    infoText << "\n[LinkKey]\n"
             << "Key=" << toHex(key->data(), key->size()) << "\n"
             << "Type=4\n"
             << "PINLength=0\n"
             << "\n[DeviceID]\n"
             << "Source=2\n"
             << "Vendor=" << SonyVendor << "\n"
             << "Product=" << controller->product << "\n"
             << "Version=256\n";

    bool ok = writeFile(deviceDir / "info", infoText.str());
    if (ok && !records.empty())
        ok = writeFile(adapterDir / "cache" / deviceMac, "[General]\nName=DualSense Wireless Controller\n\n" + records);

    if (std::system("systemctl start bluetooth.service") != 0)
        std::fprintf(stderr, "redanchor-pair-controller: warning: failed to start bluetooth.service\n");

    if (!ok)
        return fail(4, "cannot write BlueZ storage");
    std::printf("paired %s new\n", deviceMac.c_str());
    return 0;
}
