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

void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = fragCoord / iResolution.xy;
    vec2 uvRay = (2.0 * fragCoord - iResolution.xy) / iResolution.y;
    
    // Init globals
    g_ar = iResolution.x / iResolution.y;
    g_camDir = texture(iChannel0, vec2(1.5, 0.5) / iResolution.xy).xyz;
    g_camPos = texture(iChannel0, vec2(2.5, 0.5) / iResolution.xy).xyz + vec3(0.0, 16.0, -64.0);
    g_camDir = length(g_camDir) < 0.001 ? vec3(0.0, 0.0, 1.0) : normalize(g_camDir);
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
    
    // Colors
    vec3 triColors[19] = vec3[](vec3(1,0,0),vec3(0,1,0),vec3(0,0,1),vec3(0,1,1),vec3(1,1,1),
        vec3(1,1,0),vec3(1,0,1),vec3(.9,0,.9),vec3(.8,0,.8),vec3(.7,0,.7),vec3(0,0,0),
        vec3(.5,0,.5),vec3(.5,.5,0),vec3(0,.5,.5),vec3(1,.5,0),vec3(.5,.25,0),
        vec3(.8,.2,.2),vec3(.2,.8,.4),vec3(.3,.3,.8));
    vec3 cornerColors[8] = vec3[](vec3(.2,.3,.5),vec3(.25,.35,.55),vec3(.5,.2,.3),vec3(.55,.25,.35),
        vec3(.3,.5,.2),vec3(.35,.55,.25),vec3(.4,.3,.4),vec3(.45,.35,.45));
    
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
    
    for (int i = 0; i < 128; ++i) {
        vec2 r = objec(p, uv);
        l = r.x; objectID = r.y;
        if (l < 0.001 || totalDist > 150.0) break;
        p += l * ray; totalDist += l;
    }
    
    // Normal
    const float s = 0.015625;
    vec3 normal = normalize(vec3(objec(p+vec3(s,0,0),uv).x-l, objec(p+vec3(0,s,0),uv).x-l, objec(p+vec3(0,0,s),uv).x-l));
    
    // Triangle coloring
    vec3 col = vec3(1.0);
    float cs = 2.75, t1=abs(cos(iTime/cs)), t2=abs(sin(iTime/cs)), t3=abs(sin(iTime*.5));
    
    for (int i = 0; i < TRI_SIZE; i++) {
        vec2 p1=tri[i*3], p2=tri[i*3+1], p3=tri[i*3+2];
        if (pointInTriangle(uv, p1, p2, p3)) {
            col = triColors[i];
            float dt=abs(uv.y-p1.y), dl=abs(uv.x-p2.x), dr=abs(uv.x-p3.x);
            float rl=fract(atan(uv.x-p2.x,uv.y-p2.y)*11.3), rr=fract(atan(uv.x-p3.x,uv.y-p3.y)*5.3);
            float rv=fract(atan(uv.x-p1.x,uv.y-p1.y)*6.3);
            if(min(rl,1.-rl)<.05||min(rr,1.-rr)<.05||min(rv,1.-rv)<.05) col=mix(vec3(1),vec3(0),dt);
            col=mix(col,vec3(t1),dr-dl); col=mix(col,vec3(t2),dt-dl-dr); col=mix(col/1.72,vec3(t3),dt-dr-dl*.4);
            
            vec3 bary = getBarycentricCoords(uv, p1, p2, p3);
            float cornerProximity = smoothstep(0.33, 1.0, max(bary.x, max(bary.y, bary.z)));
            col *= mix(1.0, 0.3, pow(cornerProximity, 2.0));
        }
    }
    for (int i = 0; i < CORNER_SIZE; i++) {
        vec2 p1=cornerTri[i*3], p2=cornerTri[i*3+1], p3=cornerTri[i*3+2];
        if (pointInTriangle(uv, p1, p2, p3)) {
            col = cornerColors[i];
            float dt=abs(uv.y-p1.y), dl=abs(uv.x-p2.x), dr=abs(uv.x-p3.x);
            float rl=fract(atan(uv.x-p2.x,uv.y-p2.y)*11.3), rr=fract(atan(uv.x-p3.x,uv.y-p3.y)*5.3);
            float rv=fract(atan(uv.x-p1.x,uv.y-p1.y)*6.3);
            if(min(rl,1.-rl)<.05||min(rr,1.-rr)<.05||min(rv,1.-rv)<.05) col=mix(vec3(1),vec3(0),dt);
            col=mix(col*2.,vec3(t1),dr-dl); col=mix(col/1.2,vec3(t2),dt-dl-dr); col=mix(col/1.72,vec3(t2),dt-dr-dl*.4);
            
            vec3 bary = getBarycentricCoords(uv, p1, p2, p3);
            float cornerProximity = smoothstep(0.33, 1.0, max(bary.x, max(bary.y, bary.z)));
            col *= mix(1.0, 0.3, pow(cornerProximity, 2.0));
        }
    }
    
    // Final shading
    float lighting = dot(normal, normalize(vec3(1,1,-1)))*.5+.5;
    float objectVisible = smoothstep(0.1, 0.01, l);
    float lum = getLuminance(col), cf = 1.-lum;
    
    float cornerDist = hitIdx>=0 ? distanceToTriangleVertices(uv,hitP1,hitP2,hitP3) : 1000.0;
    float dm = 1.0 - max(pow(1.-smoothstep(0.,.08,g_globalEdgeDist),1.5), pow(1.-smoothstep(0.,.12,cornerDist),1.2)) * .7;
    
    vec3 lightDir = normalize(vec3(1,1,-1));
    float rim = pow(1.-max(0.,dot(normal,-ray)),3.);
    vec3 finalColor;
    
    if (objectID > 1.5) {
        vec3 sc = vec3(.3,.4,.6)*lighting + vec3(.1,.05,.15)*sin(p.x*.2+p.z*.3) + (cf-.5)*.4;
        sc = clamp(sc,0.,1.); sc = mix(sc*col*8., mix(vec3(.4,.5,.7),vec3(.8,.85,1.),cf)*col*8., rim*.5);
        finalColor = sc * dm;
    } else if (objectID > 0.5) {
        vec3 tc = clamp(vec3(.4,.3,.2)*lighting + (cf-.5)*.6, 0.,1.);
        tc = mix(tc*col*10., mix(vec3(.2),vec3(.8),cf)*col*10., rim*.4*col*10.);
        finalColor = tc * dm;
    } else {
        float diff = max(0.,dot(normal,lightDir));
        vec3 sf = clamp(sphereColor+(.5-lum)*.5,.1,.9) * (.3+diff*.7);
        sf = clamp(sf + vec3(.3)*pow(max(dot(-ray,reflect(-lightDir,normal)),0.),32.),0.,1.);
        finalColor = sf * dm;
    }
    
    fragColor = vec4(mix(col, finalColor, objectVisible), 1.0);
}