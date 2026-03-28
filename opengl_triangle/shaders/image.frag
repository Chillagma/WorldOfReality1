#version 330 core
out vec4 fragColor;
uniform sampler2D uChannel0;
uniform float iTime;
uniform vec2 iResolution;
uniform vec4 iMouse;

float side(vec2 A, vec2 B, vec2 P) {
    return (B.x - A.x) * (P.y - A.y) 
         - (B.y - A.y) * (P.x - A.x);
}

float triangleArea(vec2 p1, vec2 p2, vec2 p3) {
    float ar = iResolution.x / iResolution.y;
    return 0.5 * abs((p2.x * ar - p1.x * ar) * (p3.y - p1.y) 
                    - (p3.x * ar - p1.x * ar) * (p2.y - p1.y));
}
// Add this helper function near the top with other utility functions
float distanceToTriangleVertices(vec2 uv, vec2 p1, vec2 p2, vec2 p3) {
    float ar = iResolution.x / iResolution.y;
    vec2 uvScaled = vec2(uv.x * ar, uv.y);
    vec2 p1Scaled = vec2(p1.x * ar, p1.y);
    vec2 p2Scaled = vec2(p2.x * ar, p2.y);
    vec2 p3Scaled = vec2(p3.x * ar, p3.y);
    
    float d1 = distance(uvScaled, p1Scaled);
    float d2 = distance(uvScaled, p2Scaled);
    float d3 = distance(uvScaled, p3Scaled);
    
    return min(d1, min(d2, d3));
}
float distanceToScreenEdge(vec2 uv) {
    float left   = uv.x;
    float right  = 1.0 - uv.x;
    float bottom = uv.y;
    float top    = 1.0 - uv.y;
    
    return min(min(left, right), min(bottom, top));
}
float edgeAlignment(vec2 a, vec2 b) {
    vec2 d = normalize(b - a);
    
    // horizontal alignment = y is small
    float horizontal = 1.0 - abs(d.y);
    
    // vertical alignment = x is small
    float vertical = 1.0 - abs(d.x);
    
    // return strongest alignment
    return max(horizontal, vertical);
}
float getLuminance(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

float arDistance(vec2 a, vec2 b) {
    float ar = iResolution.x / iResolution.y;
    vec2 d = vec2((b.x - a.x) * ar, b.y - a.y);
    return length(d);
}

float pointToSegmentDistance(vec2 p, vec2 a, vec2 b) {
    float ar = iResolution.x / iResolution.y;
    vec2 ps = vec2(p.x * ar, p.y);
    vec2 as = vec2(a.x * ar, a.y);
    vec2 bs = vec2(b.x * ar, b.y);
    vec2 ab = bs - as;
    vec2 ap = ps - as;
    float t = clamp(dot(ap, ab) / dot(ab, ab), 0.0, 1.0);
    vec2 closest = as + t * ab;
    return distance(ps, closest);
}

float distanceToTriangleEdges(vec2 uv, vec2 p1, vec2 p2, vec2 p3) {
    float d1 = pointToSegmentDistance(uv, p1, p2);
    float d2 = pointToSegmentDistance(uv, p2, p3);
    float d3 = pointToSegmentDistance(uv, p3, p1);
    return min(d1, min(d2, d3));
}

bool g_areaColour = true;
bool g_sphereGoesForward = true;

// ==========================================
// GLOBAL TRIANGLE INFO FOR WARPING
// ==========================================
vec2 g_triP1 = vec2(0.0, 0.0);
vec2 g_triP2 = vec2(1.0, 0.0);
vec2 g_triP3 = vec2(0.5, 1.0);
vec3 g_triColor = vec3(0.5);
float g_triArea = 0.1;
float g_globalEdgeDist = 1000.0;

bool pointInTriangle(vec2 uv, vec2 p1, vec2 p2, vec2 p3) {
    float e1 = (p1.x - p2.x) * (uv.y - p2.y) - (p1.y - p2.y) * (uv.x - p2.x);
    float e2 = (p2.x - p3.x) * (uv.y - p3.y) - (p2.y - p3.y) * (uv.x - p3.x);
    float e3 = (p3.x - p1.x) * (uv.y - p1.y) - (p3.y - p1.y) * (uv.x - p1.x);
    
    return (e1 <= 0.0 && e2 <= 0.0 && e3 <= 0.0) ||
           (e1 >= 0.0 && e2 >= 0.0 && e3 >= 0.0);
}

float hash(float n) {
    return fract(sin(n) * 43758.5453123);
}

const int NUM_HOUSES = 4;
const int NUM_CUBES = 5;
const int NUM_ARCHES = 3;

// ==========================================
// STANDARD BOX SDF (no warping)
// ==========================================
float sdBoxStandard(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

// ==========================================
// TRIANGLE-RELATIVE PERSPECTIVE BOX
// Geometry RECEDES near triangle edges
// ==========================================
float sdBoxTriangle(vec3 p, vec3 b, vec2 fragCoord, vec2 triP1, vec2 triP2, vec2 triP3, vec3 triangleColor) {
    vec3 camDir = texture(uChannel0, (vec2(1., 0.) + .5) / iResolution.xy).xyz;
    vec3 camPos = texture(uChannel0, (vec2(2., 0.) + .5) / iResolution.xy).xyz;
    
    if (length(camDir) < 0.001) camDir = vec3(0.0, 0.0, 1.0);
    camDir = normalize(camDir);
    
    vec3 camRight = normalize(vec3(camDir.z, 0.0, -camDir.x));
    if (length(camRight) < 0.001) camRight = vec3(1.0, 0.0, 0.0);
    vec3 camUp = cross(camDir, camRight);
    
    vec2 uv = fragCoord / iResolution.xy;
    
    // ==========================================
    // TRIANGLE METRICS
    // ==========================================
    
    float ar = iResolution.x / iResolution.y;
    float triArea = 0.5 * abs(
        (triP2.x * ar - triP1.x * ar) * (triP3.y - triP1.y) - 
        (triP3.x * ar - triP1.x * ar) * (triP2.y - triP1.y)
    );
    
    float areaFactor = 1.0 / (triArea + 0.005);
    areaFactor = clamp(areaFactor, 1.0, 80.0);
    
    float normalizedArea = smoothstep(0.0, 40.0, areaFactor);
    
    // ==========================================
    // GLOBAL EDGE DISTANCE
    // ==========================================
    
    float minEdgeDist = g_globalEdgeDist;
    
    float screenDiagonal = sqrt(1.0 + ar * ar);
    float normalizedEdgeDist = clamp(minEdgeDist / (screenDiagonal * 0.15), 0.0, 1.0);
    
    // INVERTED: centerProximity is HIGH at center, LOW at edges
    float centerProximity = normalizedEdgeDist;
    centerProximity = pow(centerProximity, 0.125);
    
    // ==========================================
    // DEPTH PUSH - Push geometry TOWARD camera at center, AWAY at edges
    // ==========================================
    
    float maxDepthPush = 35.0;
    // At center (centerProximity=1): no push
    // At edges (centerProximity=0): push away
    float depthPush = (1.0 - centerProximity) * maxDepthPush;
    
    vec3 pPushed = p + camDir * depthPush;
    
    // ==========================================
    // SCALE geometry - BIGGER at center, SMALLER at edges
    // ==========================================
    
    float scaleAmount = 0.3 + centerProximity * 0.32;
    scaleAmount = clamp(scaleAmount, 0.001, 1.0);
    vec3 scaledB = b * scaleAmount;
    
    // ==========================================
    // BARYCENTRIC POSITION IN TRIANGLE
    // ==========================================
    
    vec2 v0 = triP3 - triP1;
    vec2 v1 = triP2 - triP1;
    vec2 v2 = uv - triP1;
    
    float dot00 = dot(v0, v0);
    float dot01 = dot(v0, v1);
    float dot02 = dot(v0, v2);
    float dot11 = dot(v1, v1);
    float dot12 = dot(v1, v2);
    
    float invDenom = 1.0 / (dot00 * dot11 - dot01 * dot01 + 0.0001);
    float baryU = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float baryV = (dot00 * dot12 - dot01 * dot02) * invDenom;
    float baryW = 1.0 - baryU - baryV;
    
    float baryDist = length(vec3(baryU, baryV, baryW) - vec3(0.333));
    
    // ==========================================
    // COLOR INFLUENCE
    // ==========================================
    
    float r = pow(triangleColor.r, 0.5) * 2.0;
    float g = pow(triangleColor.g, 0.5) * 1.8;
    float bl = pow(triangleColor.b, 0.5) * 1.9;
    
    float colorIntensity = (r + g + bl) / 3.0;
    
    // ==========================================
    // ADDITIONAL WARPING
    // ==========================================
    
    float baseStrength = 0.1 + normalizedArea * 0.3;
    float colorStrength = colorIntensity * 0.1;
    float strength = baseStrength + colorStrength;
    
    vec2 triCenter = (triP1 + triP2 + triP3) / 3.0;
    vec2 relPos = uv - triCenter;
    
    float warpFactor = length(relPos) * (1.0 - centerProximity) * strength;
    
    vec3 finalB = scaledB * (1.0 - warpFactor * 0.5);
    finalB = max(finalB, b * 0.05);
    
    // ==========================================
    // CAMERA-SPACE SDF
    // ==========================================
    
    vec3 camP = vec3(
        dot(pPushed, camRight),
        dot(pPushed, camUp),
        dot(pPushed, camDir)
    );
    
    vec3 q = abs(camP) - finalB;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

// ==========================================
// WRAPPER: Uses global triangle info
// ==========================================
float sdBox(vec3 p, vec3 b, vec2 fragCoord) {
    return sdBoxTriangle(p, b, fragCoord, g_triP1, g_triP2, g_triP3, g_triColor);
}

// ==========================================
// SIMPLE OVERLOAD (no perspective)
// ==========================================
float sdBox(vec3 p, vec3 b) {
    return sdBoxStandard(p, b);
}

// ==========================================
// HOUSE - Made entirely of boxes (all warped)
// ==========================================
float sdHouse(vec3 p, float size, vec2 fragCoord) {
    float wallHeight = size * 0.8;
    float wallWidth = size * 0.7;
    float wallDepth = size * 0.6;
    float walls = sdBox(p - vec3(0.0, wallHeight * 0.5, 0.0), vec3(wallWidth, wallHeight * 0.5, wallDepth), fragCoord);
    
    float roofHeight = size * 0.5;
    vec3 roofP = p - vec3(0.0, wallHeight + roofHeight * 0.25, 0.0);
    
    float angle = 0.6;
    float ca = cos(angle);
    float sa = sin(angle);
    
    vec3 rp1 = roofP - vec3(-size * 0.3, 0.0, 0.0);
    vec3 rotated1 = vec3(rp1.x * ca - rp1.y * sa, rp1.x * sa + rp1.y * ca, rp1.z);
    float roof1 = sdBox(rotated1, vec3(size * 0.45, size * 0.08, wallDepth * 1.15), fragCoord);
    
    vec3 rp2 = roofP - vec3(size * 0.3, 0.0, 0.0);
    vec3 rotated2 = vec3(rp2.x * ca + rp2.y * sa, -rp2.x * sa + rp2.y * ca, rp2.z);
    float roof2 = sdBox(rotated2, vec3(size * 0.45, size * 0.08, wallDepth * 1.15), fragCoord);
    
    float roof = min(roof1, roof2);
    
    vec3 chimP = p - vec3(size * 0.35, wallHeight + roofHeight * 0.6, size * 0.2);
    float chimney = sdBox(chimP, vec3(size * 0.12, size * 0.35, size * 0.12), fragCoord);
    
    vec3 doorP = p - vec3(0.0, wallHeight * 0.4, -wallDepth);
    float door = sdBox(doorP, vec3(size * 0.18, wallHeight * 0.4, size * 0.15), fragCoord);
    
    vec3 winP1 = p - vec3(size * 0.35, wallHeight * 0.6, -wallDepth);
    float window1 = sdBox(winP1, vec3(size * 0.12, size * 0.12, size * 0.15), fragCoord);
    
    vec3 winP2 = p - vec3(-size * 0.35, wallHeight * 0.6, -wallDepth);
    float window2 = sdBox(winP2, vec3(size * 0.12, size * 0.12, size * 0.15), fragCoord);
    
    vec3 winP3 = p - vec3(wallWidth, wallHeight * 0.6, 0.0);
    float window3 = sdBox(winP3, vec3(size * 0.15, size * 0.12, size * 0.12), fragCoord);
    
    float house = walls;
    house = max(house, -door);
    house = max(house, -window1);
    house = max(house, -window2);
    house = max(house, -window3);
    house = min(house, roof);
    house = min(house, chimney);
    
    return house;
}

// ==========================================
// SCENE FUNCTION
// ==========================================
vec2 objec(vec3 p, vec2 fragCoord) {
    float groundTerrain = sin(p.x * 0.5) * 2.0 + sin(p.z * 0.5) * 2.0 + p.y + 50.;
    
    float skyHeight = 90.0;
    float skyTerrain = skyHeight - p.y + sin(p.x * 0.3) * 3.0 + sin(p.z * 0.4) * 3.0 + sin(p.x * 0.7 + p.z * 0.5) * 2.0;
    
    float minDist = groundTerrain;
    float objectID = 1.0;
    
    if (skyTerrain < minDist) {
        minDist = skyTerrain;
        objectID = 2.0;
    }
    
    float baseSeed = 123.456;
    
    float areaInfluence = clamp(g_triArea * 10.0, 0.3, 2.0);
    float baseSize = 5.0 * areaInfluence;
    
    float spreadX = 100.0;
    float spreadY = 25.0;
    float spreadZ = 50.0;
    float zBase = 20.0;
    
    // === HOUSES ===
    for (int i = 0; i < NUM_HOUSES; i++) {
        float fi = float(i) + baseSeed * 200.0 + 1000.0;
        
        float x = (float(i) - float(NUM_HOUSES) * 0.5) * 25.0 + (hash(fi * 17.31) - 0.5) * 10.0;
        float y = 0.0;
        float z = zBase + hash(fi * 41.17) * 15.0;
        
        vec3 housePos = vec3(x, y, z);
        
        float sizeVariation = 0.9 + hash(fi * 61.23) * 0.3;
        float houseSize = baseSize * sizeVariation * 3.0;
        houseSize = max(houseSize, 8.0);
        
        float houseAngle = hash(fi * 88.88) * 6.28;
        vec3 hp = p - housePos;
        float ca = cos(houseAngle);
        float sa = sin(houseAngle);
        hp.xz = vec2(hp.x * ca - hp.z * sa, hp.x * sa + hp.z * ca);
        
        float houseDist = sdHouse(hp, houseSize, fragCoord);
        
        if (houseDist < minDist) {
            minDist = houseDist;
            objectID = 0.0;
        }
    }
    
    // === FLOATING CUBES ===
    for (int i = 0; i < NUM_CUBES; i++) {
        float fi = float(i) + baseSeed * 300.0 + 2000.0;
        
        float x = (hash(fi * 19.43) - 0.5) * spreadX;
        float y = hash(fi * 29.87) * spreadY + 8.0;
        float z = zBase + (hash(fi * 47.61) - 0.5) * spreadZ;
        
        vec3 cubePos = vec3(x, y, z);
        
        float sizeVariation = 0.6 + hash(fi * 71.11) * 0.8;
        float cubeSize = baseSize * sizeVariation;
        
        vec3 cp = p - cubePos;
        
        float angle1 = hash(fi * 33.33) * 6.28;
        float angle2 = hash(fi * 44.44) * 6.28;
        
        float c1 = cos(angle1), s1 = sin(angle1);
        float c2 = cos(angle2), s2 = sin(angle2);
        
        cp.xy = vec2(cp.x * c1 - cp.y * s1, cp.x * s1 + cp.y * c1);
        cp.xz = vec2(cp.x * c2 - cp.z * s2, cp.x * s2 + cp.z * c2);
        
        float cubeDist = sdBox(cp, vec3(cubeSize), fragCoord);
        
        if (cubeDist < minDist) {
            minDist = cubeDist;
            objectID = 0.0;
        }
    }
    
    // === ARCHES ===
    for (int i = 0; i < NUM_ARCHES; i++) {
        float fi = float(i) + baseSeed * 700.0 + 6000.0;
        
        float x = (hash(fi * 27.83) - 0.5) * spreadX;
        float y = 0.0;
        float z = zBase + (hash(fi * 57.91) - 0.5) * spreadZ;
        
        vec3 archPos = vec3(x, y, z);
        
        float sizeVariation = 0.8 + hash(fi * 97.41) * 0.4;
        float archSize = baseSize * sizeVariation * 1.5;
        
        vec3 ap = p - archPos;
        
        float angle = hash(fi * 77.77) * 3.14159;
        float ca = cos(angle), sa = sin(angle);
        ap.xz = vec2(ap.x * ca - ap.z * sa, ap.x * sa + ap.z * ca);
        
        float pillar1 = sdBox(ap - vec3(-archSize, archSize * 0.8, 0.0), 
                              vec3(archSize * 0.2, archSize * 0.8, archSize * 0.2), fragCoord);
        float pillar2 = sdBox(ap - vec3(archSize, archSize * 0.8, 0.0), 
                              vec3(archSize * 0.2, archSize * 0.8, archSize * 0.2), fragCoord);
        float beam = sdBox(ap - vec3(0.0, archSize * 1.8, 0.0), 
                          vec3(archSize * 1.4, archSize * 0.2, archSize * 0.25), fragCoord);
        
        float archDist = min(pillar1, min(pillar2, beam));
        
        if (archDist < minDist) {
            minDist = archDist;
            objectID = 0.0;
        }
    }
    
    return vec2(minDist, objectID);
}

float rand(float x) { 
    return fract(sin(x) * 43758.5453123);
}

vec2 rotate(vec2 mouse, vec2 pos) {
    float x = pos.x - 0.5;
    float y = pos.y - 0.5;
    
    float mx = mouse.x * 3.14159 * 4.;
    float angle = mx;
    
    if (iMouse.z > 0.0) {
        angle += sin(iTime * 2.) / 25.;
    }
    
    float newX = x * cos(angle) - y * sin(angle);
    float newY = x * sin(angle) + y * cos(angle);
    
    return vec2(newX + 0.5, newY + 0.5);
}

float distanceToVisibleEdge(
    vec2 uv, 
    vec2 p1, vec2 p2, vec2 p3,
    int currentTriIndex,
    bool currentIsCorner,
    vec2 tri[57],
    vec2 cornerTri[24],
    int TRI_SIZE,
    int CORNER_SIZE
) {
    float minDist = distanceToTriangleEdges(uv, p1, p2, p3);
    
    if (!currentIsCorner) {
        for (int i = 0; i < TRI_SIZE; i++) {
            if (i > currentTriIndex) {
                vec2 op1 = tri[i * 3 + 0];
                vec2 op2 = tri[i * 3 + 1];
                vec2 op3 = tri[i * 3 + 2];
                
                if (pointInTriangle(uv, op1, op2, op3)) {
                    float d = distanceToTriangleEdges(uv, op1, op2, op3);
                    minDist = min(minDist, d);
                }
            }
        }
    }
    
    if (!currentIsCorner) {
        for (int i = 0; i < CORNER_SIZE; i++) {
            vec2 op1 = cornerTri[i * 3 + 0];
            vec2 op2 = cornerTri[i * 3 + 1];
            vec2 op3 = cornerTri[i * 3 + 2];
            
            if (pointInTriangle(uv, op1, op2, op3)) {
                float d = distanceToTriangleEdges(uv, op1, op2, op3);
                minDist = min(minDist, d);
            }
        }
    } else {
        for (int i = 0; i < CORNER_SIZE; i++) {
            if (i > currentTriIndex) {
                vec2 op1 = cornerTri[i * 3 + 0];
                vec2 op2 = cornerTri[i * 3 + 1];
                vec2 op3 = cornerTri[i * 3 + 2];
                
                if (pointInTriangle(uv, op1, op2, op3)) {
                    float d = distanceToTriangleEdges(uv, op1, op2, op3);
                    minDist = min(minDist, d);
                }
            }
        }
    }
    
    return minDist;
}

void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec3 camDir = texture(uChannel0, (vec2(1., 0.) + .5) / iResolution.xy).xyz;
    vec3 camPos = texture(uChannel0, (vec2(2., 0.) + .5) / iResolution.xy).xyz;
    camPos += vec3(0., 16., -64.);

    vec2 uv = fragCoord / iResolution.xy;
    vec2 uvRay = (2.0 * fragCoord - iResolution.xy) / iResolution.y;

    if (length(camDir) == 0.0) {
        camDir = vec3(0.0, 0.0, 1.0);
    }

    float colour_speed = 2.75;
    vec2 mouse = iMouse.xy / iResolution.xy;

    if (iMouse.x == 0.0 && iMouse.y == 0.0) {
        mouse = vec2(0.5, 0.5);
    }

    vec3 col = vec3(1.0);
    vec3 sphereColor = vec3(0.5);
    
    int hitTriangleIndex = -1;
    bool isCornerTriangle = false;
    bool insideAnyTriangle = false;
    
    vec2 hitP1, hitP2, hitP3;

    vec2 c_bl = vec2(0.0, 0.0);
    vec2 c_br = vec2(1.0, 0.0);
    vec2 c_tr = vec2(1.0, 1.0);
    vec2 c_tl = vec2(0.0, 1.0);
    vec2 c_bm = vec2(0.5, 0.0);
    vec2 c_tm = vec2(0.5, 1.0);
    vec2 c_ml = vec2(0.0, 0.5);
    vec2 c_mr = vec2(1.0, 0.5);
    
    vec2 c_mid1 = (c_bm + c_ml) / 2.0;
    vec2 c_mid2 = (c_br + c_mr) / 2.0;
    vec2 c_mid3 = (c_tr + c_tm) / 2.0;
    vec2 c_mid4 = (c_tm + c_tl) / 2.0;

    vec3 triangleColors[19] = vec3[19](
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, 0.0, 1.0),
        vec3(0.0, 1.0, 1.0),
        vec3(1.0, 1.0, 1.0),
        vec3(1.0, 1.0, 0.0),
        vec3(1.0, 0.0, 1.0),
        vec3(0.9, 0.0, 0.9),
        vec3(0.8, 0.0, 0.8),
        vec3(0.7, 0.0, 0.7),
        vec3(0.0, 0.0, 0.0),
        vec3(0.5, 0.0, 0.5),
        vec3(0.5, 0.5, 0.0),
        vec3(0.0, 0.5, 0.5),
        vec3(1.0, 0.5, 0.0),
        vec3(0.5, 0.25, 0.0),
        vec3(0.8, 0.2, 0.2),
        vec3(0.2, 0.8, 0.4),
        vec3(0.3, 0.3, 0.8)
    );

    const int TRI_SIZE = 19;
    const int CORNER_SIZE = 8;

    vec2 vert_0 = vec2(0.0, 0.0);
    vec2 vert_1 = vec2(0.5, 0.5);
    vec2 vert_2 = vec2(0.25, 0.05);
    vec2 vert_3 = vec2(0.75, 0.25);
    vec2 vert_4 = vec2(0.55, 0.55);
    vec2 vert_7 = vec2(1., 1.);
    vec2 vert_9 = vec2(0.5, 0.15);
    vec2 vert_12 = vec2(0.0, 0.55);
    
    vec2 vert_mag_mid1 = (vert_0 + vert_7) / 2.0;
    vec2 vert_mag_mid2 = (vert_0 + vert_12) / 2.0;
    vec2 vert_mag_mid3 = (vert_7 + vert_12) / 2.0;

    vec2 tri[57] = vec2[57](
        rotate(mouse, vert_1), rotate(mouse, vert_2), rotate(mouse, vert_3),
        rotate(mouse, vert_4), rotate(mouse, vert_1), rotate(mouse, vert_3),
        rotate(mouse, vert_7), rotate(mouse, vert_4), rotate(mouse, vert_3),
        rotate(mouse, vert_1), rotate(mouse, vert_2), rotate(mouse, vert_9),
        rotate(mouse, vert_0), rotate(mouse, vert_2), rotate(mouse, vert_1),
        rotate(mouse, vert_0), rotate(mouse, vert_2), rotate(mouse, vert_12),
        rotate(mouse, vert_0), rotate(mouse, vert_mag_mid1), rotate(mouse, vert_mag_mid2),
        rotate(mouse, vert_mag_mid2), rotate(mouse, vert_mag_mid1), rotate(mouse, vert_12),
        rotate(mouse, vert_mag_mid1), rotate(mouse, vert_7), rotate(mouse, vert_mag_mid3),
        rotate(mouse, vert_mag_mid1), rotate(mouse, vert_mag_mid3), rotate(mouse, vert_12),
        rotate(mouse, c_bm), rotate(mouse, c_br), rotate(mouse, vert_3),
        rotate(mouse, vert_3), rotate(mouse, c_br), rotate(mouse, c_mr),
        rotate(mouse, c_bl), rotate(mouse, c_bm), rotate(mouse, vert_2),
        rotate(mouse, vert_3), rotate(mouse, c_mr), rotate(mouse, vert_7),
        rotate(mouse, vert_7), rotate(mouse, c_tr), rotate(mouse, c_mr),
        rotate(mouse, vert_9), rotate(mouse, c_bm), rotate(mouse, vert_3),
        rotate(mouse, c_tl), rotate(mouse, vert_7), rotate(mouse, c_ml),
        rotate(mouse, vert_2), rotate(mouse, c_bm), rotate(mouse, vert_9),
        rotate(mouse, c_tl), rotate(mouse, c_ml), rotate(mouse, c_bl)
    );

    vec3 cornerColors[8] = vec3[8](
        vec3(0.2, 0.3, 0.5),
        vec3(0.25, 0.35, 0.55),
        vec3(0.5, 0.2, 0.3),
        vec3(0.55, 0.25, 0.35),
        vec3(0.3, 0.5, 0.2),
        vec3(0.35, 0.55, 0.25),
        vec3(0.4, 0.3, 0.4),
        vec3(0.45, 0.35, 0.45)
    );
    
    vec2 cornerTri[24] = vec2[24](
        c_bl, c_bm, c_mid1,
        c_bl, c_mid1, c_ml,
        c_bm, c_br, c_mid2,
        c_bm, c_mid2, c_mr,
        c_mr, c_tr, c_mid3,
        c_mr, c_mid3, c_tm,
        c_ml, c_tm, c_mid4,
        c_ml, c_mid4, c_tl
    );

    // ==========================================
    // COMPUTE GLOBAL EDGE DISTANCE FOR THIS PIXEL
    // ==========================================
    {
        float minDist = 1000.0;
        
        for (int i = 0; i < TRI_SIZE; i++) {
            vec2 p1 = tri[i * 3 + 0];
            vec2 p2 = tri[i * 3 + 1];
            vec2 p3 = tri[i * 3 + 2];
            
            float d1 = pointToSegmentDistance(uv, p1, p2);
            float d2 = pointToSegmentDistance(uv, p2, p3);
            float d3 = pointToSegmentDistance(uv, p3, p1);
            
            minDist = min(minDist, min(d1, min(d2, d3)));
        }
        
        for (int i = 0; i < CORNER_SIZE; i++) {
            vec2 p1 = cornerTri[i * 3 + 0];
            vec2 p2 = cornerTri[i * 3 + 1];
            vec2 p3 = cornerTri[i * 3 + 2];
            
            float d1 = pointToSegmentDistance(uv, p1, p2);
            float d2 = pointToSegmentDistance(uv, p2, p3);
            float d3 = pointToSegmentDistance(uv, p3, p1);
            
            minDist = min(minDist, min(d1, min(d2, d3)));
        }
        
        g_globalEdgeDist = minDist;
    }

    // ==========================================
    // FIND WHICH TRIANGLE WE'RE IN
    // ==========================================
    for (int i = 0; i < TRI_SIZE; i++) {
        vec2 p1 = tri[i * 3 + 0];
        vec2 p2 = tri[i * 3 + 1];
        vec2 p3 = tri[i * 3 + 2];

        if (pointInTriangle(uv, p1, p2, p3)) {
            hitTriangleIndex = i;
            isCornerTriangle = false;
            hitP1 = p1;
            hitP2 = p2;
            hitP3 = p3;
        }
    }
    
    for (int i = 0; i < CORNER_SIZE; i++) {
        vec2 p1 = cornerTri[i * 3 + 0];
        vec2 p2 = cornerTri[i * 3 + 1];
        vec2 p3 = cornerTri[i * 3 + 2];
        
        if (pointInTriangle(uv, p1, p2, p3)) {
            hitTriangleIndex = i;
            isCornerTriangle = true;
            hitP1 = p1;
            hitP2 = p2;
            hitP3 = p3;
        }
    }

    // ==========================================
    // SET GLOBAL TRIANGLE INFO FOR WARPING
    // ==========================================
    if (hitTriangleIndex >= 0) {
        insideAnyTriangle = true;
        
        if (isCornerTriangle) {
            sphereColor = cornerColors[hitTriangleIndex];
        } else {
            sphereColor = triangleColors[hitTriangleIndex];
        }
        
        g_triP1 = hitP1;
        g_triP2 = hitP2;
        g_triP3 = hitP3;
        g_triColor = sphereColor;
        g_triArea = triangleArea(hitP1, hitP2, hitP3);
        
    } else {
        g_triP1 = vec2(0.0, 0.0);
        g_triP2 = vec2(1.0, 0.0);
        g_triP3 = vec2(0.5, 1.0);
        g_triColor = vec3(0.5);
        g_triArea = 0.5;
    }

    mat3 mtx;
    mtx[0] = normalize(vec3(camDir.z, 0., -camDir.x));
    mtx[1] = cross(camDir, mtx[0]);
    mtx[2] = camDir;
    vec3 ray = mtx * normalize(vec3(uvRay, 1.0));

    vec3 p = camPos;
    float l = 0.0;
    float totalDist = 0.0;
    float objectID = 0.0;

    // ==========================================
    // RAYMARCHING
    // ==========================================
    for (int i = 0; i < 256; ++i) {
        vec2 result = objec(p, fragCoord);
        l = result.x;
        objectID = result.y;
        
        if (l < 0.001) break;
        if (totalDist > 150.0) break;
        p += l * ray;
        totalDist += l;
    }

    // ==========================================
    // COMPUTE NORMALS
    // ==========================================
    float s = 1.0 / 64.0;
    vec3 normal = normalize(vec3(
        objec(p + s * vec3(1, 0, 0), fragCoord).x - objec(p - s * vec3(1, 0, 0), fragCoord).x,
        objec(p + s * vec3(0, 1, 0), fragCoord).x - objec(p - s * vec3(0, 1, 0), fragCoord).x,
        objec(p + s * vec3(0, 0, 1), fragCoord).x - objec(p - s * vec3(0, 0, 1), fragCoord).x
    ));

    // ==========================================
    // TRIANGLE COLORING (2D overlay)
    // ==========================================
    for (int i = 0; i < TRI_SIZE; i++) {
        vec2 p1 = tri[i * 3 + 0];
        vec2 p2 = tri[i * 3 + 1];
        vec2 p3 = tri[i * 3 + 2];

        if (pointInTriangle(uv, p1, p2, p3)) {
            float angle = atan(uv.x - p1.x, uv.y - p1.y);
            col = triangleColors[i];

            float dist_top = abs(uv.y - p1.y);
            float dist_left = abs(uv.x - p2.x);
            float dist_right = abs(uv.x - p3.x);
            float angle_left = atan(uv.x - p2.x, uv.y - p2.y);
            float angle_right = atan(uv.x - p3.x, uv.y - p3.y);

            float right_lines = fract(angle_right * 5.3);
            float left_lines = fract(angle_left * 11.3);
            float vert_lines = fract(angle * 6.3);

            for (int j = 0; j < 10; j++) {
                if ((left_lines < 0.05 || left_lines > 0.95)) {
                    col = mix(vec3(1.0), vec3(0.0), (dist_top + floor(float(j) / 2.)));
                }
                if ((right_lines < 0.05 || right_lines > 0.95)) {
                    col = mix(vec3(1.0), vec3(0.0), (dist_top + floor(float(j) / 2.)));
                }
                if ((vert_lines < 0.05 || vert_lines > 0.95)) {
                    col = mix(vec3(1.0), vec3(0.0), (dist_top + floor(float(j) / 2.)));
                }
            }

            for (float k = 0.; k < 2.; k++) {
                col = mix(col, vec3(abs(cos(iTime/colour_speed))), (dist_right - dist_left));
                col = mix(col, vec3(abs(sin(iTime/colour_speed))), (dist_top - dist_left - dist_right));
                col = mix(col / 1.72, vec3(abs(sin(iTime/2.))), (dist_top - dist_right - dist_left / 2.5));
            }
        }
    }

    for (int i = 0; i < CORNER_SIZE; i++) {
        vec2 p1 = cornerTri[i * 3 + 0];
        vec2 p2 = cornerTri[i * 3 + 1];
        vec2 p3 = cornerTri[i * 3 + 2];
        
        if (pointInTriangle(uv, p1, p2, p3)) {
            float angle = atan(uv.x - p1.x, uv.y - p1.y);
            col = cornerColors[i];

            float dist_top = abs(uv.y - p1.y);
            float dist_left = abs(uv.x - p2.x);
            float dist_right = abs(uv.x - p3.x);
            float angle_left = atan(uv.x - p2.x, uv.y - p2.y);
            float angle_right = atan(uv.x - p3.x, uv.y - p3.y);

            float right_lines = fract(angle_right * 5.3);
            float left_lines = fract(angle_left * 11.3);
            float vert_lines = fract(angle * 6.3);

            for (int j = 0; j < 10; j++) {
                if ((left_lines < 0.05 || left_lines > 0.95)) {
                    col = mix(vec3(1.0), vec3(0.0), (dist_top + floor(float(j) / 2.)));
                }
                if ((right_lines < 0.05 || right_lines > 0.95)) {
                    col = mix(vec3(1.0), vec3(0.0), (dist_top + floor(float(j) / 2.)));
                }
                if ((vert_lines < 0.05 || vert_lines > 0.95)) {
                    col = mix(vec3(1.0), vec3(0.0), (dist_top + floor(float(j) / 2.)));
                }
            }

            for (float k = 0.; k < 2.; k++) {
                col = mix(col/0.5, vec3(abs(cos(iTime/colour_speed))), (dist_right - dist_left));
                col = mix(col/1.2, vec3(abs(sin(iTime/colour_speed))), (dist_top - dist_left - dist_right));
                col = mix(col / 1.72, vec3(abs(sin(iTime/colour_speed))), (dist_top - dist_right - dist_left / 2.5));
            }
        }
    }

      // ==========================================
    // FINAL RENDERING
    // ==========================================
    float lighting = dot(normal, normalize(vec3(1.0, 1.0, -1.0))) * 0.5 + 0.5;
    float objectVisible = smoothstep(0.1, 0.01, l);
    
    float triangleLuminance = getLuminance(col);
    float contrastFactor = 1.0 - triangleLuminance;
    
    // ==========================================
    // EDGE AND CORNER DARKENING
    // ==========================================
    float edgeDist = g_globalEdgeDist;
    float cornerDist = 1000.0;
    
    if (hitTriangleIndex >= 0) {
        cornerDist = distanceToTriangleVertices(uv, hitP1, hitP2, hitP3);
    }
    
    // Normalize distances for darkening calculation
    float screenDiagonal = sqrt(1.0 + (iResolution.x / iResolution.y) * (iResolution.x / iResolution.y));
    
    // Edge darkening: closer to edge = darker
    float edgeDarkenRadius = 0.08; // How far from edge the darkening extends
    float edgeDarkenAmount = 1.0 - smoothstep(0.0, edgeDarkenRadius, edgeDist);
    edgeDarkenAmount = pow(edgeDarkenAmount, 1.5); // Make it more gradual
    
    // Corner darkening: closer to corner = darker
    float cornerDarkenRadius = 0.12; // How far from corner the darkening extends
    float cornerDarkenAmount = 1.0 - smoothstep(0.0, cornerDarkenRadius, cornerDist);
    cornerDarkenAmount = pow(cornerDarkenAmount, 1.2);
    
    // Combine edge and corner darkening (take the stronger of the two)
    float totalDarken = max(edgeDarkenAmount, cornerDarkenAmount);
    
    // Darkening strength (0.0 = no darkening, 1.0 = full black at edges/corners)
    float darkenStrength = 0.7;
    float darkenMultiplier = 1.0 - (totalDarken * darkenStrength);
    
    if (objectID > 1.5) {
        vec3 skyTerrainColor = vec3(0.3, 0.4, 0.6) * lighting;
        skyTerrainColor += vec3(0.1, 0.05, 0.15) * sin(p.x * 0.2 + p.z * 0.3);
        
        float brightnessAdjust = (contrastFactor - 0.5) * 0.4;
        skyTerrainColor = skyTerrainColor + brightnessAdjust;
        skyTerrainColor = clamp(skyTerrainColor, 0.0, 1.0);
        
        float rim = 1.0 - max(0.0, dot(normal, -ray));
        rim = pow(rim, 3.0);
        vec3 rimColor = mix(vec3(0.4, 0.5, 0.7), vec3(0.8, 0.85, 1.0), contrastFactor);
        skyTerrainColor = mix(skyTerrainColor * col * 8.0, rimColor * col * 8.0, rim * 0.5);
        
        // Apply edge/corner darkening to geometry
        skyTerrainColor *= darkenMultiplier;
        
        fragColor = vec4(mix(col, skyTerrainColor, objectVisible), 1.0);
        
    } else if (objectID > 0.5) {
        vec3 terrainColor = vec3(0.4, 0.3, 0.2) * lighting;
        
        float brightnessAdjust = (contrastFactor - 0.5) * 0.6;
        terrainColor = terrainColor + brightnessAdjust;
        terrainColor = clamp(terrainColor, 0.0, 1.0);
        
        float rim = 1.0 - max(0.0, dot(normal, -ray));
        rim = pow(rim, 3.0);
        vec3 rimColor = mix(vec3(0.2), vec3(0.8), contrastFactor);
        terrainColor = mix(terrainColor*col*10., rimColor*col*10., rim * 0.4*col*10.);
        
        // Apply edge/corner darkening to geometry
        terrainColor *= darkenMultiplier;
        
        fragColor = vec4(mix(col, terrainColor, objectVisible), 1.0);
        
    } else {
        float brightnessAdjust = (0.5 - triangleLuminance) * 0.5;
        
        vec3 lightDir = normalize(vec3(1.0, 1.0, -1.0));
        float diffuse = max(0.0, dot(normal, lightDir));
        float ambient = 0.3;
        float lightingSphere = ambient + diffuse * 0.7;
        
        vec3 sphereFinal = sphereColor;
        
        sphereFinal = sphereFinal + brightnessAdjust;
        sphereFinal = clamp(sphereFinal, 0.1, 0.9);
        sphereFinal = sphereFinal * lightingSphere;
        
        vec3 viewDir = -ray;
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        sphereFinal += vec3(0.3) * spec;
        
        sphereFinal = clamp(sphereFinal, 0.0, 1.0);
        
        // Apply edge/corner darkening to geometry
        sphereFinal *= darkenMultiplier;
        
        fragColor = vec4(mix(col, sphereFinal, objectVisible), 1.0);
    }
}
