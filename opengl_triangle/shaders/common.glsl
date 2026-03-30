#version 330 core
#define COMMON_INCLUDED
out vec4 fragColor;
uniform sampler2D iChannel0;
uniform float iTime;
uniform vec2 iResolution;
uniform vec4 iMouse;

// Globals
float g_ar;
vec2 g_mouse;
vec3 g_camDir, g_camPos, g_camRight, g_camUp;
vec2 g_triP1, g_triP2, g_triP3;
vec3 g_triColor;
float g_triArea, g_globalEdgeDist;

// Constants
const int NUM_HOUSES = 4;
const int NUM_CUBES = 5;
const int NUM_ARCHES = 3;
const int TRI_SIZE = 19;
const int CORNER_SIZE = 8;

// Utilities
float hash(float n) { return fract(sin(n) * 43758.5453123); }
float getLuminance(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

// Triangle functions
float pointToSegmentDistance(vec2 p, vec2 a, vec2 b) {
    vec2 ps = vec2(p.x * g_ar, p.y);
    vec2 as = vec2(a.x * g_ar, a.y);
    vec2 bs = vec2(b.x * g_ar, b.y);
    vec2 ab = bs - as, ap = ps - as;
    float t = clamp(dot(ap, ab) / dot(ab, ab), 0.0, 1.0);
    return distance(ps, as + t * ab);
}

bool pointInTriangle(vec2 uv, vec2 p1, vec2 p2, vec2 p3) {
    float e1 = (p1.x - p2.x) * (uv.y - p2.y) - (p1.y - p2.y) * (uv.x - p2.x);
    float e2 = (p2.x - p3.x) * (uv.y - p3.y) - (p2.y - p3.y) * (uv.x - p3.x);
    float e3 = (p3.x - p1.x) * (uv.y - p1.y) - (p3.y - p1.y) * (uv.x - p1.x);
    return (e1 <= 0.0 && e2 <= 0.0 && e3 <= 0.0) || (e1 >= 0.0 && e2 >= 0.0 && e3 >= 0.0);
}

float triangleArea(vec2 p1, vec2 p2, vec2 p3) {
    return 0.5 * abs((p2.x - p1.x) * g_ar * (p3.y - p1.y) - (p3.x - p1.x) * g_ar * (p2.y - p1.y));
}

float distanceToTriangleVertices(vec2 uv, vec2 p1, vec2 p2, vec2 p3) {
    vec2 uvS = vec2(uv.x * g_ar, uv.y);
    return min(distance(uvS, vec2(p1.x * g_ar, p1.y)),
           min(distance(uvS, vec2(p2.x * g_ar, p2.y)),
               distance(uvS, vec2(p3.x * g_ar, p3.y))));
}

float minEdgeDist(vec2 uv, vec2 p1, vec2 p2, vec2 p3) {
    return min(pointToSegmentDistance(uv, p1, p2),
           min(pointToSegmentDistance(uv, p2, p3),
               pointToSegmentDistance(uv, p3, p1)));
}

vec3 getBarycentricCoords(vec2 p, vec2 a, vec2 b, vec2 c) {
    vec2 v0 = c - a, v1 = b - a, v2 = p - a;
    float dot00 = dot(v0, v0), dot01 = dot(v0, v1);
    float dot02 = dot(v0, v2), dot11 = dot(v1, v1), dot12 = dot(v1, v2);
    float invDenom = 1.0 / (dot00 * dot11 - dot01 * dot01);
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;
    return vec3(1.0 - u - v, v, u);
}

vec2 rotate(vec2 pos) {
    vec2 p = pos - 0.5;
    float angle = g_mouse.x * 12.566370614 + (iMouse.z > 0.0 ? sin(iTime * 2.0) * 0.04 : 0.0);
    return vec2(p.x * cos(angle) - p.y * sin(angle), p.x * sin(angle) + p.y * cos(angle)) + 0.5;
}

// SDF primitives
float sdBoxStandard(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdBox(vec3 p, vec3 b, vec2 uv) {
    float areaFactor = clamp(1.0 / (g_triArea + 0.005), 1.0, 80.0);
    float normalizedArea = smoothstep(0.0, 40.0, areaFactor);
    float screenDiag = sqrt(1.0 + g_ar * g_ar);
    float centerProximity = pow(clamp(g_globalEdgeDist / (screenDiag * 0.15), 0.0, 1.0), 0.125);
    
    vec3 pPushed = p + g_camDir * (1.0 - centerProximity) * 35.0;
    float scaleAmount = clamp(0.3 + centerProximity * 0.32, 0.001, 1.0);
    
    float colorIntensity = (pow(g_triColor.r, 0.5) * 2.0 + pow(g_triColor.g, 0.5) * 1.8 + pow(g_triColor.b, 0.5) * 1.9) / 3.0;
    float strength = 0.1 + normalizedArea * 0.3 + colorIntensity * 0.1;
    
    vec2 triCenter = (g_triP1 + g_triP2 + g_triP3) / 3.0;
    float warpFactor = length(uv - triCenter) * (1.0 - centerProximity) * strength;
    
    vec3 finalB = max(b * scaleAmount * (1.0 - warpFactor * 0.5), b * 0.05);
    vec3 camP = vec3(dot(pPushed, g_camRight), dot(pPushed, g_camUp), dot(pPushed, g_camDir));
    vec3 q = abs(camP) - finalB;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdHouse(vec3 p, float size, vec2 uv) {
    float wH = size * 0.8, wW = size * 0.7, wD = size * 0.6;
    float walls = sdBox(p - vec3(0.0, wH * 0.5, 0.0), vec3(wW, wH * 0.5, wD), uv);
    
    vec3 roofP = p - vec3(0.0, wH + size * 0.125, 0.0);
    float ca = 0.8253, sa = 0.5646;
    
    vec3 rp1 = roofP - vec3(-size * 0.3, 0.0, 0.0);
    float roof1 = sdBox(vec3(rp1.x * ca - rp1.y * sa, rp1.x * sa + rp1.y * ca, rp1.z), 
                        vec3(size * 0.45, size * 0.08, wD * 1.15), uv);
    vec3 rp2 = roofP - vec3(size * 0.3, 0.0, 0.0);
    float roof2 = sdBox(vec3(rp2.x * ca + rp2.y * sa, -rp2.x * sa + rp2.y * ca, rp2.z), 
                        vec3(size * 0.45, size * 0.08, wD * 1.15), uv);
    
    float chimney = sdBox(p - vec3(size * 0.35, wH + size * 0.3, size * 0.2), vec3(size * 0.12, size * 0.35, size * 0.12), uv);
    float door = sdBox(p - vec3(0.0, wH * 0.4, -wD), vec3(size * 0.18, wH * 0.4, size * 0.15), uv);
    float windows = min(sdBox(p - vec3(size * 0.35, wH * 0.6, -wD), vec3(size * 0.12, size * 0.12, size * 0.15), uv),
                    min(sdBox(p - vec3(-size * 0.35, wH * 0.6, -wD), vec3(size * 0.12, size * 0.12, size * 0.15), uv),
                        sdBox(p - vec3(wW, wH * 0.6, 0.0), vec3(size * 0.15, size * 0.12, size * 0.12), uv)));
    
    return min(min(roof1, roof2), min(chimney, max(max(walls, -door), -windows)));
}

// Scene
vec2 objec(vec3 p, vec2 uv) {
    float ground = sin(p.x * 0.5) * 2.0 + sin(p.z * 0.5) * 2.0 + p.y + 50.0;
    float sky = 90.0 - p.y + sin(p.x * 0.3) * 3.0 + sin(p.z * 0.4) * 3.0 + sin(p.x * 0.7 + p.z * 0.5) * 2.0;
    
    float minDist = ground, objectID = 1.0;
    if (sky < minDist) { minDist = sky; objectID = 2.0; }
    
    float baseSize = 5.0 * clamp(g_triArea * 10.0, 0.3, 2.0);
    const float zBase = 20.0, spreadX = 100.0, spreadZ = 50.0;
    
    for (int i = 0; i < NUM_HOUSES; i++) {
        float fi = float(i) * 17.31 + 24691.2;
        vec3 hPos = vec3((float(i) - 1.5) * 25.0 + (hash(fi) - 0.5) * 10.0, 0.0, zBase + hash(fi * 2.4) * 15.0);
        float hSize = max(baseSize * (0.9 + hash(fi * 3.5) * 0.3) * 3.0, 8.0);
        float ang = hash(fi * 5.1) * 6.28;
        vec3 hp = p - hPos;
        hp.xz = vec2(hp.x * cos(ang) - hp.z * sin(ang), hp.x * sin(ang) + hp.z * cos(ang));
        float d = sdHouse(hp, hSize, uv);
        if (d < minDist) { minDist = d; objectID = 0.0; }
    }
    
    for (int i = 0; i < NUM_CUBES; i++) {
        float fi = float(i) * 19.43 + 36923.4;
        vec3 cPos = vec3((hash(fi) - 0.5) * spreadX, hash(fi * 1.5) * 25.0 + 8.0, zBase + (hash(fi * 2.5) - 0.5) * spreadZ);
        float cSize = baseSize * (0.6 + hash(fi * 3.7) * 0.8);
        vec3 cp = p - cPos;
        float a1 = hash(fi * 1.7) * 6.28, a2 = hash(fi * 2.3) * 6.28;
        cp.xy = vec2(cp.x * cos(a1) - cp.y * sin(a1), cp.x * sin(a1) + cp.y * cos(a1));
        cp.xz = vec2(cp.x * cos(a2) - cp.z * sin(a2), cp.x * sin(a2) + cp.z * cos(a2));
        float d = sdBox(cp, vec3(cSize), uv);
        if (d < minDist) { minDist = d; objectID = 0.0; }
    }
    
    for (int i = 0; i < NUM_ARCHES; i++) {
        float fi = float(i) * 27.83 + 48571.6;
        vec3 aPos = vec3((hash(fi) - 0.5) * spreadX, 0.0, zBase + (hash(fi * 2.1) - 0.5) * spreadZ);
        float aSize = baseSize * (0.8 + hash(fi * 3.5) * 0.4) * 1.5;
        vec3 ap = p - aPos;
        float ang = hash(fi * 2.8) * 3.14159;
        ap.xz = vec2(ap.x * cos(ang) - ap.z * sin(ang), ap.x * sin(ang) + ap.z * cos(ang));
        float d = min(sdBox(ap - vec3(-aSize, aSize * 0.8, 0.0), vec3(aSize * 0.2, aSize * 0.8, aSize * 0.2), uv),
                  min(sdBox(ap - vec3(aSize, aSize * 0.8, 0.0), vec3(aSize * 0.2, aSize * 0.8, aSize * 0.2), uv),
                      sdBox(ap - vec3(0.0, aSize * 1.8, 0.0), vec3(aSize * 1.4, aSize * 0.2, aSize * 0.25), uv)));
        if (d < minDist) { minDist = d; objectID = 0.0; }
    }
    
    return vec2(minDist, objectID);
}