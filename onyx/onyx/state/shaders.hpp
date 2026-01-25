#pragma once

#include "onyx/core/core.hpp"
#include "vkit/state/shader.hpp"
#include "tkit/utils/non_copyable.hpp"

namespace Onyx
{
enum ShaderStage : u8
{
    ShaderStage_Vertex,
    ShaderStage_Fragment,
    ShaderStage_Compute,
    ShaderStage_Geometry,
    ShaderStage_Unknown,
};
enum ShaderArgumentName : u16
{
    ShaderArgument_MacroDefine, // stringValue0: macro name;  stringValue1: macro value
    ShaderArgument_DepFile,
    ShaderArgument_EntryPointName,
    ShaderArgument_Specialize,
    ShaderArgument_Help,
    ShaderArgument_HelpStyle,
    ShaderArgument_Include, // stringValue: additional include path.
    ShaderArgument_Language,
    ShaderArgument_MatrixLayoutColumn,         // bool
    ShaderArgument_MatrixLayoutRow,            // bool
    ShaderArgument_ZeroInitialize,             // bool
    ShaderArgument_IgnoreCapabilities,         // bool
    ShaderArgument_RestrictiveCapabilityCheck, // bool
    ShaderArgument_ModuleName,                 // stringValue0: module name.
    ShaderArgument_Output,
    ShaderArgument_Profile, // intValue0: profile
    ShaderArgument_Stage,   // intValue0: stage
    ShaderArgument_Target,  // intValue0: CodeGenTarget
    ShaderArgument_Version,
    ShaderArgument_WarningsAsErrors, // stringValue0: "all" or comma separated list of warning codes or names.
    ShaderArgument_DisableWarnings,  // stringValue0: comma separated list of warning codes or names.
    ShaderArgument_EnableWarning,    // stringValue0: warning code or name.
    ShaderArgument_DisableWarning,   // stringValue0: warning code or name.
    ShaderArgument_DumpWarningDiagnostics,
    ShaderArgument_InputFilesRemain,
    ShaderArgument_EmitIr,                        // bool
    ShaderArgument_ReportDownstreamTime,          // bool
    ShaderArgument_ReportPerfBenchmark,           // bool
    ShaderArgument_ReportCheckpointIntermediates, // bool
    ShaderArgument_SkipSPIRVValidation,           // bool
    ShaderArgument_SourceEmbedStyle,
    ShaderArgument_SourceEmbedName,
    ShaderArgument_SourceEmbedLanguage,
    ShaderArgument_DisableShortCircuit,            // bool
    ShaderArgument_MinimumSlangOptimization,       // bool
    ShaderArgument_DisableNonEssentialValidations, // bool
    ShaderArgument_DisableSourceMap,               // bool
    ShaderArgument_UnscopedEnum,                   // bool
    ShaderArgument_PreserveParameters,             // bool: preserve all resource parameters in the output code.
                                                   // Target

    ShaderArgument_Capability,                // intValue0: CapabilityName
    ShaderArgument_DefaultImageFormatUnknown, // bool
    ShaderArgument_DisableDynamicDispatch,    // bool
    ShaderArgument_DisableSpecialization,     // bool
    ShaderArgument_FloatingPointMode,         // intValue0: FloatingPointMode
    ShaderArgument_DebugInformation,          // intValue0: DebugInfoLevel
    ShaderArgument_LineDirectiveMode,
    ShaderArgument_Optimization, // intValue0: OptimizationLevel
    ShaderArgument_Obfuscate,    // bool

    ShaderArgument_VulkanBindShift,         // intValue0 (higher 8 bits): kind; intValue0(lower bits): set; intValue1:
                                            // shift
    ShaderArgument_VulkanBindGlobals,       // intValue0: index; intValue1: set
    ShaderArgument_VulkanInvertY,           // bool
    ShaderArgument_VulkanUseDxPositionW,    // bool
    ShaderArgument_VulkanUseEntryPointName, // bool
    ShaderArgument_VulkanUseGLLayout,       // bool
    ShaderArgument_VulkanEmitReflection,    // bool

    ShaderArgument_GLSLForceScalarLayout,   // bool
    ShaderArgument_EnableEffectAnnotations, // bool

    ShaderArgument_EmitSpirvViaGLSL,     // bool (will be deprecated)
    ShaderArgument_EmitSpirvDirectly,    // bool (will be deprecated)
    ShaderArgument_SPIRVCoreGrammarJSON, // stringValue0: json path
    ShaderArgument_IncompleteLibrary,    // bool, when set, will not issue an error when the linked program has
                                         // unresolved extern function symbols.

