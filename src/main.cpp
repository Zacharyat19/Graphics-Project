#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "camera.h"
#include "shader.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>

static GLFWwindow *window;
static Camera camera;

static ShaderProgram *raytrace_shader = nullptr;
static GLuint quad_vao = 0;

static glm::vec2 mouse_pos;
static bool needs_repaint = true;

struct PanInfo
{
	bool enabled;
	glm::vec3 target;
	glm::vec3 right, up;
	glm::vec2 mouse_down_pos;
};

static PanInfo pan_info = { false, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec2(0.0f) };

static void create_window_and_context();
static void load_shaders();
static void display();

static void on_resize(GLFWwindow *window, int width, int height);
static void on_key(GLFWwindow *window, int key, int scancode, int action, int mods);
static void on_mouse_button(GLFWwindow *window, int button, int action, int mods);
static void on_mouse_motion(GLFWwindow *window, double xpos, double ypos);
static void on_scroll(GLFWwindow *window, double xoffset, double yoffset);

int main()
{
	if (!glfwInit())
	{
		printf("glfwInit failed\n");
		return 1;
	}

	create_window_and_context();
	load_shaders();

	// Core profile needs a VAO bound to draw, even with no vertex attributes.
	glGenVertexArrays(1, &quad_vao);

	glfwSetFramebufferSizeCallback(window, on_resize);
	glfwSetKeyCallback(window, on_key);
	glfwSetMouseButtonCallback(window, on_mouse_button);
	glfwSetCursorPosCallback(window, on_mouse_motion);
	glfwSetScrollCallback(window, on_scroll);

	camera.reset();

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	camera.set_size(width, height);

	glfwShowWindow(window);

	while (!glfwWindowShouldClose(window))
	{
		if (needs_repaint)
			display();
		glfwWaitEvents();
	}

	glDeleteVertexArrays(1, &quad_vao);
	delete raytrace_shader;
	glfwTerminate();
	return 0;
}

static void create_window_and_context()
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);  // required on macOS
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

	window = glfwCreateWindow(800, 600, "Ray Tracing Harmonic Functions", nullptr, nullptr);
	if (!window)
	{
		const char *desc;
		glfwGetError(&desc);
		printf("Failed to create an OpenGL 3.3 window (%s).\n", desc);
		exit(1);
	}

	glfwMakeContextCurrent(window);
	if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress))
	{
		printf("Failed to load OpenGL functions.\n");
		exit(1);
	}

	glfwSwapInterval(1);

	printf("OpenGL Renderer: %s\n", glGetString(GL_RENDERER));
	printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
	printf("GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
}

static void load_shaders()
{
	raytrace_shader = new ShaderProgram();
	raytrace_shader->compile_and_attach(GL_VERTEX_SHADER, "shaders/quad.vert");
	raytrace_shader->compile_and_attach(GL_FRAGMENT_SHADER, "shaders/raytrace.frag");
	raytrace_shader->link();
}

static void repaint()
{
	needs_repaint = true;
}

static void display()
{
	needs_repaint = false;

	glViewport(0, 0, camera.width(), camera.height());
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	raytrace_shader->use();
	
	glm::mat4 invView = glm::inverse(camera.projection() * camera.view());
	glUniformMatrix4fv(glGetUniformLocation(raytrace_shader->id(), "uInvViewProj"), 1, GL_FALSE, glm::value_ptr(invView));

	glBindVertexArray(quad_vao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	raytrace_shader->done();

	glfwSwapBuffers(window);
}

static void on_resize(GLFWwindow *window, int width, int height)
{
	camera.set_size(width, height);
	repaint();
}

static void on_key(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	if (action != GLFW_PRESS)
		return;

	switch (key)
	{
	default:
		break;
	case GLFW_KEY_ESCAPE:
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		break;
	case GLFW_KEY_R:
		camera.reset();
		repaint();
		break;
	case GLFW_KEY_P:
		camera.set_type(camera.type() == Camera::ORTHOGRAPHIC ? Camera::PERSPECTIVE : Camera::ORTHOGRAPHIC);
		repaint();
		break;
	}
}

static void on_mouse_button(GLFWwindow *window, int button, int action, int mods)
{
	if (button != GLFW_MOUSE_BUTTON_1)
		return;

	if (action == GLFW_PRESS && (mods & GLFW_MOD_CONTROL))
	{
		pan_info.enabled = true;
		pan_info.target = camera.target();
		pan_info.right = camera.right();
		pan_info.up = camera.up();
		pan_info.mouse_down_pos = mouse_pos;
	}
	else if (action == GLFW_RELEASE)
	{
		pan_info.enabled = false;
	}
}

static void on_mouse_motion(GLFWwindow *window, double xpos, double ypos)
{
	const glm::vec2 new_pos(static_cast<float>(xpos), static_cast<float>(ypos));
	const glm::vec2 old_pos = mouse_pos;
	mouse_pos = new_pos;

	if (pan_info.enabled)
	{
		int width, height;
		glfwGetWindowSize(window, &width, &height);

		glm::vec2 delta = mouse_pos - pan_info.mouse_down_pos;
		glm::vec2 local = delta / glm::vec2(-width, height);
		if (camera.type() == Camera::ORTHOGRAPHIC)
			local *= camera.orthographic_scale();
		else
			local *= camera.target_distance();
		camera.set_target(pan_info.target + pan_info.right * local.x + pan_info.up * local.y);
		repaint();
	}
	else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS)
	{
		camera.trackball(old_pos, new_pos);
		repaint();
	}
}

static void on_scroll(GLFWwindow *window, double xoffset, double yoffset)
{
	if (!std::isnormal(yoffset))
		return;

	float factor;
	if (yoffset > 0.0)
		factor = 1.0f / 1.125f;
	else
		factor = 1.125f;

	if (camera.type() == Camera::ORTHOGRAPHIC)
		camera.set_orthographic_scale(camera.orthographic_scale() * factor);
	camera.set_target_distance(camera.target_distance() * factor);

	repaint();
}
