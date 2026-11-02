#include "menufile.hpp"

#include <algorithm>
#include <cctype>

namespace FalloutMenu
{
    namespace
    {
        struct EntityRow
        {
            const char* mName;
            float mValue;
        };

        // The values the games' menu loader knows by name (the codes of the menus, the stacking types, the
        // justifications, the system colours and the pad buttons), read from the executable of New Vegas. The ones that
        // depend on the machine (`&xbox;`, `&xenon;`, `&widescreen;`) are 0 here, a PC without a pad; the program that
        // shows the menu gives the right value. The menu files also use a few words that the executable does not know
        // (`&xitem;`,
        // `&xlist;`, `&xboxhint;`, `&first;`, `&last;`, `&prev;`, `&next;`); those stay as the text they are, as in the
        // game.
        constexpr EntityRow entityTable[] = {
            { "false", 0 },
            { "true", 1 },
            { "generic", -1 },
            { "left", 1 },
            { "center", 2 },
            { "right", 4 },
            { "up", 5 },
            { "down", 6 },
            { "scale", -1 },
            { "click_past", 101 },
            { "no_click_past", 102 },
            { "does_not_stack", 6008 },
            { "mixed_menu", 103 },
            { "console", 0 },
            { "highdef", 1 },
            { "glow_branch", 110 },
            { "noglow_branch", 111 },
            { "pipboymenu", 112 },
            { "uselocalcolor", -1 },
            { "nosystemcolor", 0 },
            { "nosound", -1 },
            { "default_accept", 9 },
            { "xbuttona", 9 },
            { "xbuttonb", 10 },
            { "xbuttonx", 11 },
            { "xbuttony", 12 },
            { "xbuttonlt", 13 },
            { "xbuttonrt", 14 },
            { "xbuttonlb", 15 },
            { "xbuttonrb", 16 },
            { "xbuttonls", 17 },
            { "xbuttonrs", 18 },
            { "xbuttonl", 17 },
            { "xbuttonr", 18 },
            { "audiomenu", 1017 },
            { "bookmenu", 1026 },
            { "chargenmenu", 1048 },
            { "containermenu", 1008 },
            { "dialogmenu", 1009 },
            { "gameplaymenu", 1020 },
            { "hudmainmenu", 1004 },
            { "inventorymenu", 1002 },
            { "loadingmenu", 1007 },
            { "lockpickmenu", 1014 },
            { "mapmenu", 1023 },
            { "messagemenu", 1001 },
            { "tutorialmenu", 1059 },
            { "texteditmenu", 1051 },
            { "startmenu", 1013 },
            { "quantitymenu", 1016 },
            { "racesexmenu", 1036 },
            { "sleepwaitmenu", 1012 },
            { "statsmenu", 1003 },
            { "videomenu", 1018 },
            { "levelupmenu", 1027 },
            { "repairmenu", 1035 },
            { "itemmodmenu", 1061 },
            { "repairservicesmenu", 1058 },
            { "creditsmenu", 1047 },
            { "bartermenu", 1053 },
            { "surgerymenu", 1054 },
            { "hackingmenu", 1055 },
            { "computersmenu", 1057 },
            { "vatsmenu", 1056 },
            { "specialbookmenu", 1060 },
            { "companionwheelmenu", 1075 },
            { "lovetestermenu", 1074 },
            { "traitselectmenu", 1076 },
            { "slotmachinemenu", 1080 },
            { "blackjackmenu", 1081 },
            { "roulettemenu", 1082 },
            { "recipemenu", 1077 },
            { "caravanmenu", 1083 },
            { "traitmenu", 1084 },
            // The system colours, numbered in the order the executable's colour manager lists them (the HUD's main
            // colour is the first); none is 0 and the colour of the tile itself is -1.
            { "hudmain", 1 },
            { "hudalt", 2 },
            { "terminal", 3 },
            { "pipboy", 4 },
            { "mainmenu", 5 },
            { "system", 6 },
            // The classes of the menus the games open without a menu of their own, and the kinds of items in a list of
            // saves; the exe gives them numbers that no menu file depends on
            { "genericmenu", 1999 },
            { "_cancelbutton", 5001 },
            { "_listitem", 5002 },
            { "_savedpicture", 5003 },
            { "_listcontents", 5004 },
            // False on a PC without a pad; the screen that has one sets it
            { "xbox", 0 },
            { "xenon", 0 },
            { "widescreen", 0 },
        };

        std::optional<TileType> typeOf(std::string_view tag)
        {
            if (tag == "menu")
                return TileType::Menu;
            if (tag == "rect")
                return TileType::Rect;
            if (tag == "image")
                return TileType::Image;
            if (tag == "text")
                return TileType::Text;
            if (tag == "hotrect")
                return TileType::HotRect;
            if (tag == "window")
                return TileType::Window;
            if (tag == "radial")
                return TileType::Radial;
            if (tag == "nif" || tag == "3d")
                return TileType::Nif;
            if (tag == "template")
                return TileType::Template;
            return std::nullopt;
        }

