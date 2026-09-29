#version 330

// Bloom, Aufwärtsstufe: weicher 3x3-Filter (Zeltfilter). Wird additiv auf die nächstgrößere Stufe gezeichnet.

in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec2 texel;       // Größe eines Quellpixels
uniform float radius;

out vec4 finalColor;

void main()
{
    vec2 uv = fragTexCoord;
    vec2 t = texel * radius;
    vec3 c = texture(texture0, uv).rgb * 4.0;
    c += texture(texture0, uv + vec2(-t.x, 0.0)).rgb * 2.0;
    c += texture(texture0, uv + vec2(t.x, 0.0)).rgb * 2.0;
    c += texture(texture0, uv + vec2(0.0, -t.y)).rgb * 2.0;
    c += texture(texture0, uv + vec2(0.0, t.y)).rgb * 2.0;
    c += texture(texture0, uv + vec2(-t.x, -t.y)).rgb;
    c += texture(texture0, uv + vec2(t.x, -t.y)).rgb;
    c += texture(texture0, uv + vec2(-t.x, t.y)).rgb;
    c += texture(texture0, uv + vec2(t.x, t.y)).rgb;
    finalColor = vec4(c / 16.0, 1.0);
}
