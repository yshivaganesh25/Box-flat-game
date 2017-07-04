#include <iostream>
#include <stdlib.h>
#include <cmath>
#include <fstream>
#include <vector>
#include <string.h>
#include <cstring>
#include <string>
#include <pthread.h>

#include <stdlib.h>
#include <assert.h>
#include <ao/ao.h>


#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace std;

static const int BUF_SIZE = 4096;

struct WavHeader {
    char id[4]; //should contain RIFF
    int32_t totalLength;
    char wavefmt[8];
    int32_t format; // 16 for PCM
    int16_t pcm; // 1 for PCM
    int16_t channels;
    int32_t frequency;
    int32_t bytesPerSecond;
    int16_t bytesByCapture;
    int16_t bitsPerSample;
    char data[4]; // "data"
    int32_t bytesInData;
};

struct VAO {
    GLuint VertexArrayID;
    GLuint VertexBuffer;
    GLuint ColorBuffer;

    GLenum PrimitiveMode;
    GLenum FillMode;
    int NumVertices;

    float x,y,z;

};
typedef struct VAO VAO;

struct point {
  float xc;
  float yc;
  int pos;
};


int level = 1;
int levelchange = 0;
int displaycuboid = 1;
int switchescount = 0;
int fragilecount = 0;
int notilescount = 0;
int onfragile = 0;
int bridgeflag[103];
struct point* hole = (struct point*)malloc(sizeof(struct point));
vector<struct point*> switches;
vector<struct point*> fragiletiles;
vector<struct point*> notiles;
vector<vector<int> >bridgetiles;
double stepedonfragiletime,currenttime,fellofftime;

float camera_rotation_angle = -45;

double e3 = 13,
       t1 = 0,t2 = 0,t3 = 0,
       u1 = -1,u2 = 1,u3 = 1,
       e1 = 20*cos(camera_rotation_angle*M_PI/180.0f),
       e2 = 20*sin(camera_rotation_angle*M_PI/180.0f);

struct GLMatrices {
	glm::mat4 projection;
	glm::mat4 model;
	glm::mat4 view;
	GLuint MatrixID;
} Matrices;

GLuint programID;

/* Function to load Shaders - Use it as it is */
GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path) {

	// Create the shaders
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Read the Vertex Shader code from the file
	std::string VertexShaderCode;
	std::ifstream VertexShaderStream(vertex_file_path, std::ios::in);
	if(VertexShaderStream.is_open())
	{
		std::string Line = "";
		while(getline(VertexShaderStream, Line))
			VertexShaderCode += "\n" + Line;
		VertexShaderStream.close();
	}

	// Read the Fragment Shader code from the file
	std::string FragmentShaderCode;
	std::ifstream FragmentShaderStream(fragment_file_path, std::ios::in);
	if(FragmentShaderStream.is_open()){
		std::string Line = "";
		while(getline(FragmentShaderStream, Line))
			FragmentShaderCode += "\n" + Line;
		FragmentShaderStream.close();
	}

	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Compile Vertex Shader
	printf("Compiling shader : %s\n", vertex_file_path);
	char const * VertexSourcePointer = VertexShaderCode.c_str();
	glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
	glCompileShader(VertexShaderID);

	// Check Vertex Shader
	glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> VertexShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
	fprintf(stdout, "%s\n", &VertexShaderErrorMessage[0]);

	// Compile Fragment Shader
	printf("Compiling shader : %s\n", fragment_file_path);
	char const * FragmentSourcePointer = FragmentShaderCode.c_str();
	glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
	glCompileShader(FragmentShaderID);

	// Check Fragment Shader
	glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> FragmentShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
	fprintf(stdout, "%s\n", &FragmentShaderErrorMessage[0]);

	// Link the program
	fprintf(stdout, "Linking program\n");
	GLuint ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);
	glLinkProgram(ProgramID);

	// Check the program
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> ProgramErrorMessage( max(InfoLogLength, int(1)) );
	glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
	fprintf(stdout, "%s\n", &ProgramErrorMessage[0]);

	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);

	return ProgramID;
}

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

void quit(GLFWwindow *window)
{
    glfwDestroyWindow(window);
    glfwTerminate();
//    exit(EXIT_SUCCESS);
}


/* Generate VAO, VBOs and return VAO handle */
struct VAO* create3DObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat* color_buffer_data, GLenum fill_mode=GL_FILL)
{
    struct VAO* vao = new struct VAO;
    vao->PrimitiveMode = primitive_mode;
    vao->NumVertices = numVertices;
    vao->FillMode = fill_mode;

    // Create Vertex Array Object
    // Should be done after CreateWindow and before any other GL calls
    glGenVertexArrays(1, &(vao->VertexArrayID)); // VAO
    glGenBuffers (1, &(vao->VertexBuffer)); // VBO - vertices
    glGenBuffers (1, &(vao->ColorBuffer));  // VBO - colors

    glBindVertexArray (vao->VertexArrayID); // Bind the VAO
    glBindBuffer (GL_ARRAY_BUFFER, vao->VertexBuffer); // Bind the VBO vertices
    glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), vertex_buffer_data, GL_STATIC_DRAW); // Copy the vertices into VBO
    glVertexAttribPointer(
                          0,                  // attribute 0. Vertices
                          3,                  // size (x,y,z)
                          GL_FLOAT,           // type
                          GL_FALSE,           // normalized?
                          0,                  // stride
                          (void*)0            // array buffer offset
                          );

    glBindBuffer (GL_ARRAY_BUFFER, vao->ColorBuffer); // Bind the VBO colors
    glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), color_buffer_data, GL_STATIC_DRAW);  // Copy the vertex colors
    glVertexAttribPointer(
                          1,                  // attribute 1. Color
                          3,                  // size (r,g,b)
                          GL_FLOAT,           // type
                          GL_FALSE,           // normalized?
                          0,                  // stride
                          (void*)0            // array buffer offset
                          );

    return vao;
}

