#include "game.h"

#include "render.h"
#include "util.h"

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <algorithm> // howbig?
#include <assert.h>
#include <cmath>
#include <math.h>
#include <numeric>
#include <string>
#include <time.h> 
#include <vmath.h> // TODO: Upgrade version, or use better library?
#include <windows.h>

#define CAMERA_HEIGHT 0.0f
#define PLAYER_RADIUS 0.0f
#define CHUNK_WIDTH 0.0f

// 1. TODO: Apply C++11 features
// 2. TODO: Apply C++14 features
// 3. TODO: Apply C++17 features
// 4. TODO: Make everything more object-oriented.
//		That way, I can define functions without having to declare them first, and shit.
//		And more good shit comes of it too.
//		Then from WinMain(), just call MyApp a = new MyApp, a.run(); !!

using namespace std;
using namespace vmath;

// Windows main
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	glfwSetErrorCallback(glfw_onError);
	App::app = new App();
	App::app->run();
}

void App::run() {
	// create glfw window
	setup_glfw(&windowInfo, &window);

	// set callbacks
	glfwSetWindowSizeCallback(window, glfw_onResize);
	glfwSetKeyCallback(window, glfw_onKey);
	glfwSetMouseButtonCallback(window, glfw_onMouseButton);
	glfwSetCursorPosCallback(window, glfw_onMouseMove);
	glfwSetScrollCallback(window, glfw_onMouseWheel);

	// set debug message callback
	if (windowInfo.debug) {
		if (gl3wIsSupported(4, 3))
		{
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback((GLDEBUGPROC)gl_onDebugMessage, nullptr);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
		}
	}

	// Start up app
	startup();

	// run until user presses ESC or tries to close window
	last_render_time = (float)glfwGetTime(); // updated in render()
	while ((glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_RELEASE) && (!glfwWindowShouldClose(window))) {
		// run rendering function
		render((float)glfwGetTime());

		// display drawing buffer on screen
		glfwSwapBuffers(window);

		// poll window system for events
		glfwPollEvents();
	}

	shutdown();
}

void App::startup() {
	/* SET VARS */

	// prepare opengl
	setup_opengl(&glInfo);
}


void App::render(float time) {
	char buf[256];

	// change in time
	const float dt = time - last_render_time;
	last_render_time = time;

	/* CHANGES IN WORLD */

	/* TRANSFORMATION MATRICES */

	// Create Model->World matrix
	mat4 model_world_matrix = vmath::mat4::identity();

	// Create World->View matrix
	mat4 world_view_matrix = vmath::mat4::identity();

	// Combine them into Model->View matrix
	mat4 model_view_matrix = world_view_matrix * model_world_matrix;

	// Update projection matrix too, in case if width/height changed
	// NOTE: Careful, if (nearplane/farplane) ratio is too small, things get fucky.
	mat4 proj_matrix = perspective(
		(float)windowInfo.vfov, // virtual fov
		(float)windowInfo.width / (float)windowInfo.height, // aspect ratio
		0.9f,
		10000.0f
	);

	/* BACKGROUND / SKYBOX */

	// Draw background color
	const GLfloat black[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	const GLfloat sky_blue[] = { 135 / 255.0f, 206 / 255.0f, 235 / 255.0f, 1.0f };
	glClearBufferfv(GL_COLOR, 0, black);
	glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0); // used for depth test somehow

	// Update transformation buffer with matrices
	glNamedBufferSubData(glInfo.trans_buf, 0, sizeof(model_view_matrix), model_view_matrix);
	glNamedBufferSubData(glInfo.trans_buf, sizeof(model_view_matrix), sizeof(proj_matrix), proj_matrix); // proj matrix

	// PRINT FPS
	sprintf(buf, "Drawing (took %d ms) (render distance = %d)\n", (int)(dt * 1000), min_render_distance);
	OutputDebugString(buf);

	/* COMPUTE */
	glUseProgram(glInfo.compute_program);
	glDispatchCompute(TOTAL_PARTICLES / 64 * 20, 1, 1);

	/* RENDER */
	glUseProgram(glInfo.rendering_program);
	glBindVertexArray(glInfo.vao_particle);

	// vao: attach position buffer
	glVertexArrayVertexBuffer(glInfo.vao_particle, glInfo.position_bidx, glInfo.posBuf, 0, sizeof(vec4));

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	glDrawArrays(GL_POINTS, 0, TOTAL_PARTICLES);
}

void App::onKey(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// ignore unknown keys
	if (key == GLFW_KEY_UNKNOWN) {
		return;
	}

	// handle key presses
	if (action == GLFW_PRESS) {
		held_keys[key] = true;

		// N = toggle noclip
		if (key == GLFW_KEY_N) {
			noclip = !noclip;
		}

		// + = increase render distance
		if (key == GLFW_KEY_KP_ADD || key == GLFW_KEY_EQUAL) {
			min_render_distance++;
		}

		// - = decrease render distance
		if (key == GLFW_KEY_KP_SUBTRACT || key == GLFW_KEY_MINUS) {
			if (min_render_distance > 0) {
				min_render_distance--;
			}
		}

		// p = cycle poylgon mode
		if (key == GLFW_KEY_P) {
			GLint polygon_mode;
			glGetIntegerv(GL_POLYGON_MODE, &polygon_mode);
			switch (polygon_mode) {
			case GL_FILL:
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				break;
			case GL_LINE:
				glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
				break;
			case GL_POINT:
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				break;
			}
		}

		// c = toggle face culling
		if (key == GLFW_KEY_C) {
			GLboolean is_enabled = glIsEnabled(GL_CULL_FACE);
			if (is_enabled) {
				glDisable(GL_CULL_FACE);
			}
			else {
				glEnable(GL_CULL_FACE);
			}
		}
	}

	// handle key releases
	if (action == GLFW_RELEASE) {
		held_keys[key] = false;
	}
}

