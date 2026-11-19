// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/document-usd/nau_usd_document_factory.hpp"
#include "nau/prim-factory/nau_usd_prim_creator.hpp"
#include "nau/prim-factory/nau_usd_prim_factory.hpp"

#include <map>


namespace
{
    // ** NauUsdDocumentTypeRegistry
    //
    // Stage 1 adapter: node types are prim creators in NauUsdPrimFactory

    class NauUsdDocumentTypeRegistry final : public NauDocumentTypeRegistryInterface
    {
    public:
        void registerType(const NauDocumentNodeTypeInfo& type) override
        {
            NauUsdPrimCreatorAbstractPtr creator;
            if (type.engineComponent) {
                creator = std::make_shared<NauUsdPrimComponentCreator>(type.typeName);
            } else if (type.defaultAssetReference) {
                const std::string& subAsset = type.defaultAssetReference->subAsset;
                creator = std::make_shared<NauResourceUsdPrimCreator>(type.defaultAssetReference->asset,
                    subAsset.empty() ? PXR_NS::SdfPath() : PXR_NS::SdfPath(subAsset));
            } else {
                creator = std::make_shared<NauDefaultUsdPrimCreator>();
            }

            NauUsdPrimFactory::instance().addCreator(type.typeName, creator, type.displayName);
            m_types[type.typeName] = type;
        }

        std::vector<NauDocumentNodeTypeInfo> types() const override
        {
            std::vector<NauDocumentNodeTypeInfo> result;

            const auto registered = NauUsdPrimFactory::instance().registeredPrimCreatorsWithDisplayNames([](const std::string&) {
                return true;
            });

            for (const auto& [typeName, displayName] : registered) {
                if (const auto it = m_types.find(typeName); it != m_types.end()) {
                    result.push_back(it->second);
                } else {
                    result.push_back({ .typeName = typeName, .displayName = displayName });
                }
            }

            return result;
        }

    private:
        std::map<std::string, NauDocumentNodeTypeInfo> m_types;
    };
}


// ** Nau

NauDocumentTypeRegistryInterface& Nau::DocumentTypeRegistry()
{
    static NauUsdDocumentTypeRegistry registry;
    return registry;
}
