#version 330 core
#define COMMON_INCLUDED
out vec4 fragColor; // The final color we'll draw to the screen
uniform sampler2D iChannel0; // A texture we can read from (like an image)
uniform float iTime; // How many seconds have passed since the shader started
uniform vec2 iResolution; // Width and height of the screen in pixels
uniform vec4 iMouse; // Mouse position and click state

// Globals
float g_ar; // Screen aspect ratio (width divided by height)
vec2 g_mouse; // Mouse position converted to 0-1 range
vec3 g_camDir, g_camPos, g_camRight, g_camUp; // Camera: where it looks, where it is, its right and up directions
vec2 g_triP1, g_triP2, g_triP3; // The three corner points of the current triangle we're inside
vec3 g_triColor; // The color of the current triangle
float g_triArea, g_globalEdgeDist; // How big the triangle is, and how far we are from its edges

// Constants
const int NUM_HOUSES = 5; // SDF houses in a ring around spawn
const int TRI_SIZE = 19; // (unused but defined)
const int CORNER_SIZE = 8; // (unused but defined)

// Utilities
float hash(float n) { return fract(sin(n) * 43758.5453123); } // Random-looking number from any input number
float getLuminance(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); } // How bright a color is (converts RGB to grayscale brightness)
uniform sampler2D uMeshTex;
uniform int uNumTris;
uniform int uMeshTexWidth;

uniform int uSdfRes;
uniform int uSlicesPerRow;
float sampleSDF(vec3 p) {
    // p in [-0.5, 0.5]
    vec3 uvw = clamp(p + 0.5, 0.0, 1.0); // remap to [0,1]

    float zf  = uvw.z * float(uSdfRes - 1);
    int   z0  = int(floor(zf));
    int   z1  = min(z0 + 1, uSdfRes - 1);
    float zfr = zf - float(z0);

    // tile position in atlas
    vec2 tile0 = vec2(float(z0 % uSlicesPerRow), float(z0 / uSlicesPerRow));
    vec2 tile1 = vec2(float(z1 % uSlicesPerRow), float(z1 / uSlicesPerRow));

    // total atlas dimensions in tiles
    float atlasSize = float(uSlicesPerRow);

    // uv within the tile + tile offset, divided by atlas size
    vec2 uv0 = (tile0 + uvw.xy) / atlasSize;
    vec2 uv1 = (tile1 + uvw.xy) / atlasSize;

    float d0 = texture(uMeshTex, uv0).r;
    float d1 = texture(uMeshTex, uv1).r;

    return mix(d0, d1, zfr);
}

float sdSTL(vec3 p, float size, vec2 uv) {
    float areaFactor      = clamp(1.0 / (g_triArea + 0.005), 1.0, 80.0);
    float normalizedArea  = smoothstep(0.0, 40.0, areaFactor);
    float screenDiag      = sqrt(1.0 + g_ar * g_ar);
    float centerProximity = pow(clamp(g_globalEdgeDist / (screenDiag * 0.15), 0.0, 1.0), 0.125);

    float scaleAmount = clamp(0.3 + centerProximity * 0.32, 0.001, 1.0);

    float colorIntensity = (pow(g_triColor.r,0.5)*12.0
                          + pow(g_triColor.g,0.5)*12.8
                          + pow(g_triColor.b,0.5)*12.9) / 3.0;
    float strength = 0.1 + normalizedArea*0.3 + colorIntensity*1.1;

    vec2  triCenter  = (g_triP1 + g_triP2 + g_triP3) / 3.0;
    float warpFactor = length(uv - triCenter) * (1.0 - centerProximity) * strength * 5.0;
    float finalScale = max(scaleAmount * (1.0 - warpFactor * 0.5), 0.03);

    // USE p DIRECTLY in world space — no camera projection
    vec3 camP = p / (finalScale * size);

    vec3 outsideVec = abs(camP) - 0.5;
    float outsideDist = length(max(outsideVec, 0.0));

    float d = sampleSDF(clamp(camP, -0.5, 0.5));
    float signedDist = (d - 0.5) * 2.0;
    float finalDist = signedDist + outsideDist;

    return finalDist * finalScale * size;
}
vec3 getMeshVert(int i) {
    int x = i % uMeshTexWidth;   // column
    int y = i / uMeshTexWidth;   // row
    return texelFetch(uMeshTex, ivec2(x, y), 0).xyz;
}