void App::onMouseMove(GLFWwindow* window, double x, double y)
{
	// bonus of using deltas for yaw/pitch:
	// - can cap easily -- if we cap without deltas, and we move 3000x past the cap, we'll have to move 3000x back before mouse moves!
	// - easy to do mouse sensitivity
	double delta_x = x - last_mouse_x;
	double delta_y = y - last_mouse_y;

	// update pitch/yaw
	char_yaw += (float)(windowInfo.mouseX_Sensitivity * delta_x);
	char_pitch += (float)(windowInfo.mouseY_Sensitivity * delta_y);

	// cap pitch
	char_pitch = clamp(char_pitch, -90.0f, 90.0f);

	// update old values
	last_mouse_x = x;
	last_mouse_y = y;
}

void App::onResize(GLFWwindow* window, int width, int height) {
	windowInfo.width = width;
	windowInfo.height = height;

	int px_width, px_height;
	glfwGetFramebufferSize(window, &px_width, &px_height);
	glViewport(0, 0, px_width, px_height);
}

void App::onDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message) {
	char buf[4096];
	char *bufp = buf;

	// ignore non-significant error/warning codes (e.g. 131185 = "buffer created")
	if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

	bufp += sprintf(bufp, "---------------\n");
	bufp += sprintf(bufp, "OpenGL debug message (%d): %s\n", id, message);

	switch (source)
	{
	case GL_DEBUG_SOURCE_API:             bufp += sprintf(bufp, "Source: API"); break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   bufp += sprintf(bufp, "Source: Window System"); break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER: bufp += sprintf(bufp, "Source: Shader Compiler"); break;
	case GL_DEBUG_SOURCE_THIRD_PARTY:     bufp += sprintf(bufp, "Source: Third Party"); break;
	case GL_DEBUG_SOURCE_APPLICATION:     bufp += sprintf(bufp, "Source: Application"); break;
	case GL_DEBUG_SOURCE_OTHER:           bufp += sprintf(bufp, "Source: Other"); break;
	} bufp += sprintf(bufp, "\n");

	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR:               bufp += sprintf(bufp, "Type: Error"); break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: bufp += sprintf(bufp, "Type: Deprecated Behaviour"); break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  bufp += sprintf(bufp, "Type: Undefined Behaviour"); break;
	case GL_DEBUG_TYPE_PORTABILITY:         bufp += sprintf(bufp, "Type: Portability"); break;
	case GL_DEBUG_TYPE_PERFORMANCE:         bufp += sprintf(bufp, "Type: Performance"); break;
	case GL_DEBUG_TYPE_MARKER:              bufp += sprintf(bufp, "Type: Marker"); break;
	case GL_DEBUG_TYPE_PUSH_GROUP:          bufp += sprintf(bufp, "Type: Push Group"); break;
	case GL_DEBUG_TYPE_POP_GROUP:           bufp += sprintf(bufp, "Type: Pop Group"); break;
	case GL_DEBUG_TYPE_OTHER:               bufp += sprintf(bufp, "Type: Other"); break;
	} bufp += sprintf(bufp, "\n");

	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH:         bufp += sprintf(bufp, "Severity: high"); break;
	case GL_DEBUG_SEVERITY_MEDIUM:       bufp += sprintf(bufp, "Severity: medium"); break;
	case GL_DEBUG_SEVERITY_LOW:          bufp += sprintf(bufp, "Severity: low"); break;
	case GL_DEBUG_SEVERITY_NOTIFICATION: bufp += sprintf(bufp, "Severity: notification"); break;
	} bufp += sprintf(bufp, "\n");
	bufp += sprintf(bufp, "\n");

	OutputDebugString(buf);
	exit(-1);
}

void App::onMouseButton(int button, int action) {
	// left click
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {

	}

	// right click
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {

	}
}

void App::onMouseWheel(int pos) {}

namespace {
	/* GLFW/GL callback functions */

	void glfw_onError(int error, const char* description) {
		MessageBox(NULL, description, "GLFW error", MB_OK);
	}

	void glfw_onKey(GLFWwindow* window, int key, int scancode, int action, int mods) {
		App::app->onKey(window, key, scancode, action, mods);
	}

	void glfw_onMouseMove(GLFWwindow* window, double x, double y) {
		App::app->onMouseMove(window, x, y);
	}

	void glfw_onResize(GLFWwindow* window, int width, int height) {
		App::app->onResize(window, width, height);
	}

	void glfw_onMouseButton(GLFWwindow* window, int button, int action, int mods)
	{
		App::app->onMouseButton(button, action);
	}

	static void glfw_onMouseWheel(GLFWwindow* window, double xoffset, double yoffset)
	{
		App::app->onMouseWheel(yoffset);
	}

	void APIENTRY gl_onDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, GLvoid* userParam) {
		App::app->onDebugMessage(source, type, id, severity, length, message);
	}
}