/* Generate VAO, VBOs and return VAO handle - Common Color for all vertices */
struct VAO* create3DObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat red, const GLfloat green, const GLfloat blue, GLenum fill_mode=GL_FILL)
{
    GLfloat* color_buffer_data = new GLfloat [3*numVertices];
    for (int i=0; i<numVertices; i++) {
        color_buffer_data [3*i] = red;
        color_buffer_data [3*i + 1] = green;
        color_buffer_data [3*i + 2] = blue;
    }

    return create3DObject(primitive_mode, numVertices, vertex_buffer_data, color_buffer_data, fill_mode);
}

/* Render the VBOs handled by VAO */
void draw3DObject (struct VAO* vao)
{
    // Change the Fill Mode for this object
    glPolygonMode (GL_FRONT_AND_BACK, vao->FillMode);

    // Bind the VAO to use
    glBindVertexArray (vao->VertexArrayID);

    // Enable Vertex Attribute 0 - 3d Vertices
    glEnableVertexAttribArray(0);
    // Bind the VBO to use
    glBindBuffer(GL_ARRAY_BUFFER, vao->VertexBuffer);

    // Enable Vertex Attribute 1 - Color
    glEnableVertexAttribArray(1);
    // Bind the VBO to use
    glBindBuffer(GL_ARRAY_BUFFER, vao->ColorBuffer);

    // Draw the geometry !
    glDrawArrays(vao->PrimitiveMode, 0, vao->NumVertices); // Starting from vertex 0; 3 vertices total -> 1 triangle
}

/**************************
 * Customizable functions *
 **************************/
vector<vector<VAO*> > fl1,fl2,fl3;

int fl1m[11][11] = {//0  1  2  3  4  5  6  7  8  9  10
                    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},//0
                    { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, -4},//1
                    { 0, 1, 1, 1,-1, 1, 0, 1, 1, 1, 1},//2
                    { 0, 1, 1, 4, 4, 4, 4, 1, 1, 1, 1},//3
                    { 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, -3},//4
                    { 0, 1, 1, 1, 1, 2, 1, 1, 1, 2, 1},//5
                    { 0, 1, 0, 0, 0, 0, 0, 0, 1, 2, 1},//6
                    { 0, 1, 0, 0, 0, 0, 0, 0 , 0, 1, 1},//7
                    { 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1},//8
                    { 0, 1, 3, 3, 3, 3, 3, 3, 3, 1, 1},//9
                    { 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1},//10
                  };

int fl2m[11][11] = {//0  1  2  3  4  5  6  7  8  9  10
                    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},//0
                    { 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0},//1
                    { 0, 1,-4, 1, 4, 4, 4, 4, -1, 1, 0},//2
                    { 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0},//3
                    { 0, 0, 3, 0, 0, 0, 0, 1, 1, 1, 0},//4
                    { 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0},//5
                    { 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0},//6
                    { 0, 0, 3, 0, 0, 0, 0, 0 ,0, 0, 0},//7
                    { 0, 0, 3, 0, 0, 0, 0, 1, 1, 1, 1},//8
                    { 0, 0, 1, 1, 1, 1, 1, -3, 1, 1, 1},//9
                    { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1},//10
                  };

int fl3m[11][11] = {//0  1  2  3  4  5  6  7  8  9  10
                    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},//0
                    { 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0},//1
                    { 0, 1,-1, 4, 0, 0, 0, 0, 0, 0, 0},//2
                    { 0, 2, 2, 4, 0, 0, 0, 0, 0, 0, 0},//3
                    { 0, 0, 2, 4, 0, 0, 0, 0, 0, 0, 0},//4
                    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0},//5
                    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 1, -3},//6
                    { 0, 0, 0, 4, 0, -4, 3, 3 ,3, 3, 1},//7
                    { 0, 0, 0, 4, 2, 0, 0, 0, 2, 1, 1},//8
                    { 0, 0, 0, 4, 2, 2, 2, 1, 1, 1, 0},//9
                    { 0, 0, 0, 1, 1, 1, 1, 2, 2, 1, 1},//10
                  };



vector<VAO*> cuboid;

float X,Y,Z;
char orientation = 'z';
float a = 2.5; // edge enght of tile
int noofmoves = 0;
double timetaken[6];

double xpos,ypos;

int left_drag = 0;
bool gameover = false;
int score = 0;
int fbwidth = 800,fbheight = 800;


void *playaudio (void* parg)
{
  const char* filepath = (const char*)parg;
  ao_device* device;
  ao_sample_format format;
  int defaultDriver;
  WavHeader header;

  std::ifstream file;
  file.open(filepath, std::ios::binary | std::ios::in);

  file.read(header.id, sizeof(header.id));
  assert(!std::memcmp(header.id, "RIFF", 4)); //is it a WAV file?
  file.read((char*)&header.totalLength, sizeof(header.totalLength));
  file.read(header.wavefmt, sizeof(header.wavefmt)); //is it the right format?
  assert(!std::memcmp(header.wavefmt, "WAVEfmt ", 8));
  file.read((char*)&header.format, sizeof(header.format));
  file.read((char*)&header.pcm, sizeof(header.pcm));
  file.read((char*)&header.channels, sizeof(header.channels));
  file.read((char*)&header.frequency, sizeof(header.frequency));
  file.read((char*)&header.bytesPerSecond, sizeof(header.bytesPerSecond));
  file.read((char*)&header.bytesByCapture, sizeof(header.bytesByCapture));
  file.read((char*)&header.bitsPerSample, sizeof(header.bitsPerSample));
  file.read(header.data, sizeof(header.data));
  file.read((char*)&header.bytesInData, sizeof(header.bytesInData));

  ao_initialize();

  defaultDriver = ao_default_driver_id();

  memset(&format, 0, sizeof(format));
  format.bits = header.format;
  format.channels = header.channels;
  format.rate = header.frequency;
  format.byte_format = AO_FMT_LITTLE;

  device = ao_open_live(defaultDriver, &format, NULL);
  if (device == NULL) {
      std::cout << "Unable to open driver" << std::endl;

  }

  char* buffer = (char*)malloc(BUF_SIZE * sizeof(char));

  // determine how many BUF_SIZE chunks are in file
  int fSize = header.bytesInData;
  int bCount = fSize / BUF_SIZE;

  for (int i = 0; i < bCount; ++i) {
      file.read(buffer, BUF_SIZE);
      ao_play(device, buffer, BUF_SIZE);
  }

  int leftoverBytes = fSize % BUF_SIZE;
  //std::cout << leftoverBytes;
  file.read(buffer, leftoverBytes);
  memset(buffer + leftoverBytes, 0, BUF_SIZE - leftoverBytes);
  ao_play(device, buffer, BUF_SIZE);
  ao_close(device);
  ao_shutdown();


}
void playwav (const char* x)
{
  pthread_t      tid;  // thread ID
  pthread_attr_t attr; // thread attribute

  // set thread detachstate attribute to DETACHED
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  // create the thread
  const char* parg = x;
  pthread_create(&tid, &attr, playaudio, (void *)parg);

}
/*executed when something is pressed*/


