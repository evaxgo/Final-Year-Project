//
//  main.cpp
//  OpenGLApp
//
//  Created by Eva Leonard on 30/01/2014.
//  Copyright (c) 2014 Eva Leonard. All rights reserved.
//

#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "maths_funcs.h"

#include <assimp/cimport.h> // C importer
#include <assimp/scene.h> // collects data
#include <assimp/postprocess.h> // various extra operations

#include <vector>
#include <string>

#include <exception>

#include "stb_image.h"

#include <CoreFoundation/CoreFoundation.h>

#include <jdksmidi/world.h>
#include <jdksmidi/midi.h>
#include <jdksmidi/msg.h>
#include <jdksmidi/sysex.h>
#include <jdksmidi/parser.h>

// Audio Toolbox (for loading audio files)
#include <AudioToolbox/AudioToolbox.h>

// OpenAL
#include <OpenAL/al.h>
#include <OpenAl/alc.h>

#include "MidiProcessor.h"

using namespace OpenGLApp;

MidiProcessor* midiProc;

#define AUDIO_FILE_PATH "track1.mp3"

ALCdevice *audioDevice;
ALCcontext *audioContext;

ALuint* sources;
ALuint* buffers;

#pragma mark user-data struct
typedef struct LoopAudioSample {
	AudioStreamBasicDescription	dataFormat;
	UInt16				*sampleBuffer;
	UInt32				bufferSizeBytes;
	ALuint				sources[1];
} LoopAudioSample;

LoopAudioSample* sample;

// Global variables
GLuint shaderProgramID;



// Mesh variables
std::vector<float> g_vp, g_vn, g_vt;
std::vector<int> point_counts;
std::vector<unsigned int> vaos, texes;
unsigned int mesh_vao = 0;
int g_point_count = 0;

GLuint loc1, loc2, loc3;

GLfloat rotate_y = 0.0f;
GLfloat wheel_rotation = 0.0f;
GLfloat translateObjX = 0.0f;
GLfloat translateObjZ = 0.0f;
GLfloat carMovementOffset = 1.0f;

int width = 800;
int height = 600;



bool keyStates[1024];


bool load_image_to_texture (const char* file_name, unsigned int& tex, bool gen_mips);

static void error_callback(int error, const char* description)
{
    fputs(description, stderr);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    
    if (action == GLFW_PRESS) {
        keyStates[key] = true;
    } else if (action == GLFW_RELEASE) {
        keyStates[key] = false;
    }
}

bool load_mesh (const char* file_name) {
    const aiScene* scene = aiImportFile (file_name, aiProcess_Triangulate); // TRIANGLES!
    fprintf (stderr, "ERROR: reading mesh %s\n", file_name);
    if (!scene) {
        fprintf (stderr, "ERROR: reading mesh %s\n", file_name);
        return false;
    }
    printf ("  %i animations\n", scene->mNumAnimations);
    printf ("  %i cameras\n", scene->mNumCameras);
    printf ("  %i lights\n", scene->mNumLights);
    printf ("  %i materials\n", scene->mNumMaterials);
    printf ("  %i meshes\n", scene->mNumMeshes);
    printf ("  %i textures\n", scene->mNumTextures);
    
    for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
        const aiMesh* mesh = scene->mMeshes[m_i];
        printf ("    %i vertices in mesh\n", mesh->mNumVertices);
        g_point_count = mesh->mNumVertices;
        for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
            if (mesh->HasPositions ()) {
                const aiVector3D* vp = &(mesh->mVertices[v_i]);
                //printf ("      vp %i (%f,%f,%f)\n", v_i, vp->x, vp->y, vp->z);
                g_vp.push_back (vp->x);
                g_vp.push_back (vp->y);
                g_vp.push_back (vp->z);
            }
            if (mesh->HasNormals ()) {
                const aiVector3D* vn = &(mesh->mNormals[v_i]);
                //printf ("      vn %i (%f,%f,%f)\n", v_i, vn->x, vn->y, vn->z);
                g_vn.push_back (vn->x);
                g_vn.push_back (vn->y);
                g_vn.push_back (vn->z);
            }
            if (mesh->HasTextureCoords (0)) {
                const aiVector3D* vt = &(mesh->mTextureCoords[0][v_i]);
                //printf ("      vt %i (%f,%f)\n", v_i, vt->x, vt->y);
                g_vt.push_back (vt->x);
                g_vt.push_back (vt->y);
            }
            if (mesh->HasTangentsAndBitangents ()) {
                // NB: could store/print tangents here
            }
        }
    }
    point_counts.push_back(g_point_count);
    aiReleaseImport (scene);
    return true;
}

