#version 440

// Живой фон оболочки: аура, дымка и лучи света в духе PS5.
// Всё строится из плавного шума с искажением пространства (domain warping),
// поэтому формы медленно перетекают и никогда не повторяются точно.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;        // секунды
    float intensity;   // общая яркость эффектов, 0..1
    vec2 resolution;   // размер в пикселях
};

float hash(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

float fbm(vec2 p)
{
    float v = 0.0;
    float a = 0.5;
    const mat2 rot = mat2(0.8, 0.6, -0.6, 0.8);
    for (int i = 0; i < 5; ++i) {
        v += a * noise(p);
        p = rot * p * 2.02 + vec2(1.7, 9.2);
        a *= 0.5;
    }
    return v;
}

void main()
{
    vec2 uv = qt_TexCoord0;                          // 0..1, y вниз
    float aspect = resolution.x / max(resolution.y, 1.0);
    vec2 p = vec2((uv.x - 0.5) * aspect, uv.y - 0.5);
    float t = time;

    // Основа: глубокий синий сверху, почти чёрный снизу
    vec3 col = mix(vec3(0.020, 0.036, 0.095), vec3(0.006, 0.008, 0.024), smoothstep(-0.1, 1.0, uv.y));

    // --- Дымка: шум, искажённый шумом ---
    vec2 q = vec2(fbm(p * 1.5 + vec2(0.0, t * 0.020)),
                  fbm(p * 1.5 + vec2(5.2, -t * 0.017)));
    vec2 r = vec2(fbm(p * 1.7 + 3.0 * q + vec2(1.7, 9.2) + t * 0.024),
                  fbm(p * 1.7 + 3.0 * q + vec2(8.3, 2.8) - t * 0.019));
    float smoke = fbm(p * 1.3 + 2.6 * r);

    vec3 hazeCol = mix(vec3(0.09, 0.17, 0.46), vec3(0.24, 0.16, 0.52), smoothstep(0.3, 0.7, q.x));
    col += hazeCol * pow(smoke, 2.4) * 0.65 * intensity;

    // --- Аура: светящаяся лента в верхней части, колышется и «занавешивается» ---
    float wave = 0.20 * (r.x - 0.5) + 0.07 * sin(p.x * 1.6 + t * 0.06) + 0.04 * sin(p.x * 3.1 - t * 0.045);
    float band = p.y + 0.16 - wave;
    float aura = exp(-band * band * 9.0);
    // вертикальные складки, как у северного сияния
    float folds = fbm(vec2(p.x * 2.6 + r.y * 1.8, t * 0.04));
    aura *= 0.35 + 1.5 * folds * folds;
    // лента мягко затухает к краям экрана
    aura *= smoothstep(1.25, 0.15, abs(p.x + 0.15 * sin(t * 0.03)));

    vec3 auraCol = mix(vec3(0.13, 0.34, 1.00), vec3(0.47, 0.26, 0.98),
                       smoothstep(-0.7, 0.7, p.x + 0.4 * sin(t * 0.025)));
    auraCol = mix(auraCol, vec3(0.18, 0.78, 1.00), smoothstep(0.55, 0.80, r.y) * 0.7);
    col += auraCol * aura * 0.95 * intensity;

    // --- Лучи света из-за верхнего края ---
    vec2 src = vec2(aspect * 0.28, -0.80);
    vec2 d = p - src;
    float dist = length(d);
    float ang = atan(d.x, d.y);
    float rays = 0.6 * noise(vec2(ang * 9.0, t * 0.07)) + 0.4 * noise(vec2(ang * 24.0 + 3.0, -t * 0.10));
    rays = pow(rays, 3.0);
    float rayFade = smoothstep(2.1, 0.25, dist);
    col += vec3(0.42, 0.60, 1.00) * rays * rayFade * (0.45 + 0.55 * smoke) * 0.36 * intensity;
    // мягкое свечение самого источника
    col += vec3(0.28, 0.42, 1.00) * exp(-dist * 1.7) * 0.22 * intensity;

    // --- Пылинки в лучах: редкие, мягкие, медленно всплывают и мерцают ---
    vec2 g = (p + vec2(0.0, t * 0.012)) * 18.0;
    vec2 id = floor(g);
    vec2 f = fract(g) - 0.5;
    float h = hash(id);
    vec2 off = (vec2(hash(id + 3.1), hash(id + 7.7)) - 0.5) * 0.6;
    float mote = smoothstep(0.10, 0.0, length(f - off)) * step(0.90, h);
    mote *= 0.5 + 0.5 * sin(t * (0.5 + h * 1.2) + h * 40.0);
    col += vec3(0.70, 0.84, 1.00) * mote * (0.15 + 0.6 * rays * rayFade + 0.3 * smoke) * 0.5 * intensity;

    // Виньетка, чтобы края и низ уходили в темноту
    float vig = smoothstep(1.3, 0.35, length(p * vec2(0.82, 1.15)));
    col *= mix(0.5, 1.0, vig);

    // Дизеринг против полос на плавных градиентах
    col += (hash(uv * resolution + fract(t)) - 0.5) / 255.0;

    fragColor = vec4(col, 1.0) * qt_Opacity;
}
