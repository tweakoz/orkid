#version 330 core
out vec4 fragColor;

uniform vec3 Color = vec3(1.0, 0.0, 0.0);  // Red color by default

void main()
{
    fragColor = vec4(Color, 1.0);
}
