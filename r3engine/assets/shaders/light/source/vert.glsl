#version 460 core

layout(location = 0) in vec3 a_Location;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

void main() {
    gl_Position = u_projection * u_view * u_model * vec4(a_Location, 1.0f);
}