float distToTri(vec3 p, vec3 a, vec3 b, vec3 c)
{
    vec3 ab = b - a;
    vec3 ac = c - a;
    vec3 ap = p - a;

    float d1 = dot(ab, ap);
    float d2 = dot(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0)
        return length(ap); // Closest to vertex A

    vec3 bp = p - b;
    float d3 = dot(ab, bp);
    float d4 = dot(ac, bp);
    if (d3 >= 0.0 && d4 <= d3)
        return length(bp); // Closest to vertex B

    float vc = d1*d4 - d3*d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0)
    {
        float v = d1 / (d1 - d3);
        vec3 closest = a + v * ab;
        return length(p - closest); // Closest on AB
    }

    vec3 cp = p - c;
    float d5 = dot(ab, cp);
    float d6 = dot(ac, cp);
    if (d6 >= 0.0 && d5 <= d6)
        return length(cp); // Closest to vertex C

    float vb = d5*d2 - d1*d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0)
    {
        float w = d2 / (d2 - d6);
        vec3 closest = a + w * ac;
        return length(p - closest); // Closest on AC
    }

    float va = d3*d6 - d5*d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0)
    {
        vec3 bc = c - b;
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        vec3 closest = b + w * bc;
        return length(p - closest); // Closest on BC
    }

    // Inside face region
    vec3 n = cross(ab, ac);
    float distance = abs(dot(ap, normalize(n)));
    return distance;
}


// Triangle functions
float pointToSegmentDistance(vec2 p, vec2 a, vec2 b) { // How far is point p from the line between a and b?
    vec2 ps = vec2(p.x * g_ar, p.y); // Stretch p by aspect ratio so distances are correct on non-square screens
    vec2 as = vec2(a.x * g_ar, a.y); // Stretch point a the same way
    vec2 bs = vec2(b.x * g_ar, b.y); // Stretch point b the same way
    vec2 ab = bs - as, ap = ps - as; // Vector from a to b, and from a to p
    float t = clamp(dot(ap, ab) / dot(ab, ab), 0.0, 1.0); // How far along the line from a to b is the closest point to p? (0=at a, 1=at b)
    return distance(ps, as + t * ab); // Distance from p to that closest point on the line
}

bool pointInTriangle(vec2 uv, vec2 p1, vec2 p2, vec2 p3) { // Is the point uv inside the triangle made by p1, p2, p3?
    float e1 = (p1.x - p2.x) * (uv.y - p2.y) - (p1.y - p2.y) * (uv.x - p2.x); // Cross product for edge p2→p1
    float e2 = (p2.x - p3.x) * (uv.y - p3.y) - (p2.y - p3.y) * (uv.x - p3.x); // Cross product for edge p3→p2
    float e3 = (p3.x - p1.x) * (uv.y - p1.y) - (p3.y - p1.y) * (uv.x - p1.x); // Cross product for edge p1→p3
    return (e1 <= 0.0 && e2 <= 0.0 && e3 <= 0.0) || (e1 >= 0.0 && e2 >= 0.0 && e3 >= 0.0); // All same sign = inside
}

float triangleArea(vec2 p1, vec2 p2, vec2 p3) { // Calculate the area of a triangle (accounting for screen aspect ratio)
    return 0.5 * abs((p2.x - p1.x) * g_ar * (p3.y - p1.y) - (p3.x - p1.x) * g_ar * (p2.y - p1.y)); // Standard triangle area formula
}

float distanceToTriangleVertices(vec2 uv, vec2 p1, vec2 p2, vec2 p3) { // Distance to the nearest corner of the triangle
    vec2 uvS = vec2(uv.x * g_ar, uv.y); // Stretch uv by aspect ratio
    return min(distance(uvS, vec2(p1.x * g_ar, p1.y)), // Distance to corner 1
           min(distance(uvS, vec2(p2.x * g_ar, p2.y)), // Distance to corner 2
               distance(uvS, vec2(p3.x * g_ar, p3.y)))); // Distance to corner 3, return the smallest
}

float minEdgeDist(vec2 uv, vec2 p1, vec2 p2, vec2 p3) { // Distance to the nearest edge of the triangle
    return min(pointToSegmentDistance(uv, p1, p2), // Distance to edge p1→p2
           min(pointToSegmentDistance(uv, p2, p3), // Distance to edge p2→p3
               pointToSegmentDistance(uv, p3, p1))); // Distance to edge p3→p1, return the smallest
}

