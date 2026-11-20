#pragma once
// Logic of the command history of the console's Line view, without Qt (testable on its own:
//   g++ -std=c++17 -Iplugins/device/console tools/historycore_test.cpp -o /tmp/historycore_test && /tmp/historycore_test).
//
// An entry is what was sent in one go: one command or a whole block of lines (a SCPI sequence).
// Blank lines are not part of it (they are not sent), lines are trimmed. Sending the same text again
// moves it to the front. Pinned entries stay on top and are never pushed out by the limit.
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace cmdhist {

struct Entry {
    std::string text;       // normalized: lines separated by '\n'
    bool pinned = false;
};

class HistoryCore
{
public:
    explicit HistoryCore(std::size_t limit = 100) : m_limit(limit) {}

    // Non-blank, trimmed lines of a text: exactly what a send of this text transmits.
    static std::vector<std::string> lines(const std::string &text)
    {
        std::vector<std::string> out;
        std::string cur;
        auto flush = [&] {
            std::size_t b = 0, e = cur.size();
            while (b < e && isSpace(cur[b])) ++b;
            while (e > b && isSpace(cur[e - 1])) --e;
            if (e > b)
                out.push_back(cur.substr(b, e - b));
            cur.clear();
        };
        for (char c : text) {
            if (c == '\n')
                flush();
            else
                cur += c;      // '\r' is white space and goes away with the trimming
        }
        flush();
        return out;
    }

    static std::string normalize(const std::string &text)
    {
        std::string out;
        for (const std::string &l : lines(text)) {
            if (!out.empty())
                out += '\n';
            out += l;
        }
        return out;
    }

    std::size_t limit() const { return m_limit; }
    void setLimit(std::size_t n) { m_limit = n; trim(); }

    // Returns false if there is nothing to remember (blank text).
    bool add(const std::string &text)
    {
        const std::string n = normalize(text);
        if (n.empty())
            return false;
        Entry e{n, false};
        const int i = indexOf(n);
        if (i >= 0) {
            e.pinned = m_e[std::size_t(i)].pinned;
            m_e.erase(m_e.begin() + i);
        }
        m_e.insert(m_e.begin(), e);
        trim();
        return true;
    }

    // Pins (adding the text if needed) or unpins. False for blank text.
    bool setPinned(const std::string &text, bool pinned)
    {
        const std::string n = normalize(text);
        if (n.empty())
            return false;
        int i = indexOf(n);
        if (i < 0) {
            if (!pinned)
                return true;
            m_e.insert(m_e.begin(), Entry{n, true});   // pinned from the start: pushes nothing out
            return true;
        }
        m_e[std::size_t(i)].pinned = pinned;
        trim();
        return true;
    }

    bool isPinned(const std::string &text) const
    {
        const int i = indexOf(normalize(text));
        return i >= 0 && m_e[std::size_t(i)].pinned;
    }

    bool contains(const std::string &text) const { return indexOf(normalize(text)) >= 0; }

    bool remove(const std::string &text)
    {
        const int i = indexOf(normalize(text));
        if (i < 0)
            return false;
        m_e.erase(m_e.begin() + i);
        return true;
    }

    void clear(bool keepPinned = true)
    {
        if (!keepPinned) {
            m_e.clear();
            return;
        }
        m_e.erase(std::remove_if(m_e.begin(), m_e.end(), [](const Entry &e) { return !e.pinned; }),
                  m_e.end());
    }

    // For display and navigation: pinned first, then the rest, each newest first.
    std::vector<Entry> ordered() const
    {
        std::vector<Entry> out;
        for (const Entry &e : m_e)
            if (e.pinned)
                out.push_back(e);
        for (const Entry &e : m_e)
            if (!e.pinned)
                out.push_back(e);
        return out;
    }

    // Storage order (newest first); assign() takes it back and tidies it up.
    const std::vector<Entry> &raw() const { return m_e; }
    void assign(const std::vector<Entry> &entries)
    {
        m_e.clear();
        for (const Entry &e : entries) {
            const std::string n = normalize(e.text);
            if (!n.empty() && indexOf(n) < 0)
                m_e.push_back({n, e.pinned});
        }
        trim();
    }

private:
    static bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

    int indexOf(const std::string &normalized) const
    {
        for (std::size_t i = 0; i < m_e.size(); ++i)
            if (m_e[i].text == normalized)
                return int(i);
        return -1;
    }

    void trim()
    {
        std::size_t unpinned = 0;
        for (const Entry &e : m_e)
            if (!e.pinned)
                ++unpinned;
        for (std::size_t i = m_e.size(); i-- > 0 && unpinned > m_limit;) {
            if (!m_e[i].pinned) {
                m_e.erase(m_e.begin() + std::ptrdiff_t(i));
                --unpinned;
            }
        }
    }

    std::size_t m_limit;
    std::vector<Entry> m_e;    // newest first
};

} // namespace cmdhist
