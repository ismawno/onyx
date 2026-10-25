#include "full-vertex.slang"

struct PushConstants
{
    u32 AttachmentIndex;
    u32 Mode;
}

[[vk::push_constant]]
PushConstants g_PushData;

[[vk::binding(ONYX_FXAA_PASS_COLOR_ATTACHMENTS_BINDING)]]
Sampler2D g_Colors[ONYX_MAX_ATTACHMENTS];

static const u32 Mode_Fast = 0;
static const u32 Mode_Console = 1;
static const u32 Mode_QualityLow = 2;
static const u32 Mode_QualityMedium = 3;
static const u32 Mode_QualityHigh = 4;

// Quality
static const f32 QualitySubpix = 0.75;
static const f32 QualityEdgeThreshold = 0.166;
static const f32 QualityEdgeThresholdMin = 0.0833;
static const f32 QualitySteps[12] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0};

// Fast
static const f32 FastReduceMin = 1.0 / 128.0;
static const f32 FastReduceMul = 1.0 / 8.0;
static const f32 FastSpanMax = 8.0;
static const f32 FastEdgeMin = 0.0312;
static const f32 FastEdgeRel = 0.125;

// Console
static const f32 ConsoleEdgeSharpness = 8.0;
static const f32 ConsoleEdgeThreshold = 0.125;
static const f32 ConsoleEdgeThresholdMin = 0.05;

f32 luma(const f32v3 color)
{
    return dot(color, f32v3(0.299, 0.587, 0.114));
}

// SampleLevel: this runs under divergent branches and loops
f32v3 tap(const Sampler2D colors, const f32v2 uv)
{
    return colors.SampleLevel(uv, 0.0).rgb;
}

f32 lumaAt(const Sampler2D colors, const f32v2 uv)
{
    return luma(tap(colors, uv));
}

f32v3 fxaaFast(const Sampler2D colors, const f32v2 uv, const f32v2 texel)
{
    const f32v3 rgbM = tap(colors, uv);
    const f32v3 rgbNW = tap(colors, uv + f32v2(-1.0, -1.0) * texel);
    const f32v3 rgbNE = tap(colors, uv + f32v2(1.0, -1.0) * texel);
    const f32v3 rgbSW = tap(colors, uv + f32v2(-1.0, 1.0) * texel);
    const f32v3 rgbSE = tap(colors, uv + f32v2(1.0, 1.0) * texel);

    const f32 lM = luma(rgbM);
    const f32 lNW = luma(rgbNW);
    const f32 lNE = luma(rgbNE);
    const f32 lSW = luma(rgbSW);
    const f32 lSE = luma(rgbSE);
    const f32 lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    const f32 lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));

    if (lMax - lMin < max(FastEdgeMin, lMax * FastEdgeRel))
        return rgbM;

    f32v2 dir;
    dir.x = -((lNW + lNE) - (lSW + lSE));
    dir.y = (lNW + lSW) - (lNE + lSE);

    const f32 dirReduce = max((lNW + lNE + lSW + lSE) * (0.25 * FastReduceMul), FastReduceMin);
    const f32 rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, f32v2(-FastSpanMax), f32v2(FastSpanMax)) * texel;

    const f32v3 rgbA =
        0.5 * (tap(colors, uv + dir * (1.0 / 3.0 - 0.5)) + tap(colors, uv + dir * (2.0 / 3.0 - 0.5)));
    const f32v3 rgbB = rgbA * 0.5 + 0.25 * (tap(colors, uv + dir * -0.5) + tap(colors, uv + dir * 0.5));

    const f32 lB = luma(rgbB);
    return (lB < lMin || lB > lMax) ? rgbA : rgbB;
}

f32v3 fxaaConsole(const Sampler2D colors, const f32v2 uv, const f32v2 texel)
{
    const f32v3 rgbM = tap(colors, uv);
    const f32 lM = luma(rgbM);
    const f32 lNW = lumaAt(colors, uv + f32v2(-1.0, -1.0) * texel);
    const f32 lNE = lumaAt(colors, uv + f32v2(1.0, -1.0) * texel) + 1.0 / 384.0;
    const f32 lSW = lumaAt(colors, uv + f32v2(-1.0, 1.0) * texel);
    const f32 lSE = lumaAt(colors, uv + f32v2(1.0, 1.0) * texel);

    const f32 lMin = min(min(lNW, lNE), min(lSW, lSE));
    const f32 lMax = max(max(lNW, lNE), max(lSW, lSE));
    const f32 lMaxScaled = max(ConsoleEdgeThresholdMin, lMax * ConsoleEdgeThreshold);
    if (max(lMax, lM) - min(lMin, lM) < lMaxScaled)
        return rgbM;

    const f32 swMinusNe = lSW - lNE;
    const f32 seMinusNw = lSE - lNW;
    const f32v2 dir1 = normalize(f32v2(swMinusNe + seMinusNw, swMinusNe - seMinusNw));
    const f32 dirAbsMinTimesC = min(abs(dir1.x), abs(dir1.y)) * ConsoleEdgeSharpness;
    const f32v2 dir2 = clamp(dir1 / dirAbsMinTimesC, f32v2(-2.0), f32v2(2.0));

    const f32v3 rgbA = tap(colors, uv - dir1 * texel * 0.5) + tap(colors, uv + dir1 * texel * 0.5);
    const f32v3 rgbB =
        (tap(colors, uv - dir2 * texel * 2.0) + tap(colors, uv + dir2 * texel * 2.0)) * 0.25 + rgbA * 0.25;

    const f32 lB = luma(rgbB);
    return (lB < min(lMin, lM) || lB > max(lMax, lM)) ? rgbA * 0.5 : rgbB;
}