void keyboard (GLFWwindow* window, int key, int scancode, int action, int mods)
{
     // Function is called first on GLFW_PRESS.

    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_ESCAPE:
                quit(window);
                break;

            case GLFW_KEY_T :
                camera_rotation_angle = -45;
                e1 = 20*cos(camera_rotation_angle*M_PI/180.0f);
                e2 = 20*sin(camera_rotation_angle*M_PI/180.0f);
                e3 = 13;
                t1 = t2 = t3 =0;
                u1 = -1;
                u2 = 1;
                u3 = 1;


                break;

            case GLFW_KEY_S :

                e1 = e2 = 0;e3 = 8;
                t1 = t2 = t3 = 0;
                u1 = u3 =0;
                u2 = 1;

                break;

            case GLFW_KEY_J :

                if (orientation == 'z')
                {//cout << "ok" <<endl;
                  e1 = X - a;
                  e2 = Y;
                  e3 = Z;
                  t1 = e1 - 2*a;
                  t2 = e2 ;
                  t3 = e3 - a;
                }
                else if (orientation == 'x')
                {
                  e1 = X -1.5*a;
                  e2 = Y;
                  e3 = Z;
                  t1 = e1 - 2*a;
                  t2 = e2;
                  t3 = e3 - 0.5*a;
                }
                else if (orientation == 'y')
                {
                  e1 = X - a;
                  e2 = Y;
                  e3 = Z;
                  t1 = e1 -2* a;
                  t2 = e2;
                  t3 = e3 - 0.5*a;
                }


                u1 = 0;u2 = 0;u3 = 1;


                break;
            case GLFW_KEY_L :

                if (orientation == 'z')
                {//cout << "ok" <<endl;
                  e1 = X + a;
                  e2 = Y;
                  e3 = Z;
                  t1 = e1 + 2*a;
                  t2 = e2 ;
                  t3 = e3 - a;
                }
                else if (orientation == 'x')
                {
                  e1 = X +1.5*a;
                  e2 = Y;
                  e3 = Z;
                  t1 = e1 + 2*a;
                  t2 = e2;
                  t3 = e3 - 0.5*a;
                }
                else if (orientation == 'y')
                {
                  e1 = X + a;
                  e2 = Y;
                  e3 = Z;
                  t1 = e1 +2* a;
                  t2 = e2;
                  t3 = e3 - 0.5*a;
                }


                u1 = 0;u2 = 0;u3 = 1;

                break;
            case GLFW_KEY_K :

                  if (orientation == 'z')
                  {//cout << "ok" <<endl;
                    e1 = X;
                    e2 = Y  - a;
                    e3 = Z;
                    t1 = e1;
                    t2 = e2 - 2*a;
                    t3 = e3 - a;
                  }
                  else if (orientation == 'x')
                  {
                    e1 = X;
                    e2 = Y  - a;
                    e3 = Z;
                    t1 = e1;
                    t2 = e2 - 2*a;
                    t3 = e3 - 0.5*a;
                  }
                  else if (orientation == 'y')
                  {
                    e1 = X;
                    e2 = Y  - 1.5*a;
                    e3 = Z;
                    t1 = e1;
                    t2 = e2 - 2*a;
                    t3 = e3 - 0.5*a;
                  }


                  u1 = 0;u2 = 0;u3 = 1;

                break;
            case GLFW_KEY_I :


                if (orientation == 'z')
                {//cout << "ok" <<endl;
                  e1 = X;
                  e2 = Y  + a;
                  e3 = Z;
                  t1 = e1;
                  t2 = e2 + 2*a;
                  t3 = e3 - a;
                }
                else if (orientation == 'x')
                {
                  e1 = X;
                  e2 = Y  + a;
                  e3 = Z;
                  t1 = e1;
                  t2 = e2 + 2*a;
                  t3 = e3 - 0.5*a;
                }
                else if (orientation == 'y')
                {
                  e1 = X;
                  e2 = Y  + 1.5*a;
                  e3 = Z;
                  t1 = e1;
                  t2 = e2 + 2*a;
                  t3 = e3 - 0.5*a;
                }


                u1 = 0;u2 = 0;u3 = 1;


                break;

            case GLFW_KEY_LEFT :

                noofmoves++;

                if (orientation == 'z')
                {
                  X = X - 1.5*a;
                  Z = Z - 0.5*a;
                  orientation = 'x';
                }
                else if (orientation == 'x')
                {
                  X = X - 1.5*a;
                  Z = Z + 0.5*a;
                  orientation = 'z';
                }
                else if (orientation == 'y')
                {
                  X = X -a;
                }


                break;

            case GLFW_KEY_RIGHT :

                noofmoves++;

                if (orientation == 'z')
                {
                  X = X + 1.5*a;
                  Z = Z - 0.5*a;
                  orientation = 'x';
                }
                else if (orientation == 'x')
                {
                  X = X + 1.5*a;
                  Z = Z + 0.5*a;
                  orientation = 'z';
                }
                else if (orientation == 'y')
                {
                  X = X + a;
                }


                break;


            case GLFW_KEY_UP :

                noofmoves++;

                if (orientation == 'z')
                {
                  Y = Y + 1.5*a;
                  Z = Z - 0.5*a;
                  orientation = 'y';
                }
                else if (orientation == 'x')
                {
                  Y = Y + a;
                }
                else if (orientation == 'y')
                {
                  Y = Y + 1.5*a;
                  Z = Z + 0.5*a;
                  orientation = 'z';
                }


                break;



            case GLFW_KEY_DOWN :

                  noofmoves++;

                  if (orientation == 'z')
                  {
                    Z = Z - 0.5*a;
                    Y = Y - 1.5*a;
                    orientation = 'y';
                  }
                  else if (orientation == 'x')
                  {
                    Y = Y - a;
                  }
                  else if (orientation == 'y')
                  {
                    Y = Y - 1.5*a;
                    Z = Z + 0.5*a;
                    orientation = 'z';
                  }


                  break;

            default:
                break;
        }
    }
}

