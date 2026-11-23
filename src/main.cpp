#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "mesh.h"
#include "vertex.h"
#include "edge.h"
#include "triangle.h"
#include "camera.h"
#include "shader.h"
#include "deformation.h"

#include <learnply2_resources.h>
#include "stb_image.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>
#include <stdexcept>
#include <vector>
#include <cassert>

enum DisplayMode
{
	DISPLAY_WIREFRAME,
	DISPLAY_SILHOUETTE,
	DISPLAY_VERTEX_LIT,
	DISPLAY_NORMAL_MAP
};

static GLFWwindow *window;
static bool use_GL21 = false;
static bool needs_repaint = false;

static Mesh *mesh = nullptr;
static Camera camera;

static glm::vec2 mouse_pos;

static DisplayMode display_mode = DISPLAY_VERTEX_LIT;

static Deformation *deformation = nullptr;

static ShaderProgram *silhouette_shader = nullptr;

static Vertex *hovered_vertex = nullptr;
static bool can_select_vertex = false;

static bool draw_constraints = true;

static double last_frame_time;
static double repaint_timeout;

struct TrackballDragPlaneInfo
{
	bool enabled;
	glm::vec3 target;
	glm::vec3 right, up;
	glm::vec2 mouse_down_pos;
};

static TrackballDragPlaneInfo drag_plane_info = {
	false,
	glm::vec3(0.0f),
	glm::vec3(0.0f), glm::vec3(0.0f),
	glm::vec2(0.0f)
};

typedef unsigned int AxisMask;
enum _AxisMask
{
	AxisX = 0x1,
	AxisY = 0x2,
	AxisZ = 0x4,

	AxisAll = AxisX | AxisY | AxisZ
};

struct VertexDragInfo
{
	bool enabled;
	AxisMask axis;
	Vertex *v;
	glm::vec3 position;
};

static VertexDragInfo vertex_drag_info = {
	false,
	0,
	nullptr,
	glm::vec3(0.0f)
};

static void display_help();

static void create_window_and_context();
static void load_shaders();

static void display();
static void repaint();
static void setup_lighting();

static void on_resize(GLFWwindow *window, int width, int height);
static void on_key(GLFWwindow *window, int key, int scancode, int action, int mods);
static void on_mouse_button(GLFWwindow *window, int button, int action, int mods);
static void on_mouse_motion(GLFWwindow *window, double xpos, double ypos);
static void on_scroll(GLFWwindow *window, double xoffset, double yoffset);

static glm::vec2 mouse_to_ndc(const glm::vec2 &mouse_pos);
static void update_transform_drag();

int main(int argc, char *argv[])
{
	const char *path_to_load = nullptr;
	for (int i = 1; i < argc; ++i)
	{
		char *arg = argv[i];
		if (!strcmp(arg, "--help") || !strcmp(arg, "-h"))
		{
			display_help();
			return 0;
		}
		else if (i == argc - 1)
			path_to_load = arg;
		else
		{
			printf("Unknown switch `%s'\n", arg);
			return 1;
		}
	}

	if (!path_to_load)
	{
		display_help();
		return 1;
	}

	if (!glfwInit())
	{
		printf("glfwInit failed\n");
		return 1;
	}

	create_window_and_context();
	load_shaders();

	glfwSetWindowSizeCallback(window, on_resize);
	glfwSetKeyCallback(window, on_key);
	glfwSetMouseButtonCallback(window, on_mouse_button);
	glfwSetCursorPosCallback(window, on_mouse_motion);
	glfwSetScrollCallback(window, on_scroll);

	mesh = new Mesh(path_to_load);

	deformation = new Deformation(mesh);

	camera.reset();

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	camera.set_size(width, height);

	glfwShowWindow(window);

	GLFWmonitor *monitor = glfwGetWindowMonitor(window);
	if (!monitor)
		monitor = glfwGetPrimaryMonitor();

	if (monitor)
	{
		const GLFWvidmode *mode = glfwGetVideoMode(monitor);
		repaint_timeout = 1.0 / mode->refreshRate;
	}
	else
		repaint_timeout = 1.0 / 60.0;

	display();
	while (!glfwWindowShouldClose(window))
	{
		if (deformation->status() != Deformation::Stopped)
		{
			glfwPollEvents();
			if (deformation->status() != Deformation::Stopped)  // check again, may have been canceled
				deformation->solve();
			repaint();
		}
		else
			glfwWaitEvents();

		if (needs_repaint)
			display();
	}

	delete mesh;
	glfwTerminate();
	return 0;
}

