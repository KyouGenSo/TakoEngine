# Jolt.cmake のソースの一覧から、project/externals/JoltPhysics/Jolt.vcxproj と Jolt.vcxproj.filters を作る。
#   python tools/gen_jolt_vcxproj.py
#
# Jolt を差し替えたら流し直す。入れるのは、CMake のオプションに関係なくビルドされるソースだけ
# （ENABLE_OBJECT_STREAM・JPH_USE_DX12 などのオプションで足されるソースは入れない）。
import re
import uuid
from pathlib import Path

JOLT_DIR = Path(__file__).resolve().parent.parent / "project" / "externals" / "JoltPhysics"
PROJECT_GUID = "{472FAD90-E921-4A87-9F15-57C6CE5F2B64}"  # TakoEngine.sln と TakoEngine.vcxproj の参照と同じ値
CONFIGURATIONS = ("Debug", "Develop", "Release")


def read_sources() -> list[str]:
    """Jolt.cmake の最初の set(JOLT_PHYSICS_SRC_FILES ...) の中身を、JoltPhysics からの相対パスで返す。"""
    lines = (JOLT_DIR / "Jolt" / "Jolt.cmake").read_text(encoding="utf-8").splitlines()
    start = lines.index("set(JOLT_PHYSICS_SRC_FILES") + 1
    sources = []
    for line in lines[start:lines.index(")", start)]:
        match = re.fullmatch(r"\s*\$\{JOLT_PHYSICS_ROOT\}/(\S+)", line)
        if not match:
            raise SystemExit(f"Jolt.cmake の一覧に、読めない行があります: {line!r}")
        source = "Jolt\\" + match.group(1).replace("/", "\\")
        if not source.endswith((".cpp", ".h", ".inl")):
            continue  # 一覧には Jolt.cmake 自身と図のファイルも入っている
        if not (JOLT_DIR / source).is_file():
            raise SystemExit(f"Jolt.cmake にあるファイルが見つかりません: {source}")
        sources.append(source)
    return sources


def item_type(source: str) -> str:
    return "ClCompile" if source.endswith(".cpp") else "ClInclude"


def filter_of(source: str) -> str:
    """Jolt\\ の直下のファイルは空（フィルターなし）。"""
    return "\\".join(source.split("\\")[1:-1])


def make_project(sources: list[str]) -> list[str]:
    out = [
        '<?xml version="1.0" encoding="utf-8"?>',
        '<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">',
        '  <ItemGroup Label="ProjectConfigurations">',
    ]
    for configuration in CONFIGURATIONS:
        out += [
            f'    <ProjectConfiguration Include="{configuration}|x64">',
            f"      <Configuration>{configuration}</Configuration>",
            "      <Platform>x64</Platform>",
            "    </ProjectConfiguration>",
        ]
    out += [
        "  </ItemGroup>",
        '  <PropertyGroup Label="Globals">',
        "    <VCProjectVersion>17.0</VCProjectVersion>",
        "    <Keyword>Win32Proj</Keyword>",
        f"    <ProjectGuid>{PROJECT_GUID.lower()}</ProjectGuid>",
        "    <RootNamespace>Jolt</RootNamespace>",
        "    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>",
        "  </PropertyGroup>",
        '  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />',
    ]
    for configuration in CONFIGURATIONS:
        use_debug_libraries = "true" if configuration == "Debug" else "false"
        out += [
            f"""  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='{configuration}|x64'" Label="Configuration">""",
            "    <ConfigurationType>StaticLibrary</ConfigurationType>",
            f"    <UseDebugLibraries>{use_debug_libraries}</UseDebugLibraries>",
            "    <PlatformToolset>v145</PlatformToolset>",
            "  </PropertyGroup>",
        ]
    out += [
        '  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />',
        '  <ImportGroup Label="ExtensionSettings">',
        '    <Import Project="..\\..\\..\\build\\TakoEngineCommon.props" />',
        "  </ImportGroup>",
        '  <ImportGroup Label="PropertySheets">',
        """    <Import Project="$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props" Condition="exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')" Label="LocalAppDataPlatform" />""",
        "  </ImportGroup>",
        '  <PropertyGroup Label="UserMacros" />',
        "  <PropertyGroup>",
        "    <OutDir>$(TakoEngineBinDir)</OutDir>",
        "    <IntDir>$(TakoEngineObjDir)</IntDir>",
        "  </PropertyGroup>",
        "  <ItemDefinitionGroup>",
        "    <ClCompile>",
        "      <!-- TakoJoltDefines: Jolt のヘッダーを include する TakoEngine.vcxproj と同じ値を使う（食い違うと、クラスの形が合わなくなる） -->",
        "      <PreprocessorDefinitions>$(TakoJoltDefines);%(PreprocessorDefinitions)</PreprocessorDefinitions>",
        "      <AdditionalIncludeDirectories>$(ProjectDir)</AdditionalIncludeDirectories>",
        "      <LanguageStandard>stdcpp20</LanguageStandard>",
        "      <MultiProcessorCompilation>true</MultiProcessorCompilation>",
        "      <RuntimeLibrary>$(TakoRuntimeLibrary)</RuntimeLibrary>",
        "    </ClCompile>",
        "  </ItemDefinitionGroup>",
    ]
    for kind in ("ClCompile", "ClInclude"):
        out.append("  <ItemGroup>")
        out += [f'    <{kind} Include="{source}" />' for source in sources if item_type(source) == kind]
        out.append("  </ItemGroup>")
    out += [
        "  <ItemGroup>",
        '    <Natvis Include="Jolt\\Jolt.natvis" />',
        "  </ItemGroup>",
        '  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />',
        '  <ImportGroup Label="ExtensionTargets">',
        "  </ImportGroup>",
        "</Project>",
    ]
    return out


