#ifndef SDF_SHAPES_H
#define SDF_SHAPES_H

// ============== SDF PRIMITIVES ==============
// All shapes use barycentric-based scaling from sdHouse
// Usage: float d = sdShape(p, ..., uv);
// Example: float d = sdSphere(p - position, 1.0, uv);

// Basic shapes
float sdSphere(vec3 p, float r, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    return length(p) - r * scale;
}
// Example: float d = sdSphere(hp - vec3(0,2,0), 2.0, uv);

float sdTorus(vec3 p, vec2 t, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    vec2 q = vec2(length(p.xz) - t.x * scale, p.y);
    return length(q) - t.y * scale;
}
// Example: float d = sdTorus(hp, vec2(1.0, 0.3), uv); // ring/torus

float sdCylinder(vec3 p, float h, float r, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r * scale, h * scale);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}
// Example: float d = sdCylinder(hp, 2.0, 0.5, uv); // cylinder

float sdCone(vec3 p, vec2 c, float h, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    float q = length(p.xz);
    return max(dot(c.xy, vec2(q, p.y)), -h * scale - p.y);
}
// Example: float d = sdCone(hp, vec2(0.8, 0.6), 2.0, uv); // cone

float sdCapsule(vec3 p, vec3 a, vec3 b, float r, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    vec3 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - r * scale;
}
// Example: float d = sdCapsule(hp, vec3(0,0,0), vec3(0,2,0), 0.3, uv);

float sdPyramid(vec3 p, float h, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    float m2 = h * h + 0.25;
    p.xz = abs(p.xz);
    p.xz = (p.z > p.x) ? p.zx : p.xz;
    p.xz -= 0.5;
    vec3 q = vec3(p.z, h * p.y - 0.5 * p.x, h * p.x + 0.5 * p.y);
    float s = max(-q.x, 0.0);
    float t = clamp((q.y - 0.5 * p.z) / (m2 + 0.25), 0.0, 1.0);
    float a = m2 * (q.x + s) * (q.x + s) + q.y * q.y;
    float b = m2 * (q.x + 0.5 * t) * (q.x + 0.5 * t) + (q.y - m2 * t) * (q.y - m2 * t);
    float d2 = min(q.y, -q.x * m2 - 0.5 * q.y) > 0.0 ? 0.0 : min(a, b);
    return sqrt((d2 + q.z * q.z) / m2) * sign(max(q.z, -p.y)) * scale;
}
// Example: float d = sdPyramid(hp, 3.0, uv); // square pyramid

// Polyhedrons
float sdOctahedron(vec3 p, float s, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    p = abs(p);
    float m = p.x + p.y + p.z - s * scale;
    vec3 q;
    if (3.0 * p.x < m) q = p.xyz;
    else if (3.0 * p.y < m) q = p.yzx;
    else if (3.0 * p.z < m) q = p.zxy;
    else return m * 0.57735027;
    float k = clamp(0.5 * (q.z - q.y + s * scale), 0.0, s * scale);
    return length(vec3(q.x, q.y - s * scale + k, q.z - k));
}
// Example: float d = sdOctahedron(hp, 2.0, uv); // 8-sided diamond

float sdHexPrism(vec3 p, vec2 h, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    const vec3 k = vec3(-0.8660254, 0.5, 0.57735);
    p = abs(p);
    p.xy -= 2.0 * min(dot(k.xy, p.xy), 0.0) * k.xy;
    vec2 d = vec2(length(p.xy - vec2(clamp(p.x, -k.z * h.x, k.z * h.x), h.x)) * sign(p.y - h.x), p.z - h.y * scale);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}
// Example: float d = sdHexPrism(hp, vec2(1.0, 0.5), uv); // hexagon

float sdDiamond(vec3 p, vec3 b, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    vec3 q = abs(p);
    float m = q.x + q.y + q.z - b.x * scale;
    vec3 r = 3.0 * q - m;
    r = (r < 0.0) ? vec3(0.0) : r;
    r = (q.x > q.y && q.x > q.z) ? vec3(r.x, 0.0, 0.0) : r;
    r = (q.y > q.x && q.y > q.z) ? vec3(0.0, r.y, 0.0) : r;
    r = (q.z > q.x && q.z > q.y) ? vec3(0.0, 0.0, r.z) : r;
    r = (r < 0.0) ? vec3(0.0) : r;
    float k = clamp(0.5 * (r.z - r.y + b.x * scale), 0.0, b.x * scale);
    vec3 d = vec3(r.x, r.y - b.x * scale + k, r.z - k);
    return length(d) * 0.57735027;
}
// Example: float d = sdDiamond(hp, vec3(1.5, 1.0, 1.0), uv); // diamond shape

