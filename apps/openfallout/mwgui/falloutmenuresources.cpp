#include "falloutmenuresources.hpp"

#include <optional>
#include <sstream>
#include <string>

#include <components/files/istreamptr.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/pathutil.hpp>

#include "../mwbase/environment.hpp"
#include "../mwscript/falloutscripts.hpp"

namespace OFGui
{
    std::shared_ptr<FalloutMenu::Resources> makeFalloutMenuResources()
    {
        const VFS::Manager* vfs = OFBase::Environment::get().getResourceSystem()->getVFS();
        FalloutMenu::Data data;
        data.mRead = [vfs](const std::string& path) -> std::optional<std::string> {
            Files::IStreamPtr stream = vfs->find(VFS::Path::Normalized(path));
            if (!stream)
                return std::nullopt;
            std::ostringstream bytes;
            if (stream->peek() != std::istream::traits_type::eof())
                bytes << stream->rdbuf();
            return bytes.str();
        };
        data.mReadStart = [vfs](const std::string& path, std::size_t count) -> std::optional<std::string> {
            Files::IStreamPtr stream = vfs->find(VFS::Path::Normalized(path));
            if (!stream)
                return std::nullopt;
            std::string bytes(count, '\0');
            stream->read(bytes.data(), static_cast<std::streamsize>(count));
            bytes.resize(static_cast<std::size_t>(stream->gcount()));
            return bytes;
        };
        // (menus can open while the game is still loading, before its scripts are registered)
        data.mSetting = [](std::string_view name) -> std::optional<std::string> {
            const OFBase::Environment& environment = OFBase::Environment::get();
            if (!environment.hasFalloutScripts())
                return std::nullopt;
            OFScript::FalloutScripts* host = environment.getFalloutScripts();
            std::string text = host->gameSettingText(name);
            if (text.empty())
                return std::nullopt;
            return text;
        };
        // New Vegas draws its menus in amber, as its executable makes the system colours (Fallout 3's are green)
        data.mNewVegas = []() -> std::optional<bool> {
            const OFBase::Environment& environment = OFBase::Environment::get();
            if (!environment.hasFalloutScripts())
                return std::nullopt;
            return environment.getFalloutScripts()->isNewVegas();
        };
        return std::make_shared<FalloutMenu::Resources>(std::move(data));
    }
}
