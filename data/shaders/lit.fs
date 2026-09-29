#version 330

// Licht-Shader: Dreiebenen-Texturen (triplanar) mit Normalkarten, Sonne mit Schattenwurf, Punktlichter,
// Himmelslicht, Nebel. Die Ausgabe ist linear und darf über 1 liegen (HDR); Tonemapping und Gamma übernimmt die Nachbearbeitung.

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform sampler2D texture0;    // Farbtextur des Materials (sonst weiß)
uniform sampler2D texNormal;   // Normalkarte des Materials
uniform sampler2D shadowMap;   // Tiefe aus Sicht der Sonne
uniform vec4 colDiffuse;

uniform int useTex;            // 1 = Material mit Textur, 0 = einfarbig
uniform vec4 matA;             // x = 1/Kachelgröße, y = Normalstärke, z = Glanz, w = Spiegelstärke
uniform vec4 matB;             // x = Eigenleuchten, y = Fließgeschwindigkeit, z = Tönung, w = frei
uniform vec4 matC;             // x = Sättigung, y = Helligkeit
uniform int hasNormalMap;
uniform int useAnchor;         // 1 = Textur haftet an der Figur (Ort und Blickrichtung) statt an der Welt
uniform vec4 texAnchor;        // xyz = Ort der Figur, w = Blickwinkel (Yaw)
uniform float time;

uniform vec3 viewPos;
uniform vec3 sunDir;           // Richtung, in die das Licht scheint
uniform vec3 sunColor;
uniform vec3 skyAmbient;
uniform vec3 groundAmbient;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;
uniform float groundY;
uniform float ambientBoost;
uniform float sunBoost;

uniform mat4 lightVP;
uniform vec2 shadowTexel;
uniform int shadowOn;

uniform int lightCount;
uniform vec4 lightPos[8];      // xyz = Ort, w = Reichweite
uniform vec4 lightCol[8];      // rgb = Farbe mal Helligkeit

uniform int directOut;         // 1 = Tonemapping hier (ohne Nachbearbeitung)
uniform float exposure;

out vec4 finalColor;

vec3 toLin(vec3 c) { return pow(max(c, vec3(0.0)), vec3(2.2)); }

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}

const mat2 R = mat2(0.8, -0.6, 0.6, 0.8);

// Zwei Abtastungen mit verschiedener Drehung und Größe, weich nach niederfrequentem Rauschen gemischt:
// das nimmt der Textur das sichtbare Kachelmuster.
float antiTileMask(vec2 uv) { return smoothstep(0.38, 0.62, vnoise(uv * 0.11 + 7.3)); }

vec3 sampleAlbedo(vec2 uv) {
    float k = antiTileMask(uv);
    vec3 a = texture(texture0, uv).rgb;
    vec3 b = texture(texture0, R * uv * 0.73 + vec2(0.31, 0.57)).rgb;
    return mix(a, b, k);
}

vec3 sampleNormal(vec2 uv) {
    float k = antiTileMask(uv);
    vec3 a = texture(texNormal, uv).xyz * 2.0 - 1.0;
    vec3 b = texture(texNormal, R * uv * 0.73 + vec2(0.31, 0.57)).xyz * 2.0 - 1.0;
    b.xy = transpose(R) * b.xy;
    return normalize(mix(a, b, k));
}

void surface(vec3 p, vec3 n, vec2 scroll, out vec3 alb, out vec3 nrm) {
    float s = matA.x;
    vec3 w = pow(abs(n), vec3(5.0));
    w /= (w.x + w.y + w.z);
    vec2 uvx = p.zy * s + scroll, uvy = p.xz * s + scroll, uvz = p.xy * s + scroll;
    alb = sampleAlbedo(uvx) * w.x + sampleAlbedo(uvy) * w.y + sampleAlbedo(uvz) * w.z;
    nrm = n;
    if (hasNormalMap == 1) {
        vec3 tx = sampleNormal(uvx), ty = sampleNormal(uvy), tz = sampleNormal(uvz);
        tx.xy *= matA.y; ty.xy *= matA.y; tz.xy *= matA.y;
        tx = vec3(tx.xy + n.zy, abs(tx.z) * n.x);
        ty = vec3(ty.xy + n.xz, abs(ty.z) * n.y);
        tz = vec3(tz.xy + n.xy, abs(tz.z) * n.z);
        nrm = normalize(tx.zyx * w.x + ty.xzy * w.y + tz.xyz * w.z);
    }
}

