#version 440

// Живой фон оболочки: светлая стальная гамма, объёмная дымка, лучи света сверху,
// свечение горизонта и едва заметная перспективная сетка «пола».
// Палитра почти монохромная (сталь / серебро), единственный акцент — холодный
// ледяной оттенок в свечении горизонта. Всё движется очень медленно.

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
    for (int i = 0; i < 4; ++i) {
        v += a * noise(p);
        p = rot * p * 2.03 + vec2(1.7, 9.2);
        a *= 0.5;
    }
    return v;
}

// Тонкая сглаженная линия сетки: dist — расстояние до линии в единицах мира,
// pix — сколько единиц мира приходится на пиксель
float gridLine(float coord, float pix)
{
    float dist = abs(fract(coord + 0.5) - 0.5);
    return 1.0 - smoothstep(0.5, 1.5, dist / max(pix, 1e-5));
}

void main()
{
    vec2 uv = qt_TexCoord0;                          // 0..1, y вниз
    float aspect = resolution.x / max(resolution.y, 1.0);
    vec2 p = vec2((uv.x - 0.5) * aspect, uv.y - 0.5);
    float t = time;
    float pixel = 1.0 / max(resolution.y, 1.0);      // размер пикселя в единицах p

    const vec3 steelTop = vec3(0.215, 0.240, 0.275);
    const vec3 steelMid = vec3(0.150, 0.165, 0.190);
    const vec3 steelLow = vec3(0.070, 0.078, 0.092);
    const vec3 silver   = vec3(0.780, 0.810, 0.850);
    const vec3 ice      = vec3(0.600, 0.740, 0.880);  // единственный цветовой акцент

    // Линия горизонта чуть ниже середины экрана
    float horizon = 0.12;
    float dh = p.y - horizon;                        // > 0 — под горизонтом

    // --- Основа: стальной градиент сверху вниз ---
    vec3 col = mix(steelTop, steelMid, smoothstep(-0.5, horizon, p.y));
    col = mix(col, steelLow, smoothstep(horizon, 0.55, p.y));

    // --- Дымка: слоистая, вытянутая по горизонтали, с лёгким искажением ---
    vec2 q = vec2(fbm(p * 1.2 + vec2(t * 0.012, 0.0)),
                  fbm(p * 1.2 + vec2(5.2, 1.3) - vec2(0.0, t * 0.010)));
    float smoke = fbm(p * vec2(0.9, 2.1) + 1.7 * q + vec2(t * 0.018, 0.0));
    // гуще всего у горизонта, к верху и к низу редеет
    float hazeBand = exp(-abs(dh) * 2.6);
    float haze = smoothstep(0.30, 0.85, smoke) * (0.35 + 0.65 * hazeBand);
    col += silver * haze * 0.20 * intensity;

    // --- Лучи света сверху: источник за верхним краем, медленно смещается ---
    vec2 src = vec2(aspect * 0.18 * sin(t * 0.017), -1.05);
    vec2 d = p - src;
    float dist = length(d);
    float ang = atan(d.x, d.y);
    float rays = 0.65 * noise(vec2(ang * 8.0, t * 0.045))
               + 0.35 * noise(vec2(ang * 21.0 + 4.0, -t * 0.060));
    rays = pow(rays, 2.6);
    float rayFade = smoothstep(2.0, 0.4, dist) * (1.0 - 0.75 * smoothstep(horizon - 0.05, horizon + 0.25, p.y));
    // объём: лучи видны там, где есть дымка
    col += silver * rays * rayFade * (0.45 + 0.75 * smoke) * 0.30 * intensity;
    // мягкое общее свечение сверху
    col += silver * exp(-dist * 1.35) * 0.10 * intensity;

    // --- Свечение горизонта: узкая яркая полоса и широкий ореол ---
    float spread = exp(-p.x * p.x * 0.55);
    float glow = exp(-abs(dh) * 34.0) * 0.30 + exp(-abs(dh) * 7.0) * 0.10;
    col += mix(silver, ice, 0.6) * glow * spread * (0.85 + 0.15 * sin(t * 0.21)) * intensity;

    // --- Перспективная сетка «пола» под горизонтом, медленно едет к зрителю ---
    if (dh > 0.0) {
        float k = 0.32;
        float z = k / dh;                            // глубина
        float xw = p.x * z;
        float zw = z - t * 0.10;
        float pixX = z * pixel;                      // единиц мира на пиксель по x
        float pixZ = z * z / k * pixel;              // и по глубине
        float g = max(gridLine(xw, pixX), gridLine(zw, pixZ));
        // гасим вдали (до появления муара) и по бокам
        float fade = (1.0 - smoothstep(0.12, 0.45, pixZ)) * exp(-z * 0.06) * smoothstep(0.0, 0.03, dh);
        fade *= smoothstep(4.5, 1.0, abs(xw) / max(z, 1.0));
        col += mix(silver, ice, 0.4) * g * fade * 0.055 * intensity;
    }

    // Лёгкая виньетка: края чуть темнее, без провала в черноту
    float vig = smoothstep(1.35, 0.30, length(p * vec2(0.80, 1.10)));
    col *= mix(0.72, 1.0, vig);

    // Дизеринг против полос на плавных градиентах
    col += (hash(uv * resolution + fract(t)) - 0.5) / 255.0;

    fragColor = vec4(clamp(col, 0.0, 1.0), 1.0) * qt_Opacity;
}
