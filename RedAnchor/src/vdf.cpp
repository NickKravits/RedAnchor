#include "vdf.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QtEndian>

namespace Vdf {

namespace {

const Node &nullNode()
{
    static const Node node;
    return node;
}

// ---------- Текстовый VDF ----------

class Tokenizer
{
public:
    explicit Tokenizer(const QByteArray &data) : m_data(data) {}

    enum Kind { End, Open, Close, String };

    Kind next(QByteArray *text)
    {
        for (;;) {
            skipSpace();
            if (m_pos >= m_data.size())
                return End;
            const char c = m_data.at(m_pos);
            if (c == '/' && m_pos + 1 < m_data.size() && m_data.at(m_pos + 1) == '/') {
                while (m_pos < m_data.size() && m_data.at(m_pos) != '\n')
                    ++m_pos;
                continue;
            }
            if (c == '[') {
                // Условия платформы ([$WIN32]) нам не нужны
                while (m_pos < m_data.size() && m_data.at(m_pos) != ']')
                    ++m_pos;
                ++m_pos;
                continue;
            }
            if (c == '{') {
                ++m_pos;
                return Open;
            }
            if (c == '}') {
                ++m_pos;
                return Close;
            }
            text->clear();
            if (c == '"') {
                ++m_pos;
                while (m_pos < m_data.size()) {
                    const char ch = m_data.at(m_pos++);
                    if (ch == '"')
                        break;
                    if (ch == '\\' && m_pos < m_data.size()) {
                        const char esc = m_data.at(m_pos++);
                        switch (esc) {
                        case 'n': text->append('\n'); break;
                        case 't': text->append('\t'); break;
                        case '\\': text->append('\\'); break;
                        case '"': text->append('"'); break;
                        default: text->append('\\').append(esc); break;
                        }
                        continue;
                    }
                    text->append(ch);
                }
                return String;
            }
            while (m_pos < m_data.size()) {
                const char ch = m_data.at(m_pos);
                if (ch == '"' || ch == '{' || ch == '}' || ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
                    break;
                text->append(ch);
                ++m_pos;
            }
            return String;
        }
    }

private:
    void skipSpace()
    {
        while (m_pos < m_data.size()) {
            const char c = m_data.at(m_pos);
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
                break;
            ++m_pos;
        }
    }

    const QByteArray &m_data;
    qsizetype m_pos = 0;
};

void parseObject(Tokenizer &tokens, Node &object, int depth)
{
    QByteArray key;
    QByteArray value;
    for (;;) {
        const Tokenizer::Kind kind = tokens.next(&key);
        if (kind == Tokenizer::End || kind == Tokenizer::Close)
            return;
        if (kind == Tokenizer::Open)
            continue;   // «{» без ключа — пропускаем
        Node child;
        child.key = QString::fromUtf8(key);
        const Tokenizer::Kind valueKind = tokens.next(&value);
        if (valueKind == Tokenizer::Open) {
            child.object = true;
            if (depth < 64)
                parseObject(tokens, child, depth + 1);
        } else if (valueKind == Tokenizer::String) {
            child.value = QString::fromUtf8(value);
        } else {
            object.children.append(child);
            return;
        }
        object.children.append(child);
    }
}

QByteArray escape(const QString &text)
{
    QByteArray out = text.toUtf8();
    out.replace('\\', "\\\\");
    out.replace('"', "\\\"");
    return out;
}

void writeNode(QByteArray &out, const Node &node, int depth)
{
    const QByteArray indent(depth, '\t');
    out += indent + '"' + escape(node.key) + '"';
    if (!node.object) {
        out += "\t\t\"" + escape(node.value) + "\"\n";
        return;
    }
    out += '\n' + indent + "{\n";
    for (const Node &child : node.children)
        writeNode(out, child, depth + 1);
    out += indent + "}\n";
}

// ---------- Двоичный KeyValues ----------

class BinaryReader
{
public:
    BinaryReader(const QByteArray &data, qsizetype pos = 0) : m_data(data), m_pos(pos) {}

    bool ok() const { return m_ok; }
    qsizetype pos() const { return m_pos; }
    void seek(qsizetype pos)
    {
        m_ok = pos >= 0 && pos <= m_data.size();
        m_pos = m_ok ? pos : m_data.size();
    }
    void fail()
    {
        m_ok = false;
        m_pos = m_data.size();
    }