void generateObjectBufferMeshes(std::string * mesh_names, int numMeshes) {
    /*----------------------------------------------------------------------------
     LOAD MESH HERE AND COPY INTO BUFFERS
     ----------------------------------------------------------------------------*/
    
    //Note: you may get an error "vector subscript out of range" if you are using this code for a mesh that doesnt have positions and normals
    //Might be an idea to do a check for that before generating and binding the buffer.
    for(int i = 0; i < numMeshes; i++)
    {
        load_mesh (mesh_names[i].c_str());
        
        loc1 = glGetAttribLocation(shaderProgramID, "vertex_position");
        loc2 = glGetAttribLocation(shaderProgramID, "vertex_normal");
        loc3 = glGetAttribLocation(shaderProgramID, "vertex_texture");
        
        unsigned int vp_vbo = 0;
        glGenBuffers (1, &vp_vbo);
        glBindBuffer (GL_ARRAY_BUFFER, vp_vbo);
        glBufferData (GL_ARRAY_BUFFER, g_point_count * 3 * sizeof (float), &g_vp[0], GL_STATIC_DRAW);
        
        unsigned int vn_vbo = 0;
        glGenBuffers (1, &vn_vbo);
        glBindBuffer (GL_ARRAY_BUFFER, vn_vbo);
        glBufferData (GL_ARRAY_BUFFER, g_point_count * 3 * sizeof (float), &g_vn[0], GL_STATIC_DRAW);
        
        unsigned int vt_vbo = 0;
        glGenBuffers (1, &vt_vbo);
        glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
        glBufferData (GL_ARRAY_BUFFER, g_point_count * 2 * sizeof (float), &g_vt[0], GL_STATIC_DRAW);
        
        unsigned int vao = 0;
        glGenVertexArrays(1, &vao);
        glBindVertexArray (vao);
        
        glEnableVertexAttribArray (loc1);
        glBindBuffer (GL_ARRAY_BUFFER, vp_vbo);
        glVertexAttribPointer (loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray (loc2);
        glBindBuffer (GL_ARRAY_BUFFER, vn_vbo);
        glVertexAttribPointer (loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray (loc3);
        glBindBuffer(GL_ARRAY_BUFFER, vt_vbo);
        glVertexAttribPointer (loc3, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        
        unsigned int tex = 0;
        int extension = mesh_names[i].length()-4;
        std::string withoutExtension = mesh_names[i].substr(0,extension); //remove extension
        std::string withNewExtension = withoutExtension + ".png";
        load_image_to_texture (withNewExtension.c_str(), tex, true);
        
        vaos.push_back(vao); //keeping track of vao
        texes.push_back(tex);
        g_vp.clear();
        g_vn.clear();
        g_vt.clear();
    }
}

bool load_image_to_texture (
                            const char* file_name, unsigned int& tex, bool gen_mips
                            ) {
	printf ("loading image %s\n", file_name);
	int x, y, n;
	int force_channels = 4;
	unsigned char* image_data = stbi_load (file_name, &x, &y, &n, force_channels);
	if (!image_data) {
		fprintf (
                 stderr,
                 "ERROR: could not load image %s. Check file type and path\n",
                 file_name
                 );
		fprintf (stderr, "ERROR: could not load image %s", file_name);
		return false;
	}
	printf ("image loaded: %ix%i %i bytes per pixel\n", x, y, n);
	// NPOT check
	if (x & (x - 1) != 0 || y & (y - 1) != 0) {
		fprintf (
                 stderr, "WARNING: texture %s is not power-of-2 dimensions\n", file_name
                 );
		fprintf (stderr, "WARNING: texture %s is not power-of-two dimensions", file_name);
	}
	
	// FLIP UP-SIDE DIDDLY-DOWN
	// make upside-down copy for GL
	unsigned char *imagePtr = &image_data[0];
	int halfTheHeightInPixels = y / 2;
	int heightInPixels = y;
	
	// Assuming RGBA for 4 components per pixel.
	int numColorComponents = 4;
	// Assuming each color component is an unsigned char.
	int widthInChars = x * numColorComponents;
	unsigned char *top = NULL;
	unsigned char *bottom = NULL;
	unsigned char temp = 0;
	for( int h = 0; h < halfTheHeightInPixels; h++) {
		top = imagePtr + h * widthInChars;
		bottom = imagePtr + (heightInPixels - h - 1) * widthInChars;
		for (int w = 0; w < widthInChars; w++) {
			// Swap the chars around.
			temp = *top;
			*top = *bottom;
			*bottom = temp;
			++top;
			++bottom;
		}
	}
	
	// Copy into an OpenGL texture
	glGenTextures (1, &tex);
	glActiveTexture (GL_TEXTURE0);
	glBindTexture (GL_TEXTURE_2D, tex);
	//glPixelStorei (GL_UNPACK_ALIGNMENT, 1); // TODO?
	glTexImage2D (
                  GL_TEXTURE_2D,
                  0,
                  GL_RGBA,
                  x,
                  y,
                  0,
                  GL_RGBA,
                  GL_UNSIGNED_BYTE,
                  image_data
                  );
	// NOTE: need this or it will not load the texture at all
	if (gen_mips) {
		// shd be in core since 3.0 according to:
		// http://www.opengl.org/wiki/Common_Mistakes#Automatic_mipmap_generation
		// next line is to circumvent possible extant ATI bug
		// but NVIDIA throws a warning glEnable (GL_TEXTURE_2D);
		glGenerateMipmap (GL_TEXTURE_2D);
		printf ("mipmaps generated %s\n", file_name);
		glTexParameteri (
                         GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR
                         );
		glTexParameterf (
                         GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 4
                         );
	} else {
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	printf ("image loading done");
	stbi_image_free (image_data);
    
	return true;
}

// Create a NULL-terminated string by reading the provided file
char* readShaderSource(const char* shaderFile) {
    FILE* fp = fopen(shaderFile, "rb"); //!->Why does binary flag "RB" work and not "R"... wierd msvc thing?
    
    if ( fp == NULL ) { return NULL; }
    
    fseek(fp, 0L, SEEK_END);
    long size = ftell(fp);
    
    fseek(fp, 0L, SEEK_SET);
    char* buf = new char[size + 1];
    fread(buf, 1, size, fp);
    buf[size] = '\0';
    
    fclose(fp);
    
    return buf;
}

static void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
	// create a shader object
    GLuint ShaderObj = glCreateShader(ShaderType);
    
    if (ShaderObj == 0) {
        fprintf(stderr, "Error creating shader type %d\n", ShaderType);
        exit(0);
    }
	const char* pShaderSource = readShaderSource( pShaderText);
    
	// Bind the source code to the shader, this happens before compilation
	glShaderSource(ShaderObj, 1, (const GLchar**)&pShaderSource, NULL);
	// compile the shader and check for errors
    glCompileShader(ShaderObj);
    GLint success;
	// check for shader related errors using glGetShaderiv
    glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar InfoLog[1024];
        glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
        fprintf(stderr, "Error compiling shader type %d: '%s'\n", ShaderType, InfoLog);
        exit(1);
    }
	// Attach the compiled shader object to the program object
    fprintf(stdout, "Compiled shader\n");
    glAttachShader(ShaderProgram, ShaderObj);
}

GLuint CompileShaders()
{
	//Start the process of setting up our shaders by creating a program ID
	//Note: we will link all the shaders together into this ID
    shaderProgramID = glCreateProgram();
    if (shaderProgramID == 0) {
        fprintf(stderr, "Error creating shader program\n");
        exit(1);
    }
    
	// Create two shader objects, one for the vertex, and one for the fragment shader
    AddShader(shaderProgramID, "simpleVertexShader.txt", GL_VERTEX_SHADER);
    AddShader(shaderProgramID, "simpleFragmentShader.txt", GL_FRAGMENT_SHADER);
    
    GLint Success = 0;
    GLchar ErrorLog[1024] = { 0 };
	// After compiling all shader objects and attaching them to the program, we can finally link it
    glLinkProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
    glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &Success);
	if (Success == 0) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		fprintf(stderr, "Error linking shader program: '%s'\n", ErrorLog);
        exit(1);
	}
    
//	// program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
//    glValidateProgram(shaderProgramID);
//	// check for program related errors using glGetProgramiv
//    glGetProgramiv(shaderProgramID, GL_VALIDATE_STATUS, &Success);
//    if (!Success) {
//        glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
//        fprintf(stderr, "Invalid shader program: '%s'\n", ErrorLog);
//        exit(1);
//    }
	// Finally, use the linked shader program
	// Note: this program will stay in effect for all draw calls until you replace it with another or explicitly disable its use
    //glUseProgram(shaderProgramID);
	return shaderProgramID;
}