// Zwölf gedrehte Abtastpunkte für weiche Schattenränder
const vec2 kPoisson[12] = vec2[](
    vec2(-0.326, -0.406), vec2(-0.840, -0.074), vec2(-0.696, 0.457), vec2(-0.203, 0.621),
    vec2(0.962, -0.195), vec2(0.473, -0.480), vec2(0.519, 0.767), vec2(0.185, -0.893),
    vec2(0.507, 0.064), vec2(0.896, 0.412), vec2(-0.322, -0.933), vec2(-0.792, -0.598));

float shadowFactor(vec3 p, vec3 n, float ndl) {
    if (shadowOn == 0) return 1.0;
    vec4 lp = lightVP * vec4(p + n * (0.05 + 0.08 * (1.0 - ndl)), 1.0);
    vec3 sc = lp.xyz / lp.w * 0.5 + 0.5;
    if (sc.x < 0.0 || sc.x > 1.0 || sc.y < 0.0 || sc.y > 1.0 || sc.z > 1.0) return 1.0;
    float bias = 0.0004 + 0.0015 * (1.0 - ndl);
    float ang = hash12(gl_FragCoord.xy) * 6.2831853;
    mat2 rot = mat2(cos(ang), -sin(ang), sin(ang), cos(ang));
    float sum = 0.0;
    for (int i = 0; i < 12; i++) {
        vec2 o = rot * kPoisson[i] * shadowTexel * 2.2;
        sum += (sc.z - bias) > texture(shadowMap, sc.xy + o).r ? 0.0 : 1.0;
    }
    float s = sum / 12.0;
    vec2 e = min(sc.xy, 1.0 - sc.xy);
    float fade = smoothstep(0.0, 0.08, min(e.x, e.y));
    return mix(1.0, s, fade);
}

vec3 tonemapAces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void emit(vec3 linColor, float a) {
    if (directOut == 1) linColor = pow(tonemapAces(linColor * exposure), vec3(1.0 / 2.2));
    finalColor = vec4(linColor, a);
}

