#!/bin/sh

set -e

SHADER_DIR="onyx/shaders"
BIN_DIR="$SHADER_DIR/bin"
OUTPUT="onyx/onyx/spirv.hpp"

mkdir -p "$BIN_DIR"

# ── Compile shaders ──────────────────────────────────────────────────────────

compile() {
    src="$1"
    out="$2"
    entry="$3"
    echo "  [slangc] $src ($entry) -> $out"
    # we need -emit-spirv-via-glsl bc slangc emits Int8 capability even tho we dont use it. because of that, we then need to suppress a warning that slangc emits because it needs to add a bunch of other capabilities
    slangc "$SHADER_DIR/$src" -profile spirv_1_5 -matrix-layout-row-major -emit-spirv-via-glsl -Wno-41012 -target spirv -o "$BIN_DIR/$out" -entry "$entry"
}

compile_vs_fs() {
    name="$1"
    compile "$name.slang" "$name-vert.spv" mainVS
    compile "$name.slang" "$name-frag.spv" mainFS
}

echo ""
echo "══════════════════════════════════════════════════════════════"
echo "  Compiling shaders"
echo "══════════════════════════════════════════════════════════════"
echo ""

echo "── Compute ──"
compile "ray-march.slang" "ray-march.spv" main

echo "── Circle ──"
compile_vs_fs "circle-flat-2D"
compile_vs_fs "circle-flat-3D"
compile_vs_fs "circle-shaded-2D"
compile_vs_fs "circle-shaded-3D"
compile_vs_fs "circle-shadow-2D"
compile_vs_fs "circle-shadow-3D"

echo "── Compositor ──"
compile_vs_fs "compositor"

echo "── Glyph ──"
compile_vs_fs "glyph-flat-2D"
compile_vs_fs "glyph-flat-3D"
compile_vs_fs "glyph-shaded-2D"
compile_vs_fs "glyph-shaded-3D"
compile_vs_fs "glyph-shadow-2D"
compile_vs_fs "glyph-shadow-3D"

echo "── Parametric ──"
compile_vs_fs "parametric-flat-2D"
compile_vs_fs "parametric-flat-3D"
compile_vs_fs "parametric-shaded-2D"
compile_vs_fs "parametric-shaded-3D"
compile_vs_fs "parametric-shadow-2D"
compile_vs_fs "parametric-shadow-3D"

echo "── Post Process ──"
compile_vs_fs "post-process"

echo "── Static ──"
compile_vs_fs "static-flat-2D"
compile_vs_fs "static-flat-3D"
compile_vs_fs "static-shaded-2D"
compile_vs_fs "static-shaded-3D"
compile_vs_fs "static-shadow-2D"
compile_vs_fs "static-shadow-3D"

SPV_COUNT=$(find "$BIN_DIR" -name "*.spv" | wc -l)
echo ""
echo "  Compiled $SPV_COUNT shader binaries"

# ── Generate spirv.hpp ───────────────────────────────────────────────────────

echo ""
echo "══════════════════════════════════════════════════════════════"
echo "  Generating $OUTPUT"
echo "══════════════════════════════════════════════════════════════"
echo ""

{
    echo "#pragma once"
    echo ""
    echo "// Auto-generated SPIR-V shader binaries"
    echo "// Do not edit manually — regenerate with compile_shaders.sh"
    echo ""

    # Emit xxd data for each .spv
    pushd $BIN_DIR >/dev/null
    for f in $(find . -name "*.spv" -printf "%f\n" | sort); do
        echo "  [xxd] $f" >&2
        xxd -i "$f"
        echo ""
    done

    # Collect variable names
    NAMES=$(find . -name "*.spv" -printf "%f\n" | sort | sed 's/[^a-zA-Z0-9]/_/g')

    # Emit enum
    echo "enum ShaderID {"
    for name in $NAMES; do
        enum_name=$(echo "$name" | sed 's/_spv$//' | sed -r 's/(^|_)([a-zA-Z0-9])/\U\2/g')
        echo "    Shader_$enum_name,"
    done
    echo "    Shader_Count"
    echo "};"
    echo ""

    # Emit lookup table
    echo "struct ShaderBinary {"
    echo "    const unsigned char *Data;"
    echo "    const unsigned int Size;"
    echo "};"
    echo ""
    echo "inline ShaderBinary g_ShaderBinaries[Shader_Count] = {"

    for name in $NAMES; do
        enum_name=$(echo "$name" | sed 's/_spv$//' | sed -r 's/(^|_)([a-zA-Z0-9])/\U\2/g')
        varname=$(echo "$name" | sed 's/[^a-zA-Z0-9]/_/g')
        echo "    { ${varname}, ${varname}_len }, // Shader_${enum_name}"
    done

    echo "};"

    popd >/dev/null
} >"$OUTPUT.tmp"

# Add inline to all xxd declarations
sed 's/^unsigned/inline unsigned/' "$OUTPUT.tmp" >"$OUTPUT"
rm "$OUTPUT.tmp"

echo ""
echo "  Generated $OUTPUT ($SPV_COUNT shaders)"
echo ""
echo "══════════════════════════════════════════════════════════════"
echo "  Done!"
echo "══════════════════════════════════════════════════════════════"
echo ""