f32v3 fxaaQuality(const Sampler2D colors, const f32v2 uv, const f32v2 r, const u32 steps)
{
    const f32v3 rgbM = tap(colors, uv);
    const f32 lM = luma(rgbM);
    f32 lN = lumaAt(colors, uv + f32v2(0.0, -1.0) * r);
    const f32 lS = lumaAt(colors, uv + f32v2(0.0, 1.0) * r);
    const f32 lW = lumaAt(colors, uv + f32v2(-1.0, 0.0) * r);
    const f32 lE = lumaAt(colors, uv + f32v2(1.0, 0.0) * r);

    const f32 rMax = max(lM, max(max(lN, lS), max(lW, lE)));
    const f32 rMin = min(lM, min(min(lN, lS), min(lW, lE)));
    const f32 range = rMax - rMin;
    if (range < max(QualityEdgeThresholdMin, rMax * QualityEdgeThreshold))
        return rgbM;

    const f32 lNW = lumaAt(colors, uv + f32v2(-1.0, -1.0) * r);
    const f32 lNE = lumaAt(colors, uv + f32v2(1.0, -1.0) * r);
    const f32 lSW = lumaAt(colors, uv + f32v2(-1.0, 1.0) * r);
    const f32 lSE = lumaAt(colors, uv + f32v2(1.0, 1.0) * r);

    // Sub-pixel aliasing amount
    const f32 lAvg = (2.0 * (lN + lS + lW + lE) + (lNW + lNE + lSW + lSE)) / 12.0;
    const f32 subA = saturate(abs(lAvg - lM) / range);
    const f32 subC = subA * subA * (3.0 - 2.0 * subA);
    const f32 subpixH = subC * subC * QualitySubpix;

    // Edge orientation
    const f32 edgeH =
        abs(-2.0 * lW + (lNW + lSW)) + abs(-2.0 * lM + (lN + lS)) * 2.0 + abs(-2.0 * lE + (lNE + lSE));
    const f32 edgeV =
        abs(-2.0 * lN + (lNW + lNE)) + abs(-2.0 * lM + (lW + lE)) * 2.0 + abs(-2.0 * lS + (lSW + lSE));
    const bool horz = edgeH >= edgeV;

    // Steeper gradient side
    f32 lengthSign = horz ? r.y : r.x;
    const f32 l1 = horz ? lN : lW;
    const f32 l2 = horz ? lS : lE;
    const f32 g1 = l1 - lM;
    const f32 g2 = l2 - lM;
    const bool pair1 = abs(g1) >= abs(g2);
    const f32 gradient = max(abs(g1), abs(g2));
    const f32 lNN = pair1 ? (l1 + lM) : (l2 + lM);
    if (pair1)
        lengthSign = -lengthSign;

    // Walk along the edge in both directions
    f32v2 posB = uv;
    if (horz)
        posB.y += lengthSign * 0.5;
    else
        posB.x += lengthSign * 0.5;
    const f32v2 off = horz ? f32v2(r.x, 0.0) : f32v2(0.0, r.y);

    f32v2 posN = posB - off * QualitySteps[0];
    f32v2 posP = posB + off * QualitySteps[0];
    f32 lEndN = lumaAt(colors, posN) - lNN * 0.5;
    f32 lEndP = lumaAt(colors, posP) - lNN * 0.5;
    const f32 gradScaled = gradient * 0.25;
    bool doneN = abs(lEndN) >= gradScaled;
    bool doneP = abs(lEndP) >= gradScaled;

    for (u32 i = 1; i < steps && !(doneN && doneP); ++i)
    {
        if (!doneN)
        {
            posN -= off * QualitySteps[i];
            lEndN = lumaAt(colors, posN) - lNN * 0.5;
            doneN = abs(lEndN) >= gradScaled;
        }
        if (!doneP)
        {
            posP += off * QualitySteps[i];
            lEndP = lumaAt(colors, posP) - lNN * 0.5;
            doneP = abs(lEndP) >= gradScaled;
        }
    }

    const f32 dstN = horz ? (uv.x - posN.x) : (uv.y - posN.y);
    const f32 dstP = horz ? (posP.x - uv.x) : (posP.y - uv.y);
    const bool dirN = dstN < dstP;
    const f32 dst = min(dstN, dstP);

    // Only shift if the span end is on the correct side of the center pixel's luma
    const bool centerNeg = (lM - lNN * 0.5) < 0.0;
    const bool goodSpan = ((dirN ? lEndN : lEndP) < 0.0) != centerNeg;

    const f32 pixelOffset = -dst / (dstN + dstP) + 0.5;
    const f32 finalOffset = max(goodSpan ? pixelOffset : 0.0, subpixH);

    f32v2 posF = uv;
    if (horz)
        posF.y += finalOffset * lengthSign;
    else
        posF.x += finalOffset * lengthSign;
    return tap(colors, posF);
}

[shader("fragment")]
f32v4 mainFS(const FragInput input) : SV_Target
{
    const f32v2 uv = input.TexCoord;
    const Sampler2D colors = g_Colors[g_PushData.AttachmentIndex];

    u32 width, height;
    colors.GetDimensions(width, height);
    const f32v2 texel = 1.0 / f32v2(width, height);

    f32v3 result;
    switch (g_PushData.Mode)
    {
    case Mode_Fast:
        result = fxaaFast(colors, uv, texel);
        break;
    case Mode_Console:
        result = fxaaConsole(colors, uv, texel);
        break;
    case Mode_QualityLow:
        result = fxaaQuality(colors, uv, texel, 5);
        break;
    case Mode_QualityMedium:
        result = fxaaQuality(colors, uv, texel, 10);
        break;
    default:
        result = fxaaQuality(colors, uv, texel, 12);
        break;
    }
    return f32v4(result, 1.0);
}