    template<typename T>
    T read()
    {
        if (m_pos + qsizetype(sizeof(T)) > m_data.size()) {
            m_ok = false;
            m_pos = m_data.size();
            return T{};
        }
        const T value = qFromLittleEndian<T>(m_data.constData() + m_pos);
        m_pos += sizeof(T);
        return value;
    }
    quint8 byte()
    {
        if (m_pos >= m_data.size()) {
            m_ok = false;
            return 0x08;
        }
        return quint8(m_data.at(m_pos++));
    }
    void skip(qsizetype count)
    {
        m_pos += count;
        if (m_pos > m_data.size()) {
            m_ok = false;
            m_pos = m_data.size();
        }
    }
    QString cstring()
    {
        const qsizetype end = m_data.indexOf('\0', m_pos);
        if (end < 0) {
            m_ok = false;
            m_pos = m_data.size();
            return {};
        }
        const QString text = QString::fromUtf8(m_data.constData() + m_pos, end - m_pos);
        m_pos = end + 1;
        return text;
    }
    QString wstring()
    {
        QString text;
        while (m_ok) {
            const quint16 ch = read<quint16>();
            if (ch == 0)
                break;
            text.append(QChar(ch));
        }
        return text;
    }

private:
    const QByteArray &m_data;
    qsizetype m_pos = 0;
    bool m_ok = true;
};

// Список «тип, имя, значение» до маркера конца (0x08 или 0x0B).
// strings — таблица имён (appinfo версии 41: имя — номер строки в таблице)
void readBinaryObject(BinaryReader &in, Node &object, const QStringList *strings, int depth)
{
    while (in.ok()) {
        const quint8 type = in.byte();
        if (type == 0x08 || type == 0x0B)
            return;
        Node child;
        if (strings) {
            const qint32 index = in.read<qint32>();
            child.key = index >= 0 && index < strings->size() ? strings->at(index) : QString();
        } else {
            child.key = in.cstring();
        }
        switch (type) {
        case 0x00:
            child.object = true;
            if (depth > 64)
                return;
            readBinaryObject(in, child, strings, depth + 1);
            break;
        case 0x01: child.value = in.cstring(); break;
        case 0x02:
        case 0x04:
        case 0x06: child.value = QString::number(in.read<qint32>()); break;
        case 0x03: {
            const quint32 bits = in.read<quint32>();
            float f;
            memcpy(&f, &bits, sizeof(f));
            child.value = QString::number(double(f));
            break;
        }
        case 0x05: child.value = in.wstring(); break;
        case 0x07: child.value = QString::number(in.read<quint64>()); break;
        case 0x0A: child.value = QString::number(in.read<qint64>()); break;
        default:
            in.fail();   // неизвестный тип — дальше читать нельзя
            return;
        }
        object.children.append(child);
    }
}

QByteArray readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

// ---------- Node ----------

const Node *Node::find(const QString &name) const
{
    for (const Node &child : children) {
        if (child.key.compare(name, Qt::CaseInsensitive) == 0)
            return &child;
    }
    return nullptr;
}

Node *Node::find(const QString &name)
{
    for (Node &child : children) {
        if (child.key.compare(name, Qt::CaseInsensitive) == 0)
            return &child;
    }
    return nullptr;
}

const Node &Node::operator[](const QString &name) const
{
    const Node *node = find(name);
    return node ? *node : nullNode();
}

const Node &Node::at(const QString &path) const
{
    const Node *node = this;
    for (const QString &part : path.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        node = node->find(part);
        if (!node)
            return nullNode();
    }
    return *node;
}

QString Node::str(const QString &name, const QString &fallback) const
{
    const Node *node = find(name);
    return node && !node->object ? node->value : fallback;
}

qint64 Node::integer(const QString &name, qint64 fallback) const
{
    bool ok = false;
    const qint64 value = str(name).toLongLong(&ok);
    return ok ? value : fallback;
}

Node &Node::child(const QString &path)
{
    Node *node = this;
    for (const QString &part : path.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        Node *next = node->find(part);
        if (!next) {
            Node created;
            created.key = part;
            created.object = true;
            node->children.append(created);
            next = &node->children.last();
        } else if (!next->object) {
            next->object = true;
            next->value.clear();
        }
        node = next;
    }
    return *node;
}

void Node::set(const QString &name, const QString &newValue)
{
    if (Node *node = find(name)) {
        node->object = false;
        node->children.clear();
        node->value = newValue;
        return;
    }
    Node created;
    created.key = name;
    created.value = newValue;
    children.append(created);
}

bool Node::remove(const QString &name)
{
    for (qsizetype i = 0; i < children.size(); ++i) {
        if (children.at(i).key.compare(name, Qt::CaseInsensitive) == 0) {
            children.removeAt(i);
            return true;
        }
    }
    return false;
}

// ---------- Текстовый VDF ----------

Node parse(const QByteArray &text)
{
    Node root;
    root.object = true;
    Tokenizer tokens(text);
    parseObject(tokens, root, 0);
    return root;
}

Node parseFile(const QString &path, bool *ok)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (ok)
            *ok = false;
        Node root;
        root.object = true;
        return root;
    }
    if (ok)
        *ok = true;
    return parse(file.readAll());
}

QByteArray serialize(const Node &root)
{
    QByteArray out;
    for (const Node &child : root.children)
        writeNode(out, child, 0);
    return out;
}

bool writeFile(const QString &path, const Node &root)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(serialize(root));
    return file.commit();
}