float camPitch = 180.0f;
int lastMouseX = -1;

vec3 calculate_camera_position(){
	auto rotateRad = -rotate_y * ONE_DEG_IN_RAD;
	auto distanceFromCar = 10.0f;
    
	auto zPos = translateObjZ + (sin(rotateRad) * distanceFromCar);
	auto xPos = translateObjX + (cos(rotateRad) * distanceFromCar);
	auto yPos = 1.0f;
    
	return vec3(xPos, yPos,	zPos);
}

void updateScene()
{
    GLfloat rotate_y_rad= rotate_y*ONE_DEG_IN_RAD;
    
    if(keyStates[GLFW_KEY_W])
    {
        translateObjX+=(carMovementOffset*cos(-rotate_y_rad));
        translateObjZ+=(carMovementOffset*sin(-rotate_y_rad));
        wheel_rotation+=10.0f;
    }
    
    if(keyStates[GLFW_KEY_S])
    {
        translateObjX-=(carMovementOffset*cos(-rotate_y_rad));
        translateObjZ-=(carMovementOffset*sin(-rotate_y_rad));
        wheel_rotation-=10.0f;
    }
    
    if(keyStates[GLFW_KEY_A])
    {
        if (keyStates[GLFW_KEY_S]) {
            rotate_y -= 2.0f;
        } else {
            rotate_y+=2.0f;
        }
    }
    
    if(keyStates[GLFW_KEY_D])
    {
        if (keyStates[GLFW_KEY_S]) {
            rotate_y += 2.0f;
        } else {
            rotate_y -= 2.0f;
        }
    }
    
    if(keyStates[GLFW_KEY_Z])
    {
        camPitch++;
    }
    if(keyStates[GLFW_KEY_X])
    {
        camPitch--;
    }
}

