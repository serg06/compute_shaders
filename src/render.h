// All the info about our rendering practices
#ifndef __RENDER_H__
#define __RENDER_H__

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <vmath.h>

#define TRANSFORM_BUFFER_COORDS_OFFSET (2*sizeof(vmath::mat4))
#define TOTAL_PARTICLES (64*2 * 10000)
#define PARTICLE_DISTANCE_FROM_VIEWER -10000.0f

// all the GLFW info for our app
struct GlfwInfo {
	std::string title = "OpenGL";
	bool debug = GL_TRUE;
	bool msaa = GL_FALSE;
	int width = 800;
	int height = 600;
	float vfov = 59.0f; // vertical fov -- 59.0 vfov = 90.0 hfov
	float mouseX_Sensitivity = 0.25f;
	float mouseY_Sensitivity = 0.25f;
};

// all the OpenGL info for our game
struct OpenGLInfo {
	// program
	GLuint rendering_program;
	GLuint compute_program; // compute updated particle positions

	// VAOs
	GLuint vao_particle;

	// buffers
	GLuint trans_buf; // transformations buffer - currently stores view and projection transformations.
	GLuint posBuf, velBuf;

	// binding points
	GLuint position_bidx = 0;

	// uniform binding points
	const GLuint trans_buf_uni_bidx = 0; // transformation buffer's uniform binding-point index

	// shader storage binding points
	GLuint posBufSSBidx = 0;
	GLuint velBufSSBidx = 1;

	// attribute indices
	GLuint position_attr_idx = 0;
};

struct Quad3D {
	uint8_t block;
	vmath::ivec3 corners[2];
};

void setup_glfw(GlfwInfo*, GLFWwindow**);
void setup_opengl(OpenGLInfo*);

#endif /* __RENDER_H__ */