        std::optional<Op> opOf(std::string_view tag)
        {
            static const std::pair<std::string_view, Op> ops[] = {
                { "copy", Op::Copy },
                { "add", Op::Add },
                { "sub", Op::Sub },
                { "mul", Op::Mul },
                { "mult", Op::Mul },
                { "div", Op::Div },
                { "mod", Op::Mod },
                { "min", Op::Min },
                { "max", Op::Max },
                { "abs", Op::Abs },
                { "floor", Op::Floor },
                { "ceil", Op::Ceil },
                { "round", Op::Round },
                { "trunc", Op::Trunc },
                { "eq", Op::Eq },
                { "neq", Op::Neq },
                { "gt", Op::Gt },
                { "gte", Op::Gte },
                { "lt", Op::Lt },
                { "lte", Op::Lte },
                { "and", Op::And },
                { "or", Op::Or },
                { "not", Op::Not },
                { "onlyif", Op::OnlyIf },
                { "onlyifnot", Op::OnlyIfNot },
                { "ref", Op::Ref },
            };
            for (const auto& [name, op] : ops)
                if (name == tag)
                    return op;
            return std::nullopt;
        }

        /// `parent()`, `sibling(name)`, `box`: what a `src` attribute says
        void parseSource(std::string_view text, Source& source, std::string& name)
        {
            text = Xml::trim(text);
            std::string argument;
            const std::size_t open = text.find('(');
            std::string_view head = text;
            if (open != std::string_view::npos)
            {
                head = Xml::trim(text.substr(0, open));
                const std::size_t close = text.find(')', open);
                argument = std::string(Xml::trim(text.substr(
                    open + 1, close == std::string_view::npos ? std::string_view::npos : close - open - 1)));
            }
            const std::string key = Xml::lower(head);
            name = argument;
            if (key == "me")
                source = Source::Me;
            else if (key == "parent")
                source = Source::Parent;
            else if (key == "grandparent")
                source = Source::GrandParent;
            else if (key == "child")
                source = Source::Child;
            else if (key == "sibling")
                source = Source::Sibling;
            else if (key == "screen")
                source = Source::Screen;
            else if (key == "globals" || key == "strings" || key == "gloabls")
                source = Source::Globals;
            else if (key == "io")
                source = Source::Io;
            else
            {
                source = Source::Named;
                name = std::string(Xml::trim(text));
            }
        }

        std::string directoryOf(const std::string& path)
        {
            const std::size_t slash = path.rfind('/');
            return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
        }
    }

    std::optional<TileType> tileTypeOf(std::string_view tag)
    {
        return typeOf(tag);
    }

    std::optional<float> builtinEntity(std::string_view name)
    {
        for (const EntityRow& row : entityTable)
            if (name == row.mName)
                return row.mValue;
        return std::nullopt;
    }

    Loader::Loader(LoaderEnvironment environment)
        : mEnvironment(std::move(environment))
    {
    }

    const std::vector<Xml::Node>* Loader::file(const std::string& path)
    {
        const auto found = mFiles.find(path);
        if (found != mFiles.end())
            return &found->second;
        if (mMissing.count(path) > 0 || !mEnvironment.mReadFile)
            return nullptr;
        const std::optional<std::string> text = mEnvironment.mReadFile(path);
        if (!text)
        {
            mMissing[path] = true;
            return nullptr;
        }
        return &mFiles.emplace(path, Xml::parse(*text)).first->second;
    }

    std::string Loader::resolveInclude(const std::string& from, const std::string& source)
    {
        std::string name = Xml::lower(Xml::trim(source));
        std::replace(name.begin(), name.end(), '\\', '/');
        const std::string candidates[] = {
            directoryOf(from) + name,
            "menus/prefabs/" + name,
            "menus/" + name,
            name,
        };
        for (const std::string& candidate : candidates)
            if (file(candidate) != nullptr)
                return candidate;
        return {};
    }

    std::string Loader::substitute(std::string_view text)
    {
        // `&name;` becomes the value of the name. A name that is not known is left as it is. An `&` that has no `;` in
        // the text before white space ends it (the files have a few of those) is not a name either, except for the
        // setting names (`&-sName`), which the games read to the end of the word.
        std::string result;
        std::size_t i = 0;
        while (i < text.size())
        {
            if (text[i] != '&')
            {
                result.push_back(text[i++]);
                continue;
            }
            std::size_t end = i + 1;
            while (end < text.size() && text[end] != ';' && text[end] != ' ' && text[end] != '\t' && text[end] != '\r'
                && text[end] != '\n' && text[end] != '&')
                ++end;
            const bool terminated = end < text.size() && text[end] == ';';
            const std::string name = Xml::lower(text.substr(i + 1, end - i - 1));
            std::optional<std::string> replacement;
            if (name.size() > 1 && name[0] == '-')
            {
                // The setting is looked up with the case the file used (`&-sName;` asks for the setting `sName`)
                const std::string setting(text.substr(i + 2, end - i - 2));
                if (mEnvironment.mSetting)
                    replacement = mEnvironment.mSetting(setting);
                if (!replacement)
                    replacement = std::string();
            }
            else if (terminated)
            {
                std::optional<float> value = builtinEntity(name);
                if (!value && mEnvironment.mEntity)
                    value = mEnvironment.mEntity(name);
                if (value)
                    replacement = Value(*value).text();
            }
            if (replacement)
            {
                result += *replacement;
                i = terminated ? end + 1 : end;
            }
            else
            {
                result.push_back(text[i++]);
            }
        }
        return result;
    }