vec3 getBarycentricCoords(vec2 p, vec2 a, vec2 b, vec2 c) { // Get barycentric coordinates (how much weight each corner has at point p)
    vec2 v0 = c - a, v1 = b - a, v2 = p - a; // Vectors from corner a to other corners and to p
    float dot00 = dot(v0, v0), dot01 = dot(v0, v1); // Dot products needed for the formula
    float dot02 = dot(v0, v2), dot11 = dot(v1, v1), dot12 = dot(v1, v2); // More dot products
    float invDenom = 1.0 / (dot00 * dot11 - dot01 * dot01); // Inverse of the denominator (to avoid dividing)
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom; // Weight for corner c
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom; // Weight for corner b
    return vec3(1.0 - u - v, v, u); // Return weights for corners a, b, c (they add up to 1)
}

vec2 rotate(vec2 pos) { // Rotate a 2D position around the center of the screen
    vec2 p = pos - 0.5; // Move origin to center of screen (from 0-1 space)
    float angle = g_mouse.x * 12.566370614 + (iMouse.z > 0.0 ? sin(iTime * 2.0) * 0.04 : 0.0); // Rotation angle based on mouse X + wobble if mouse clicked
    return vec2(p.x * cos(angle) - p.y * sin(angle), p.x * sin(angle) + p.y * cos(angle)) + 0.5; // Standard 2D rotation matrix, then move back from center
}

// SDF primitives (Signed Distance Functions - tell you how far you are from a shape's surface)
float sdBoxStandard(vec3 p, vec3 b) { // Distance from point p to a box with half-size b (standard version, unused)
    vec3 q = abs(p) - b; // How far outside the box on each axis (negative = inside)
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0); // Distance to nearest surface
}

float sdBox(vec3 p, vec3 b, vec2 uv) { // Custom box distance with special warping effects based on triangle properties
    float areaFactor = clamp(1.0 / (g_triArea + 0.005), 1.0, 80.0); // Smaller triangles = bigger factor (inverse relationship)
    float normalizedArea = smoothstep(0.0, 40.0, areaFactor); // Convert area factor to smooth 0-1 range
    float screenDiag = sqrt(1.0 + g_ar * g_ar); // Length of screen diagonal (for distance calculations)
    float centerProximity = pow(clamp(g_globalEdgeDist / (screenDiag * 0.15), 0.0, 1.0), 0.125); // How close are we to triangle center? (0=edge, 1=center)
    
    vec3 pPushed = p + g_camDir * (1.0 - centerProximity) * 35.0; // Push point away from camera if near triangle edges (creates depth distortion)
    float scaleAmount = clamp(0.3 + centerProximity * 0.32, 0.001, 1.0); // Scale box smaller near edges, bigger near center
    
    float colorIntensity = (pow(g_triColor.r, 0.5) * 2.0 + pow(g_triColor.g, 0.5) * 1.8 + pow(g_triColor.b, 0.5) * 1.9) / 3.0; // Brightness of triangle color
    float strength = 0.1 + normalizedArea * 0.3 + colorIntensity * 1.1; // Warp strength based on triangle area and color
    
    vec2 triCenter = (g_triP1 + g_triP2 + g_triP3) / 3.0; // Find center of current triangle
    float warpFactor = length(uv - triCenter) * (1.0 - centerProximity) * strength*5; // More warp farther from triangle center
    
    vec3 finalB = max(b * scaleAmount * (1.0 - warpFactor * 0.5), b * 0.03); // Apply scale and warp, but keep minimum size
    vec3 camP = vec3(dot(pPushed, g_camRight), dot(pPushed, g_camUp), dot(pPushed, g_camDir)); // Convert point to camera space
    vec3 q = abs(camP) - finalB; // How far outside the warped box
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0); // Distance to box surface
}



