#include "onyx/core/pch.hpp"
#include "onyx/state/shaders.hpp"
#include "onyx/core/core.hpp"
#include "tkit/preprocessor/utils.hpp"
#include "tkit/utils/defer.hpp"
#include <slang.h>

namespace Onyx::Shaders
{
using namespace Detail;
static slang::IGlobalSession *s_Slang;

Compiler::Module &Compiler::Module::DeclareEntryPoint(const char *p_Name, const ShaderStage p_Stage)
{
    m_EntryPoints.Append(p_Name, m_Name, p_Stage);
    return *this;
}
Compiler &Compiler::Module::Load() const
{
    return *m_Compiler;
}
Compiler::Module &Compiler::AddModule(const char *p_Name)
{
    return m_Modules.Append(this, p_Name);
}
Compiler::Module &Compiler::AddModule(const char *p_Name, const char *p_SourceCode, const char *p_Path)
{
    return m_Modules.Append(this, p_Name, p_SourceCode, p_Path);
}
Compiler &Compiler::AddIntegerArgument(const ShaderArgumentName p_Name, const i32 p_Value0, const i32 p_Value1)
{
    ShaderArgument arg;
    arg.Name = p_Name;
    ShaderArgumentValue val{};
    val.Type = ShaderArgument_Integer;
    val.Value0 = p_Value0;
    val.Value1 = p_Value1;
    arg.Value = val;
    m_Arguments.Append(arg);
    return *this;
}
Compiler &Compiler::AddStringArgument(const ShaderArgumentName p_Name, const char *p_String0, const char *p_String1)
{
    ShaderArgument arg;
    arg.Name = p_Name;
    ShaderArgumentValue val{};
    val.Type = ShaderArgument_String;
    val.String0 = p_String0;
    val.String1 = p_String1;
    arg.Value = val;
    m_Arguments.Append(arg);
    return *this;
}
Compiler &Compiler::AddBooleanArgument(const ShaderArgumentName p_Name)
{
    ShaderArgument arg;
    arg.Name = p_Name;
    m_Arguments.Append(arg);
    return *this;
}
Compiler &Compiler::AddPreprocessorMacro(const char *p_Name, const char *p_Value)
{
    m_Macros.Append(p_Name, p_Value);
    return *this;
}
Compiler &Compiler::AddSearchPath(const char *p_Path)
{
    m_SearchPaths.Append(p_Path);
    return *this;
}
Compiler &Compiler::EnableEffectAnnotations()
{
    m_EnableEffectAnnotations = true;
    return *this;
}
Compiler &Compiler::AllowGlslSyntax()
{
    m_AllowGlslSyntax = true;
    return *this;
}
Compiler &Compiler::SkipSpirvValidation()
{
    m_SkipSpirvValidtion = true;
    return *this;
}

const Spirv &Compilation::GetSpirv(const char *p_EntryPoint, const char *p_Module) const
{
    u32 index = TKIT_U32_MAX;
    for (u32 i = 0; i < m_CompiledSpirv.GetSize(); ++i)
    {
        const Spirv &spr = m_CompiledSpirv[i];
        bool match = strcmp(p_EntryPoint, spr.EntryPoint.Name) == 0;
        match &= !p_Module || strcmp(p_Module, spr.EntryPoint.Module) == 0;
        if (match)
        {
            TKIT_ASSERT(index == TKIT_U32_MAX,
                        "[ONYX] Found multiple endpoints named '{}'. If you have endpoints with the same name, the "
                        "module name, the stage or both must be provided as well to resolve the ambiguity",
                        p_EntryPoint);
            index = i;
        }
    }
    TKIT_ASSERT(index != TKIT_U32_MAX, "[ONYX] Entry point named '{}' was not found", p_EntryPoint);
    return m_CompiledSpirv[index];
}
const Spirv &Compilation::GetSpirv(const char *p_EntryPoint, const ShaderStage p_Stage, const char *p_Module) const
{
    u32 index = TKIT_U32_MAX;
    for (u32 i = 0; i < m_CompiledSpirv.GetSize(); ++i)
    {
        const Spirv &spr = m_CompiledSpirv[index];
        bool match = strcmp(p_EntryPoint, spr.EntryPoint.Name) == 0;
        match &= !p_Module || strcmp(p_Module, spr.EntryPoint.Module) == 0;
        match &= p_Stage == spr.EntryPoint.Stage;
        if (match)
        {
            TKIT_ASSERT(index == TKIT_U32_MAX,
                        "[ONYX] Found multiple endpoints named '{}'. If you have endpoints with the same name, the "
                        "module name, the stage or both must be provided as well to resolve the ambiguity",
                        p_EntryPoint);
            index = i;
        }
    }
    TKIT_ASSERT(index != TKIT_U32_MAX, "[ONYX] Entry point named '{}' was not found", p_EntryPoint);
    return m_CompiledSpirv[index];
}
const Spirv &Compilation::GetSpirv(const char *p_EntryPoint, const char *p_Module, const ShaderStage p_Stage) const
{
    return GetSpirv(p_EntryPoint, p_Stage, p_Module);
}

VKit::Shader Compilation::CreateShader(const char *p_EntryPoint, const char *p_Module) const
{
    const Spirv &spr = GetSpirv(p_EntryPoint, p_Module);
    return Create(spr);
}
VKit::Shader Compilation::CreateShader(const char *p_EntryPoint, const ShaderStage p_Stage, const char *p_Module) const
{
    const Spirv &spr = GetSpirv(p_EntryPoint, p_Stage, p_Module);
    return Create(spr);
}
VKit::Shader Compilation::CreateShader(const char *p_EntryPoint, const char *p_Module, const ShaderStage p_Stage) const
{
    const Spirv &spr = GetSpirv(p_EntryPoint, p_Module, p_Stage);
    return Create(spr);
}

void Compilation::Destroy()
{
    for (const Spirv &spr : m_CompiledSpirv)
        TKit::Memory::Deallocate(spr.Data);
}

static SlangStage getSlangStage(const ShaderStage p_Stage)
{
    switch (p_Stage)
    {
    case ShaderStage_Vertex:
        return SLANG_STAGE_VERTEX;
    case ShaderStage_Fragment:
        return SLANG_STAGE_FRAGMENT;
    case ShaderStage_Compute:
        return SLANG_STAGE_COMPUTE;
    case ShaderStage_Geometry:
        return SLANG_STAGE_GEOMETRY;
    case ShaderStage_Unknown:
        return SLANG_STAGE_NONE;
    }
    return SLANG_STAGE_NONE;
}
static ShaderStage getMyStage(const SlangStage p_Stage)
{
    switch (p_Stage)
    {
    case SLANG_STAGE_VERTEX:
        return ShaderStage_Vertex;
    case SLANG_STAGE_FRAGMENT:
        return ShaderStage_Fragment;
    case SLANG_STAGE_COMPUTE:
        return ShaderStage_Compute;
    case SLANG_STAGE_GEOMETRY:
        return ShaderStage_Geometry;
    default:
        return ShaderStage_Unknown;
    }
}

// ahh yes
static slang::CompilerOptionName getArgumentName(const ShaderArgumentName p_Arg)
{
    using SO = slang::CompilerOptionName;

    switch (p_Arg)
    {
    case ShaderArgument_MacroDefine:
        return SO::MacroDefine;
    case ShaderArgument_DepFile:
        return SO::DepFile;
    case ShaderArgument_EntryPointName:
        return SO::EntryPointName;
    case ShaderArgument_Specialize:
        return SO::Specialize;
    case ShaderArgument_Help:
        return SO::Help;
    case ShaderArgument_HelpStyle:
        return SO::HelpStyle;
    case ShaderArgument_Include:
        return SO::Include;
    case ShaderArgument_Language:
        return SO::Language;
    case ShaderArgument_MatrixLayoutColumn:
        return SO::MatrixLayoutColumn;
    case ShaderArgument_MatrixLayoutRow:
        return SO::MatrixLayoutRow;
    case ShaderArgument_ZeroInitialize:
        return SO::ZeroInitialize;
    case ShaderArgument_IgnoreCapabilities:
        return SO::IgnoreCapabilities;
    case ShaderArgument_RestrictiveCapabilityCheck:
        return SO::RestrictiveCapabilityCheck;
    case ShaderArgument_ModuleName:
        return SO::ModuleName;
    case ShaderArgument_Output:
        return SO::Output;
    case ShaderArgument_Profile:
        return SO::Profile;
    case ShaderArgument_Stage:
        return SO::Stage;
    case ShaderArgument_Target:
        return SO::Target;
    case ShaderArgument_Version:
        return SO::Version;
    case ShaderArgument_WarningsAsErrors:
        return SO::WarningsAsErrors;
    case ShaderArgument_DisableWarnings:
        return SO::DisableWarnings;
    case ShaderArgument_EnableWarning:
        return SO::EnableWarning;
    case ShaderArgument_DisableWarning:
        return SO::DisableWarning;
    case ShaderArgument_DumpWarningDiagnostics:
        return SO::DumpWarningDiagnostics;
    case ShaderArgument_InputFilesRemain:
        return SO::InputFilesRemain;
    case ShaderArgument_EmitIr:
        return SO::EmitIr;
    case ShaderArgument_ReportDownstreamTime:
        return SO::ReportDownstreamTime;
    case ShaderArgument_ReportPerfBenchmark:
        return SO::ReportPerfBenchmark;
    case ShaderArgument_ReportCheckpointIntermediates:
        return SO::ReportCheckpointIntermediates;
    case ShaderArgument_SkipSPIRVValidation:
        return SO::SkipSPIRVValidation;
    case ShaderArgument_SourceEmbedStyle:
        return SO::SourceEmbedStyle;
    case ShaderArgument_SourceEmbedName:
        return SO::SourceEmbedName;
    case ShaderArgument_SourceEmbedLanguage:
        return SO::SourceEmbedLanguage;
    case ShaderArgument_DisableShortCircuit:
        return SO::DisableShortCircuit;
    case ShaderArgument_MinimumSlangOptimization:
        return SO::MinimumSlangOptimization;
    case ShaderArgument_DisableNonEssentialValidations:
        return SO::DisableNonEssentialValidations;
    case ShaderArgument_DisableSourceMap:
        return SO::DisableSourceMap;
    case ShaderArgument_UnscopedEnum:
        return SO::UnscopedEnum;
    case ShaderArgument_PreserveParameters:
        return SO::PreserveParameters;

    case ShaderArgument_Capability:
        return SO::Capability;
    case ShaderArgument_DefaultImageFormatUnknown:
        return SO::DefaultImageFormatUnknown;
    case ShaderArgument_DisableDynamicDispatch:
        return SO::DisableDynamicDispatch;
    case ShaderArgument_DisableSpecialization:
        return SO::DisableSpecialization;
    case ShaderArgument_FloatingPointMode:
        return SO::FloatingPointMode;
    case ShaderArgument_DebugInformation:
        return SO::DebugInformation;
    case ShaderArgument_LineDirectiveMode:
        return SO::LineDirectiveMode;
    case ShaderArgument_Optimization:
        return SO::Optimization;
    case ShaderArgument_Obfuscate:
        return SO::Obfuscate;

    case ShaderArgument_VulkanBindShift:
        return SO::VulkanBindShift;
    case ShaderArgument_VulkanBindGlobals:
        return SO::VulkanBindGlobals;
    case ShaderArgument_VulkanInvertY:
        return SO::VulkanInvertY;
    case ShaderArgument_VulkanUseDxPositionW:
        return SO::VulkanUseDxPositionW;
    case ShaderArgument_VulkanUseEntryPointName:
        return SO::VulkanUseEntryPointName;
    case ShaderArgument_VulkanUseGLLayout:
        return SO::VulkanUseGLLayout;
    case ShaderArgument_VulkanEmitReflection:
        return SO::VulkanEmitReflection;

    case ShaderArgument_GLSLForceScalarLayout:
        return SO::GLSLForceScalarLayout;
    case ShaderArgument_EnableEffectAnnotations:
        return SO::EnableEffectAnnotations;

    case ShaderArgument_EmitSpirvViaGLSL:
        return SO::EmitSpirvViaGLSL;
    case ShaderArgument_EmitSpirvDirectly:
        return SO::EmitSpirvDirectly;
    case ShaderArgument_SPIRVCoreGrammarJSON:
        return SO::SPIRVCoreGrammarJSON;
    case ShaderArgument_IncompleteLibrary:
        return SO::IncompleteLibrary;

    case ShaderArgument_CompilerPath:
        return SO::CompilerPath;
    case ShaderArgument_DefaultDownstreamCompiler:
        return SO::DefaultDownstreamCompiler;
    case ShaderArgument_DownstreamArgs:
        return SO::DownstreamArgs;
    case ShaderArgument_PassThrough:
        return SO::PassThrough;

    case ShaderArgument_DumpRepro:
        return SO::DumpRepro;
    case ShaderArgument_DumpReproOnError:
        return SO::DumpReproOnError;
    case ShaderArgument_ExtractRepro:
        return SO::ExtractRepro;
    case ShaderArgument_LoadRepro:
        return SO::LoadRepro;
    case ShaderArgument_LoadReproDirectory:
        return SO::LoadReproDirectory;
    case ShaderArgument_ReproFallbackDirectory:
        return SO::ReproFallbackDirectory;

    case ShaderArgument_DumpAst:
        return SO::DumpAst;
    case ShaderArgument_DumpIntermediatePrefix:
        return SO::DumpIntermediatePrefix;
    case ShaderArgument_DumpIntermediates:
        return SO::DumpIntermediates;
    case ShaderArgument_DumpIr:
        return SO::DumpIr;
    case ShaderArgument_DumpIrIds:
        return SO::DumpIrIds;
    case ShaderArgument_PreprocessorOutput:
        return SO::PreprocessorOutput;
    case ShaderArgument_OutputIncludes:
        return SO::OutputIncludes;
    case ShaderArgument_ReproFileSystem:
        return SO::ReproFileSystem;
    case ShaderArgument_SkipCodeGen:
        return SO::SkipCodeGen;
    case ShaderArgument_ValidateIr:
        return SO::ValidateIr;
    case ShaderArgument_VerbosePaths:
        return SO::VerbosePaths;
    case ShaderArgument_VerifyDebugSerialIr:
        return SO::VerifyDebugSerialIr;
    case ShaderArgument_NoCodeGen:
        return SO::NoCodeGen;

    case ShaderArgument_FileSystem:
        return SO::FileSystem;
    case ShaderArgument_Heterogeneous:
        return SO::Heterogeneous;
    case ShaderArgument_NoMangle:
        return SO::NoMangle;
    case ShaderArgument_NoHLSLBinding:
        return SO::NoHLSLBinding;
    case ShaderArgument_NoHLSLPackConstantBufferElements:
        return SO::NoHLSLPackConstantBufferElements;
    case ShaderArgument_ValidateUniformity:
        return SO::ValidateUniformity;
    case ShaderArgument_AllowGLSL:
        return SO::AllowGLSL;
    case ShaderArgument_EnableExperimentalPasses:
        return SO::EnableExperimentalPasses;
    case ShaderArgument_BindlessSpaceIndex:
        return SO::BindlessSpaceIndex;

    case ShaderArgument_ArchiveType:
        return SO::ArchiveType;
    case ShaderArgument_CompileCoreModule:
        return SO::CompileCoreModule;
    case ShaderArgument_Doc:
        return SO::Doc;

    case ShaderArgument_IrCompression:
        return SO::IrCompression;

    case ShaderArgument_LoadCoreModule:
        return SO::LoadCoreModule;
    case ShaderArgument_ReferenceModule:
        return SO::ReferenceModule;
    case ShaderArgument_SaveCoreModule:
        return SO::SaveCoreModule;
    case ShaderArgument_SaveCoreModuleBinSource:
        return SO::SaveCoreModuleBinSource;
    case ShaderArgument_TrackLiveness:
        return SO::TrackLiveness;
    case ShaderArgument_LoopInversion:
        return SO::LoopInversion;

    case ShaderArgument_ParameterBlocksUseRegisterSpaces:
        return SO::ParameterBlocksUseRegisterSpaces;
    case ShaderArgument_LanguageVersion:
        return SO::LanguageVersion;
    case ShaderArgument_TypeConformance:
        return SO::TypeConformance;
    case ShaderArgument_EnableExperimentalDynamicDispatch:
        return SO::EnableExperimentalDynamicDispatch;
    case ShaderArgument_EmitReflectionJSON:
        return SO::EmitReflectionJSON;

    case ShaderArgument_DebugInformationFormat:
        return SO::DebugInformationFormat;
    case ShaderArgument_VulkanBindShiftAll:
        return SO::VulkanBindShiftAll;
    case ShaderArgument_GenerateWholeProgram:
        return SO::GenerateWholeProgram;
    case ShaderArgument_UseUpToDateBinaryModule:
        return SO::UseUpToDateBinaryModule;
    case ShaderArgument_EmbedDownstreamIR:
        return SO::EmbedDownstreamIR;
    case ShaderArgument_ForceDXLayout:
        return SO::ForceDXLayout;

    case ShaderArgument_EmitSpirvMethod:
        return SO::EmitSpirvMethod;
    case ShaderArgument_SaveGLSLModuleBinSource:
        return SO::SaveGLSLModuleBinSource;
    case ShaderArgument_SkipDownstreamLinking:
        return SO::SkipDownstreamLinking;
    case ShaderArgument_DumpModule:
        return SO::DumpModule;
    case ShaderArgument_GetModuleInfo:
        return SO::GetModuleInfo;
    case ShaderArgument_GetSupportedModuleVersions:
        return SO::GetSupportedModuleVersions;
    case ShaderArgument_EmitSeparateDebug:
        return SO::EmitSeparateDebug;

    case ShaderArgument_DenormalModeFp16:
        return SO::DenormalModeFp16;
    case ShaderArgument_DenormalModeFp32:
        return SO::DenormalModeFp32;
    case ShaderArgument_DenormalModeFp64:
        return SO::DenormalModeFp64;

    case ShaderArgument_UseMSVCStyleBitfieldPacking:
        return SO::UseMSVCStyleBitfieldPacking;
    case ShaderArgument_ForceCLayout:
        return SO::ForceCLayout;
    case ShaderArgument_ExperimentalFeature:
        return SO::ExperimentalFeature;
    case ShaderArgument_REMOVED_SerialIR:
        return SO::REMOVED_SerialIR;
    case ShaderArgument_CountOfParsableOptions:
        return SO::CountOfParsableOptions;
    }

    return SO::CountOf;
}

#if defined(TKIT_ENABLE_ASSERTS) || defined(TKIT_ENABLE_INFO_LOGS)
static void reportDiagnostics(const SlangResult p_Result, slang::IBlob *p_Diagnostics)
{
    TKIT_ASSERT(SLANG_SUCCEEDED(p_Result) || p_Diagnostics, "[ONYX] Module compilation failed with no diagnostics");
    if (!p_Diagnostics)
        return;

    const char *text = static_cast<const char *>(p_Diagnostics->getBufferPointer());
    const size_t size = p_Diagnostics->getBufferSize();
    const std::string message{text, size};

    TKIT_ASSERT(SLANG_SUCCEEDED(p_Result), "[ONYX] Module compilation failed with the following diagnostics: {}",
                message);
    TKIT_LOG_INFO_IF(SLANG_SUCCEEDED(p_Result),
                     "[ONYX] Module compilation succeeded with the following diagnostics: {}", message);
}
#    define REPORT_DIAGNOSTICS(p_Result, p_Diagnostics) reportDiagnostics(p_Result, p_Diagnostics)
#else
#    define REPORT_DIAGNOSTICS(p_Result, p_Diagnostics)
#endif
Compilation Compiler::Compile() const
{
    slang::ISession *session;
    slang::TargetDesc tdesc{};
    tdesc.format = SLANG_SPIRV;
    tdesc.profile = s_Slang->findProfile("spirv_1_5");

    slang::SessionDesc cdesc{};
    cdesc.targets = &tdesc;
    cdesc.targetCount = 1;
    cdesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

    TKit::StaticArray16<slang::PreprocessorMacroDesc> defines;
    if (!m_Macros.IsEmpty())
    {
        for (const Macro &def : m_Macros)
            defines.Append(def.Name, def.Value);

        cdesc.preprocessorMacroCount = m_Macros.GetSize();
        cdesc.preprocessorMacros = defines.GetData();
    }

    TKit::StaticArray16<slang::CompilerOptionEntry> coptions;

    slang::CompilerOptionEntry entry;
    entry.name = slang::CompilerOptionName::MatrixLayoutColumn;
    coptions.Append(entry);

    for (const ShaderArgument &sa : m_Arguments)
    {
        slang::CompilerOptionEntry entry;
        entry.name = getArgumentName(sa.Name);
        entry.value.intValue0 = sa.Value.Value0;
        entry.value.intValue1 = sa.Value.Value1;
        entry.value.stringValue0 = sa.Value.String0;
        entry.value.stringValue1 = sa.Value.String1;

        coptions.Append(entry);
    }

    cdesc.compilerOptionEntries = coptions.GetData();
    cdesc.compilerOptionEntryCount = coptions.GetSize();

    if (!m_SearchPaths.IsEmpty())
    {
        cdesc.searchPaths = m_SearchPaths.GetData();
        cdesc.searchPathCount = m_SearchPaths.GetSize();
    }

    cdesc.enableEffectAnnotations = m_EnableEffectAnnotations;
    cdesc.allowGLSLSyntax = m_AllowGlslSyntax;
    cdesc.skipSPIRVValidation = m_SkipSpirvValidtion;

    SlangResult result = s_Slang->createSession(cdesc, &session);
    TKIT_ASSERT(SLANG_SUCCEEDED(result), "[ONYX] Slang compile session creation failed");

    slang::IBlob *diagnostics = nullptr;
    TKit::StaticArray32<Spirv> sprvs{};
    const auto release = [](auto &thing) {
        if (thing)
        {
            thing->Release();
            thing = nullptr;
        };
    };

    for (const Module &munit : m_Modules)
    {
        TKit::StaticArray16<slang::IComponentType *> components{};
        slang::IModule *module = nullptr;
        if (munit.m_SourceCode)
            module = session->loadModuleFromSourceString(munit.m_Name, munit.m_Path, munit.m_SourceCode, &diagnostics);
        else
            module = session->loadModule(munit.m_Name, &diagnostics);

        REPORT_DIAGNOSTICS(module ? 0 : -1, diagnostics);
        TKIT_LOG_INFO("[ONYX] Successfully loaded module '{}'", module->getName());
        release(diagnostics);

        components.Append(module);
        for (const EntryPoint &ep : munit.m_EntryPoints)
        {
            slang::IEntryPoint *entry = nullptr;
            result = module->findAndCheckEntryPoint(ep.Name, getSlangStage(ep.Stage), &entry, &diagnostics);
            REPORT_DIAGNOSTICS(result, diagnostics);
            release(diagnostics);
            components.Append(entry);
        }
        slang::IComponentType *program;
        result =
            session->createCompositeComponentType(components.GetData(), components.GetSize(), &program, &diagnostics);
        REPORT_DIAGNOSTICS(result, diagnostics);
        release(diagnostics);

        slang::IComponentType *linkedProgram;
        result = program->link(&linkedProgram, &diagnostics);
        REPORT_DIAGNOSTICS(result, diagnostics);
        release(diagnostics);

        slang::ProgramLayout *layout = linkedProgram->getLayout();
        for (u32 i = 0; i < layout->getEntryPointCount(); ++i)
        {
            slang::IBlob *code;
            result = linkedProgram->getEntryPointCode(i, 0, &code, &diagnostics);
            REPORT_DIAGNOSTICS(result, diagnostics);
            release(diagnostics);

            const size_t size = code->getBufferSize();
            void *mem = TKit::Memory::Allocate(size);
            TKit::Memory::ForwardCopy(mem, code->getBufferPointer(), size);

            EntryPoint ep;
            slang::EntryPointReflection *epr = layout->getEntryPointByIndex(i);

            ep.Module = munit.m_Name;
            ep.Stage = getMyStage(epr->getStage());
            for (const EntryPoint &mep : munit.m_EntryPoints)
                if (strcmp(mep.Name, epr->getName()) == 0)
                {
                    ep.Name = mep.Name;
                    break;
                }
            TKIT_ASSERT(ep.Name, "[ONYX] Failed to recover entry point named '{}'", epr->getName());

            Spirv sp;
            sp.EntryPoint = ep;
            sp.Data = static_cast<u32 *>(mem);
            sp.Size = size;

            sprvs.Append(sp);
        }

        release(linkedProgram);
        release(program);
        for (slang::IComponentType *cmp : components)
            release(cmp);
    }

    session->Release();
    return Compilation{sprvs};
}

void Initialize(const Specs &p_Specs)
{
    SlangGlobalSessionDesc desc{};
    desc.enableGLSL = p_Specs.EnableGlsl;

    SlangResult result = slang::createGlobalSession(&desc, &s_Slang);
    TKIT_ASSERT(SLANG_SUCCEEDED(result), "[ONYX] Slang global session creation failed");

    TKIT_UNUSED(result);
}
void Terminate()
{
    s_Slang->Release();
    slang::shutdown();
}

VKit::Shader Create(const u32 *p_Spirv, const size_t p_Size)
{
    const auto result = VKit::Shader::Create(Core::GetDevice(), p_Spirv, p_Size);
    VKIT_CHECK_RESULT(result);
    return result.GetValue();
}
VKit::Shader Create(const Spirv &p_Spirv)
{
    return Create(p_Spirv.Data, p_Spirv.Size);
}
VKit::Shader Create(const std::string_view p_SpirvPath)
{
    const auto result = VKit::Shader::Create(Core::GetDevice(), p_SpirvPath);
    VKIT_CHECK_RESULT(result);
    return result.GetValue();
}
} // namespace Onyx::Shaders
