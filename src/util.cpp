#include "util.h"

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <assert.h>
#include <fstream>
#include <functional>
#include <iterator> 
#include <tuple> 
#include <vector> 
#include <vmath.h>

#define SEED 5370157038

using namespace std;
using namespace vmath;

void print_arr(const GLfloat *arr, int size, int row_size) {
	char str[64];

	OutputDebugString("\nPRINTING ARR:\n");

	for (int i = 0; i < size; i++) {
		memset(str, '\0', 64);
		if (arr[i] >= 0) {
			str[0] = ' ';
			sprintf(str + 1, "%.2f ", arr[i]);
		}
		else {
			sprintf(str, "%.2f ", arr[i]);
		}
		OutputDebugString(str);

		if (((i + 1) % row_size) == 0) {
			OutputDebugString("\n");
		}
	}

	OutputDebugString("\nDONE\n");
}

// Temporary workaround to inability to package shader sources with executable.
namespace {
	// Fill these with actual sources before running.
	const char vshader_src[] = R"(#version 450 core

#define CHUNK_WIDTH 16
#define CHUNK_DEPTH 16
#define CHUNK_HEIGHT 256
#define CHUNK_SIZE (CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT)

layout (location = 0) in vec4 position;

layout (std140, binding = 0) uniform UNI_IN
{
	// member			base alignment			base offset		size	aligned offset	extra info
	mat4 mv_matrix;		// 16 (same as vec4)	0				64		0 				(mv_matrix[0])
						//						0						16				(mv_matrix[1])
						//						0						32				(mv_matrix[2])
						//						0						48				(mv_matrix[3])
	mat4 proj_matrix;	// 16 (same as vec4)	64				64		64				(proj_matrix[0])
						//						80						80				(proj_matrix[1])
						//						96						96				(proj_matrix[2])
						//						112						112				(proj_matrix[3])
	ivec3 base_coords;	// 16 (same as vec4)	128				12		128
} uni;

float rand(float seed) {
	return fract(1.610612741 * seed);
}

void main(void)
{
	gl_Position = uni.proj_matrix * uni.mv_matrix * position;
}
)";
	const char fshader_src[] = R"(#version 450 core

out vec4 color;

void main(void)
{
	color = vec4(0.5, 1.0, 0.0, 1.0);
}
)";
	const char gshader_src[] = R"(#version 450 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in uint vs_block_type[];
in vec4 vs_color[];

out vec4 gs_color;

void main(void)
{
	// // don't draw air
	// if (vs_block_type[0] == 0) {
	// 	return; 
	// }

	for (int i = 0; i < 3; i++) {		
		gl_Position = gl_in[i].gl_Position;
		gs_color = vs_color[i];
		EmitVertex();
	}
}
)";
	const char tcsshader_src[] = R"()";
	const char tesshader_src[] = R"()";
	const char cshader_src[] = R"(#version 450 core
#define DEFAULT_DISTANCE 250.0
#define PARTICLE_DISTANCE_FROM_VIEWER -10000.0f

layout( local_size_x = 64*2 ) in; 

uniform float Gravity1 = 1000.0; 
uniform vec3 BlackHolePos1 = vec3(DEFAULT_DISTANCE, DEFAULT_DISTANCE, PARTICLE_DISTANCE_FROM_VIEWER); 
uniform float Gravity2 = 1000.0; 
uniform vec3 BlackHolePos2 = vec3(-DEFAULT_DISTANCE, -DEFAULT_DISTANCE, PARTICLE_DISTANCE_FROM_VIEWER); 
 
