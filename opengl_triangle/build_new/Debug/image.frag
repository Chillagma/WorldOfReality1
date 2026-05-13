// Note: common.glsl is prepended - contains #version, uniforms, globals, and functions
#ifndef COMMON_INCLUDED
vec4 fragColor;
uniform sampler2D iChannel0;
uniform float iTime;
uniform vec2 iResolution;
uniform vec4 iMouse;
float g_ar, g_triArea, g_globalEdgeDist;
vec2 g_mouse, g_triP1, g_triP2, g_triP3;
vec3 g_camDir, g_camPos, g_camRight, g_camUp, g_triColor;
const int TRI_SIZE = 19, CORNER_SIZE = 8;
float hash(float n) { return 0.0; }
float getLuminance(vec3 c) { return 0.0; }
bool pointInTriangle(vec2 a, vec2 b, vec2 c, vec2 d) { return false; }
float triangleArea(vec2 a, vec2 b, vec2 c) { return 0.0; }
float distanceToTriangleVertices(vec2 a, vec2 b, vec2 c, vec2 d) { return 0.0; }
float minEdgeDist(vec2 a, vec2 b, vec2 c, vec2 d) { return 0.0; }
vec3 getBarycentricCoords(vec2 a, vec2 b, vec2 c, vec2 d) { return vec3(0); }
vec2 rotate(vec2 a) { return vec2(0); }
vec2 objec(vec3 a, vec2 b) { return vec2(0); }

#endif

// Camera override uniforms
uniform int uCameraOverride;
uniform vec3 uCameraPosOverride;
uniform vec2 uCameraRotOverride;
uniform int uDetectionMode;

// ============== AESTHETIC HELPERS ==============
vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Saturation adjustment function
vec3 adjustSaturation(vec3 color, float saturation) {
    vec3 luminanceWeights = vec3(0.299, 0.587, 0.114);
    float luma = dot(color, luminanceWeights);
    return mix(vec3(luma), color, saturation);
}

// Distance from point to line segment
float distToSegment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

// Get minimum distance to any edge of a triangle
float distToTriangleEdges(vec2 p, vec2 a, vec2 b, vec2 c) {
    float d1 = distToSegment(p, a, b);
    float d2 = distToSegment(p, b, c);
    float d3 = distToSegment(p, c, a);
    return min(d1, min(d2, d3));
}

// Disabled: each step called objec() (mesh × tris) — was the main source of lag
float softShadow(vec3 ro, vec3 rd, float mint, float maxt, float k, vec2 uv) {
    return 1.0;
}

float calcAO(vec3 pos, vec3 nor, vec2 uv) {
    return 1.0;
}