    Value Loader::literal(std::string_view text)
    {
        return Value::fromText(Xml::trim(substitute(text)));
    }

    Operand Loader::buildOperand(const Xml::Node& node)
    {
        Operand operand;
        const std::string* source = node.attribute("src");
        const std::string* trait = node.attribute("trait");
        if (trait != nullptr)
        {
            operand.mKind = Operand::Kind::Trait;
            operand.mTrait = Xml::lower(Xml::trim(*trait));
            if (source != nullptr)
                parseSource(*source, operand.mSource, operand.mSourceName);
            else
                operand.mSource = Source::Me;
            return operand;
        }
        bool nested = false;
        for (const Xml::Node& child : node.mChildren)
            nested = nested || opOf(child.mName).has_value();
        if (nested)
        {
            operand.mKind = Operand::Kind::Nested;
            operand.mNested = buildExpression(node);
            return operand;
        }
        const std::string_view text = Xml::trim(node.mText);
        if (!text.empty())
        {
            operand.mKind = Operand::Kind::Literal;
            operand.mLiteral = literal(node.mText);
        }
        return operand;
    }

    std::shared_ptr<const Expression> Loader::buildExpression(const Xml::Node& node)
    {
        auto expression = std::make_shared<Expression>();
        for (const Xml::Node& child : node.mChildren)
        {
            const std::optional<Op> op = opOf(child.mName);
            if (!op)
                continue;
            Step step;
            step.mOp = *op;
            step.mOperand = buildOperand(child);
            expression->mSteps.push_back(std::move(step));
        }
        return expression;
    }

    TraitDef Loader::buildTrait(const Xml::Node& node)
    {
        TraitDef def;
        bool calculated = false;
        for (const Xml::Node& child : node.mChildren)
            calculated = calculated || opOf(child.mName).has_value();
        if (calculated)
            def.mExpression = buildExpression(node);
        else
            def.mLiteral = literal(node.mText);
        return def;
    }

    void Loader::fill(TileDef& tile, const std::vector<Xml::Node>& nodes, const std::string& path, int depth)
    {
        for (const Xml::Node& node : nodes)
        {
            if (node.mName == "include")
            {
                const std::string* source = node.attribute("src");
                if (source == nullptr)
                    continue;
                const std::string resolved = resolveInclude(path, *source);
                if (resolved.empty() || depth > 12)
                {
                    mProblems.push_back(
                        path + ": include " + *source + (depth > 12 ? " nested too deep" : " not found"));
                    continue;
                }
                fill(tile, *file(resolved), resolved, depth + 1);
                continue;
            }
            if (typeOf(node.mName))
            {
                std::shared_ptr<TileDef> child = buildTile(node, path, depth);
                if (child->mType == TileType::Template)
                    mTemplates[Xml::lower(child->mName)] = child;
                tile.mChildren.push_back(std::move(child));
                continue;
            }
            tile.mTraits[node.mName] = buildTrait(node);
        }
    }

    std::shared_ptr<TileDef> Loader::buildTile(const Xml::Node& node, const std::string& path, int depth)
    {
        auto tile = std::make_shared<TileDef>();
        tile->mType = *typeOf(node.mName);
        if (const std::string* name = node.attribute("name"))
            tile->mName = std::string(Xml::trim(*name));
        tile->mFile = path;
        // The children of the tile are built in one go, because an include may carry traits and tiles at once and a
        // trait written after the include has to win over the one in it.
        fill(*tile, node.mChildren, path, depth);
        return tile;
    }

    std::shared_ptr<const TileDef> Loader::loadMenu(const std::string& path)
    {
        const std::vector<Xml::Node>* nodes = file(path);
        if (nodes == nullptr)
            return nullptr;
        for (const Xml::Node& node : *nodes)
            if (typeOf(node.mName) && node.mName != "template")
                return buildTile(node, path, 0);
        return nullptr;
    }

    std::shared_ptr<const TileDef> Loader::loadPlain(const std::string& path)
    {
        const std::vector<Xml::Node>* nodes = file(path);
        if (nodes == nullptr)
            return nullptr;
        for (const Xml::Node& node : *nodes)
        {
            if (typeOf(node.mName))
            {
                std::shared_ptr<TileDef> tile = buildTile(node, path, 0);
                tile->mType = TileType::Plain;
                return tile;
            }
        }
        return nullptr;
    }
}