/* Executed for character input (like in text boxes) */
void keyboardChar (GLFWwindow* window, unsigned int key)
{
	switch (key) {
		case 'Q':
		case 'q':
            quit(window);
            break;
		default:
			break;
	}
}

double p1 = 0,p2 = 0,q1 = 0,q2 = 0;

/* Executed when a mouse button is pressed/released */
void mouseButton (GLFWwindow* window, int button, int action, int mods)
{
    switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:



            if (action == GLFW_PRESS)
            {
              left_drag = 1;
              p1 = xpos;
              q1 = ypos;
              //cout <<"pressed " << p1 << " " << q1 << endl;
            }

            if (action == GLFW_RELEASE)
            {
              left_drag = 0;
              p2 = xpos;
              q2 = ypos;
              //cout <<"released " << p2 << " " << q2 << endl;

            }

            break;

        case GLFW_MOUSE_BUTTON_RIGHT:
            if (action == GLFW_PRESS)
            {

            }
            if (action == GLFW_RELEASE)
            {

            }
            break;
        default:
            break;
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
  if (yoffset == 1)
  {

  }
  else if (yoffset == -1)
  {

  }
}

void cursor_pos_callback(GLFWwindow* window, double x, double y)
{

  xpos = x/20;
  ypos = y/20;
  //cout << fbheight <<" "<<  fbwidth <<endl;
}



/* Executed when window is resized to 'width' and 'height' */
/* Modify the bounds of the screen here in glm::ortho or Field of View in glm::Perspective */
void reshapeWindow (GLFWwindow* window, int width, int height)
{
    fbwidth=width, fbheight=height;
    /* With Retina display on Mac OS X, GLFW's FramebufferSize
     is different from WindowSize */
    glfwGetFramebufferSize(window, &fbwidth, &fbheight);

	GLfloat fov = 90.0f;

	// sets the viewport of openGL renderer
	glViewport (0, 0, (GLsizei) fbwidth, (GLsizei) fbheight);

	// set the projection matrix as perspective
	/* glMatrixMode (GL_PROJECTION);
	   glLoadIdentity ();
	   gluPerspective (fov, (GLfloat) fbwidth / (GLfloat) fbheight, 0.1, 500.0); */
	// Store the projection matrix in a variable for future use
    // Perspective projection for 3D views
    // Matrices.projection = glm::perspective (fov, (GLfloat) fbwidth / (GLfloat) fbheight, 0.1f, 500.0f);

    // Ortho projection for 2D views
    Matrices.projection = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, 0.1f, 500.0f);
}



VAO* createsegment(GLfloat v1, GLfloat v2 , GLfloat v3,
                    GLfloat v4, GLfloat v5, GLfloat v6,
                    GLfloat c1, GLfloat c2, GLfloat c3,
                    GLfloat c4, GLfloat c5, GLfloat c6,
                    GLfloat c7, GLfloat c8, GLfloat c9,
                    float x,float y,float z, GLfloat r
                  )
{
  VAO* segment;
  GLfloat vertex_buffer_data [] = {
    v1, v2,v3, // vertex 0
    v4,r,v6, // vertex 1
    r*sin(10*M_PI/180.0f),r*cos(10*M_PI/180.0f),0, // vertex 2
  };

  GLfloat color_buffer_data [] = {
    c1,c2 ,c3, // color 0
    c4,c5,c6, // color 1
    c7,c8,c9, // color 2
  };

  // create3DObject creates and returns a handle to a VAO that can be used later
  segment = create3DObject(GL_TRIANGLES, 3, vertex_buffer_data, color_buffer_data, GL_FILL);
  segment->x = x;
  segment->y = y;
  segment->z = z;

  return segment;
}






// Creates the triangle object used in this sample code
VAO* createTriangle (
                      GLfloat v1, GLfloat v2 , GLfloat v3,
                      GLfloat v4, GLfloat v5, GLfloat v6,
                      GLfloat v7, GLfloat v8, GLfloat v9,
                      GLfloat c1, GLfloat c2, GLfloat c3,
                      GLfloat c4, GLfloat c5, GLfloat c6,
                      GLfloat c7, GLfloat c8, GLfloat c9,
                      float x,float y,float z
)
{
  VAO* triangle;
  GLfloat vertex_buffer_data [] = {
    v1,v2,v3,
    v4,v5,v6,
    v7,v8,v9,
  };

  GLfloat color_buffer_data [] = {
    c1,c2,c3,
    c4,c5,c6,
    c7,c8,c9,
  };

  triangle = create3DObject(GL_TRIANGLES, 3, vertex_buffer_data, color_buffer_data, GL_LINE );
  triangle->x = x;
  triangle->y = y;
  triangle->z = z;

  return triangle;

}


