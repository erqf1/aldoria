#version 330

// Bloom, Abwärtsstufe: mittelt das Bild auf die halbe Größe. In der ersten Stufe bleiben nur Werte über der Schwelle übrig.

in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec2 texel;       // Größe eines Quellpixels in Texturkoordinaten
uniform int firstPass;
uniform float threshold;

out vec4 finalColor;

vec3 prefilter(vec3 c) {
    float br = max(c.r, max(c.g, c.b));
    float soft = clamp(br - threshold + 0.5, 0.0, 1.0);
    soft = soft * soft * 0.5;
    float w = max(soft, br - threshold) / max(br, 0.0001);
    return c * w;
}

void main()
{
    vec2 uv = fragTexCoord;
    vec3 a = texture(texture0, uv + texel * vec2(-1.0, -1.0)).rgb;
    vec3 b = texture(texture0, uv + texel * vec2(1.0, -1.0)).rgb;
    vec3 c = texture(texture0, uv + texel * vec2(-1.0, 1.0)).rgb;
    vec3 d = texture(texture0, uv + texel * vec2(1.0, 1.0)).rgb;
    vec3 e = texture(texture0, uv).rgb;
    vec3 col;
    if (firstPass == 1) {
        // Helligkeitsgewichtet mitteln, damit einzelne sehr helle Pixel nicht flackern (Karis)
        vec3 pa = prefilter(a), pb = prefilter(b), pc = prefilter(c), pd = prefilter(d), pe = prefilter(e);
        float wa = 1.0 / (1.0 + dot(pa, vec3(0.333))), wb = 1.0 / (1.0 + dot(pb, vec3(0.333)));
        float wc = 1.0 / (1.0 + dot(pc, vec3(0.333))), wd = 1.0 / (1.0 + dot(pd, vec3(0.333)));
        float we = 1.0 / (1.0 + dot(pe, vec3(0.333)));
        col = (pa * wa + pb * wb + pc * wc + pd * wd + pe * we * 2.0) / (wa + wb + wc + wd + we * 2.0);
    } else {
        col = (a + b + c + d) * 0.125 + e * 0.5;
    }
    finalColor = vec4(min(col, vec3(60.0)), 1.0);
}
