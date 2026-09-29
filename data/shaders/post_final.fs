#version 330

// Endbild: Bloom dazu, Belichtung, ACES-Tonemapping, Gamma, Kantenglättung (FXAA), leichte Farbanpassung und Vignette.

in vec2 fragTexCoord;
uniform sampler2D texture0;    // Szene (linear, HDR)
uniform sampler2D texBloom;    // unscharfe helle Bereiche
uniform vec2 texel;            // Größe eines Bildpixels
uniform float exposure;
uniform float bloomStrength;
uniform float vignette;
uniform float saturation;
uniform vec3 grade;
uniform int fxaaOn;
uniform float time;

out vec4 finalColor;

vec3 tonemapAces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 display(vec3 hdr) { return pow(tonemapAces(hdr * exposure), vec3(1.0 / 2.2)); }

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main()
{
    vec2 uv = fragTexCoord;
    vec3 bloom = texture(texBloom, uv).rgb * bloomStrength;

    vec3 col;
    if (fxaaOn == 1) {
        vec3 rgbM = display(texture(texture0, uv).rgb + bloom);
        vec3 rgbNW = display(texture(texture0, uv + vec2(-1.0, -1.0) * texel).rgb + bloom);
        vec3 rgbNE = display(texture(texture0, uv + vec2(1.0, -1.0) * texel).rgb + bloom);
        vec3 rgbSW = display(texture(texture0, uv + vec2(-1.0, 1.0) * texel).rgb + bloom);
        vec3 rgbSE = display(texture(texture0, uv + vec2(1.0, 1.0) * texel).rgb + bloom);
        float lumaM = luma(rgbM), lumaNW = luma(rgbNW), lumaNE = luma(rgbNE), lumaSW = luma(rgbSW), lumaSE = luma(rgbSE);
        float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
        float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
        vec2 dir;
        dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
        dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));
        float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * (1.0 / 8.0)), 1.0 / 128.0);
        float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
        dir = min(vec2(8.0), max(vec2(-8.0), dir * rcpDirMin)) * texel;
        vec3 rgbA = 0.5 * (display(texture(texture0, uv + dir * (1.0 / 3.0 - 0.5)).rgb + bloom) +
                           display(texture(texture0, uv + dir * (2.0 / 3.0 - 0.5)).rgb + bloom));
        vec3 rgbB = rgbA * 0.5 + 0.25 * (display(texture(texture0, uv + dir * -0.5).rgb + bloom) +
                                         display(texture(texture0, uv + dir * 0.5).rgb + bloom));
        float lumaB = luma(rgbB);
        col = (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;
    } else {
        col = display(texture(texture0, uv).rgb + bloom);
    }

    float l = luma(col);
    col = mix(vec3(l), col, saturation);
    col *= grade;
    vec2 q = uv * 2.0 - 1.0;
    col *= 1.0 - vignette * dot(q, q) * 0.55;
    // Ganz feines Rauschen gegen Farbstufen in Verläufen
    float n = fract(sin(dot(gl_FragCoord.xy + fract(time), vec2(12.9898, 78.233))) * 43758.5453);
    col += (n - 0.5) / 255.0;
    finalColor = vec4(col, 1.0);
}