VAO* createrectangle (GLfloat v1, GLfloat v2 , GLfloat v3,
                      GLfloat v4, GLfloat v5, GLfloat v6,
                      GLfloat v7, GLfloat v8, GLfloat v9,
                      GLfloat v10, GLfloat v11, GLfloat v12,
                      GLfloat c1, GLfloat c2, GLfloat c3,
                      GLfloat c4, GLfloat c5, GLfloat c6,
                      GLfloat c7, GLfloat c8, GLfloat c9,
                      GLfloat c10, GLfloat c11, GLfloat c12,
                      float x,float y,float z
                    )
{
  VAO* rectangle;

  GLfloat vertex_buffer_data [] = {
    v1,v2,v3, // vertex 1
    v4,v5,v6, // vertex 2
    v7, v8,v9, // vertex 3

    v7, v8,v9, // vertex 3
    v10, v11,v12, // vertex 4
    v1,v2,v3  // vertex 1
  };

  GLfloat color_buffer_data1 [] = {
    c1,c2,c3, // vertex 1
    c4,c5,c6, // vertex 2
    c7, c8,c9, // vertex 3

    c7, c8,c9, // vertex 3
    c10, c11,c12, // vertex 4
    c1,c2,c3  // vertex 1
 };

 rectangle = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data1, GL_FILL);
 rectangle->x = x;
 rectangle->y = y;
 rectangle->z = z;

 return rectangle;

}

vector<VAO*> createcube (float l, float b, float h,
                      GLfloat c1, GLfloat c2, GLfloat c3,
                      GLfloat c4, GLfloat c5, GLfloat c6,
                      GLfloat c7, GLfloat c8, GLfloat c9,
                      float x,float y,float z)
{
  vector<VAO*> cube;

  VAO* top = createrectangle(-l/2.0,-b/2.0,h/2.0, l/2.0,-b/2.0,h/2.0, l/2.0,b/2.0,h/2.0, -l/2.0,b/2.0,h/2.0,
                                 c1,c2,c3, c1,c2,c3, c1,c2,c3, c1,c2,c3, x,y,z);

  VAO* bottom = createrectangle(-l/2.0,-b/2.0,-h/2.0, l/2.0,-b/2.0,-h/2.0, l/2.0,b/2.0,-h/2.0, -l/2.0,b/2.0,-h/2.0,
                                c1,c2,c3, c1,c2,c3, c1,c2,c3, c1,c2,c3,  x,y,z);

  VAO* back = createrectangle(-l/2.0,b/2.0,-h/2.0, l/2.0,b/2.0,-h/2.0, l/2.0,b/2.0,h/2.0, -l/2.0,b/2.0,h/2.0,
                                c4,c5,c6, c4,c5,c6, c4,c5,c6, c4,c5,c6, x,y,z);

  VAO* front = createrectangle(-l/2.0,-b/2.0,-h/2.0, l/2.0,-b/2.0,-h/2.0, l/2.0,-b/2.0,h/2.0, -l/2.0,-b/2.0,h/2.0,
                                 c4,c5,c6, c4,c5,c6, c4,c5,c6, c4,c5,c6,x,y,z);

  VAO* left = createrectangle(-l/2.0,-b/2.0,-h/2.0, -l/2.0,b/2.0,-h/2.0, -l/2.0,b/2.0,h/2.0, -l/2.0,-b/2.0,h/2.0,
                                c7,c8,c9, c7,c8,c9, c7,c8,c9, c7,c8,c9, x,y,z);

  VAO* right = createrectangle(l/2.0,-b/2.0,-h/2.0, l/2.0,b/2.0,-h/2.0, l/2.0,b/2.0,h/2.0, l/2.0,-b/2.0,h/2.0,
                                c7,c8,c9, c7,c8,c9, c7,c8,c9, c7,c8,c9, x,y,z);

  cube.push_back(front);
  cube.push_back(back);
  cube.push_back(top);
  cube.push_back(bottom);
  cube.push_back(left);
  cube.push_back(right);

  return cube;

}

void createcuboid()
{
  cuboid = createcube(a,a,2*a, 1,1,0, 0,1,0, 0,1,0, 5*a,-5*a,a+0.25);
  X = 5*a;
  Y = -5*a;
  Z = a+0.25;
}

vector<vector<VAO*> > createfloor(int fl[11][11])
{
  vector<vector<VAO*> > Fl;
  int i,j;
  vector<VAO*> tile;
  for (i = 1; i <=10; i++)
  {//cout << i << endl;
    for (j = 1; j <= 10; j++)
    {//cout << j << endl;

      if (fl[i][j] == 0)
      {
        tile = createcube(a,a,0.5, 0,0,0, 0,0,0, 0,0,0, -a*(5-j),-a*(i-5),0 );
      }
      else if (fl[i][j] == -1)// hole
      {
        tile = createcube(a,a,0.5, 1,1,0, 1,1,0, 1,1,0, -a*(5-j),-a*(i-5),0 );
      }
      else if (fl[i][j] == 1)//normal white
      {
        tile = createcube(a,a,0.5, 1,1,1, 69.0/255.0,234.0/255.0,9.0/255.0, 69.0/255.0,234.0/255.0,9.0/255.0, -a*(5-j),-a*(i-5),0 );
      }
      else if (fl[i][j] == 2)// fragile red
      {
        tile = createcube(a,a,0.5, 1,0,0, 69.0/255.0,234.0/255.0,9.0/255.0, 69.0/255.0,234.0/255.0,9.0/255.0, -a*(5-j),-a*(i-5),0 );
      }
      else if (fl[i][j] >= 3)// bridge grey
      {
        tile = createcube(a,a,0.5, 163.0/255.0,168.0/255.0,175.0/255.0, 69.0/255.0,234.0/255.0,9.0/255.0, 69.0/255.0,234.0/255.0,9.0/255.0, -a*(5-j),-a*(i-5),0 );
      }
      else if (fl[i][j] <= -3)// switch blue
      {
        tile = createcube(a,a,0.5, 0,0,1, 69.0/255.0,234.0/255.0,9.0/255.0, 69.0/255.0,234.0/255.0,9.0/255.0, -a*(5-j),-a*(i-5),0 );
      }

      Fl.push_back(tile);
    }
  }

  return Fl;

}

