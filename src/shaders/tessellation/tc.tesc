#version 430
layout(vertices = 3) out;

//in vec3 vPosition[];
in vec3 passPosition[];

out vec3 tcPosition[];

uniform float tessLevelInner;
uniform float tessLevelOuter;

#define ID gl_InvocationID

void main()
{
    //tcPosition[ID] = vPosition[ID];
	tcPosition[ID] = passPosition[ID];
    if (ID == 0) {
        gl_TessLevelInner[0] = tessLevelInner;
        gl_TessLevelOuter[0] = tessLevelOuter;
        gl_TessLevelOuter[1] = tessLevelOuter;
        gl_TessLevelOuter[2] = tessLevelOuter;
    }
}