void main()
{
    float alpha = colDiffuse.a * fragColor.a;
    vec3 toView = viewPos - fragPosition;
    float dist = length(toView);
    vec3 V = toView / max(dist, 0.0001);
    vec3 L = normalize(-sunDir);
    vec3 fogLin = toLin(fogColor);
    float fogT = smoothstep(fogStart, fogEnd, dist);
    vec3 fogC = fogLin + toLin(sunColor) * pow(max(dot(-V, L), 0.0), 8.0) * 0.20;

    // Halbtransparentes (Lichthöfe, Funken, Schatten-Flecken) leuchtet selbst und wird nicht beleuchtet
    if (useTex == 0 && alpha < 0.99) {
        vec3 g = toLin(colDiffuse.rgb * fragColor.rgb) * 2.4;
        emit(mix(g, fogC, fogT * 0.7), alpha);
        return;
    }

    vec3 Ng = normalize(fragNormal);
    vec3 N = Ng;
    vec3 albedo;
    float gloss = 0.16, specK = 0.18;
    vec3 emissive = vec3(0.0);

    if (useTex == 1) {
        vec2 scroll = vec2(time * matB.y, time * matB.y * 0.62);
        vec3 alb, nrm;
        vec3 macroPos = fragPosition;
        if (useAnchor == 1) {
            // Figuren: in den Raum der Figur umrechnen, damit die Textur beim Laufen und Drehen mitgeht
            float cy = cos(texAnchor.w), sy = sin(texAnchor.w);
            vec3 pl = fragPosition - texAnchor.xyz;
            pl = vec3(cy * pl.x - sy * pl.z, pl.y, sy * pl.x + cy * pl.z);
            vec3 nl = vec3(cy * Ng.x - sy * Ng.z, Ng.y, sy * Ng.x + cy * Ng.z);
            surface(pl, nl, scroll, alb, nrm);
            N = vec3(cy * nrm.x + sy * nrm.z, nrm.y, -sy * nrm.x + cy * nrm.z);
            macroPos = pl;
        } else {
            surface(fragPosition, Ng, scroll, alb, nrm);
            N = nrm;
        }
        vec3 tintCol = colDiffuse.rgb * fragColor.rgb;
        alb *= mix(vec3(1.0), tintCol * 2.0, matB.z);
        float lum = dot(alb, vec3(0.299, 0.587, 0.114));
        alb = mix(vec3(lum), alb, matC.x) * matC.y;
        // Großflächige Helligkeitsschwankung gegen gleichförmige Flächen
        float macro = vnoise(macroPos.xz * 0.11 + macroPos.y * 0.07);
        alb *= mix(0.80, 1.14, macro);
        albedo = toLin(alb);
        gloss = matA.z;
        specK = matA.w;
        if (matB.x > 0.0) {
            // Lava: zwei gegenläufig fließende Schichten, pulsierend
            vec3 alb2, nrm2;
            surface(fragPosition * 0.63 + vec3(11.0, 0.0, 5.0), Ng, -scroll * 1.4, alb2, nrm2);
            vec3 lava = toLin(mix(alb, alb2, 0.5));
            float pulse = 0.88 + 0.16 * sin(time * 1.6 + fragPosition.x * 0.35 + fragPosition.z * 0.27);
            vec3 c = lava * matB.x * pulse;
            emit(mix(c, fogC, fogT), 1.0);
            return;
        }
    } else {
        albedo = toLin(colDiffuse.rgb * fragColor.rgb);
        // Feine Körnung, damit flache Flächen nicht wie Plastik wirken
        albedo *= 0.94 + 0.12 * vnoise(fragPosition.xz * 6.0 + fragPosition.y * 5.0);
    }

    float ndl = max(dot(N, L), 0.0);
    float ndlG = max(dot(Ng, L), 0.0);
    float sh = ndlG > 0.0 ? shadowFactor(fragPosition, Ng, ndlG) : 1.0;

    // Ungefähre Verdunklung an Wandfüßen und in Bodennähe
    float ao = 1.0;
    if (abs(Ng.y) < 0.6) ao *= mix(0.62, 1.0, smoothstep(0.0, 1.6, fragPosition.y - groundY));
    if (Ng.y < -0.5) ao *= 0.6;

    vec3 sunLin = toLin(sunColor) * 3.0 * sunBoost;
    vec3 amb = mix(toLin(groundAmbient), toLin(skyAmbient), N.y * 0.5 + 0.5) * 1.6 * ambientBoost * ao;
    vec3 diff = amb + sunLin * ndl * sh;

    float shin = mix(6.0, 220.0, gloss);
    vec3 H = normalize(L + V);
    float sp = pow(max(dot(N, H), 0.0), shin) * (shin + 8.0) / 60.0;
    vec3 spec = sunLin * sp * specK * ndl * sh;

    // Punktlichter (Fackeln, Feuerschalen, Lava, Kristalle)
    for (int i = 0; i < lightCount; i++) {
        vec3 d = lightPos[i].xyz - fragPosition;
        float dl = length(d);
        float att = clamp(1.0 - dl / lightPos[i].w, 0.0, 1.0);
        att *= att;
        vec3 Lp = d / max(dl, 0.001);
        float nd = max(dot(N, Lp), 0.0);
        diff += lightCol[i].rgb * att * nd;
        vec3 Hp = normalize(Lp + V);
        spec += lightCol[i].rgb * att * pow(max(dot(N, Hp), 0.0), shin) * (shin + 8.0) / 60.0 * specK * nd;
    }

    // Spiegelung des Himmels an glatten Flächen (Fresnel) und ein leichtes Randlicht
    float fres = pow(1.0 - max(dot(N, V), 0.0), 4.0);
    vec3 Rf = reflect(-V, N);
    vec3 envRefl = mix(toLin(groundAmbient), toLin(skyAmbient), clamp(Rf.y * 0.5 + 0.5, 0.0, 1.0)) * 1.6 * ambientBoost;
    spec += envRefl * fres * (0.05 + gloss * 0.9) * specK;
    vec3 rim = toLin(skyAmbient) * pow(1.0 - max(dot(Ng, V), 0.0), 3.0) * 0.10 * ambientBoost;

    vec3 color = albedo * diff + spec + rim * albedo;
    color = mix(color, fogC, fogT);
    emit(color, 1.0);
}