void createfloorvariables(int fl[11][11])
{
  int i,j;
  vector<VAO*> tile;
  for (i = 1; i <=10; i++)
  {//cout << i << endl;
    for (j = 1; j <= 10; j++)
    {//cout << j << endl;

      if (fl[i][j] == 0)
      {
        notilescount++;
        notiles[notilescount]->xc = -a*(5-j);
        notiles[notilescount]->yc = -a*(i-5);
        //cout << "notiles at " <<   notiles[notilescount]->xc <<" "<<  notiles[notilescount]->yc<<endl;
        notiles[notilescount]->pos = (i-1)*10+j;
        bridgeflag[(i-1)*10 + j] = -2;
      }
      else if (fl[i][j] == -1)// hole
      {
        hole->xc = -a*(5-j);
        hole->yc = -a*(i-5);
        hole->pos = (i-1)*10 + j;
        bridgeflag[(i-1)*10 + j] = -1;
      }
      else if (fl[i][j] == 1)//normal white
      {
        bridgeflag[(i-1)*10 + j] = -1;
      }
      else if (fl[i][j] == 2)// fragile red
      {
        fragilecount++;
        fragiletiles[fragilecount]->xc = -a*(5-j);
        fragiletiles[fragilecount]->yc = -a*(i-5);
        fragiletiles[fragilecount]->pos = (i-1)*10+j;
        bridgeflag[(i-1)*10 + j] = -1;
      }
      else if (fl[i][j] >= 3)// bridge grey
      {
        bridgetiles[fl[i][j]-3].push_back((i-1)*10 + j);
        //cout << "yes it is bridge" << (i-1)*10 + j <<endl;
        bridgeflag[(i-1)*10 + j] = 0;
        //cout << "bridgetiles at" << -a*(5-j) << " " <<-a*(i-5)<<endl;
      }

      else if (fl[i][j] <= -3)// switch blue
      {
        switches[-fl[i][j]-3]->xc = -a*(5-j);
        switches[-fl[i][j]-3]->yc = -a*(i-5);
        //cout << "switch " << switches[0]->xc << " " << switches[0]->yc << endl;
        switchescount++;
        bridgeflag[(i-1)*10 + j] = -1;
      }

    }
  }

  for (int j = 1; j <= notilescount;j++)
  {
    //cout << "notile" << notiles[j]->xc << " " << notiles[j]->yc << endl;
  }
  //cout << notilescount << "########################" <<endl;

}


void createfloors (int m1[11][11],int m2[11][11], int m3[11][11])
{
  fl1 = createfloor(m1);
  fl2 = createfloor(m2);
  fl3 = createfloor(m3);
}


void isfalling ()
{
  if ( X > 12.5 || X < -10 || Y > 10 || Y < -12.5)
  {
    //cout << "no" << endl;
    displaycuboid = 0;
    const char* audio = "died.wav";
    playaudio((void*) audio);
  }
  else
  {

    for (int j = 1; j <= notilescount; j++)
    {//cout  << "notiles are at "<< notiles[j]->xc <<" " << notiles[j]->yc << endl;
      if (
          (orientation == 'z' && X == notiles[j]->xc && Y == notiles[j]->yc) ||
          (orientation == 'y' && notiles[j]->xc == X && (notiles[j]->yc == Y - 0.5*a || notiles[j]->yc == Y + 0.5*a)) ||
          (orientation == 'x' && notiles[j]->yc == Y && (notiles[j]->xc == X - 0.5*a || notiles[j]->xc == X + 0.5*a))
        )
        {//cout  << "nox "<<notiles[j]->xc <<" " << notiles[j]->yc << endl;
          displaycuboid = 0;
          const char* audio = "died.wav";
          playaudio((void*) audio);
          break;
        }
    }
    //cout <<notilescount<<"-------------------------------"<<endl;
    //cout << "block " << X << " " << Y << endl;
    for (int i = 0; i < switchescount;i++)
    {
      for (int j = 0; j < bridgetiles[i].size(); j++)
      {
        float xc1 = -a*(5- bridgetiles[i][j]%10);
        float yc1 = -a*( bridgetiles[i][j]/10 + 1 - 5);
        //cout << "checking bridgetiles at "<< i<<" " << xc1<<" "<<yc1 <<" "<<bridgeflag[bridgetiles[i][j]]<<endl;
        if (
            ((orientation == 'z' && X == xc1 && Y == yc1) ||
            (orientation == 'y' && xc1 == X && (yc1 == Y - 0.5*a || yc1 == Y + 0.5*a)) ||
            (orientation == 'x' && yc1 == Y && (xc1 == X - 0.5*a || xc1 == X + 0.5*a)))
            && (bridgeflag[bridgetiles[i][j]] == 0)
          )
          {
            displaycuboid = 0;
            const char* audio = "died.wav";
            playaudio((void*) audio);
            //cout << "nooooooooo" << endl;
            break;
          }
      }
    }
    //cout <<"----------------"<<endl;

  }
}

