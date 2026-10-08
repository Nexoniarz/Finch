// Everything examples/opengl.fch needs from C. GL_GLEXT_PROTOTYPES makes <GL/glext.h>
// declare the modern (OpenGL 3+) functions, which Linux's libGL exports directly.
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>
