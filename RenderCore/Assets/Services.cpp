// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Services.h"
#include "LocalCompiledShaderSource.h"
#include "MaterialCompiler.h"
#include "Material.h"   // just for MaterialScaffold::CompileProcessType
#include "ColladaCompilerInterface.h"
#include "../Metal/Shader.h"            // (for Metal::CreateLowLevelShaderCompiler)
#include "../ShaderService.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Assets/AssetServices.h"
#include "../../ConsoleRig/AttachableInternal.h"
#include "../../BufferUploads/IBufferUploads.h"

namespace RenderCore { namespace Assets
{
    Services* Services::s_instance = nullptr;

    Services::Services(RenderCore::IDevice* device)
    {
        _shaderService = std::make_unique<ShaderService>();

        auto shaderSource = std::make_shared<LocalCompiledShaderSource>(
            Metal::CreateLowLevelShaderCompiler());
        _shaderService->AddShaderSource(shaderSource);

        auto& asyncMan = ::Assets::Services::GetAsyncMan();
        asyncMan.GetIntermediateCompilers().AddCompiler(
            CompiledShaderByteCode::CompileProcessType, shaderSource);

        if (device) {
            BufferUploads::AttachLibrary(ConsoleRig::GlobalServices::GetInstance());
            _bufferUploads = BufferUploads::CreateManager(device);
        }

        // The technique config search directories are used to search for
        // technique configuration files. These are the files that point to
        // shaders used by rendering models. Each material can reference one
        // of these configuration files. But we can add some flexibility to the
        // engine by searching for these files in multiple directories. 
        _techConfDirs.AddSearchDirectory("game/xleres/techniques");

            // Setup required compilers.
            //  * material scaffold compiler
        auto& compilers = asyncMan.GetIntermediateCompilers();
        compilers.AddCompiler(
            RenderCore::Assets::MaterialScaffold::CompileProcessType,
            std::make_shared<RenderCore::Assets::MaterialScaffoldCompiler>());

        ConsoleRig::GlobalServices::GetCrossModule().Publish(*this);
    }

    Services::~Services()
    {
            // attempt to flush out all background operations current being performed
        auto& asyncMan = ::Assets::Services::GetAsyncMan();
        asyncMan.GetIntermediateCompilers().StallOnPendingOperations(true);

        if (_bufferUploads) {
            _bufferUploads.reset();
            BufferUploads::DetachLibrary();
        }

        ConsoleRig::GlobalServices::GetCrossModule().Withhold(*this);
    }

    void Services::InitColladaCompilers()
    {
            // attach the collada compilers to the assert services
            // this is optional -- not all applications will need these compilers
        auto& asyncMan = ::Assets::Services::GetAsyncMan();
        auto& compilers = asyncMan.GetIntermediateCompilers();

        typedef RenderCore::Assets::ColladaCompiler ColladaCompiler;
        auto colladaProcessor = std::make_shared<ColladaCompiler>();
        compilers.AddCompiler(ColladaCompiler::Type_Model, colladaProcessor);
        compilers.AddCompiler(ColladaCompiler::Type_AnimationSet, colladaProcessor);
        compilers.AddCompiler(ColladaCompiler::Type_Skeleton, colladaProcessor);
        compilers.AddCompiler(ColladaCompiler::Type_RawMat, colladaProcessor);
    }

    void Services::AttachCurrentModule()
    {
        assert(s_instance==nullptr);
        s_instance = this;
        ShaderService::SetInstance(_shaderService.get());
    }

    void Services::DetachCurrentModule()
    {
        assert(s_instance==this);
        ShaderService::SetInstance(nullptr);
        s_instance = nullptr;
    }

}}

