#ifndef OPENFALLOUT_COMPONENTS_FALLOUTMENU_MENUFILE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTMENU_MENUFILE_H

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "value.hpp"
#include "xml.hpp"

namespace FalloutMenu
{
    enum class TileType
    {
        Menu,
        Rect,
        Image,
        Text,
        HotRect,
        Window,
        Radial,
        Nif,
        Template,
        /// The tile that stands for the screen, the globals and similar: it only holds traits
        Plain,
    };

    /// The tile type a tag of a menu file stands for, if it stands for one
    std::optional<TileType> tileTypeOf(std::string_view tag);

    /// The things a trait can be calculated from, `src` of an operand
    enum class Source
    {
        Me,
        Parent,
        GrandParent,
        Child,
        Sibling,
        Screen,
        Globals,
        Io,
        /// A tile that is found by its name wherever it is in the menu
        Named,
    };

    enum class Op
    {
        Copy,
        Add,
        Sub,
        Mul,
        Div,
        Mod,
        Min,
        Max,
        Abs,
        Floor,
        Ceil,
        Round,
        Trunc,
        Eq,
        Neq,
        Gt,
        Gte,
        Lt,
        Lte,
        And,
        Or,
        Not,
        OnlyIf,
        OnlyIfNot,
        Ref,
    };

    struct Expression;

    /// The right hand side of a step: a number or a text, the trait of another tile, or a whole expression
    struct Operand
    {
        enum class Kind
        {
            None,
            Literal,
            Trait,
            Nested,
        };

        Kind mKind = Kind::None;
        Value mLiteral;
        Source mSource = Source::Me;
        std::string mSourceName;
        std::string mTrait;
        std::shared_ptr<const Expression> mNested;
    };

    struct Step
    {
        Op mOp = Op::Copy;
        Operand mOperand;
    };

    /// A trait that is calculated: the steps work on one running value from left to right, with no precedence
    struct Expression
    {
        std::vector<Step> mSteps;
    };

    struct TraitDef
    {
        std::shared_ptr<const Expression> mExpression;
        Value mLiteral;
    };

    struct TileDef
    {
        TileType mType = TileType::Rect;
        std::string mName;
        /// The traits the file gives, by name in lower case; the one written last wins
        std::map<std::string, TraitDef> mTraits;
        std::vector<std::shared_ptr<const TileDef>> mChildren;
        /// The file the tile was written in, for what the loader has to say
        std::string mFile;
    };

    /// What the loader asks of the world
    struct LoaderEnvironment
    {
        /// The text of a file of the game's data, from its path with `/` and in lower case (menus/globals.xml)
        std::function<std::optional<std::string>(const std::string& path)> mReadFile;
        /// The text of a game setting for `&-sName;`
        std::function<std::optional<std::string>(std::string_view name)> mSetting;
        /// More names for `&name;`, beside the ones every menu has; the name is in lower case
        std::function<std::optional<float>(std::string_view name)> mEntity;
    };

    /// The value of `&name;` for the names the games have in their menu loader (`&true;`, the codes of the menus, the
    /// buttons of the pad, ...), with the name in lower case and without the signs; nothing if the name is not one
    std::optional<float> builtinEntity(std::string_view name);

    class Loader
    {
    public:
        explicit Loader(LoaderEnvironment environment);

        /// A menu, its includes taken in. Nothing if the file is not there or has no tile.
        std::shared_ptr<const TileDef> loadMenu(const std::string& path);

        /// The tile of a file that holds only traits and tiles that are all part of one tile (`globals.xml`)
        std::shared_ptr<const TileDef> loadPlain(const std::string& path);

        /// What went wrong on the way, one line each (a file that an include named was not there, ...)
        const std::vector<std::string>& problems() const { return mProblems; }

        /// The templates that were seen, by name in lower case
        const std::unordered_map<std::string, std::shared_ptr<const TileDef>>& templates() const { return mTemplates; }

    private:
        const std::vector<Xml::Node>* file(const std::string& path);
        std::string resolveInclude(const std::string& from, const std::string& source);
        std::shared_ptr<TileDef> buildTile(const Xml::Node& node, const std::string& path);
        void fill(TileDef& tile, const std::vector<Xml::Node>& nodes, const std::string& path, int depth);
        TraitDef buildTrait(const Xml::Node& node);
        Operand buildOperand(const Xml::Node& node);
        std::shared_ptr<const Expression> buildExpression(const Xml::Node& node);
        Value literal(std::string_view text);
        std::string substitute(std::string_view text);

        LoaderEnvironment mEnvironment;
        std::unordered_map<std::string, std::vector<Xml::Node>> mFiles;
        std::unordered_map<std::string, bool> mMissing;
        std::unordered_map<std::string, std::shared_ptr<const TileDef>> mTemplates;
        std::vector<std::string> mProblems;
    };
}

#endif