static void display_help()
{
	printf("Usage: learnply2 [options...] <path>\n");
	printf("Where options include:\n");
	printf("  --help -h     Display this message and exit\n");
	printf("and path is a path to a PLY file to display.\n");
}

static void create_window_and_context()
{
	glfwWindowHint(GLFW_DEPTH_BITS, 24);
	glfwWindowHint(GLFW_STENCIL_BITS, 8);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

	printf("Creating window with OpenGL 3.2 compatability context...");
	window = glfwCreateWindow(640, 480, "learnply2", nullptr, nullptr);
	if (!window)
	{
		printf(" Failed.\nTrying 2.1 context...");
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
		window = glfwCreateWindow(640, 480, "learnply2", nullptr, nullptr);
		if (!window)
		{
			const char *desc;
			glfwGetError(&desc);
			printf(" Failed (%s).\n", desc);
			exit(1);
		}
		printf(" OK.\n");

		glfwMakeContextCurrent(window);
		gladLoadGL((GLADloadfunc)glfwGetProcAddress);

		printf("Checking for GL_EXT_geometry_shader4...");

		const char *ext = (const char *)glGetString(GL_EXTENSIONS);	
		bool has_gs = ext && strstr(ext, "GL_EXT_geometry_shader4");
		if (!has_gs)
		{
			printf(" Failed.\n");
			exit(1);
		}

		printf(" OK.\n");

		use_GL21 = true;
	}
	else
	{
		printf(" OK.\n");

		glfwMakeContextCurrent(window);
		gladLoadGL((GLADloadfunc)glfwGetProcAddress);

		use_GL21 = false;
	}

	glfwSwapInterval(1);

	printf("OpenGL Vendor: %s\n", glGetString(GL_VENDOR));
	printf("OpenGL Renderer: %s\n", glGetString(GL_RENDERER));
	printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
	printf("GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
}

static void load_shaders()
{
	silhouette_shader = new ShaderProgram();
	silhouette_shader->compile_and_attach(GL_VERTEX_SHADER, "shaders/silhouette.vert");
	if (use_GL21)
		silhouette_shader->compile_and_attach(GL_GEOMETRY_SHADER, "shaders/silhouette_120.geom");
	else
		silhouette_shader->compile_and_attach(GL_GEOMETRY_SHADER, "shaders/silhouette.geom");
	silhouette_shader->compile_and_attach(GL_FRAGMENT_SHADER, "shaders/silhouette.frag");	
	
	if (use_GL21)
	{
		glProgramParameteriEXT(silhouette_shader->id(), GL_GEOMETRY_INPUT_TYPE_EXT, GL_TRIANGLES_ADJACENCY);
		glProgramParameteriEXT(silhouette_shader->id(), GL_GEOMETRY_OUTPUT_TYPE_EXT, GL_LINE_STRIP);
		glProgramParameteriEXT(silhouette_shader->id(), GL_GEOMETRY_VERTICES_OUT_EXT, 6);
	}

	silhouette_shader->link();
}

static void draw_sphere(const glm::vec3 &p, float r)
{
	constexpr float PI = glm::pi<float>();

	const int N = 4;
	const int M = 8;

	for (int i = 0; i < N; ++i)
	{
		float lat0 = -PI/2 + PI*(float)i/N;
		float lat1 = -PI/2 + PI*(float)(i+1)/N;

		glBegin(GL_TRIANGLE_STRIP);
		for (int j = 0; j <= M; ++j)
		{
			float lon = 2*PI*(float)j/M;

			glm::vec3 n0(cos(lat0)*cos(lon), sin(lat0), cos(lat0)*sin(lon));
			glm::vec3 n1(cos(lat1)*cos(lon), sin(lat1), cos(lat1)*sin(lon));
			glm::vec3 v0 = p + r * n0;
			glm::vec3 v1 = p + r * n1;

			glNormal3f(n1.x, n1.y, n1.z);
			glVertex3f(v1.x, v1.y, v1.z);
			glNormal3f(n0.x, n0.y, n0.z);
			glVertex3f(v0.x, v0.y, v0.z);
		}
		glEnd();
	}
}

static void display()
{
	last_frame_time = glfwGetTime();
	needs_repaint = false;
	glViewport(0, 0, camera.width(), camera.height());

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glEnable(GL_CULL_FACE);
	glFrontFace(GL_FRONT);

	glPolygonMode(GL_BACK, GL_FILL);
	
	glShadeModel(GL_SMOOTH);
	glDisable(GL_LIGHTING);

	setup_lighting();

	const glm::mat4 &projection = camera.projection();
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(glm::value_ptr(projection));

	const glm::mat4 &view = camera.view();
	glm::mat4 modelview = view * mesh->model;
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(glm::value_ptr(modelview));

	switch (display_mode)
	{
	default:
		break;
	case DISPLAY_WIREFRAME: {
		glLineWidth(1.0f);

		glColor3f(0.0f, 0.0f, 0.0f);

		glBegin(GL_LINES);
		for (size_t i = 0; i < mesh->edges.size(); ++i)
		{
			Edge *e = mesh->edges[i];
			glVertex3fv(glm::value_ptr(e->vertices[0]->position));
			glVertex3fv(glm::value_ptr(e->vertices[1]->position));
		}
		glEnd();

		break;
	}
	case DISPLAY_SILHOUETTE: {
		glColor3f(1.0f, 1.0f, 1.0f);

		glBegin(GL_TRIANGLES);
		for (Triangle *t : mesh->triangles)
		{
			glVertex3fv(glm::value_ptr(t->vertices[0]->position));
			glVertex3fv(glm::value_ptr(t->vertices[1]->position));
			glVertex3fv(glm::value_ptr(t->vertices[2]->position));
		}
		glEnd();

		glm::mat4 MVP = camera.projection() * modelview;
		glm::mat3 normal_matrix = glm::inverse(glm::transpose(glm::mat3(modelview)));

		glLineWidth(2.0f);

		glColor3f(0.0f, 0.0f, 0.0f);

		silhouette_shader->use();

		const auto opposite_vertex = [](Triangle *t, int v_idx) {
			Edge *e = t->edges[v_idx];  // edges[vidx] is the edge opposite the vertex
			Triangle *u = e->triangles[0] == t ? e->triangles[1] : e->triangles[0];
			for (int i = 0; i < u->vertices.size(); ++i)
			{
				if (u->vertices[i] != e->vertices[0] && u->vertices[i] != e->vertices[1])
					return u->vertices[i];
			}
			abort();  // unreachable
		};

		glBegin(GL_TRIANGLES_ADJACENCY);

		for (Triangle *t : mesh->triangles)
		{
			glNormal3fv(glm::value_ptr(t->normal));
			glVertex3fv(glm::value_ptr(t->vertices[0]->position));
			glVertex3fv(glm::value_ptr(opposite_vertex(t, 0)->position));
			glVertex3fv(glm::value_ptr(t->vertices[1]->position));
			glVertex3fv(glm::value_ptr(opposite_vertex(t, 1)->position));
			glVertex3fv(glm::value_ptr(t->vertices[2]->position));
			glVertex3fv(glm::value_ptr(opposite_vertex(t, 2)->position));
		}

		glEnd();

		silhouette_shader->done();

		break;
	}
	case DISPLAY_VERTEX_LIT: {
		glEnable(GL_LIGHTING);


		glBegin(GL_TRIANGLES);
		for (Triangle *t : mesh->triangles)
		{
			for (Vertex *v : t->vertices)
			{
				glNormal3fv(glm::value_ptr(v->normal));
				glVertex3fv(glm::value_ptr(v->position));
			}
		}
		glEnd();

		break;
	}
	case DISPLAY_NORMAL_MAP: {
		glDisable(GL_LIGHTING);

		glBegin(GL_TRIANGLES);
		for (Triangle *t : mesh->triangles)
		{
			for (Vertex *v : t->vertices)
			{
				glm::vec3 color = glm::abs(v->normal);
				glColor3fv(glm::value_ptr(color));
				glVertex3fv(glm::value_ptr(v->position));
			}
		}
		glEnd();
		break;
	}
	}

	glEnable(GL_LIGHTING);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	if (draw_constraints)
	{
		if (deformation->actuator())
		{
			glm::vec3 R = glm::inverse(mesh->model) * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
			float r = R.x;

			GLfloat mat_diffuse[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
			glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
			draw_sphere(deformation->actuator()->position, 0.025f * r);

			deformation->validate_regions();

			mat_diffuse[0] = 1.0f;
			mat_diffuse[1] = 0.0f;
			mat_diffuse[2] = 1.0f;
			mat_diffuse[3] = 0.0f;
			glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
			for (Vertex *v : deformation->constrained_vertices())
				draw_sphere(v->position, 0.0125f * r);
		}
	}

	if (hovered_vertex)
	{
		glDisable(GL_LIGHTING);

		glDisable(GL_DEPTH_TEST);
		glColor3f(1.0f, 0.0f, 0.0f);
		glPointSize(8.0f);
		glBegin(GL_POINTS);
		glVertex3fv(glm::value_ptr(hovered_vertex->position));
		glEnd();
	}

	if (vertex_drag_info.enabled && vertex_drag_info.axis != AxisAll)
	{
		glDisable(GL_LIGHTING);
		glEnable(GL_DEPTH_TEST);
		glLineWidth(2.0f);

		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(glm::value_ptr(camera.view()));

		glm::vec3 p_world = glm::vec3(mesh->model * glm::vec4(vertex_drag_info.position, 1.0f));

		if (vertex_drag_info.axis & AxisX)
		{
			glColor3f(1.0f, 0.0f, 0.0f);
			glBegin(GL_LINES);
			glVertex3f(-1000.0f, p_world.y, p_world.z);
			glVertex3f(1000.0f, p_world.y, p_world.z);
			glEnd();
		}

		if (vertex_drag_info.axis & AxisY)
		{
			glColor3f(0.0f, 1.0f, 0.0f);
			glBegin(GL_LINES);
			glVertex3f(p_world.x, -1000.0f, p_world.z);
			glVertex3f(p_world.x, 1000.0f, p_world.z);
			glEnd();
		}

		if (vertex_drag_info.axis & AxisZ)
		{
			glColor3f(0.0f, 0.0f, 1.0f);
			glBegin(GL_LINES);
			glVertex3f(p_world.x, p_world.y, -1000.0f);
			glVertex3f(p_world.x, p_world.y, 1000.0f);
			glEnd();
		}
	}

	glfwSwapBuffers(window);
}

static void repaint()
{
	needs_repaint = true;

	// display immediately if been too long
	double time = glfwGetTime();
	double dt = time = last_frame_time;
	if (dt >= repaint_timeout)
		display();
}

static void setup_lighting()
{
	GLfloat light_ambient0[] = { 0.3f, 0.3f, 0.3f, 1.0f };
	GLfloat light_diffuse0[] = { 0.7f, 0.7f, 0.7f, 1.0f };
	GLfloat light_specular0[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	GLfloat light_ambient1[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	GLfloat light_diffuse1[] = { 0.5f, 0.5f, 0.5f, 1.0f };
	GLfloat light_specular1[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	GLfloat light_ambient2[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	GLfloat light_diffuse2[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	GLfloat light_specular2[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	GLfloat light_position[] = { 0.0f, 0.0f, 0.0f, 1.0f };

	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHT1);

	glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse0);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular0);
	glLightfv(GL_LIGHT1, GL_AMBIENT, light_ambient1);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, light_diffuse1);
	glLightfv(GL_LIGHT1, GL_SPECULAR, light_specular1);

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(glm::value_ptr(camera.view()));

	light_position[0] = 5.5f;
	light_position[1] = 0.0f;
	light_position[2] = 0.0f;
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	light_position[0] = -0.1f;
	light_position[1] = 0.0f;
	light_position[2] = 0.0f;
	glLightfv(GL_LIGHT2, GL_POSITION, light_position);

	const GLfloat mat_diffuse[4] = {1.0f, 1.0f, 0.0f, 1.0f};
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
}

static void on_resize(GLFWwindow *window, int width, int height)
{
	int w, h;
	glfwGetFramebufferSize(window, &w, &h);
	
	camera.set_size(w, h);
	repaint();
}

static void on_key(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	switch (key)
	{
	default:
		break;
	case GLFW_KEY_ESCAPE:
		if (action == GLFW_PRESS)
		{
			if (deformation->status() != Deformation::Stopped)
				deformation->cancel();
		}
		break;
	case GLFW_KEY_R:
		if (action == GLFW_PRESS)
		{
			camera.reset();
			repaint();
		}
		break;
	case GLFW_KEY_P:
		if (action == GLFW_PRESS)
		{
			camera.set_type(camera.type() == Camera::ORTHOGRAPHIC ? Camera::PERSPECTIVE : Camera::ORTHOGRAPHIC);
			repaint();
		}
		break;
	case GLFW_KEY_1:
		if (action == GLFW_PRESS)
		{
			display_mode = DISPLAY_WIREFRAME;
			repaint();
		}
		break;
	case GLFW_KEY_2:
		if (action == GLFW_PRESS)
		{
			display_mode = DISPLAY_SILHOUETTE;
			repaint();
		}
		break;
	case GLFW_KEY_3:
		if (action == GLFW_PRESS)
		{
			display_mode = DISPLAY_VERTEX_LIT;
			repaint();
		}
		break;
	case GLFW_KEY_4:
		if (action == GLFW_PRESS)
		{
			display_mode = DISPLAY_NORMAL_MAP;
			repaint();
		}
		break;
	case GLFW_KEY_LEFT_ALT:
		if (action == GLFW_PRESS)
		{
			can_select_vertex = true;
		}
		else if (action == GLFW_RELEASE)
		{
			can_select_vertex = false;
			if (hovered_vertex)
			{
				hovered_vertex = nullptr;
				repaint();
			}
		}
		break;
	case GLFW_KEY_G:
		if (action == GLFW_PRESS)
		{
			if (!vertex_drag_info.enabled && deformation->actuator())
			{
				vertex_drag_info.enabled = true;
				vertex_drag_info.axis = AxisAll;
				vertex_drag_info.v = deformation->actuator();
				vertex_drag_info.position = vertex_drag_info.v->position;
			}
		}
		break;
	case GLFW_KEY_X:
		if (vertex_drag_info.enabled)
		{
			if (mods & GLFW_MOD_SHIFT)
				vertex_drag_info.axis = AxisY | AxisZ;
			else
				vertex_drag_info.axis = AxisX;
			update_transform_drag();
		}
		break;
	case GLFW_KEY_Y:
		if (vertex_drag_info.enabled)
		{
			if (mods & GLFW_MOD_SHIFT)
				vertex_drag_info.axis = AxisX | AxisZ;
			else
				vertex_drag_info.axis = AxisY;
			update_transform_drag();
		}
		break;
	case GLFW_KEY_Z:
		if (vertex_drag_info.enabled)
		{
			if (mods & GLFW_MOD_SHIFT)
				vertex_drag_info.axis = AxisX | AxisY;
			else
				vertex_drag_info.axis = AxisZ;
			update_transform_drag();
		}
		break;
	case GLFW_KEY_C:
		if (vertex_drag_info.enabled)
		{
			vertex_drag_info.axis = AxisAll;
			update_transform_drag();
		}
		break;
	case GLFW_KEY_ENTER:
		if (action == GLFW_PRESS && (mods & GLFW_MOD_CONTROL))
		{
			if (deformation->status() == Deformation::Paused)
				deformation->accept();  // accept the current state
			else
				deformation->solve();  // start the solver
			repaint();
		}
		break;
	case GLFW_KEY_H:
		if (action == GLFW_PRESS)
		{
			draw_constraints = !draw_constraints;
			if (draw_constraints)
				printf("Constraints shown\n");
			else
				printf("Constraints hidden\n");
			repaint();
		}
		break;
	case GLFW_KEY_L:
		if (action == GLFW_PRESS)
		{
			deformation->set_weight_type(deformation->weight_type() == Edge::UniformWeights ? Edge::CordWeights : Edge::UniformWeights);
		}
	}
}

static void on_mouse_button(GLFWwindow *window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_1)
	{
		if (action == GLFW_PRESS)
		{
			if (vertex_drag_info.enabled)
			{
				if (mods & GLFW_MOD_CONTROL)
				{
					vertex_drag_info.enabled = false;

					repaint();
					return;
				}
			}
			else if (can_select_vertex)
			{
				deformation->set_actuator(hovered_vertex);
			}

			if (mods & GLFW_MOD_CONTROL)
			{
				drag_plane_info.enabled = true;
				drag_plane_info.target = camera.target();
				drag_plane_info.right = camera.right();
				drag_plane_info.up = camera.up();
				drag_plane_info.mouse_down_pos = mouse_pos;
			}
		}
		else if (action == GLFW_RELEASE)
		{
			if (drag_plane_info.enabled)
				drag_plane_info.enabled = false;
		}
	}
	else if (button == GLFW_MOUSE_BUTTON_2)
	{
		if (vertex_drag_info.enabled)
		{
			vertex_drag_info.v->position = vertex_drag_info.position;
			vertex_drag_info.enabled = false;
			
			mesh->compute_normals_and_areas();

			repaint();
		}
	}
}

static void on_mouse_motion(GLFWwindow *window, double xpos, double ypos)
{
	const glm::vec2 new_pos(static_cast<float>(xpos), static_cast<float>(ypos));
	const glm::vec2 old_pos = mouse_pos;
	mouse_pos = new_pos;

	hovered_vertex = nullptr;

	if (drag_plane_info.enabled)
	{
		glm::vec2 delta = mouse_pos - drag_plane_info.mouse_down_pos;
		glm::vec2 local = delta / glm::vec2(-camera.width(), camera.height());
		if (camera.type() == Camera::ORTHOGRAPHIC)
			local *= camera.orthographic_scale();
		else
			local *= camera.target_distance();
		glm::vec3 world = drag_plane_info.right*local.x + drag_plane_info.up*local.y;
		camera.set_target(drag_plane_info.target + world);
		repaint();

		return;
	}
	else if (vertex_drag_info.enabled)
	{
		update_transform_drag();

		// no return -> allow camera rotation while editing vertex positions
	}
	else if (can_select_vertex)
	{
		glm::vec2 ndc = mouse_to_ndc(new_pos);

		// Pick vertex and display if different
		Vertex *v = camera.pick_vertex(mesh, ndc);
		if (v != hovered_vertex)
		{
			hovered_vertex = v;
			repaint();
		}

		return;
	}

	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS)
	{
		camera.trackball(old_pos, new_pos);
		repaint();
	}
}

static void on_scroll(GLFWwindow *window, double xoffset, double yoffset)
{
	if (!std::isnormal(yoffset))
		return;

	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL))
	{
		float strength = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) ? 0.0625f : 0.25f;
		float d = deformation->actuation_distance();
		if (yoffset > 0.0)
			d -= strength;
		else
			d += strength;
		deformation->set_actuation_distance(d);
	}
	else
	{
		if (camera.type() == Camera::ORTHOGRAPHIC)
		{
			if (yoffset > 0.0)
				camera.set_orthographic_scale(camera.orthographic_scale() / 1.125f);
			else
				camera.set_orthographic_scale(camera.orthographic_scale() * 1.125f);
		}

		float d = camera.target_distance();
		if (yoffset > 0.0)
			d /= 1.125f;
		else
			d *= 1.125f;
		camera.set_target_distance(d);
	}

	repaint();
}

