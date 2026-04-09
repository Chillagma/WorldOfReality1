#version 330 core
out vec4 fragColor;

uniform sampler2D iChannel0;
uniform vec2 iResolution;
uniform vec4 iMouse;
uniform float uKeyW;
uniform float uKeyS;
uniform float uKeyA;
uniform float uKeyD;

// Camera override uniforms - MUST be at top level, not inside main()
uniform int uCameraOverride;
uniform vec3 uCameraPosOverride;
uniform vec2 uCameraRotOverride;

void rotVec(inout vec4 v, float x, float y)
{
    y = asin(v.y) - y;
    if(-1.56 > y){y = -1.56;}
    if( 1.56 < y){y =  1.56;}
    float l = cos(y);
    x = -x;
    v.xz = vec2(v.x*cos(x) - v.z*sin(x),
                v.x*sin(x) + v.z*cos(x));
    v.xz *= l / length(v.xz);
    v.y = sin(y);
}

void main()
{
    vec2 fragCoord = gl_FragCoord.xy;
    vec4 final = vec4(0.0);
    
    // If camera override is enabled, store override values directly
    if (uCameraOverride == 1) {
        if (fragCoord.x < 1.0 && fragCoord.y < 1.0) {
            // Pixel 0: mouse state - not needed for override, just store zeros
            final = vec4(0.0);
        }
        else if (fragCoord.x < 2.0 && fragCoord.y < 1.0) {
            // Pixel 1: camera direction - calculate from rotation angles
            float pitch = uCameraRotOverride.x;
            float yaw = uCameraRotOverride.y;
            vec3 dir;
            dir.x = cos(pitch) * sin(yaw);
            dir.y = sin(pitch);
            dir.z = cos(pitch) * cos(yaw);
            final = vec4(normalize(dir), 0.0);
        }
        else if (fragCoord.x < 3.0 && fragCoord.y < 1.0) {
            // Pixel 2: camera position - use override directly
            final = vec4(uCameraPosOverride, 0.0);
        }
        
        fragColor = final;
        return;
    }
    
    // Normal camera logic (original code)
    float keyW = uKeyW;
    float keyS = uKeyS;
    float keyA = uKeyA;
    float keyD = uKeyD;
    
    vec4 mouse  = texture(iChannel0, vec2(0.5, 0.5) / iResolution.xy);
    vec4 camDir = texture(iChannel0, vec2(1.5, 0.5) / iResolution.xy);
    vec4 camPos = texture(iChannel0, vec2(2.5, 0.5) / iResolution.xy);
    
    if (dot(camDir, camDir) == 0.0) {
        camDir = vec4(0.0, 0.0, 1.0, 0.0);
    }

    if (fragCoord.x < 1.0 && fragCoord.y < 1.0) {
        // Pixel 0: mouse state
        if (iMouse.z > 0.0) {
            mouse = iMouse;
        } else {
            mouse.z = 0.0;
        }
        final = mouse;
    }
    else if (fragCoord.x < 2.0 && fragCoord.y < 1.0) {
        // Pixel 1: camera direction
        if (iMouse.z > 0.0 && mouse.z > 0.0) {
            mouse = (iMouse - mouse) * 8.0 / iResolution.y;
            rotVec(camDir, mouse.x, mouse.y);
        }
        final = camDir;
    }
    else if (fragCoord.x < 3.0 && fragCoord.y < 1.0) {
        // Pixel 2: camera position
        vec3 camRight = normalize(cross(camDir.xyz, vec3(0.0, 1.0, 0.0)));
        float speed = 0.4;
        final = camPos + (keyW - keyS) * speed * camDir;
        final.xyz += (keyD - keyA) * speed * camRight * -1.0;
    }
    
    fragColor = final;
}