void drawAndPlaySource(float x, float z, ALuint source, vec3 diffuse)
{
    mat4 model = identity_mat4();
    model = rotate_x_deg(model, -90.0f);
    model = translate(model, vec3(x, 0, z));
    
    int matrix_location = glGetUniformLocation (shaderProgramID, "model");
    int diffuse_location = glGetUniformLocation (shaderProgramID, "Kd");
    glUniformMatrix4fv (matrix_location, 1, GL_FALSE, model.m);
    glUniform3f(diffuse_location, diffuse.v[0], diffuse.v[1], diffuse.v[2]);
    glDrawArrays(GL_TRIANGLES, 0, point_counts[0]);
    
    // Play the sound
    
    ALint playing;
    alGetSourcei(sources[source], AL_SOURCE_STATE, &playing);
    if (playing != AL_PLAYING) {
        alSource3f(sources[source], AL_POSITION, x, 0.0f, z);
        alSourcePlay(sources[source]);
    }
}

void draw(GLFWwindow* window)
{
    // tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable (GL_DEPTH_TEST); // enable depth-testing
	glDepthFunc (GL_LESS); // depth-testing interprets a smaller value as "closer"
	glClearColor (0.5f, 0.5f, 0.5f, 1.0f);
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram (shaderProgramID);
    
    
	//Declare your uniform variables that will be used in your shader
	int view_mat_location = glGetUniformLocation (shaderProgramID, "view");
	int proj_mat_location = glGetUniformLocation (shaderProgramID, "proj");
    
	// Camera
    auto camMat = vec3(translateObjX, 0.0f, translateObjZ);
    auto camLookAt = calculate_camera_position();
    ALfloat lookAtF[6] = { camLookAt.v[0], camLookAt.v[1], camLookAt.v[2], 0.0f, 1.0f, 0.0f};
    
    alListener3f(AL_POSITION, translateObjX, 0.0f, translateObjZ);
    alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    alListenerfv(AL_ORIENTATION, lookAtF);
    
	mat4 view = identity_mat4 ();
	mat4 persp_proj = perspective(45.0, (float)width/(float)height, 0.1, 1000.0);
    
    view = view * look_at(camMat, camLookAt, vec3(0, 1, 0));
    
    glUniformMatrix4fv (proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv (view_mat_location, 1, GL_FALSE, view.m);
    
    glBindVertexArray(vaos[0]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texes[0]);
    
    // Render and play the sources/tracks
    for (int i = 0; i < midiProc->getNumTracks(); ++i) {
        float x = (i % 3) * 25;
        float z = (i / 3) * 25;
        vec3 diffuse;
        switch (i % 4) {
            case 0:
                diffuse = vec3(1.0f, 0.0f, 0.0f);
                break;
            case 1:
                diffuse = vec3(0.0f, 1.0f, 0.0f);
                break;
            case 2:
                diffuse = vec3(0.0f, 0.0f, 1.0f);
                break;
            case 3:
                diffuse = vec3(1.0f, 1.0f, 0.0f);
                break;
                
            default:
                diffuse = vec3(0.5f, 0.5f, 0.5f);
                break;
        }
        drawAndPlaySource(x, z, i, diffuse);
    }
    
    glfwSwapBuffers(window);
    glfwPollEvents();
    updateScene();
}

void drawTest(GLFWwindow* window)
{
    float ratio;
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    ratio = width / (float) height;
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-ratio, ratio, -1.f, 1.f, 1.f, -1.f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef((float) glfwGetTime() * 50.f, 0.f, 0.f, 1.f);
    glBegin(GL_TRIANGLES);
    glColor3f(1.f, 0.f, 0.f);
    glVertex3f(-0.6f, -0.4f, 0.f);
    glColor3f(0.f, 1.f, 0.f);
    glVertex3f(0.6f, -0.4f, 0.f);
    glColor3f(0.f, 0.f, 1.f);
    glVertex3f(0.f, 0.6f, 0.f);
    glEnd();
    glfwSwapBuffers(window);
    glfwPollEvents();
}

OSStatus loadAudioBuffers(LoopAudioSample *sample, const char* path)
{
    CFURLRef fileUrl = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFStringCreateWithCString(NULL, path, kCFStringEncodingASCII), kCFURLPOSIXPathStyle, false);
 
    sample->dataFormat.mFormatID = kAudioFormatLinearPCM;
    sample->dataFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    sample->dataFormat.mSampleRate = 44100.0;
    sample->dataFormat.mChannelsPerFrame = 1;
    sample->dataFormat.mFramesPerPacket = 1;
    sample->dataFormat.mBitsPerChannel = 16;
    sample->dataFormat.mBytesPerFrame = 2;
    sample->dataFormat.mBytesPerPacket = 2;
    
    ExtAudioFileRef extAudioFile;
    OSStatus err;
    err = ExtAudioFileOpenURL(fileUrl, &extAudioFile);
    if (!(err == 0)) {
        std::cout << "Couldn't open extAudioFile for reading." << std::endl;
        return err;
    }
    
    err = ExtAudioFileSetProperty(extAudioFile, kExtAudioFileProperty_ClientDataFormat, sizeof(AudioStreamBasicDescription), &sample->dataFormat);
    if (!(err == 0)) {
        std::cout << "Couldn't set extAudioFile format properties." << std::endl;
        return err;
    }
    
    SInt64 fileLengthFrames;
    UInt32 propSize = sizeof(fileLengthFrames);
    ExtAudioFileGetProperty(extAudioFile, kExtAudioFileProperty_FileLengthFrames, &propSize, &fileLengthFrames);
    
    sample->bufferSizeBytes = fileLengthFrames * sample->dataFormat.mBytesPerFrame;
    
    AudioBufferList *buffers;
    UInt32 ablSize = offsetof(AudioBufferList, mBuffers[0]) + (sizeof(AudioBuffer) * 1);
    buffers = (AudioBufferList*)malloc(ablSize);
    
    // allocate sample buffer
    sample->sampleBuffer = (UInt16*)malloc(sizeof(UInt16) * sample->bufferSizeBytes);
    
    buffers->mNumberBuffers = 1;
    buffers->mBuffers[0].mNumberChannels = 1;
    buffers->mBuffers[0].mDataByteSize = sample->bufferSizeBytes;
    buffers->mBuffers[0].mData = sample->sampleBuffer;
    
    // read into the AudioBufferList until it is full
    UInt32 totalFramesRead = 0;
    do {
        UInt32 framesRead = fileLengthFrames - totalFramesRead;
        buffers->mBuffers[0].mData = sample->sampleBuffer + (totalFramesRead * (sizeof(UInt16)));
        err = ExtAudioFileRead(extAudioFile, &framesRead, buffers);
        if (!(err == 0)) {
            std::cout << "Couldn't read extAudioFile." << std::endl;
            return err;
        }
        totalFramesRead += framesRead;
    } while (totalFramesRead < fileLengthFrames);
    
    free(buffers);
    return err;
}

