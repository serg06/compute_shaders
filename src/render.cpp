#include "render.h"
#include "util.h"

#include "GLFW/glfw3.h"

#include <string>
#include <time.h>
#include <tuple>
#include <vector>
#include <vmath.h>

using namespace vmath;

// setup glfw window
// windowInfo -> info for setting up this window
// window -> pointer to a window pointer, which we're gonna set up
void setup_glfw(GlfwInfo* windowInfo, GLFWwindow** window) {
	if (!glfwInit()) {
		MessageBox(NULL, "Failed to initialize GLFW.", "GLFW error", MB_OK);
		return;
	}

	// OpenGL 4.5
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);

	// using OpenGL core profile
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// remove deprecated functionality (might as well, 'cause I'm using gl3w)
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	// disable MSAA
	glfwWindowHint(GLFW_SAMPLES, windowInfo->msaa);

	// debug mode
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, windowInfo->debug);

	// create window
	*window = glfwCreateWindow(windowInfo->width, windowInfo->height, windowInfo->title.c_str(), nullptr, nullptr);

	if (!*window) {
		MessageBox(NULL, "Failed to create window.", "GLFW error", MB_OK);
		return;
	}

	// set this window as current window
	glfwMakeContextCurrent(*window);

	// lock mouse into screen, for camera controls
	glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetInputMode(*window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

	// finally init gl3w
	if (gl3wInit()) {
		MessageBox(NULL, "Failed to initialize OpenGL.", "gl3w error", MB_OK);
		return;
	}

	// disable vsync
	glfwSwapInterval(0);
}

namespace {
	void setup_render_program(OpenGLInfo* glInfo) {
		// list of shaders to create program with
		// TODO: Embed these into binary somehow - maybe generate header file with cmake.
		std::vector <std::tuple<std::string, GLenum>> shader_fnames = {
			{ "../src/simple.vs.glsl", GL_VERTEX_SHADER },
			//{"../src/simple.tcs.glsl", GL_TESS_CONTROL_SHADER },
			//{"../src/simple.tes.glsl", GL_TESS_EVALUATION_SHADER },
			//{ "../src/simple.gs.glsl", GL_GEOMETRY_SHADER },
			{ "../src/simple.fs.glsl", GL_FRAGMENT_SHADER },
		};

		// create program
		glInfo->rendering_program = compile_shaders(shader_fnames);

		//// use our program object for rendering
		//glUseProgram(glInfo->rendering_program);
	}

	void setup_compute_program(OpenGLInfo* glInfo) {
		// list of shaders to create program with
		// TODO: Embed these into binary somehow - maybe generate header file with cmake.
		std::vector <std::tuple<std::string, GLenum>> shader_fnames = {
			{ "../src/simple.cs.glsl", GL_COMPUTE_SHADER },
		};

		// create program
		glInfo->compute_program = compile_shaders(shader_fnames);
	}

	void setup_opengl_vao_particle(OpenGLInfo* glInfo) {
		// create VAO for postions

		// vao: create VAO for Quads, so we can tell OpenGL how to use it when it's bound
		glCreateVertexArrays(1, &glInfo->vao_particle);

		// vao: enable all Quad's attributes, 1 at a time
		glEnableVertexArrayAttrib(glInfo->vao_particle, glInfo->position_attr_idx);

		// vao: set up formats for cube's attributes, 1 at a time
		glVertexArrayAttribFormat(glInfo->vao_particle, glInfo->position_attr_idx, 4, GL_FLOAT, GL_FALSE, 0);

		// vao: match attributes to binding indices
		glVertexArrayAttribBinding(glInfo->vao_particle, glInfo->position_attr_idx, glInfo->position_bidx);

		// done vao
		glBindVertexArray(0);
	}

	void setup_opengl_uniforms(OpenGLInfo* glInfo) {
		// create buffers
		glCreateBuffers(1, &glInfo->trans_buf);

		// bind them to binding points
		glBindBufferBase(GL_UNIFORM_BUFFER, glInfo->trans_buf_uni_bidx, glInfo->trans_buf);

		// allocate
		// - mat4:  projection matrix
		// - mat4:  model-view matrix
		// - ivec3: current minichunk base coordinate
		glNamedBufferStorage(glInfo->trans_buf, sizeof(mat4) * 2 + sizeof(ivec3), NULL, GL_DYNAMIC_STORAGE_BIT);
	}

	void setup_opengl_storage_blocks(OpenGLInfo* glInfo) {
		srand(time(NULL));

		// size of buffer we'll need
		GLuint bufSize = TOTAL_PARTICLES * sizeof(vec4);

		// set initial particle positions
		vec4* initPos = new vec4[TOTAL_PARTICLES];
		for (int i = 0; i < TOTAL_PARTICLES; i++) {
			vec3 particle;
			float rand_0_1 = (rand() / (float)RAND_MAX); // generate random number 0.0f - 1.0f
			particle[0] = (rand() / (float)RAND_MAX) * 20.0f - 10.0f;
			particle[1] = (rand() / (float)RAND_MAX) * 20.0f - 10.0f;
			particle[2] = PARTICLE_DISTANCE_FROM_VIEWER;
			//for (int j = 0; j < 3; j++) {
			//	particle[j] = rand_0_1;
			//}
			for (int j = 0; j < 3; j++) {
				initPos[i][j] = particle[j];
			}
			initPos[i][3] = 1.0f;
		}

		// set initial velocities
		vec4* initVel = new vec4[TOTAL_PARTICLES];
		memset(initVel, 0, bufSize);

		// position buffer
		glCreateBuffers(1, &glInfo->posBuf);
		glNamedBufferData(glInfo->posBuf, bufSize, &initPos[0], GL_DYNAMIC_DRAW);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, glInfo->posBufSSBidx, glInfo->posBuf);

		// velocity buffer
		glCreateBuffers(1, &glInfo->velBuf);
		glNamedBufferData(glInfo->velBuf, bufSize, &initVel[0], GL_DYNAMIC_DRAW);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, glInfo->velBufSSBidx, glInfo->velBuf);

		// free
		delete[] initPos, initVel;
	}

	void setup_opengl_extra_props(OpenGLInfo* glInfo) {
		glPointSize(1.0f);
		//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CW);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
	}
}



void setup_opengl(OpenGLInfo* glInfo) {
	// setup shaders
	setup_render_program(glInfo);
	setup_compute_program(glInfo);

	// setup VAOs
	setup_opengl_storage_blocks(glInfo);
	setup_opengl_vao_particle(glInfo);

	// setup uniforms
	setup_opengl_uniforms(glInfo);

	// setup extra [default] properties
	setup_opengl_extra_props(glInfo);
}