/* Render the scene with openGL */
/* Edit this function according to your assignment */
void draw ()
{

  // use the loaded shader program
  // Don't change unless you know what you are doing
  glUseProgram (programID);

  // Eye - Location of camera. Don't change unless you are sure!!

  glm::vec3 eye (e1,e2,e3);
  // Target - Where is the camera looking at.  Don't change unless you are sure!!
  glm::vec3 target (t1, t2, t3);
  // Up - Up vector defines tilt of camera.  Don't change unless you are sure!!
  glm::vec3 up (u1,u2,u3);

  // Compute Camera matrix (view)
  // Matrices.view = glm::lookAt( eye, target, up ); // Rotating Camera for 3D
  //  Don't change unless you are sure!!
  Matrices.view = glm::lookAt(eye, target, up); // Fixed camera for 2D (ortho) in XY plane

  // Compute ViewProject matrix as view/camera might not be changed for this frame (basic scenario)
  //  Don't change unless you are sure!!
  glm::mat4 VP = Matrices.projection * Matrices.view;

  // Send our transformation to the currently bound shader, in the "MVP" uniform
  // For each model you render, since the MVP will be different (at least the M part)
  //  Don't change unless you are sure!!
  glm::mat4 MVP;	// MVP = Projection * View * Model

  // Load identity to model matrix

  if (!gameover)
  {


      if (left_drag == 1)
      {
        float v = -(xpos-p1)*9*M_PI/180.0f;
        //cout << "v is " << v <<endl;

        //cout << "eye before "<<e1<<" "<<e2<<" "<<e3<<endl;
        camera_rotation_angle += 9*(-xpos + p1);
        e1 = 20*cos(camera_rotation_angle*M_PI/180.0f);
        e2 = 20*sin(camera_rotation_angle*M_PI/180.0f);

        e3 += (ypos - p2);
        if (e3 > 17)
        {
          e3 = 17;
        }
        else if (e3 < -17 )
        {
          e3 = -17;
        }

        //cout << "eye after "<<e1<<" "<<e2<<" "<<e3<<endl;
      //  cout << "radius "<< sqrt(e1*e1+e2*e2) << endl;

        //cout << "up before " <<u1<<" "<<u2<<" "<<u3<<endl;

        u1 = 0;u2 = 0; u3 = 1;

        p1 = xpos;
        p2 = ypos;

      //  cout << "up after " <<u1<<" "<<u2<<" "<<u3<<endl;
      }

      if (orientation == 'z' && X == hole->xc && Y == hole->yc && Z == a + 0.25)
      {
        levelchange = 1;
      }

      //cout << "switch count " << switchescount << endl;
      for (int j = 0; j < switchescount;j++)
      {//cout << "switch is" << switches[j]->xc << " " << switches[j]->yc << endl;
        if (
            (orientation == 'z' && X == switches[j]->xc && Y == switches[j]->yc) ||
            (orientation == 'y' && switches[j]->xc == X && (switches[j]->yc == Y - 0.5*a || switches[j]->yc == Y + 0.5*a)) ||
            (orientation == 'x' && switches[j]->yc == Y && (switches[j]->xc == X - 0.5*a || switches[j]->xc == X + 0.5*a))
          )
        {//cout << "it hit " << endl;

          for (int k = 0; k < bridgetiles[j].size();k++)
          {//cout << "bridge tiles are " << bridgetiles[j][k] << endl;
            if (bridgeflag[bridgetiles[j][k]] == 0)
            {
              bridgeflag[bridgetiles[j][k]] = 1;
            }

          }
        }


      }

      int u = 1;
      for (u = 1; u <= fragilecount; u++)
      {
        if (orientation == 'z' && X == fragiletiles[u]->xc && Y == fragiletiles[u]->yc)
        {displaycuboid = 0;
          const char* audio = "died.wav";
          playaudio((void*) audio);
          //cout << "fragile at " << fragiletiles[u]->xc << " " << fragiletiles[u]->yc <<endl;
          /*if (onfragile != u)
          {
            stepedonfragiletime = glfwGetTime();
          }
          onfragile = u;
          break;*/
        }
      }
      /*if (u == fragilecount+1)
      {
        onfragile = 0;
        stepedonfragiletime = glfwGetTime();
      }*/

      if (level == 1)
      {
        for (int i = 0; i < fl1.size(); i++)
        {
          vector<VAO*> tile = fl1[i];

          for (int j = 0; j < tile.size(); j++)
          {
            if (bridgeflag[i+1] == -1 || bridgeflag[i+1] == 1)
            {
              VAO* face = tile[j];

              Matrices.model = glm::mat4(1.0f);
              glm::mat4 translateface = glm::translate (glm::vec3(face->x, face->y, face->z));        // glTranslatef
              Matrices.model *= (translateface);
              MVP = VP * Matrices.model;
              glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
              draw3DObject(face);
            }

          }

        }
      }
      else if (level == 2)
      {
        //cout << "yes " << fl2.size()<< endl;
        for (int i = 0; i < fl2.size(); i++)
        {
          vector<VAO*> tile = fl2[i];

          for (int j = 0; j < tile.size(); j++)
          {
            if (bridgeflag[i+1] == -1 || bridgeflag[i+1] == 1)
            {
              VAO* face = tile[j];

              Matrices.model = glm::mat4(1.0f);
              glm::mat4 translateface = glm::translate (glm::vec3(face->x, face->y, face->z));        // glTranslatef
              Matrices.model *= (translateface);
              MVP = VP * Matrices.model;
              glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
              draw3DObject(face);
            }

          }

        }
      }
      else if (level == 3)
      {
        //cout << "yes " << fl3.size()<< endl;
        for (int i = 0; i < fl3.size(); i++)
        {
          vector<VAO*> tile = fl3[i];

          for (int j = 0; j < tile.size(); j++)
          {
            if (bridgeflag[i+1] == -1 || bridgeflag[i+1] == 1)
            {
              VAO* face = tile[j];

              Matrices.model = glm::mat4(1.0f);
              glm::mat4 translateface = glm::translate (glm::vec3(face->x, face->y, face->z));        // glTranslatef
              Matrices.model *= (translateface);
              MVP = VP * Matrices.model;
              glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
              draw3DObject(face);
            }

          }

        }
      }



      isfalling();

      if (displaycuboid == 1)
      {

        fellofftime = glfwGetTime();
        for (int j = 0; j < cuboid.size(); j++)
        {
          VAO* face = cuboid[j];

          Matrices.model = glm::mat4(1.0f);
          glm::mat4 translateface = glm::translate (glm::vec3(X, Y, Z));
          glm::mat4 rotateface;
          if (orientation == 'x')
          {
            rotateface = glm::rotate((float)(90.0*M_PI/180.0f), glm::vec3(0,1,0));
          }
          else if (orientation == 'y')
          {
            rotateface = glm::rotate((float)(90.0*M_PI/180.0f), glm::vec3(-1,0,0));
          }
          else if (orientation == 'z')
          {
            rotateface = glm::rotate((float)(0.0*M_PI/180.0f), glm::vec3(-1,0,0));
          }

          Matrices.model *= (translateface*rotateface);

          MVP = VP * Matrices.model;
          glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
          draw3DObject(face);
        }
      }



  }

}