uniform float ParticleInvMass = 1.0 / 0.1; 
uniform float DeltaT = 0.05; 
 
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
)";

	GLuint compile_shaders_hardcoded(std::vector <std::tuple<std::string, GLenum>> shader_fnames) {
		GLuint program;
		std::vector <GLuint> shaders; // store compiled shaders

		// for each input shader
		for (const auto&[fname, shadertype] : shader_fnames)
		{
			// get src
			const GLchar * shader_src_ptr;

			switch (shadertype) {
			case GL_VERTEX_SHADER:
				if (vshader_src[0] == '\0') {
					throw "missing vshader source";
				}
				shader_src_ptr = vshader_src;
				break;
			case GL_FRAGMENT_SHADER:
				if (fshader_src[0] == '\0') {
					throw "missing fshader source";
				}
				shader_src_ptr = fshader_src;
				break;
			case GL_GEOMETRY_SHADER:
				if (gshader_src[0] == '\0') {
					throw "missing fshader source";
				}
				shader_src_ptr = gshader_src;
				break;
			case GL_COMPUTE_SHADER:
				if (cshader_src[0] == '\0') {
					throw "missing fshader source";
				}
				shader_src_ptr = cshader_src;
				break;
			default:
				throw "missing a shader source";
			}

			// Create and compile shader
			const GLuint shader = glCreateShader(shadertype); // create empty shader
			glShaderSource(shader, 1, &shader_src_ptr, NULL); // set shader source code
			glCompileShader(shader); // compile shader

			// CHECK IF COMPILATION SUCCESSFUL
			GLint status = GL_TRUE;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
			if (status == GL_FALSE)
			{
				GLint logLen;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
				std::vector <char> log(logLen);
				GLsizei written;
				glGetShaderInfoLog(shader, logLen, &written, log.data());

				OutputDebugString("compilation error with shader ");
				OutputDebugString(fname.c_str());
				OutputDebugString(":\n\n");
				OutputDebugString(log.data());
				OutputDebugString("\n");
				exit(1);
			}

			// Save shader for later
			shaders.push_back(shader);
		}

		// Create program, attach shaders to it, and link it
		program = glCreateProgram(); // create (empty?) program

									 // attach shaders
		for (const GLuint &shader : shaders) {
			glAttachShader(program, shader);
		}

		glLinkProgram(program); // link together all attached shaders

		// CHECK IF LINKING SUCCESSFUL
		GLint status = GL_TRUE;
		glGetProgramiv(program, GL_LINK_STATUS, &status);
		if (status == GL_FALSE)
		{
			GLint logLen;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
			std::vector <char> log(logLen);
			GLsizei written;
			glGetProgramInfoLog(program, logLen, &written, log.data());

			OutputDebugString("linking error with program:\n\n");
			OutputDebugString(log.data());
			OutputDebugString("\n");
			exit(1);
		}


		// Delete the shaders as the program has them now
		for (const GLuint &shader : shaders) {
			glDeleteShader(shader);
		}

		return program;
	}
}

GLuint link_program(GLuint program) {
	// link program w/ error-checking

	glLinkProgram(program); // link together all attached shaders

	// CHECK IF LINKING SUCCESSFUL
	GLint status = GL_TRUE;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		GLint logLen;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
		std::vector <char> log(logLen);
		GLsizei written;
		glGetProgramInfoLog(program, logLen, &written, log.data());

		OutputDebugString("linking error with program:\n\n");
		OutputDebugString(log.data());
		OutputDebugString("\n");
		exit(1);
	}

	return program;
}

