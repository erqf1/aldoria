#version 330

// Himmel: Farbverlauf vom Horizont (Nebelfarbe) zum Zenit, Sonne mit Hof und Scheibe, ziehende Wolken.
// Ausgabe linear (HDR), die Sonnenscheibe ist heller als 1 und lässt den Bloom aufleuchten.

uniform vec2 resolution;
uniform vec3 camF;
uniform vec3 camR;
uniform vec3 camU;
uniform float tanHalf;
uniform float aspect;

uniform vec3 skyTop;
uniform vec3 horizon;
uniform vec3 sunDir;
uniform vec3 sunColor;
uniform float time;
uniform float cloudAmount;
uniform float sunGlow;
uniform int directOut;
uniform float exposure;

out vec4 finalColor;

vec3 toLin(vec3 c) { return pow(max(c, vec3(0.0)), vec3(2.2)); }

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}

float fbm(vec2 p) {
    float a = 0.5, s = 0.0;
    for (int i = 0; i < 5; i++) {
        s += a * noise(p);
        p = p * 2.03 + vec2(17.1, 9.2);
        a *= 0.5;
    }
    return s;
}

vec3 tonemapAces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec2 ndc = gl_FragCoord.xy / resolution * 2.0 - 1.0;
    vec3 dir = normalize(camF + camR * (ndc.x * tanHalf * aspect) + camU * (ndc.y * tanHalf));
    vec3 toSun = normalize(-sunDir);

    vec3 hz = toLin(horizon), top = toLin(skyTop);
    float h = dir.y;
    vec3 col = mix(hz, top, pow(clamp(h * 1.5, 0.0, 1.0), 0.65)) * 1.6;
    if (h < 0.0) col = mix(hz * 1.6, hz * 0.7, clamp(-h * 4.0, 0.0, 1.0));

    float sd = max(dot(dir, toSun), 0.0);
    vec3 sun = toLin(sunColor);
    col += sun * (pow(sd, 6.0) * 0.20 + pow(sd, 90.0) * 0.55) * sunGlow;
    col += sun * smoothstep(0.9990, 0.9996, sd) * 40.0 * sunGlow;

    if (cloudAmount > 0.01 && h > 0.0) {
        vec2 cuv = dir.xz / (h + 0.14) * 0.85 + vec2(time * 0.010, time * 0.004);
        float c = fbm(cuv * 1.4);
        float cover = smoothstep(1.0 - cloudAmount * 0.75 - 0.18, 1.0 - 0.12, c);
        float lit = clamp(0.55 + 0.6 * (fbm(cuv * 1.4 + toSun.xz * 0.08) - c) * 4.0, 0.25, 1.15);
        vec3 cloudCol = mix(hz * 1.1, vec3(1.0) * (0.55 + 0.7 * max(toSun.y, 0.2)), 0.7) * lit;
        cloudCol += sun * pow(sd, 4.0) * 0.4 * (1.0 - c);
        col = mix(col, cloudCol, cover * smoothstep(0.0, 0.22, h));
    }

    if (directOut == 1) col = pow(tonemapAces(col * exposure), vec3(1.0 / 2.2));
    finalColor = vec4(col, 1.0);
}