/* Initialise glfw window, I/O callbacks and the renderer to use */
/* Nothing to Edit here */
GLFWwindow* initGLFW (int width, int height)
{
    GLFWwindow* window; // window desciptor/handle

    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
//        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, "Cube Game", NULL, NULL);

    if (!window) {
        glfwTerminate();
//        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glfwSwapInterval( 1 );

    /* --- register callbacks with GLFW --- */

    /* Register function to handle window resizes */
    /* With Retina display on Mac OS X GLFW's FramebufferSize
     is different from WindowSize */
    glfwSetFramebufferSizeCallback(window, reshapeWindow);
    glfwSetWindowSizeCallback(window, reshapeWindow);

    /* Register function to handle window close */
    glfwSetWindowCloseCallback(window, quit);

    /* Register function to handle keyboard input */
    glfwSetKeyCallback(window, keyboard);      // general keyboard input
    glfwSetCharCallback(window, keyboardChar);  // simpler specific character handling

    /* Register function to handle mouse click */
    glfwSetMouseButtonCallback(window, mouseButton);// mouse button clicks

    glfwSetCursorPosCallback(window, cursor_pos_callback);//get cursorposition

    glfwSetScrollCallback(window, scroll_callback);//scroll position



    return window;
}

/* Initialize the OpenGL rendering properties */
/* Add all the models to be created here */
void initGL (GLFWwindow* window, int width, int height)
{
    /* Objects should be created before any other gl function and shaders */
	// Create the models
	 // Generate the VAO, VBOs, vertices data & copy into the array buffer

   createfloors(fl1m,fl2m,fl3m);
   createfloorvariables(fl1m);
   createcuboid();

	// Create and compile our GLSL program from the shaders
	programID = LoadShaders( "Sample_GL.vert", "Sample_GL.frag" );
	// Get a handle for our "MVP" uniform

	Matrices.MatrixID = glGetUniformLocation(programID, "MVP");


	reshapeWindow (window, width, height);

    // Background color of the scene
	glClearColor (0.0f, 0.0f, 0.0f, 0.0f); // R, G, B, A
	glClearDepth (1.0f);

	glEnable (GL_DEPTH_TEST);
	glDepthFunc (GL_LEQUAL);

    cout << "VENDOR: " << glGetString(GL_VENDOR) << endl;
    cout << "RENDERER: " << glGetString(GL_RENDERER) << endl;
    cout << "VERSION: " << glGetString(GL_VERSION) << endl;
    cout << "GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
}

int main (int argc, char** argv)
{
	int width = 800;
	int height = 800;

    GLFWwindow* window = initGLFW(width, height);

    timetaken[0] = 0;

    for (int i = 0; i < 100 ; i++)
    {
      vector<int> a;
      bridgetiles.push_back(a);

      struct point* p = (struct point*)malloc(sizeof(struct point));
      p->xc = 0;
      p->yc = 0;
      switches.push_back(p);
      struct point* q = (struct point*)malloc(sizeof(struct point));
      q->xc = 0;
      q->yc = 0;
      fragiletiles.push_back(q);
      struct point* r = (struct point*)malloc(sizeof(struct point));
      r->xc = 0;
      r->yc = 0;
      notiles.push_back(r);


    }


	  initGL (window, width, height);


    stepedonfragiletime = fellofftime = currenttime = glfwGetTime();


    /* Draw in loop */
    while (!glfwWindowShouldClose(window))
    {

        // OpenGL Draw commands
        // clear the color and depth in the frame buffer
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        draw();
        //ao_play(device, buffer, BUF_SIZE);

        if (gameover)
        {

          cout << "Game Over" <<  endl;
          const char* audio = "gameover.wav";
          playaudio((void*) audio);
          quit(window);
          return 0;

        }

        // Swap Frame Buffer in double buffering
        glfwSwapBuffers(window);

        // Poll for Keyboard and mouse events
        glfwPollEvents();

        // Control based on time (Time based transformation like 5 degrees rotation every 0.5s)

        currenttime = glfwGetTime();
        /*if (currenttime - stepedonfragiletime > 1)
        {
          bridgeflag[ fragiletiles[onfragile]->pos ] = -3;
          displaycuboid = 0;
        }
        currenttime = glfwGetTime();*/
        if (currenttime - fellofftime > 0.5)
        {
          displaycuboid = 1;
          X = 12.5;
          Y = -12.5;
          Z = 2.5 + 0.25;
          orientation = 'z';
          for (int i = 0; i < switchescount;i++)
          {
            for (int j = 0; j < bridgetiles[i].size(); j++)
            {
              if (bridgeflag[bridgetiles[i][j]] == 1)
              {
                bridgeflag[bridgetiles[i][j]] = 0;
              }
            }
          }
        }
        if (levelchange == 1)
        {
          if (level < 3)
          {
            const char* audio = "levelup.wav";
            playaudio((void*) audio);
            timetaken[level] = glfwGetTime() - timetaken[level-1];
            cout << "Level " << level <<" completed" <<endl;
            cout << " no of moves in this level " << noofmoves << endl;
            cout << "total time taken for this level " << timetaken[level] << "seconds"<< endl;
            noofmoves = 0;
            X = 12.5;
            Y = -12.5;
            Z = 2.5 + 0.25;
            orientation = 'z';
            level++;
            levelchange = 0;
            displaycuboid = 1;
            switchescount = 0;
            fragilecount = 0;
            notilescount = 0;
            onfragile = 0;
            hole->xc = 0;
            hole->yc = 0;
            double stepedonfragiletime,currenttime,fellofftime;
            for (int i = 0; i < 100 ; i++)
            {
              if (i < switchescount)
              {
                for (int j = 0; j < bridgetiles[i].size();j++)
                {
                  bridgetiles[i].pop_back();
                }

              }

              switches[i]->xc = 0;
              switches[i]->yc = 0;

              fragiletiles[i]->xc = 0;
              fragiletiles[i]->yc = 0;

              notiles[i]->xc = 0;
              notiles[i]->yc = 0;


            }
            stepedonfragiletime = fellofftime = currenttime = glfwGetTime();

            if (level == 2)
            {
              createfloorvariables(fl2m);
            }
            else if (level == 3)
            {
              createfloorvariables(fl3m);
            }


          }
          else
          {
            timetaken[level] = glfwGetTime() - timetaken[level-1];
            cout << "Level " << level <<" completed" <<endl;
            cout << " no of moves in this level " << noofmoves << endl;
            cout << "total time taken for this level " << timetaken[level] << "seconds"<< endl;
            gameover = true;
          }
        }

    }


    glfwTerminate();
//    exit(EXIT_SUCCESS);
}
