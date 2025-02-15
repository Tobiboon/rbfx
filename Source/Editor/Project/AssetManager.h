//
// Copyright (c) 2017-2020 the rbfx project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#include <Urho3D/Core/Signal.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/FileWatcher.h>
#include <Urho3D/Scene/Serializable.h>
#include <Urho3D/Utility/AssetTransformerHierarchy.h>

#include <EASTL/map.h>
#include <EASTL/optional.h>
#include <EASTL/unordered_set.h>

namespace Urho3D
{

class JSONFile;
class Project;

/// Manages assets of the project.
class AssetManager : public Object
{
    URHO3D_OBJECT(AssetManager, Object);

public:
    static const ea::string ResourceNameSuffix;
    Signal<void()> OnInitialized;

    explicit AssetManager(Context* context);
    ~AssetManager() override;

    void Update();

    /// Serialize
    /// @{
    void SerializeInBlock(Archive& archive) override;
    void LoadFile(const ea::string& fileName);
    void SaveFile(const ea::string& fileName) const;
    /// @}

private:
    /// Sorted list of asset pipeline files.
    using AssetPipelineList = ea::map<ea::string, FileTime>;

    struct AssetPipelineDesc
    {
        ea::string resourceName_;
        FileTime modificationTime_{};
        ea::vector<SharedPtr<AssetTransformer>> transformers_;
        ea::vector<ea::pair<ea::string, ea::string>> dependencies_;
    };
    using AssetPipelineDescVector = ea::vector<AssetPipelineDesc>;

    struct AssetPipelineDiff
    {
        const AssetPipelineDesc* oldPipeline_{};
        const AssetPipelineDesc* newPipeline_{};
    };
    using AssetPipelineDiffMap = ea::unordered_map<ea::string, AssetPipelineDiff>;

    struct AssetDesc
    {
        ea::string resourceName_;
        ea::vector<ea::string> outputs_;
        ea::unordered_set<ea::string> transformers_;
        FileTime modificationTime_{};

        bool cacheInvalid_{};

        void SerializeInBlock(Archive& archive);
        bool IsAnyTransformerUsed(const StringVector& transformers) const;
        ea::string GetTransformerDebugString() const;
    };

    struct Stats
    {
        unsigned numProcessedAssets_{};
        unsigned numIgnoredAssets_{};
        unsigned numUpToDateAssets_{};
    };

    /// Utility functions that don't change internal state
    /// @{
    StringVector EnumerateAssetFiles(const ea::string& resourcePath) const;
    AssetPipelineList EnumerateAssetPipelineFiles() const;
    AssetPipelineDesc LoadAssetPipeline(const JSONFile& jsonFile,
        const ea::string& resourceName, FileTime modificationTime) const;
    AssetPipelineDescVector LoadAssetPipelines(const AssetPipelineList& assetPipelineFiles) const;
    AssetPipelineDiffMap DiffAssetPipelines(const AssetPipelineDescVector& oldPipelines,
        const AssetPipelineDescVector& newPipelines) const;
    StringVector GetTransformerTypes(const AssetPipelineDesc& pipeline) const;
    AssetTransformerVector GetTransformers(const AssetPipelineDesc& pipeline) const;
    ea::string GetFileName(const ea::string& resourceName) const;
    bool IsAssetUpToDate(const AssetDesc& assetDesc) const;
    /// @}

    /// Cache manipulation.
    /// @{
    void InvalidateAssetsInPath(const ea::string& resourcePath);
    void InvalidateTransformedAssetsInPath(const ea::string& resourcePath, const StringVector& transformers);
    void InvalidateApplicableAssetsInPath(const ea::string& resourcePath, const AssetTransformerVector& transformers);
    void InvalidateOutdatedAssetsInPath(const ea::string& resourcePath);
    void CleanupInvalidatedAssets();
    void CleanupCacheFolder();
    /// @}

    StringVector GetUpdatedPaths(bool updateAll);

    void Initialize();

    void InitializeAssetPipelines();
    void UpdateAssetPipelines();
    void UpdateTransformHierarchy();
    void ProcessFileSystemUpdates();
    void EnsureAssetsAndCacheValid();
    void ScanAndQueueAssetProcessing();

    void ScanAssetsInPath(const ea::string& resourcePath, Stats& stats);
    bool QueueAssetProcessing(const ea::string& resourceName, const ApplicationFlavor& flavor);
    void ProcessAsset(const AssetTransformerInput& input);

    const WeakPtr<Project> project_;
    SharedPtr<FileWatcher> dataWatcher_;

    ApplicationFlavor defaultFlavor_; // TODO(editor): Make configurable

    bool initialized_{};
    bool reloadAssetPipelines_{};
    bool hasInvalidAssets_{};
    bool scanAssets_{};

    AssetPipelineDescVector assetPipelines_;
    SharedPtr<AssetTransformerHierarchy> transformerHierarchy_;
    ea::unordered_map<ea::string, AssetDesc> assets_;
    AssetPipelineList assetPipelineFiles_;

    ea::vector<AssetTransformerInput> requestQueue_;
};

}
