#include "hh.h"
#include "../gfx/instance.h"
#include "HedgeLib/Materials/HHMaterial.h"
#include "HedgeLib/Geometry/HHModel.h"
#include "HedgeLib/Geometry/HHTerrainInstanceInfo.h"
#include "HedgeLib/IO/HedgehogEngine.h"
#include <string_view>

namespace HedgeEdit::Data::Parsers
{
    DEFINE_PARSER(ResMirageMaterial)
    {
        hl::DHHFixData(data);

        // TODO: Get version number and use the correct material struct based on that
        hl::HHMaterialV3* mat = hl::DHHGetData<hl::HHMaterialV3>(data);
        mat->EndianSwapRecursive(true);

        inst.AddMaterial(name, new GFX::Material(inst, *mat));
    }

    DEFINE_PARSER(ResModel)
    {
        hl::DHHFixData(data);

        // TODO: Get version number and use the correct model struct based on that
        hl::HHSkeletalModel* mdl = hl::DHHGetData<hl::HHSkeletalModel>(data);
        mdl->EndianSwapRecursive(true);

        inst.AddModel(name, *mdl);
    }

    DEFINE_PARSER(ResMirageTerrainInstanceInfo)
    {
        hl::DHHFixData(data);

        // TODO: Get version number and use the correct terrain instanceinfo struct based on that
        hl::HHTerrainInstanceInfoV0* instInfo = hl::DHHGetData<hl::HHTerrainInstanceInfoV0>(data);
        instInfo->EndianSwapRecursive(true);
        
        GFX::Model* mdl = inst.GetModel(std::string(instInfo->ModelName)); // TODO: nullptr check

        TerrainGroup& terrainGroup = inst.GetOrAddTerrainGroup(groupName);
        GameObject* terrainInst = terrainGroup.GetTerrainInstance(instInfo->ModelName); // TODO: nullptr check

        terrainInst->Instances.emplace_back(HHFixMatrix(*instInfo->Matrix.Get()));
    }

    DEFINE_PARSER(ResMirageTerrainModel)
    {
        hl::DHHFixData(data);

        // TODO: Get version number and use the correct model struct based on that
        hl::HHTerrainModel* mdl = hl::DHHGetData<hl::HHTerrainModel>(data);
        mdl->EndianSwapRecursive(true);

        GFX::Model& model = inst.AddModel(name, *mdl);

        TerrainGroup& terrainGroup = inst.GetOrAddTerrainGroup(groupName);
        terrainGroup.Terrain.emplace_back(name);
    }
}
