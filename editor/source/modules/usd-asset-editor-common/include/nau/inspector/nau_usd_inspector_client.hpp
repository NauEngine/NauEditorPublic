// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Inspector client USD implementation

#pragma once

#include "nau/nau_usd_asset_editor_common_config.hpp"

#include "nau/inspector/nau_inspector.hpp"
#include "nau_usd_inspector_widgets.hpp"
#include "nau/ui-translator/nau_usd_scene_ui_translator.hpp"

#include "usd_proxy/usd_proxy.h"

#include "pxr/usd/usd/prim.h"

#include <QObject>
#include <QTimer>


// ** NauUsdInspectorClient

class NAU_USD_ASSET_EDITOR_COMMON_API NauUsdInspectorClient : public QObject
                                                            , public NauUITranslatorClientInterface
{
    Q_OBJECT

    using NauPropertyMap = std::unordered_map<std::string, NauUsdPropertyAbstract*>;

public:
    NauUsdInspectorClient(NauInspectorPage* inspector);

    void handleNotice(NauUITranslatorProxyNotice const& notice) override;

    void buildFromPrim(PXR_NS::UsdPrim prim, bool hideAddButton = false);

    // TODO: Temp function for material editor
    void buildFromMaterial(PXR_NS::UsdPrim prim);

    // TODO: Needed for update system
    void updateFromPrim(PXR_NS::UsdPrim prim);

    void clear();
signals:
    void eventPropertyChanged(const PXR_NS::SdfPath& path, const PXR_NS::TfToken propName, const PXR_NS::VtValue& value);
    void eventAssetReferenceChanged(const PXR_NS::SdfPath& path, const PXR_NS::VtValue& value);
    void eventComponentAdded(const PXR_NS::SdfPath& parentPath, const PXR_NS::TfToken typeName);
    void eventComponentRemoved(const PXR_NS::SdfPath& path);

private:

    void buildFromPrimInternal(PXR_NS::UsdPrim prim);

    NauUsdPropertyAbstract* createPropertyWidget(const PXR_NS::SdfPath& primPath, const PXR_NS::VtValue& value, const std::string& rawTypeName, const std::string& metaInfo, PXR_NS::TfToken propName);
    NauUsdPropertyAbstract* createReferenceWidget(const PXR_NS::SdfPath& primPath, const PXR_NS::VtValue& value, const std::string& rawTypeName, const std::string& metaInfo);

    void buildTransformProperty(PXR_NS::UsdPrim prim, PXR_NS::VtArray<PXR_NS::TfToken>& transformTokens);
    void buildProperties(const UsdProxy::UsdProxyPrim& proxyPrim, const std::string& rawTypeName, const PXR_NS::VtArray<PXR_NS::TfToken>& transformTokensToSkip);
    void stopComponentBuildTimers();

    // TODO: Needed for update system
    void tick();

private:
    PXR_NS::SdfPath m_currentPrimPath;
    PXR_NS::UsdStageWeakPtr m_currentScene;
    std::vector<QTimer*> m_componentBuildTimers;

    // TODO: Needed for update system
    NauPropertyMap m_propertyMap;
    bool m_needUpdate;
    bool m_needRebuild;
    QTimer m_updateTimer;

    NauInspectorPage* m_inspector;

    bool m_hideAddButton;
};