// Modified boxes
float sdRoundedBox(vec3 p, vec3 b, float r, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    vec3 q = abs(p) - b * scale;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r * scale;
}
// Example: float d = sdRoundedBox(hp, vec3(1,1,1), 0.2, uv); // rounded edges box

float sdBentBox(vec3 p, vec3 b, float k, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    float c = cos(k * p.x * scale);
    float s = sin(k * p.x * scale);
    mat2 m = mat2(c, -s, s, c);
    p.xy = m * p.xy;
    vec3 q = abs(p) - b * scale;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}
// Example: float d = sdBentBox(hp, vec3(1,1,1), 0.5, uv); // bent/curved box

float sdCross(vec3 p, vec3 b, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    p = abs(p);
    p.xy = (p.y > p.x) ? p.yx : p.xy;
    p.xz = (p.z > p.x) ? p.zx : p.xz;
    p.xy -= b.x * scale;
    vec3 d = abs(p) - b.yzx * scale;
    return min(min(length(p.xy), length(p.xz)), length(p.z)) - b.w * scale;
}
// Example: float d = sdCross(hp, vec3(1, 0.3, 0.2), uv); // + cross shape

// Organic shapes
float sdApple(vec3 p, float r, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    p.y -= 0.3 * r * scale;
    float d = length(p) - r * scale;
    float indent = length(p - vec3(0.0, 0.5 * r * scale, 0.0)) - 0.2 * r * scale;
    return max(d, -indent);
}
// Example: float d = sdApple(hp, 1.5, uv); // apple shape

float sdGourd(vec3 p, float r, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    float sy = p.y * 0.8;
    float d1 = length(p.xz) - r * scale * (1.0 - 0.5 * sy * sy);
    float d2 = abs(p.y) - 1.5 * r * scale;
    return max(d1, d2);
}
// Example: float d = sdGourd(hp, 1.0, uv); // gourd/bottle shape

float sdSkull(vec3 p, float r, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    float d = length(p) - r * scale;
    float jaw = length(p - vec3(0.0, -0.4 * r * scale, 0.0)) - 0.6 * r * scale;
    float eyes = min(
        length(p - vec3(0.3, 0.2, 0.6) * r * scale) - 0.15 * r * scale,
        length(p - vec3(-0.3, 0.2, 0.6) * r * scale) - 0.15 * r * scale);
    return max(max(d, -jaw), -eyes);
}
// Example: float d = sdSkull(hp, 2.0, uv); // skull with eye sockets

float sdCup(vec3 p, float r, float h, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    float body = length(vec2(length(p.xz), p.y + h * 0.3 * scale)) - r * scale;
    float bottom = p.y + h * 0.5 * scale;
    return max(body, bottom);
}
// Example: float d = sdCup(hp, 1.0, 2.0, uv); // drinking cup

// Tubes and pipes
float sdPipe(vec3 p, float r, float t, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    float d = abs(length(p.xz) - r * scale) - t * scale;
    vec2 w = vec2(d, abs(p.y) - t * scale);
    return min(max(w.x, w.y), 0.0) + length(max(w, 0.0));
}
// Example: float d = sdPipe(hp, 1.0, 0.2, uv); // pipe with wall thickness

float sdHelix(vec3 p, float r, float t, float h, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    vec2 q = vec2(length(p.xz) - r * scale, p.y);
    float angle = atan(p.z, p.x);
    float k = h * angle / 6.28318;
    q.y -= k;
    return length(q) - t * scale;
}
// Example: float d = sdHelix(hp, 1.0, 0.2, 0.5, uv); // spring/helix

float sdPipeOrgan(vec3 p, vec3 start, vec3 end, float r, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    vec3 pa = p - start, ba = end - start;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - r * scale;
}
// Example: float d = sdPipeOrgan(hp, vec3(0,0,0), vec3(0,5,0), 0.3, uv);

float sdEllipsoid(vec3 p, vec3 r, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    vec3 scaledR = r * scale;
    float k0 = length(p / scaledR);
    float k1 = length(p / (scaledR * scaledR));
    return k0 * (k0 - 1.0) / k1;
}
// Example: float d = sdEllipsoid(hp, vec3(1.5, 1.0, 0.7), uv); // egg shape

float sdCappedCylinder(vec3 p, float h, float r, vec2 uv) {
    vec3 bary = getBarycentricCoords(uv, g_triP1, g_triP2, g_triP3);
    bary = max(bary, 0.0);
    bary /= max(bary.x + bary.y + bary.z, 1e-5);
    float maxBary = max(bary.x, max(bary.y, bary.z));
    float baryFactor = 1.0 - smoothstep(0.0, 1.0, maxBary);
    float scale = 0.0 + baryFactor * 0.75;
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r * scale, h * scale);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}
// Example: float d = sdCappedCylinder(hp, 1.0, 0.5, uv); // capped cylinder

#endif