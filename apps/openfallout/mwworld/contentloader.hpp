#ifndef CONTENTLOADER_HPP
#define CONTENTLOADER_HPP

#include <filesystem>

namespace Loading
{
    class Listener;
}

namespace OFWorld
{

    struct ContentLoader
    {
        virtual ~ContentLoader() = default;

        virtual void load(const std::filesystem::path& filepath, int& index, Loading::Listener* listener) = 0;
    };

} /* namespace OFWorld */

#endif /* CONTENTLOADER_HPP */