void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord / iResolution.xy;
    vec2 uvRay = (2.0 * fragCoord - iResolution.xy) / iResolution.y;
    
    // Init globals
    g_ar = iResolution.x / iResolution.y;
    
    // Camera setup
    if (uCameraOverride == 1) {
        float pitch = uCameraRotOverride.x;
        float yaw = uCameraRotOverride.y;
        g_camDir = normalize(vec3(cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw)));
        g_camPos = uCameraPosOverride + vec3(0.0, 16.0, -64.0);
    } else {
        g_camDir = texture(iChannel0, vec2(1.5, 0.5) / iResolution.xy).xyz;
        g_camPos = texture(iChannel0, vec2(2.5, 0.5) / iResolution.xy).xyz + vec3(0.0, 16.0, -64.0);
        g_camDir = length(g_camDir) < 0.001 ? vec3(0.0, 0.0, 1.0) : normalize(g_camDir);
    }
    
    g_camRight = normalize(vec3(g_camDir.z, 0.0, -g_camDir.x));
    g_camUp = cross(g_camDir, g_camRight);
    g_mouse = iMouse.xy == vec2(0.0) ? vec2(0.5) : iMouse.xy / iResolution.xy;
    
    // Vertices
    vec2 c_bl=vec2(0,0), c_br=vec2(1,0), c_tr=vec2(1,1), c_tl=vec2(0,1);
    vec2 c_bm=vec2(.5,0), c_tm=vec2(.5,1), c_ml=vec2(0,.5), c_mr=vec2(1,.5);
    vec2 c_mid1=(c_bm+c_ml)*.5, c_mid2=(c_br+c_mr)*.5, c_mid3=(c_tr+c_tm)*.5, c_mid4=(c_tm+c_tl)*.5;
    vec2 v0=vec2(0,0), v1=vec2(.5,.5), v2=vec2(.25,.05), v3=vec2(.75,.25);
    vec2 v4=vec2(.55,.55), v7=vec2(1,1), v9=vec2(.5,.15), v12=vec2(0,.55);
    vec2 vm1=(v0+v7)*.5, vm2=(v0+v12)*.5, vm3=(v7+v12)*.5;
    
    // MORE SATURATED color palettes (increased color intensity)
    vec3 triColors[19] = vec3[](
        vec3(1.0, 0.15, 0.2),    // vivid red
        vec3(0.1, 0.95, 0.4),    // vivid green
        vec3(0.15, 0.35, 1.0),   // vivid blue
        vec3(0.0, 0.95, 0.95),   // vivid cyan
        vec3(0.95, 0.95, 0.98),  // white (keep neutral)
        vec3(1.0, 0.9, 0.0),     // vivid yellow
        vec3(1.0, 0.2, 0.9),     // vivid magenta
        vec3(0.95, 0.15, 0.85),  // vivid pink
        vec3(0.85, 0.1, 0.75),   // vivid pink 2
        vec3(0.75, 0.08, 0.65),  // vivid purple-pink
        vec3(0.08, 0.08, 0.12),  // dark (keep)
        vec3(0.65, 0.1, 0.7),    // vivid purple
        vec3(0.75, 0.65, 0.0),   // vivid olive/gold
        vec3(0.0, 0.6, 0.65),    // vivid teal
        vec3(1.0, 0.5, 0.0),     // vivid orange
        vec3(0.75, 0.35, 0.0),   // vivid brown/orange
        vec3(1.0, 0.2, 0.3),     // vivid coral
        vec3(0.2, 0.95, 0.5),    // vivid sea green
        vec3(0.35, 0.4, 1.0)     // vivid slate blue
    );
    
    vec3 cornerColors[8] = vec3[](
        vec3(0.15, 0.35, 0.7),   // saturated blue
        vec3(0.2, 0.45, 0.8),
        vec3(0.7, 0.2, 0.35),    // saturated red
        vec3(0.8, 0.25, 0.4),
        vec3(0.3, 0.7, 0.2),     // saturated green
        vec3(0.35, 0.8, 0.3),
        vec3(0.55, 0.35, 0.6),   // saturated purple
        vec3(0.65, 0.4, 0.7)
    );
    
    // Triangles
    vec2 tri[57] = vec2[](
        rotate(v1),rotate(v2),rotate(v3), rotate(v4),rotate(v1),rotate(v3),
        rotate(v7),rotate(v4),rotate(v3), rotate(v1),rotate(v2),rotate(v9),
        rotate(v0),rotate(v2),rotate(v1), rotate(v0),rotate(v2),rotate(v12),
        rotate(v0),rotate(vm1),rotate(vm2), rotate(vm2),rotate(vm1),rotate(v12),
        rotate(vm1),rotate(v7),rotate(vm3), rotate(vm1),rotate(vm3),rotate(v12),
        rotate(c_bm),rotate(c_br),rotate(v3), rotate(v3),rotate(c_br),rotate(c_mr),
        rotate(c_bl),rotate(c_bm),rotate(v2), rotate(v3),rotate(c_mr),rotate(v7),
        rotate(v7),rotate(c_tr),rotate(c_mr), rotate(v9),rotate(c_bm),rotate(v3),
        rotate(c_tl),rotate(v7),rotate(c_ml), rotate(v2),rotate(c_bm),rotate(v9),
        rotate(c_tl),rotate(c_ml),rotate(c_bl));
    vec2 cornerTri[24] = vec2[](c_bl,c_bm,c_mid1,c_bl,c_mid1,c_ml,c_bm,c_br,c_mid2,c_bm,c_mid2,c_mr,
        c_mr,c_tr,c_mid3,c_mr,c_mid3,c_tm,c_ml,c_tm,c_mid4,c_ml,c_mid4,c_tl);
    
    // ====== CLEAN OUTLINE SETTINGS ======
    float outlineWidth = 0.0;        // Thin clean line
    float outlineSoftness = 0.0;     // Very sharp edge (almost no blur)
    outlineSoftness= min(outlineSoftness,0.007); // Ensure softness is not zero to avoid artifacts
    // Outline color options:
    // Option 1: Pure white (clean)
    vec3 outlineColor = vec3(0.0);
    
    // Option 2: Off-white / cream
    // vec3 outlineColor = vec3(0.95, 0.92, 0.85);
    
    // Option 3: Light gray
    // vec3 outlineColor = vec3(0.7);
    
    // Option 4: Black (for dark outlines)
    // vec3 outlineColor = vec3(0.0);
    
    // Calculate minimum distance to ANY triangle edge
    float minEdgeDistance = 2000.0;
    
    for (int i = 0; i < TRI_SIZE; i++) {
        vec2 p1 = tri[i*3], p2 = tri[i*3+1], p3 = tri[i*3+2];
        float d = distToTriangleEdges(uv, p1, p2, p3);
        minEdgeDistance = min(minEdgeDistance, d);
    }
    
    for (int i = 0; i < CORNER_SIZE; i++) {
        vec2 p1 = cornerTri[i*3], p2 = cornerTri[i*3+1], p3 = cornerTri[i*3+2];
        float d = distToTriangleEdges(uv, p1, p2, p3);
        minEdgeDistance = min(minEdgeDistance, d);
    }
    
    // Sharp outline factor (nearly binary)
    float outlineFactor = 1.0 - smoothstep(outlineWidth - outlineSoftness, outlineWidth + outlineSoftness, minEdgeDistance);
    
    // Find hit triangle
    g_globalEdgeDist = 1000.0;
    int hitIdx = -1;
    bool isCorner = false;
    vec2 hitP1, hitP2, hitP3;
    vec3 sphereColor = vec3(0.5);
    
    for (int i = 0; i < TRI_SIZE; i++) {
        vec2 p1=tri[i*3], p2=tri[i*3+1], p3=tri[i*3+2];
        g_globalEdgeDist = min(g_globalEdgeDist, minEdgeDist(uv, p1, p2, p3));
        if (pointInTriangle(uv, p1, p2, p3)) { hitIdx=i; isCorner=false; hitP1=p1; hitP2=p2; hitP3=p3; }
    }
    for (int i = 0; i < CORNER_SIZE; i++) {
        vec2 p1=cornerTri[i*3], p2=cornerTri[i*3+1], p3=cornerTri[i*3+2];
        g_globalEdgeDist = min(g_globalEdgeDist, minEdgeDist(uv, p1, p2, p3));
        if (pointInTriangle(uv, p1, p2, p3)) { hitIdx=i; isCorner=true; hitP1=p1; hitP2=p2; hitP3=p3; }
    }
    
    // Set globals from hit
    if (hitIdx >= 0) {
        sphereColor = isCorner ? cornerColors[hitIdx] : triColors[hitIdx];
        g_triP1=hitP1; g_triP2=hitP2; g_triP3=hitP3; g_triColor=sphereColor;
        g_triArea = triangleArea(hitP1, hitP2, hitP3);
    } else {
        g_triP1=vec2(0,0); g_triP2=vec2(1,0); g_triP3=vec2(.5,1); g_triColor=vec3(.5); g_triArea=.5;
    }
    
    // Raymarch
    mat3 mtx = mat3(normalize(vec3(g_camDir.z,0,-g_camDir.x)), cross(g_camDir,normalize(vec3(g_camDir.z,0,-g_camDir.x))), g_camDir);
    vec3 ray = mtx * normalize(vec3(uvRay, 1.0));
    vec3 p = g_camPos;
    float l=0.0, totalDist=0.0, objectID=0.0;
    
    for (int i = 0; i < 24; ++i) {
        vec2 r = objec(p, uv);
        l = r.x; objectID = r.y;
        if (l < 0.001 || totalDist > 120.0) break;
        p += l * ray; totalDist += l;
    }
    
    // Mesh normals: forward differences (4× objec vs 6× central differences)
    vec3 normal;
    if (objectID < 0.5) {
        const float eps = 0.012;
        float c = objec(p, uv).x;
        normal = normalize(vec3(
            objec(p + vec3(eps, 0.0, 0.0), uv).x - c,
            objec(p + vec3(0.0, eps, 0.0), uv).x - c,
            objec(p + vec3(0.0, 0.0, eps), uv).x - c
        ));
    } else {
        normal = vec3(0.0, 1.0, 0.0);
    }
    
    // ============== TRIANGLE COLORING WITH GRID LINES ==============
    vec3 col = vec3(0.05);
    float cs = 2.75;
    float t1 = abs(cos(iTime / cs));
    float t2 = abs(sin(iTime / cs));
    float t3 = abs(sin(iTime * 0.5));
    
    // ====== BARYCENTRIC CORNER DARKENING SETTINGS ======
    float cornerDarknessAmount = 0.05;
    float cornerFalloffStart = 0.4;
    float cornerFalloffEnd = 0.95;
    float cornerPower = 2.0;
    
    for (int i = 0; i < TRI_SIZE; i++) {
        vec2 p1=tri[i*3], p2=tri[i*3+1], p3=tri[i*3+2];
        if (pointInTriangle(uv, p1, p2, p3)) {
            col = triColors[i];
            
            // === ORIGINAL GRID LINES ===
            float dt = abs(uv.y - p1.y);
            float dl = abs(uv.x - p2.x);
            float dr = abs(uv.x - p3.x);
            
            float rl = fract(atan(uv.x - p2.x, uv.y - p2.y) * 11.3);
            float rr = fract(atan(uv.x - p3.x, uv.y - p3.y) * 5.3);
            float rv = fract(atan(uv.x - p1.x, uv.y - p1.y) * 6.3);
            
            // Radial grid lines
            if (min(rl, 1.0 - rl) < 0.05 || min(rr, 1.0 - rr) < 0.05 || min(rv, 1.0 - rv) < 0.05) {
                col = mix(vec3(1.0), vec3(0.0), dt);
            }
            
            // Color mixing based on position
            col = mix(col, vec3(t1), dr - dl);
            col = mix(col, vec3(t2), dt - dl - dr);
            col = mix(col / 1.72, vec3(t3), dt - dr - dl * 0.4);
            
            // ====== BARYCENTRIC CORNER DARKENING ======
            vec3 bary = getBarycentricCoords(uv, p1, p2, p3);
            float maxBary = max(bary.x, max(bary.y, bary.z));
            float cornerProximity = smoothstep(cornerFalloffStart, cornerFalloffEnd, maxBary);
            cornerProximity = pow(cornerProximity, cornerPower);
            float darkenFactor = mix(1.0, cornerDarknessAmount, cornerProximity);
            col *= darkenFactor;
            
            // Boost saturation per-triangle
            col = adjustSaturation(col, 0.6);
        }
    }
    
    for (int i = 0; i < CORNER_SIZE; i++) {
        vec2 p1=cornerTri[i*3], p2=cornerTri[i*3+1], p3=cornerTri[i*3+2];
        if (pointInTriangle(uv, p1, p2, p3)) {
            col = cornerColors[i];
            
            // === ORIGINAL GRID LINES ===
            float dt = abs(uv.y - p1.y);
            float dl = abs(uv.x - p2.x);
            float dr = abs(uv.x - p3.x);
            
            float rl = fract(atan(uv.x - p2.x, uv.y - p2.y) * 11.3);
            float rr = fract(atan(uv.x - p3.x, uv.y - p3.y) * 5.3);
            float rv = fract(atan(uv.x - p1.x, uv.y - p1.y) * 6.3);
            
            // Radial grid lines
            if (min(rl, 1.0 - rl) < 0.05 || min(rr, 1.0 - rr) < 0.05 || min(rv, 1.0 - rv) < 0.05) {
                col = mix(vec3(1.0), vec3(0.0), dt);
            }
            
            // Color mixing based on position
            col = mix(col * 2.0, vec3(t1), dr - dl);
            col = mix(col / 1.2, vec3(t2), dt - dl - dr);
            col = mix(col / 1.72, vec3(t2), dt - dr - dl * 0.4);
            
            // ====== BARYCENTRIC CORNER DARKENING ======
            vec3 bary = getBarycentricCoords(uv, p1, p2, p3);
            float maxBary = max(bary.x, max(bary.y, bary.z));
            float cornerProximity = smoothstep(cornerFalloffStart, cornerFalloffEnd, maxBary);
            cornerProximity = pow(cornerProximity, cornerPower);
            float darkenFactor = mix(1.0, cornerDarknessAmount, cornerProximity);
            col *= darkenFactor;
            
            // Boost saturation per-triangle
            col = adjustSaturation(col, 1.3);
        }
    }
    
    // ====== APPLY CLEAN OUTLINES - ONLY ON THE LINE ITSELF ======
    col = mix(col, outlineColor, outlineFactor);
    
    // ============== ENHANCED 3D LIGHTING ==============
    vec3 lightDir = normalize(vec3(0.8, 0.6, -0.5));
    vec3 lightDir2 = normalize(vec3(-0.15, 0.3, 0.7));
    vec3 lightCol = vec3(1.0, 0.95, 0.85);
    vec3 lightCol2 = vec3(0.4, 0.5, 0.7);
    vec3 ambientCol = vec3(0.15, 0.18, 0.25);
    
    float objectVisible = smoothstep(0.1, 0.01, l);
    float lum = getLuminance(col);
    float cf = 1.0 - lum;
    
    // Edge/corner darkening
    float cornerDist = hitIdx >= 0 ? distanceToTriangleVertices(uv, hitP1, hitP2, hitP3) : 1000.0;
    float dm = 1.0 - max(pow(1.0 - smoothstep(0.0, 0.08, g_globalEdgeDist), 1.5), 
                         pow(1.0 - smoothstep(0.0, 0.12, cornerDist), 1.2)) * 0.7;
    
    // Lighting calculations
    float diff = max(0.0, dot(normal, lightDir));
    float diff2 = max(0.0, dot(normal, lightDir2)) * 0.4;
    vec3 halfVec = normalize(lightDir - ray);
    float spec = pow(max(0.0, dot(normal, halfVec)), 64.0);
    float fresnel = pow(1.0 - max(0.0, dot(normal, -ray)), 4.0);
    float rim = pow(1.0 - max(0.0, dot(normal, -ray)), 3.0);
    
    // AO and shadows
    float ao = calcAO(p, normal, uv);
    float shadow = softShadow(p + normal * 0.05, lightDir, 0.1, 40.0, 12.0, uv);
    
    // Fog
    vec3 fogCol = vec3(0.55, 0.7, 0.9);
    float fogAmount = 0;
    
    vec3 finalColor;
    
    // ====== 3D OBJECT DARKENING FACTOR ======
    float objDarken = 0.5;
    
    if (objectID > 1.5) {
        // SKY
        vec3 skyTop = vec3(0.2, 0.4, 0.85);
        vec3 skyHorizon = vec3(0.65, 0.8, 1.0);
        vec3 skyBottom = vec3(0.95, 0.7, 0.5);
        
        float skyGrad = ray.y * 0.5 + 0.5;
        vec3 skyCol = vec3(0);
        
        // Sun glow
        float sunDot = max(0.0, dot(ray, lightDir));
        skyCol += vec3(1.0, 0.7, 0.2) * pow(sunDot, 32.0) * 0.6;
        skyCol += vec3(1.0, 0.85, 0.6) * pow(sunDot, 4.0) * 0.25;
        
        // Subtle clouds
        float clouds = sin(p.x * 0.02 + iTime * 0.1) * sin(p.z * 0.03) * 0.5 + 0.5;
        skyCol = mix(skyCol, vec3(0.95), clouds * 0.15 * smoothstep(0.4, 0.8, skyGrad));
        
        vec3 sc = vec3(0.3, 0.4, 0.6) * (diff * 0.5 + 0.5) + vec3(0.1, 0.05, 0.15) * sin(p.x * 0.2 + p.z * 0.3) + (cf - 0.5) * 0.4;
        sc = clamp(sc, 0.0, 1.0);
        sc = mix(sc * col * 8.0, mix(vec3(0.4, 0.5, 0.7), vec3(0.8, 0.85, 1.0), cf) * col * 8.0, rim * 0.5);
        finalColor = mix(skyCol, sc, 0.3) * dm;
        
        finalColor = adjustSaturation(finalColor, 1.3);
        finalColor *= objDarken;
        
    } else if (objectID > 0.5) {
        // GROUND
        vec3 groundCol = vec3(0.4, 0.25, 0.15);
        vec3 grassCol = vec3(0.2, 0.45, 0.12);
        
        float grassAmount = smoothstep(0.7, 0.95, normal.y);
        vec3 terrainCol = mix(groundCol, grassCol, grassAmount);
        
        vec3 tc = clamp(vec3(0.4, 0.3, 0.2) * (diff * shadow * 0.7 + 0.3) + (cf - 0.5) * 0.6, 0.0, 1.0);
        tc = mix(tc * col * 10.0, mix(vec3(0.2), vec3(0.8), cf) * col * 10.0, rim * 0.4 * col * 10.0);
        
        vec3 lit = terrainCol * (ambientCol * ao + lightCol * diff * shadow + lightCol2 * diff2);
        lit += vec3(0.02) * fresnel;
        
        finalColor = mix(lit, tc, 0.5) * col * 2.0 * dm;
        
        finalColor = adjustSaturation(finalColor, 1.25);
        finalColor *= objDarken;
        
    } else {
        // OBJECTS
        vec3 objCol = sphereColor;
        
        // Micro detail
        float detail = sin(p.x * 5.0) * sin(p.y * 5.0) * sin(p.z * 5.0) * 0.05;
        objCol += detail;
        
        // Original sphere shading
        vec3 sf = clamp(sphereColor + (0.5 - lum) * 0.5, 0.1, 0.9) * (0.3 + diff * 0.7);
        sf = clamp(sf + vec3(0.3) * pow(max(dot(-ray, reflect(-lightDir, normal)), 0.0), 32.0), 0.0, 1.0);
        
        // Enhanced lighting
        vec3 ambient = ambientCol * objCol * ao;
        vec3 diffuse = lightCol * objCol * diff * shadow;
        vec3 diffuse2 = lightCol2 * objCol * diff2;
        vec3 specular = lightCol * spec * 0.5;
        vec3 rimLight = vec3(0.2, 0.25, 0.35) * fresnel * 0.6;
        
        finalColor = mix(sf, ambient + diffuse + diffuse2 + specular + rimLight, 0.6) * dm;
        
        finalColor = adjustSaturation(finalColor, 1.35);
        finalColor *= objDarken;
    }
    
    // Apply fog (not to sky)
    if (objectID < 1.5) {
        finalColor = mix(finalColor, fogCol * objDarken, fogAmount * 0.6);
    }
    
    // Blend 2D triangles with 3D scene
    vec3 blended = mix(col, finalColor, objectVisible);
    
    // ============== POST-PROCESSING ==============
    
    // Global saturation boost
    blended = adjustSaturation(blended, 1.35);
    
    // Vignette
    vec2 vigUV = uv * (1.0 - uv);
    float vig = pow(vigUV.x * vigUV.y * 16.0, 0.25);
    blended *= mix(0.7, 1.0, vig);
    
    // Color grading
    blended = pow(blended, vec3(0.92, 0.98, 1.08));
    
    // ACES tone mapping
    blended = aces(blended);
    
    // Gamma correction
    blended = pow(blended, vec3(1.0 / 2.2));
    
    // Subtle film grain
    float grain = hash(uv.x * 1000.0 + uv.y * 1000.0 + iTime) * 0.03;
    blended += grain - 0.015;
    
       // Detection mode: output objectID as color for CPU readback
    if (uDetectionMode == 1) {
        // objectID: 0 = 3D objects, 1 = ground, 2 = sky
        // Encode as: objects = black, ground = gray, sky = white
        float id = clamp(objectID / 2.0, 0.0, 1.0);
        fragColor = vec4(vec3(id), 1.0);
    } else {
        fragColor = vec4(clamp(blended, 0.0, 1.0), 1.0);
    }
}
