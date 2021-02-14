#include<assert.h>

#include<GLFW/glfw3.h>

int main() {

	int rc = glfwInit();

	assert(rc);

	GLFWwindow *window = glfwCreateWindow(1024, 764, "renderer", NULL, NULL);

	assert(window);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	return 0;
}