GLuint compile_shaders(std::vector <std::tuple<std::string, GLenum>> shader_fnames) {
	//return compile_shaders_hardcoded(shader_fnames);

	GLuint program;
	std::vector <GLuint> shaders; // store compiled shaders

	// for each input shader
	for (const auto&[fname, shadertype] : shader_fnames)
	{
		// load shader src
		std::ifstream shader_file(fname);

		if (!shader_file.is_open()) {
			OutputDebugString("could not open shader file: ");
			OutputDebugString(fname.c_str());
			OutputDebugString("\n");
			exit(1);
		}

		const std::string shader_src((std::istreambuf_iterator<char>(shader_file)), std::istreambuf_iterator<char>());
		const GLchar * shader_src_ptr = shader_src.c_str();

		// Create and compile shader
		const GLuint shader = glCreateShader(shadertype); // create empty shader
		glShaderSource(shader, 1, &shader_src_ptr, NULL); // set shader source code
		glCompileShader(shader); // compile shader

	// CHECK IF COMPILATION SUCCESSFUL
		GLint status = GL_TRUE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
		if (status == GL_FALSE)
		{
			GLint logLen;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
			std::vector <char> log(logLen);
			GLsizei written;
			glGetShaderInfoLog(shader, logLen, &written, log.data());

			OutputDebugString("compilation error with shader ");
			OutputDebugString(fname.c_str());
			OutputDebugString(":\n\n");
			OutputDebugString(log.data());
			OutputDebugString("\n");
			exit(1);
		}

		// Close file, save shader for later
		shader_file.close();
		shaders.push_back(shader);
	}

	// Create program, attach shaders to it, and link it
	program = glCreateProgram(); // create (empty?) program

	// attach shaders
	for (const GLuint &shader : shaders) {
		glAttachShader(program, shader);
	}

	// link program
	program = link_program(program);

	// Delete the shaders as the program has them now
	for (const GLuint &shader : shaders) {
		glDeleteShader(shader);
	}

	return program;
}

// create a (deterministic) 2D random gradient
vec2 rand_grad(int seed, int x, int y) {
	vec2 result;
	auto h = hash<int>();
	srand(h(seed) ^ h(x) ^ h(y));

	for (int i = 0; i < result.size(); i++) {
		result[i] = ((float)rand() / (float)RAND_MAX);
	}

	result = normalize(result);

	return result;
}

// apply ease curve to p, which should be a number btwn 0 and 1
// p -> 3p^2 - 2p^3
double ease(double p) {
	assert(0 <= p && p <= 1 && "Invalid p value.");
	return 3 * pow(p, 2) - 2 * pow(p, 3);
}

// calculate weighted avg
double weighted_avg(double point, double left, double right) {
	assert(0 <= point && point <= 1 && "Invalid p value 2.");

	return left + (right - left)*point;
}

// perlin function gives us y value at (x,z).
// for now call it (x,y) for math-clarity
double noise2d(float x, float y) {
	// get coordinates
	int xMin = (int)floorf(x);
	int xMax = (int)ceilf(x);
	int yMin = (int)floorf(y);
	int yMax = (int)ceilf(y);

	ivec2 coords[4] = {
		{ xMin, yMin },
		{ xMax, yMin },
		{ xMin, yMax },
		{ xMax, yMax },
	};

	int len = sizeof(coords) / sizeof(coords[0]);

	// get "random" gradient for each coordinate
	vec2 grads[4];

	for (int i = 0; i < len; i++) {
		grads[i] = rand_grad(SEED, coords[i][0], coords[i][1]);
	}

	// generate vectors from coordinates to (x,y)
	vec2 vecs_to_xy[4];

	for (int i = 0; i < len; i++) {
		vecs_to_xy[i] = vec2(x, y) - vec2((float)coords[i][0], (float)coords[i][1]);
	}

	// dot product vec_to_xy with grad
	float dots[4];

	for (int i = 0; i < len; i++) {
		dots[i] = dot(grads[i], vecs_to_xy[i]);
	}

	// get x-dimension weight, Sx = ease(x-x0)
	double Sx = ease(x - xMin);

	// apply weighted avg along top corners (where both have y=yMin), and along bottom corners (where both have y=yMax)
	double a = weighted_avg(Sx, dots[0], dots[1]);
	double b = weighted_avg(Sx, dots[2], dots[3]);

	// get y-dimension weight, Sy = ease(y-y0)
	double Sy = ease(y - yMin);

	// apply weighted avg using our two previous weighted avgs
	double z = weighted_avg(Sy, a, b);

	z = (z + 1.0) / 2.0;
	assert(z >= 0.0 && z <= 1.0);

	return z;
}