float sdHouse(vec3 p, float size, vec2 uv) { // Distance to a house shape made of boxes
    float wH = size * 0.8, wW = size * 0.7, wD = size * 0.6; // Wall dimensions: height, width, depth
    float walls = sdBox(p - vec3(0.0, wH * 0.5, 0.0), vec3(wW, wH * 0.5, wD), uv); // Main box for walls, centered at half-height
    
    vec3 roofP = p - vec3(0.0, wH + size * 0.125, 0.0); // Point relative to roof position (above walls)
    float ca = 0.8253, sa = 0.5646; // Cosine and sine of roof angle (~33.7 degrees)
    
    vec3 rp1 = roofP - vec3(-size * 0.3, 0.0, 0.0); // Position for left roof panel
    float roof1 = sdBox(vec3(rp1.x * ca - rp1.y * sa, rp1.x * sa + rp1.y * ca, rp1.z), // Rotate to angle the roof
                        vec3(size * 0.45, size * 0.08, wD * 1.15), uv); // Thin angled box
    vec3 rp2 = roofP - vec3(size * 0.3, 0.0, 0.0); // Position for right roof panel
    float roof2 = sdBox(vec3(rp2.x * ca + rp2.y * sa, -rp2.x * sa + rp2.y * ca, rp2.z), // Rotate opposite direction
                        vec3(size * 0.45, size * 0.08, wD * 1.15), uv); // Thin angled box
    
    float chimney = sdBox(p - vec3(size * 0.35, wH + size * 0.3, size * 0.2), vec3(size * 0.12, size * 0.35, size * 0.12), uv); // Small box for chimney
    float door = sdBox(p - vec3(0.0, wH * 0.4, -wD), vec3(size * 0.18, wH * 0.4, size * 0.15), uv); // Box for door opening
    float windows = min(sdBox(p - vec3(size * 0.35, wH * 0.6, -wD), vec3(size * 0.12, size * 0.12, size * 0.15), uv), // Right window
                    min(sdBox(p - vec3(-size * 0.35, wH * 0.6, -wD), vec3(size * 0.12, size * 0.12, size * 0.15), uv), // Left window
                        sdBox(p - vec3(wW, wH * 0.6, 0.0), vec3(size * 0.15, size * 0.12, size * 0.12), uv))); // Side window, combine with min
    
    return min(min(roof1, roof2), min(chimney, max(max(walls, -door), -windows))); // Combine: both roof panels, chimney, walls with door and windows subtracted (negative = cut out)
}
// 3D 5-pointed star SDF with the same distortion as sdBox
float sdStar(vec3 p, float r, float h, vec2 uv) {
    // --- same warp preamble as sdBox ---
    float areaFactor = clamp(1.0 / (g_triArea + 0.005), 1.0, 80.0); // Smaller triangles = bigger factor
    float normalizedArea = smoothstep(0.0, 40.0, areaFactor); // Convert area factor to smooth 0-1 range
    float screenDiag = sqrt(1.0 + g_ar * g_ar); // Length of screen diagonal
    float centerProximity = pow(clamp(g_globalEdgeDist / (screenDiag * 0.15), 0.0, 1.0), 0.125); // How close to triangle center? (0=edge, 1=center)

    vec3 pPushed = p + g_camDir * (1.0 - centerProximity) * 35.0; // Push point away from camera if near triangle edges
    float scaleAmount = clamp(0.3 + centerProximity * 0.32, 0.001, 1.0); // Scale smaller near edges, bigger near center

    float colorIntensity = (pow(g_triColor.r, 0.5) * 2.0 + pow(g_triColor.g, 0.5) * 1.8 + pow(g_triColor.b, 0.5) * 1.9) / 3.0; // Brightness of triangle color
    float strength = 0.1 + normalizedArea * 0.3 + colorIntensity * 1.1; // Warp strength based on triangle area and color

    vec2 triCenter = (g_triP1 + g_triP2 + g_triP3) / 3.0; // Find center of current triangle
    float warpFactor = length(uv - triCenter) * (1.0 - centerProximity) * strength * 5.0; // More warp farther from triangle center

    float finalScale = max(scaleAmount * (1.0 - warpFactor * 0.5), 0.03); // Apply scale and warp, keep minimum size

    // Transform into camera space (same as sdBox)
    vec3 camP = vec3(dot(pPushed, g_camRight), dot(pPushed, g_camUp), dot(pPushed, g_camDir));

    // Apply inverse scale so the star shrinks/grows with the warp
    camP /= finalScale;

    // --- 2D star cross-section in XY plane so the star stands upright ---
    // Using XY instead of XZ means the star face points along Z (its thin axis)
    vec2 q = camP.xy;
    float a = atan(q.y, q.x); // Angle around Z axis
    float seg = 6.2831853 / 5.0; // 72 degrees per segment
    a = mod(a + seg * 0.5, seg) - seg * 0.5; // Fold into one wedge
    q = length(q) * vec2(cos(a), abs(sin(a))); // Canonical wedge coords

    // Inner radius ratio for the star (smaller = pointier)
    float innerRatio = 0.42;
    // Half-angle of one triangle tip
    float halfSeg = seg * 0.5;

    // Signed distance to the star outline in 2D
    float d2 = q.x - r; // Outer circle clip

    // Line from outer tip to inner notch
    vec2 tipOuter = vec2(r, 0.0);
    vec2 tipInner = vec2(cos(halfSeg) * r * innerRatio, sin(halfSeg) * r * innerRatio);
    vec2 edge = tipInner - tipOuter;
    vec2 w = q - tipOuter;
    float t = clamp(dot(w, edge) / dot(edge, edge), 0.0, 1.0); // How far along the notch edge is the closest point?
    float dEdge = length(w - edge * t); // Distance to that closest point on the notch edge
    float side = sign(edge.x * w.y - edge.y * w.x); // Positive = outside the star edge
    float star2D = side * dEdge; // Signed distance to star profile
    star2D = max(star2D, d2); // Combine with outer circle clip

    // Extrude along Z for thickness h (Z is now the thin axis since star face is in XY)
    float dz = abs(camP.z) - h;
    float exterior = length(max(vec2(star2D, dz), 0.0)); // Distance when outside both star and slab
    float interior = min(max(star2D, dz), 0.0); // Distance when inside both (negative)
    float dist = exterior + interior; // Final 3D extruded star distance

    return dist * finalScale; // Undo the scale division to restore valid distance field
}
// ============== SCENE - FIXED WORLD POSITIONS ==============
vec2 objec(vec3 p, vec2 uv) { // Find distance to nearest object and which object it is (returns distance, objectID)
    float ground = sin(p.x * 0.5) * 2.0 + sin(p.z * 0.5) * 2.0 + p.y + 50.0; // Wavy ground plane (y=-50 with waves)
    float sky = 90.0 - p.y + sin(p.x * 0.3) * 3.0 + sin(p.z * 0.4) * 3.0 + sin(p.x * 0.7 + p.z * 0.5) * 2.0; // Wavy sky plane (y=90 with waves)
    
    float minDist = ground, objectID = 1.0; // Start with ground as nearest (ID=1 for ground)
    if (sky < minDist) { minDist = sky; objectID = 2.0; } // If sky is closer, use that (ID=2 for sky)
    
    // Ring of SDF stars around initial camera (buffer 0,0,0 + offset in image.frag)
    vec3 worldAnchor = vec3(0.0, 16.0, -64.0);
    float orbitR = 12.0;
    float baseSize = 5.0 * clamp(g_triArea * 10.0, 0.3, 2.0);

    for (int i = 0; i < NUM_HOUSES; i++) {
        float ringAng = float(i) * 6.2831853 / float(NUM_HOUSES); // Evenly space stars around the ring
        vec3 hPos = worldAnchor + vec3(cos(ringAng) * orbitR, 0.0, sin(ringAng) * orbitR); // Position on ring
        float fi = float(i) * 17.31 + 24691.2; // Unique seed per star for random variation
        float hSize = max(baseSize * (0.9 + hash(fi * 3.5) * 0.3) * 3.0, 8.0); // Random size, minimum 8
        vec3 hp = p - hPos; // Point relative to star center

        // Stand the star upright: rotate so XZ plane becomes XY plane
        // i.e. swap Y and Z so the star stands like a sign post rather than lying flat
        // Then rotate around Y axis to face outward from the ring center
        float ca = cos(ringAng), sa = sin(ringAng); // Cosine and sine of ring angle for this star
        // Reproject hp so X points along ring tangent, Y points up, Z points outward from ring center
        vec3 outward = vec3(ca, 0.0, sa);           // Direction pointing away from ring center
        vec3 tangent = vec3(-sa, 0.0, ca);          // Direction along the ring (perpendicular to outward)
        vec3 up = vec3(0.0, 1.0, 0.0);             // World up so the star stands vertically
        // Dot hp into this basis: star flat face points outward, star stands upright along Y
        hp = vec3(dot(hp, tangent), dot(hp, up), dot(hp, outward));

        // Stack vertically: shift only Y by i * boxHeight
        vec3 stackOffset = vec3(0.0, float(i) * hSize, 0.0);

        //float tower = sdBox(hp +stackOffset, vec3(hSize/4, hSize/4, hSize*4), uv);
       //float d = tower;

   float d = sdHouse(hp, hSize, uv);
      //  vec3 starPos = vec3(0.0, 30.0, -64.0); // (kept for reference, unused since hp is already relative)
     // float starDist = sdStar(hp, 28.0, 1.0, uv); // r=28 radius, h=2 half-thickness
      //float d = starDist;
//float d = sdSTL(hp, 55.0, uv);

    
        if (d < minDist) { minDist = d; objectID = 0.0; } // ID=0 for stars
    }
    
    return vec2(minDist, objectID); // Return distance to nearest object and its ID
}