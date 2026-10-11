#version 440

// Живой фон оболочки: «графитовый атриум». Светлая стальная гамма, мягкие
// поля света, которые медленно переползают по экрану, слоистая дымка у горизонта,
// широкие объёмные лучи из высокого окна за кадром и редкие пылинки в них.
// Палитра почти монохромная (графит / сталь / серебро), единственный акцент —
// сдержанный ледяной оттенок в свечении горизонта. Всё движется очень медленно.
// Шейдер намеренно простой: считается в половинном разрешении и должен
// тянуться даже программной отрисовкой в виртуальной машине.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;        // секунды
    float intensity;   // общая яркость эффектов, 0..1
    vec2 resolution;   // размер в пикселях
    float dim;         // затемнение переднего плана, 0..1
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

// Три октавы: дымке нужны только крупные формы, мелочь всё равно размоется
float fbm(vec2 p)
{
    float v = 0.0;
    float a = 0.5;
    const mat2 rot = mat2(0.8, 0.6, -0.6, 0.8);
    for (int i = 0; i < 3; ++i) {
        v += a * noise(p);
        p = rot * p * 2.03 + vec2(1.7, 9.2);
        a *= 0.5;
    }
    return v / 0.875;
}

void main()
{
    vec2 uv = qt_TexCoord0;                          // 0..1, y вниз
    float aspect = resolution.x / max(resolution.y, 1.0);
    vec2 p = vec2((uv.x - 0.5) * aspect, uv.y - 0.5);
    float t = time;

    const vec3 steelTop  = vec3(0.268, 0.290, 0.320);
    const vec3 steelMid  = vec3(0.184, 0.198, 0.220);
    const vec3 graphite  = vec3(0.100, 0.108, 0.122);
    const vec3 silver    = vec3(0.800, 0.830, 0.865);
    const vec3 ice       = vec3(0.640, 0.780, 0.920);  // единственный цветовой акцент

    // Горизонт чуть ниже середины экрана
    const float horizon = 0.16;
    float dh = p.y - horizon;                        // > 0 — под горизонтом

    // --- Основа: сталь сверху, графит снизу ---
    vec3 col = mix(steelTop, steelMid, smoothstep(-0.55, horizon, p.y));
    col = mix(col, graphite, smoothstep(horizon - 0.10, 0.66, p.y));

    // --- Медленные поля света: два широких пятна плывут по своим траекториям ---
    vec2 c1 = vec2(-0.30 * aspect + 0.22 * sin(t * 0.011), -0.28 + 0.07 * cos(t * 0.013));
    vec2 c2 = vec2( 0.28 * aspect + 0.18 * cos(t * 0.008), 0.02 + 0.06 * sin(t * 0.017));
    vec2 e1 = (p - c1) * vec2(0.75, 1.0);
    vec2 e2 = (p - c2) * vec2(0.85, 1.2);
    float fields = exp(-dot(e1, e1) * 2.4) * 0.085 + exp(-dot(e2, e2) * 3.2) * 0.050;
    col += silver * fields * intensity;

    // --- Дымка: крупные пласты, вытянутые по горизонтали, плывут в стороны ---
    float fogA = fbm(vec2(p.x * 0.85 + t * 0.012, p.y * 2.1 - t * 0.002));
    float fogB = fbm(vec2(p.x * 1.6 - t * 0.017, p.y * 3.4 + 4.0));
    float fog = fogA * 0.62 + fogB * 0.38;
    // гуще всего у горизонта, к верху редеет, у самого низа стелется по «полу»
    float fogBand = exp(-dh * dh * 14.0) + 0.35 * smoothstep(0.0, 0.45, dh);
    float haze = smoothstep(0.36, 0.78, fog) * (0.30 + 0.70 * fogBand);
    col = mix(col, silver * 0.60, haze * 0.28 * intensity);

    // --- Лучи: высокий источник далеко слева вверху, лучи почти параллельны ---
    vec2 src = vec2(-0.95 * aspect, -2.2);
    vec2 d = p - src;
    float ang = atan(d.x, d.y);                      // угол луча
    float dist = length(d);
    // широкие мягкие полосы (две частоты) и медленное «дыхание» каждой
    float beams = smoothstep(0.42, 0.88, noise(vec2(ang * 15.0 + 2.0, t * 0.020)));
    beams = beams * 0.75 + 0.25 * smoothstep(0.50, 0.92, noise(vec2(ang * 37.0 + 11.0, -t * 0.028)));
    // лучи видны в верхних двух третях и гаснут к горизонту, плюс тают у самого верха
    float beamFade = smoothstep(horizon + 0.32, -0.30, p.y) * smoothstep(-0.62, -0.38, p.y);
    // и только в полосе, куда «светит окно»
    beamFade *= smoothstep(0.30, 0.12, abs(ang - 0.60));
    float shaft = beams * beamFade * (0.45 + 0.85 * fog);
    col += silver * shaft * 0.100 * intensity;
    // мягкое свечение там, откуда падает свет (у верхнего левого края)
    vec2 b = (p - vec2(-0.42 * aspect, -0.62)) * vec2(0.7, 1.3);
    col += silver * exp(-dot(b, b) * 3.0) * 0.055 * intensity;

    // --- Свечение горизонта: широкий ореол и мягкая светлая полоса ---
    float spread = exp(-p.x * p.x * 0.65);
    float glow = exp(-dh * dh * 26.0) * 0.065 + exp(-dh * dh * 260.0) * 0.040;
    col += mix(silver, ice, 0.65) * glow * spread * (0.9 + 0.1 * sin(t * 0.19)) * intensity;
    // отражение ореола на «полу»
    col += ice * 0.030 * smoothstep(-0.02, 0.06, dh) * exp(-max(dh, 0.0) * 4.0) * spread * intensity;

    // --- Пылинки: редкие, мягкие, медленно всплывают; ярче в лучах ---
    vec2 g = (p + vec2(t * 0.004, t * 0.009)) * 16.0;
    vec2 id = floor(g);
    vec2 f = fract(g) - 0.5;
    float h = hash(id);
    vec2 off = (vec2(hash(id + 3.1), hash(id + 7.7)) - 0.5) * 0.6;
    float mote = smoothstep(0.085, 0.0, length(f - off)) * step(0.88, h);
    mote *= 0.55 + 0.45 * sin(t * (0.35 + h * 0.9) + h * 40.0);
    col += silver * mote * (0.02 + 0.60 * shaft + 0.08 * haze) * 0.55 * intensity;

    // Мягкая виньетка: края чуть темнее, без провала в черноту
    float vig = smoothstep(1.40, 0.30, length(p * vec2(0.78, 1.05)));
    col *= mix(0.74, 1.0, vig);

    // Затемнение переднего плана (dimmed): слабее сверху, заметнее к низу и к краям.
    // Считается здесь, а не полупрозрачными градиентами поверх: так нет полос
    // и лишних проходов заливки.
    float edge = smoothstep(0.30, 0.0, min(uv.x, 1.0 - uv.x));
    col *= 1.0 - dim * (0.14 + 0.24 * smoothstep(0.35, 1.0, uv.y) + 0.14 * edge);

    // Дизеринг против полос на плавных градиентах
    col += (hash(uv * resolution + fract(t)) - 0.5) / 255.0;

    fragColor = vec4(clamp(col, 0.0, 1.0), 1.0) * qt_Opacity;
}