def make_filters(sources: list[str]) -> list[str]:
    # 親のフィルターも宣言が要る（Physics\Collision なら Physics も）
    filters = set()
    for source in sources:
        parts = filter_of(source).split("\\") if filter_of(source) else []
        filters.update("\\".join(parts[:depth]) for depth in range(1, len(parts) + 1))

    out = [
        '<?xml version="1.0" encoding="utf-8"?>',
        '<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">',
        "  <ItemGroup>",
    ]
    for name in sorted(filters):
        # 流し直しても同じ ID になるよう、名前から作る
        identifier = uuid.uuid5(uuid.UUID(PROJECT_GUID), name)
        out += [
            f'    <Filter Include="{name}">',
            f"      <UniqueIdentifier>{{{identifier}}}</UniqueIdentifier>",
            "    </Filter>",
        ]
    out.append("  </ItemGroup>")
    for kind in ("ClCompile", "ClInclude"):
        out.append("  <ItemGroup>")
        for source in sources:
            if item_type(source) != kind:
                continue
            if filter_of(source):
                out += [f'    <{kind} Include="{source}">', f"      <Filter>{filter_of(source)}</Filter>", f"    </{kind}>"]
            else:
                out.append(f'    <{kind} Include="{source}" />')
        out.append("  </ItemGroup>")
    out += [
        "  <ItemGroup>",
        '    <Natvis Include="Jolt\\Jolt.natvis" />',
        "  </ItemGroup>",
        "</Project>",
    ]
    return out


def write(path: Path, lines: list[str], bom: bool) -> None:
    # Visual Studio が書く形に合わせる（CRLF。.filters は BOM 付き）
    path.write_bytes((b"\xef\xbb\xbf" if bom else b"") + "\r\n".join(lines).encode("utf-8"))


def main() -> None:
    sources = read_sources()
    if len({Path(source).stem.lower() for source in sources if item_type(source) == "ClCompile"}) != sum(item_type(source) == "ClCompile" for source in sources):
        # obj は 1 つのフォルダーに出るので、同じ名前の .cpp があると上書きし合う
        raise SystemExit("同じ名前の .cpp があります。ObjectFileName を分ける必要があります")
    write(JOLT_DIR / "Jolt.vcxproj", make_project(sources), bom=False)
    write(JOLT_DIR / "Jolt.vcxproj.filters", make_filters(sources), bom=True)
    compiled = sum(item_type(source) == "ClCompile" for source in sources)
    print(f"Jolt.vcxproj: .cpp {compiled}, headers {len(sources) - compiled}")


if __name__ == "__main__":
    main()
