#version 330 core
out vec4 fragColor;

uniform sampler2D uChannel0;
uniform vec2 iResolution;
uniform vec4 iMouse;
uniform float uKeyW;
uniform float uKeyS;
uniform float uKeyA;
uniform float uKeyD;

void rotVec(inout vec4 v, float x, float y)
{
    y = asin(v.y) - y;
    if(-1.56 > y){y =-1.56;};
    if( 1.56 < y){y = 1.56;};
    float l = cos(y);
    x = -x;
    v.xz = vec2(v.x*cos(x) - v.z*sin(x),
                v.x*sin(x) + v.z*cos(x));
    v.xz*= l/length(v.xz);
    v.y  = sin(y);
}

void main()
{
    vec2 fragCoord = gl_FragCoord.xy;
    float keyW  = uKeyW;
    float keyS  = uKeyS;
    float keyA  = uKeyA;
    float keyD  = uKeyD;
    vec4 mouse  = texture( uChannel0, vec2(0.5,.5)/iResolution.xy);
    vec4 camDir = texture( uChannel0, vec2(1.5,.5)/iResolution.xy);
    vec4 camPos = texture( uChannel0, vec2(2.5,.5)/iResolution.xy);
    vec4 final = vec4(0.);

    if(dot(camDir,camDir)==0.){camDir = vec4(0.,0.,1.,0.);}

    if(fragCoord.x == 0.5)
    {
        if(iMouse.z>0.){ mouse = iMouse; }
        else           { mouse.z = 0.;   }
        final = mouse;
    }
    if(fragCoord.x == 1.5)
    {
        if(iMouse.z>0. && mouse.z>0.)
        {
            mouse = (iMouse-mouse)*8./iResolution.y;
            rotVec(camDir, mouse.x, mouse.y);
        }
        final = camDir;
    }
    if(fragCoord.x == 2.5)
    {
        vec3 camRight = normalize(cross(camDir.xyz, vec3(0.0, 1.0, 0.0)));
        float speed = 0.4;
        final = camPos + (keyW - keyS) * speed * camDir;
        final.xyz += (keyD - keyA) * speed * camRight*-1.;
    }
    fragColor = final;
}