// Mouse coords -> ndc
static glm::vec2 mouse_to_ndc(const glm::vec2 &mouse_pos)
{
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	glm::vec2 ndc = 2.0f * mouse_pos / glm::vec2(width, height);
	return glm::vec2(ndc.x - 1.0f, 1.0f - ndc.y);
}

static void update_transform_drag()
{
	glm::vec4 p = mesh->model * glm::vec4(vertex_drag_info.position, 1.0f);

	Ray ray = camera.ndc_to_world_ray(mouse_to_ndc(mouse_pos));
	float t = ray.isect_plane(p, camera.front());
	glm::vec3 mouse_world_space_proj = ray(t);
	if (std::isfinite(mouse_world_space_proj.x) &&
		std::isfinite(mouse_world_space_proj.y) &&
		std::isfinite(mouse_world_space_proj.z))
	{
		if (vertex_drag_info.axis & AxisX) p.x = mouse_world_space_proj.x;
		if (vertex_drag_info.axis & AxisY) p.y = mouse_world_space_proj.y;
		if (vertex_drag_info.axis & AxisZ) p.z = mouse_world_space_proj.z;
		p = glm::inverse(mesh->model) * p;
		vertex_drag_info.v->position = glm::vec3(p);

		mesh->compute_normals_and_areas();
		repaint();
	}
}