// ---------- Двоичный кэш ----------

bool readAppInfo(const QString &path, const QHash<quint32, bool> &wanted, QHash<quint32, AppInfo> *out)
{
    const QByteArray data = readAll(path);
    BinaryReader in(data);
    const quint32 magic = in.read<quint32>();
    const quint32 version = magic & 0xff;
    if ((magic >> 8) != 0x075644 || version < 0x27 || version > 0x29) {
        if (!data.isEmpty())
            qWarning("Steam appinfo.vdf: unknown format 0x%08x", magic);
        return false;
    }
    in.read<quint32>();   // вселенная

    QStringList table;
    const QStringList *strings = nullptr;
    if (version >= 0x29) {
        // Имена ключей — в таблице строк в конце файла
        const qint64 tableOffset = in.read<qint64>();
        const qsizetype start = in.pos();
        in.seek(qsizetype(tableOffset));
        const quint32 count = in.read<quint32>();
        for (quint32 i = 0; i < count && in.ok(); ++i)
            table.append(in.cstring());
        if (!in.ok())
            return false;
        in.seek(start);
        strings = &table;
    }

    while (in.ok()) {
        const quint32 appId = in.read<quint32>();
        if (appId == 0 || !in.ok())
            break;
        const quint32 size = in.read<quint32>();
        const qsizetype end = in.pos() + qsizetype(size);
        if (!wanted.isEmpty() && !wanted.contains(appId)) {
            in.seek(end);
            continue;
        }
        // infoState, lastUpdated, picsToken, sha1, changeNumber, (v40+) sha1 данных
        in.skip(4 + 4 + 8 + 20 + 4 + (version >= 0x28 ? 20 : 0));
        Node root;
        readBinaryObject(in, root, strings, 0);
        in.seek(end);
        if (!in.ok())
            break;
        const Node &app = root.children.isEmpty() ? root : root.children.constFirst();
        const Node &common = app[QStringLiteral("common")];
        AppInfo info;
        info.appId = appId;
        info.name = common.str(QStringLiteral("name"));
        info.type = common.str(QStringLiteral("type")).toLower();
        info.osList = common.str(QStringLiteral("oslist")).toLower();
        info.controllerSupport = common.str(QStringLiteral("controller_support")).toLower();
        info.installDir = app.at(QStringLiteral("config")).str(QStringLiteral("installdir"));
        out->insert(appId, info);
    }
    return true;
}

bool readPackageInfo(const QString &path, QHash<quint32, QList<quint32>> *out)
{
    const QByteArray data = readAll(path);
    BinaryReader in(data);
    const quint32 magic = in.read<quint32>();
    const quint32 version = magic & 0xff;
    if ((magic >> 8) != 0x065655 || version < 0x27 || version > 0x28) {
        if (!data.isEmpty())
            qWarning("Steam packageinfo.vdf: unknown format 0x%08x", magic);
        return false;
    }
    in.read<quint32>();   // вселенная
    while (in.ok()) {
        const quint32 packageId = in.read<quint32>();
        if (packageId == 0xffffffff || !in.ok())
            break;
        in.skip(20 + 4 + (version >= 0x28 ? 8 : 0));   // sha1, changeNumber, (v40) токен
        Node root;
        readBinaryObject(in, root, nullptr, 0);
        if (!in.ok())
            break;
        const Node &package = root.children.isEmpty() ? root : root.children.constFirst();
        QList<quint32> apps;
        for (const Node &app : package[QStringLiteral("appids")].children)
            apps.append(app.value.toUInt());
        out->insert(packageId, apps);
    }
    return true;
}

} // namespace Vdf
