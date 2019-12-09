#version 450 core
#define DEFAULT_DISTANCE 250.0
#define PARTICLE_DISTANCE_FROM_VIEWER -10000.0f

layout( local_size_x = 64*2 ) in; 

uniform float Gravity1 = 1000.0; 
uniform vec3 BlackHolePos1 = vec3(DEFAULT_DISTANCE, DEFAULT_DISTANCE, PARTICLE_DISTANCE_FROM_VIEWER); 
uniform float Gravity2 = 1000.0; 
uniform vec3 BlackHolePos2 = vec3(-DEFAULT_DISTANCE, -DEFAULT_DISTANCE, PARTICLE_DISTANCE_FROM_VIEWER); 
 
uniform float ParticleInvMass = 1.0 / 0.1; 
uniform float DeltaT = 0.005; 
 
layout(std430, binding=0) buffer Pos { 
  vec4 Position[]; 
}; 
layout(std430, binding=1) buffer Vel { 
  vec4 Velocity[]; 
}; 
 
void main() { 
  uint idx = gl_GlobalInvocationID.x; 
 
  vec3 p = Position[idx].xyz; 
  vec3 v = Velocity[idx].xyz; 
 
  // Force from black hole #1 
  vec3 d = BlackHolePos1 - p; 
  vec3 force = (Gravity1 / length(d)) * normalize(d); 
   
  // Force from black hole #2 
  d = BlackHolePos2 - p; 
  force += (Gravity2 / length(d)) * normalize(d); 
 
  // Apply simple Euler integrator 
  vec3 a = force * ParticleInvMass; 
  Position[idx] = vec4( 
        p + v * DeltaT + 0.5 * a * DeltaT * DeltaT, 1.0); 
  Velocity[idx] = vec4( v + a * DeltaT, 0.0); 
}