    // Downstream

    ShaderArgument_CompilerPath,
    ShaderArgument_DefaultDownstreamCompiler,
    ShaderArgument_DownstreamArgs, // stringValue0: downstream compiler name. stringValue1: argument list, one
                                   // per line.
    ShaderArgument_PassThrough,

    // Repro

    ShaderArgument_DumpRepro,
    ShaderArgument_DumpReproOnError,
    ShaderArgument_ExtractRepro,
    ShaderArgument_LoadRepro,
    ShaderArgument_LoadReproDirectory,
    ShaderArgument_ReproFallbackDirectory,

    // Debugging

    ShaderArgument_DumpAst,
    ShaderArgument_DumpIntermediatePrefix,
    ShaderArgument_DumpIntermediates, // bool
    ShaderArgument_DumpIr,            // bool
    ShaderArgument_DumpIrIds,
    ShaderArgument_PreprocessorOutput,
    ShaderArgument_OutputIncludes,
    ShaderArgument_ReproFileSystem,
    ShaderArgument_REMOVED_SerialIR, // deprecated and removed
    ShaderArgument_SkipCodeGen,      // bool
    ShaderArgument_ValidateIr,       // bool
    ShaderArgument_VerbosePaths,
    ShaderArgument_VerifyDebugSerialIr,
    ShaderArgument_NoCodeGen, // Not used.

    // Experimental

    ShaderArgument_FileSystem,
    ShaderArgument_Heterogeneous,
    ShaderArgument_NoMangle,
    ShaderArgument_NoHLSLBinding,
    ShaderArgument_NoHLSLPackConstantBufferElements,
    ShaderArgument_ValidateUniformity,
    ShaderArgument_AllowGLSL,
    ShaderArgument_EnableExperimentalPasses,
    ShaderArgument_BindlessSpaceIndex, // int

    // Internal

    ShaderArgument_ArchiveType,
    ShaderArgument_CompileCoreModule,
    ShaderArgument_Doc,

    ShaderArgument_IrCompression, //< deprecated

    ShaderArgument_LoadCoreModule,
    ShaderArgument_ReferenceModule,
    ShaderArgument_SaveCoreModule,
    ShaderArgument_SaveCoreModuleBinSource,
    ShaderArgument_TrackLiveness,
    ShaderArgument_LoopInversion, // bool, enable loop inversion optimization

    ShaderArgument_ParameterBlocksUseRegisterSpaces, // Deprecated
    ShaderArgument_LanguageVersion,                  // intValue0: SlangLanguageVersion
    ShaderArgument_TypeConformance, // stringValue0: additional type conformance to link, in the format of
                                    // "<TypeName>:<IInterfaceName>[=<sequentialId>]", for example
                                    // "Impl:IFoo=3" or "Impl:IFoo".
    ShaderArgument_EnableExperimentalDynamicDispatch, // bool, experimental
    ShaderArgument_EmitReflectionJSON,                // bool

    ShaderArgument_CountOfParsableOptions,

    // Used in parsed options only.
    ShaderArgument_DebugInformationFormat,  // intValue0: DebugInfoFormat
    ShaderArgument_VulkanBindShiftAll,      // intValue0: kind; intValue1: shift
    ShaderArgument_GenerateWholeProgram,    // bool
    ShaderArgument_UseUpToDateBinaryModule, // bool, when set, will only load
                                            // precompiled modules if it is up-to-date with its source.
    ShaderArgument_EmbedDownstreamIR,       // bool
    ShaderArgument_ForceDXLayout,           // bool

    // Add this new option to the end of the list to avoid breaking ABI as much as possible.
    // Setting of EmitSpirvDirectly or EmitSpirvViaGLSL will turn into this option internally.
    ShaderArgument_EmitSpirvMethod, // enum SlangEmitSpirvMethod

    ShaderArgument_SaveGLSLModuleBinSource,

    ShaderArgument_SkipDownstreamLinking, // bool, experimental
    ShaderArgument_DumpModule,

    ShaderArgument_GetModuleInfo,              // Print serialized module version and name
    ShaderArgument_GetSupportedModuleVersions, // Print the min and max module versions this compiler supports

    ShaderArgument_EmitSeparateDebug, // bool

    // Floating point denormal handling modes
    ShaderArgument_DenormalModeFp16,
    ShaderArgument_DenormalModeFp32,
    ShaderArgument_DenormalModeFp64,

