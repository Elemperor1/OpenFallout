#ifndef MWRENDER_TERRAINSTORAGE_H
#define MWRENDER_TERRAINSTORAGE_H

#include <map>
#include <memory>

#include <components/esmterrain/storage.hpp>

#include <components/resource/resourcesystem.hpp>

namespace OFRender
{

    class LandManager;

    /// @brief Connects the ESM Store used in OpenMW with the ESMTerrain storage.
    class TerrainStorage : public ESMTerrain::Storage
    {
    public:
        TerrainStorage(Resource::ResourceSystem* resourceSystem, std::string_view normalMapPattern = {},
            std::string_view normalHeightMapPattern = {}, bool autoUseNormalMaps = false,
            std::string_view specularMapPattern = {}, bool autoUseSpecularMaps = false);
        ~TerrainStorage();

        std::shared_ptr<const ESMTerrain::LandObject> getLand(ESM::ExteriorCellLocation cellLocation) override;
        const VFS::Path::Normalized* getLandTexture(std::uint16_t index, int plugin) override;

        const ESM4::LandTexture* getEsm4LandTexture(ESM::RefId ltexId) const override;
        const ESM4::TextureSet* getEsm4TextureSet(ESM::RefId txstId) const override;

        bool hasData(ESM::ExteriorCellLocation cellLocation) override;

        /// Get bounds of the whole terrain in cell units
        void getBounds(float& minX, float& maxX, float& minY, float& maxY, ESM::RefId worldspace) override;

        /// The west and south edge of the terrain of the worldspace is moved down to a multiple of `alignment` cells,
        /// where the nodes of the quad tree and the tiles of the level of detail files of the games start. Called
        /// before the quad tree of the worldspace is built, and only for worldspaces that draw those tiles.
        void setBoundsAlignment(ESM::RefId worldspace, int alignment);

        LandManager* getLandManager() const;

    private:
        std::unique_ptr<LandManager> mLandManager;
        std::map<ESM::RefId, int> mBoundsAlignment;

        Resource::ResourceSystem* mResourceSystem;
    };

}

#endif
