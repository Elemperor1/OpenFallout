#include "xml.hpp"

#include <algorithm>
#include <cctype>

namespace FalloutMenu::Xml
{
    namespace
    {
        bool isSpace(char c)
        {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
        }

        bool isNameEnd(char c)
        {
            return isSpace(c) || c == '>' || c == '/' || c == '=' || c == '<';
        }
    }

    const std::string* Node::attribute(std::string_view name) const
    {
        for (const auto& [key, value] : mAttributes)
            if (key == name)
                return &value;
        return nullptr;
    }

    std::string_view trim(std::string_view text)
    {
        while (!text.empty() && isSpace(text.front()))
            text.remove_prefix(1);
        while (!text.empty() && isSpace(text.back()))
            text.remove_suffix(1);
        return text;
    }

    std::string lower(std::string_view text)
    {
        std::string result(text);
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    std::vector<Node> parse(std::string_view text)
    {
        // The nodes are built in a stack of unfinished elements and moved to the parent when they close, so no pointer
        // to a node is kept while its parent grows.
        std::vector<Node> stack(1);
        const auto close = [&stack]() {
            Node done = std::move(stack.back());
            stack.pop_back();
            stack.back().mChildren.push_back(std::move(done));
        };

        // A byte order mark in front of the file
        if (text.size() >= 3 && text.substr(0, 3) == "\xEF\xBB\xBF")
            text.remove_prefix(3);

        std::size_t i = 0;
        const std::size_t n = text.size();
        while (i < n)
        {
            if (text[i] != '<')
            {
                const std::size_t next = text.find('<', i);
                const std::size_t end = next == std::string_view::npos ? n : next;
                stack.back().mText.append(text.substr(i, end - i));
                i = end;
                continue;
            }

            if (text.compare(i, 4, "<!--") == 0)
            {
                // `<!--- text --->` ends with the same three characters as a well formed comment
                const std::size_t end = text.find("-->", i + 4);
                i = end == std::string_view::npos ? n : end + 3;
                continue;
            }
            if (i + 1 < n && (text[i + 1] == '?' || text[i + 1] == '!'))
            {
                const std::size_t end = text.find('>', i);
                i = end == std::string_view::npos ? n : end + 1;
                continue;
            }

            if (i + 1 < n && text[i + 1] == '/')
            {
                std::size_t end = text.find('>', i);
                const std::string name
                    = lower(trim(text.substr(i + 2, (end == std::string_view::npos ? n : end) - i - 2)));
                i = end == std::string_view::npos ? n : end + 1;
                // A closing tag ends the nearest open element of that name, and the elements that were left open on the
                // way are closed with it. One that names nothing that is open (`<hotrect>` ended by `</rect>`, which
                // the menu files have) ends the element that is open.
                const auto open = std::find_if(
                    stack.rbegin(), stack.rend() - 1, [&name](const Node& node) { return node.mName == name; });
                if (open != stack.rend() - 1)
                    while (stack.back().mName != name)
                        close();
                if (stack.size() > 1)
                    close();
                continue;
            }

            // An opening tag
            std::size_t p = i + 1;
            const std::size_t nameStart = p;
            while (p < n && !isNameEnd(text[p]))
                ++p;
            Node node;
            node.mName = lower(text.substr(nameStart, p - nameStart));
            bool selfClosing = false;
            while (p < n)
            {
                while (p < n && isSpace(text[p]))
                    ++p;
                if (p >= n)
                    break;
                if (text[p] == '>')
                {
                    ++p;
                    break;
                }
                if (text[p] == '/')
                {
                    selfClosing = true;
                    ++p;
                    continue;
                }
                if (text[p] == '<')
                    break;
                const std::size_t keyStart = p;
                while (p < n && !isNameEnd(text[p]))
                    ++p;
                std::string key = lower(text.substr(keyStart, p - keyStart));
                while (p < n && isSpace(text[p]))
                    ++p;
                std::string value;
                if (p < n && text[p] == '=')
                {
                    ++p;
                    while (p < n && isSpace(text[p]))
                        ++p;
                    if (p < n && (text[p] == '"' || text[p] == '\''))
                    {
                        const char quote = text[p++];
                        const std::size_t valueStart = p;
                        while (p < n && text[p] != quote)
                            ++p;
                        value = std::string(text.substr(valueStart, p - valueStart));
                        if (p < n)
                            ++p;
                    }
                    else
                    {
                        // `name=value"`: no opening quote. The value ends at the closing quote or at white space.
                        const std::size_t valueStart = p;
                        while (p < n && text[p] != '"' && !isSpace(text[p]) && text[p] != '>' && text[p] != '/')
                            ++p;
                        value = std::string(text.substr(valueStart, p - valueStart));
                        if (p < n && text[p] == '"')
                            ++p;
                    }
                }
                if (!key.empty())
                    node.mAttributes.emplace_back(std::move(key), std::move(value));
                else if (p < n && !isSpace(text[p]) && text[p] != '>' && text[p] != '/')
                    ++p;
            }
            i = p;
            if (node.mName.empty())
                continue;
            stack.push_back(std::move(node));
            if (selfClosing)
                close();
        }
        while (stack.size() > 1)
            close();
        return std::move(stack.front().mChildren);
    }
}