    // Bitfield options
    ShaderArgument_UseMSVCStyleBitfieldPacking, // bool
    ShaderArgument_ForceCLayout,                // bool
    ShaderArgument_ExperimentalFeature,         // bool, enable experimental features
};

} // namespace Onyx

namespace Onyx::Detail
{
enum ShaderArgumentType : u8
{
    ShaderArgument_Integer,
    ShaderArgument_String,
};

struct ShaderArgumentValue
{
    ShaderArgumentType Type;
    const char *String0;
    const char *String1;
    i32 Value0;
    i32 Value1;
};

struct ShaderArgument
{
    ShaderArgumentName Name;
    ShaderArgumentValue Value;
};

struct SourceCode
{
    const char *Path;
    const char *Source;
};

struct Macro
{
    const char *Name;
    const char *Value;
};
} // namespace Onyx::Detail

namespace Onyx::Shaders
{
struct EntryPoint
{
    const char *Name;
    const char *Module;
    ShaderStage Stage;
};

// owned by compilation! if compilation is destroyed, this is no longer valid. user has to copy this if they want to
// outlive compilation
struct Spirv
{
    EntryPoint EntryPoint;
    u32 *Data;
    size_t Size;
};

class Compilation
{
  public:
    Compilation(const TKit::TierArray<Spirv> &compiledSpirv) : m_CompiledSpirv(compiledSpirv)
    {
    }

    ONYX_NO_DISCARD Result<const Spirv *> GetSpirv(const char *entryPoint, const char *module = nullptr) const;
    ONYX_NO_DISCARD Result<const Spirv *> GetSpirv(const char *entryPoint, ShaderStage stage,
                                                   const char *module = nullptr) const;
    ONYX_NO_DISCARD Result<const Spirv *> GetSpirv(const char *entryPoint, const char *module, ShaderStage stage) const;

    ONYX_NO_DISCARD Result<VKit::Shader> CreateShader(const char *entryPoint, const char *module = nullptr) const;
    ONYX_NO_DISCARD Result<VKit::Shader> CreateShader(const char *entryPoint, ShaderStage stage,
                                                      const char *module = nullptr) const;
    ONYX_NO_DISCARD Result<VKit::Shader> CreateShader(const char *entryPoint, const char *module,
                                                      ShaderStage stage) const;

    void Destroy();

  private:
    TKit::TierArray<Spirv> m_CompiledSpirv{};
};

class Compiler
{
    class Module
    {
      public:
        Module(Compiler *compiler, const char *name, const char *sourceCode = nullptr, const char *path = nullptr)
            : m_Compiler(compiler), m_Name(name), m_SourceCode(sourceCode), m_Path(path)
        {
        }

        Module &DeclareEntryPoint(const char *name, ShaderStage stage);
        Compiler &Load() const;

      private:
        Compiler *m_Compiler;
        const char *m_Name;
        const char *m_SourceCode;
        const char *m_Path;
        TKit::TierArray<EntryPoint> m_EntryPoints{};

        friend class Compiler;
    };

  public:
    Module &AddModule(const char *name);
    Module &AddModule(const char *name, const char *sourceCode, const char *path);

    Compiler &AddIntegerArgument(ShaderArgumentName name, i32 value0, i32 value1 = 0);
    Compiler &AddStringArgument(ShaderArgumentName name, const char *string0, const char *string1 = nullptr);
    Compiler &AddBooleanArgument(ShaderArgumentName name);

    Compiler &AddPreprocessorMacro(const char *name, const char *value = nullptr);
    Compiler &AddSearchPath(const char *path);

    Compiler &EnableEffectAnnotations();
    Compiler &AllowGlslSyntax();
    Compiler &SkipSpirvValidation();

    ONYX_NO_DISCARD Result<Compilation> Compile() const;

  private:
    TKit::TierArray<Module> m_Modules{};
    TKit::TierArray<Detail::ShaderArgument> m_Arguments{};
    TKit::TierArray<Detail::Macro> m_Macros{};
    TKit::TierArray<const char *> m_SearchPaths{};

    bool m_EnableEffectAnnotations = false;
    bool m_AllowGlslSyntax = false;
    bool m_SkipSpirvValidtion = false;
};

struct Specs
{
    bool EnableGlsl = false;
};

ONYX_NO_DISCARD Result<> Initialize(const Specs &specs);
void Terminate();

ONYX_NO_DISCARD Result<VKit::Shader> Create(const u32 *spirv, size_t size);
ONYX_NO_DISCARD Result<VKit::Shader> Create(const Spirv &spirv);
ONYX_NO_DISCARD Result<VKit::Shader> Create(std::string_view spirvPath);

} // namespace Onyx::Shaders