int main(int argc, const char * argv[])
{
    if (argc < 2) {
        std::cerr << "You must specify an input MIDI file to process!" << std::endl;
        return -1;
    }
    
    std::string inputFile = argv[1];
    
    try {
        midiProc = new MidiProcessor(inputFile);
        midiProc->splitTracks();
        midiProc->convertTracks();
    } catch (std::runtime_error e) {
        std::cerr << "Error in MIDIProcessor: " << e.what() << std::endl;
        return -1;
    }
    
    auto numAudioSources = midiProc->getNumTracks();
    auto audioFilenames = midiProc->getConvertedTrackNames();
    
    sources = new ALuint[numAudioSources]();
    buffers = new ALuint[numAudioSources]();
    
    sample = new LoopAudioSample[numAudioSources]();
    
    // init OpenAL stuff
    audioDevice = alcOpenDevice(NULL);
    if (!audioDevice) {
        std::cout << "Error initializing audio device!" << std::endl;
        return 0;
    }
    
    audioContext = alcCreateContext(audioDevice, NULL);
    if (!alcMakeContextCurrent(audioContext)) {
        std::cout << "Error initializing audio context!" << std::endl;
        return 0;
    }
    alGetError();
    alGenSources(numAudioSources, sources);
    if (alGetError() != AL_NO_ERROR) {
        std::cout << "Error generating sources!" << std::endl;
        return 0;
    }
    for (int i = 0; i < numAudioSources; i++) {
        alSourcef(sources[i], AL_PITCH, 1);
        alSourcef(sources[i], AL_GAIN, 1.0f);
        alSource3f(sources[i], AL_VELOCITY, 0.0f, 0.0f, 0.0f);
        alSourcei(sources[i], AL_LOOPING, AL_TRUE);
        alSourcef(sources[i], AL_REFERENCE_DISTANCE, 25.0f);
        alSourcef(sources[i], AL_MAX_DISTANCE, 200.0f);
        alGenBuffers((ALuint)1, &buffers[i]);
        loadAudioBuffers(&(sample[i]), audioFilenames[i].c_str());
        alBufferData(buffers[i], AL_FORMAT_MONO16, sample[i].sampleBuffer, sample[i].bufferSizeBytes, sample[i].dataFormat.mSampleRate);
        alSourcei(sources[i], AL_BUFFER, buffers[i]);
    }
    
    glfwSetErrorCallback(error_callback);
    
    GLFWwindow* window;
    
    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }
    
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint (GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    window = glfwCreateWindow(width, height, "OpenGL Window", NULL, NULL);
    int i;
    std::cout << "Made window" << std::endl;
    if (!window) {
        glfwTerminate();
        std::cout << "Failed" << std::endl;
        std::cin >> i;
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);
    
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    
    if (err != GLEW_OK) {
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
    }
    
    const GLubyte* renderer = glGetString (GL_RENDERER); // get renderer string
    const GLubyte* version = glGetString (GL_VERSION); // version as a string
    printf ("Renderer: %s\n", renderer);
    printf ("OpenGL version supported %s\n", version);
    
    glfwSwapInterval(2);
    glfwSetKeyCallback(window, key_callback);
    // insert code here...
    CompileShaders();
    const int numMesh = 1;
    std::string meshes[numMesh] = {"man.dae"};
    
	// load mesh into a vertex buffer array
	generateObjectBufferMeshes(meshes, numMesh);
    
    
    
    
    
    std::cout << "Hello, World!\n";
    while (!glfwWindowShouldClose(window)) {
        draw(window);
    }
    
    glfwTerminate();
    return 0;
}

