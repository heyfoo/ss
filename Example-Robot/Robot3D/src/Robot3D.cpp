#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define GLEW_STATIC
#include <GL/glew.h>
#define FREEGLUT_STATIC
#include <GL/freeglut.h>

#include "Vectors.h"
#include "QuadMesh.h"

const int vWidth = 800;
const int vHeight = 600;

const float boothWidth = 18.0f;
const float boothDepth = 9.0f;
const float boothHeight = 10.0f;

const float groundLevel = 0.0f;

const float waterLeftX = -6.0f;
const float waterRightX = 6.0f;
const float waterBaseHeight = 4.0f;
const float waterThickness = 0.8f;
const float waterFrontZ = 2.2f;
const float waterBackZ = -2.2f;
const int waterResolution = 48;
const float waterWaveCycles = 2.0f;
const float waterWaveAmplitude = 0.6f;
const float waterWaveSpeed = 1.6f;

const float objectSpeed = 2.6f;
const float gravity = 18.0f;
const float waitDuration = 1.6f;
const float objectZPosition = 0.0f;

enum class WaterState
{
Wavy,
Flat
};

enum class ObjectState
{
Duck,
TargetOnly
};

enum class MovementPhase
{
MoveAcross,
Falling,
Waiting
};

enum class CameraState
{
Front,
Perspective
};

struct SceneState
{
WaterState water = WaterState::Wavy;
ObjectState object = ObjectState::Duck;
MovementPhase movement = MovementPhase::MoveAcross;
CameraState camera = CameraState::Front;
};

SceneState sceneState;

float waterTime = 0.0f;
std::vector<float> waterHeights(waterResolution + 1, waterBaseHeight);

float objectPosX = waterLeftX;
float objectPosY = waterBaseHeight;
float objectVelocityY = 0.0f;
float waitTimer = 0.0f;

QuadMesh *groundMesh = NULL;
int meshSize = 32;

GLuint groundProgram = 0;
GLint groundColorLocation = -1;
Vector3 groundBaseColor = Vector3(0.12f, 0.45f, 0.2f);

GLfloat light_position0[] = { -12.0F, 18.0F, 18.0F, 1.0F };
GLfloat light_position1[] = { 12.0F, 18.0F, 18.0F, 1.0F };
GLfloat light_diffuse[] = { 1.0F, 1.0F, 1.0F, 1.0F };
GLfloat light_specular[] = { 1.0F, 1.0F, 1.0F, 1.0F };
GLfloat light_ambient[] = { 0.9F, 0.9F, 0.9F, 1.0F };

int lastFrameTime = 0;

GLUquadric *targetQuadric = NULL;

void initOpenGL(int w, int h);
void display(void);
void reshape(int w, int h);
void keyboard(unsigned char key, int x, int y);
void animationHandler(int param);

void updateAnimation(float dt);
void applyCamera();
void updateWaterSurface();
float computeWaterHeight(float x);
float getObjectFloatOffset();
float getObjectGroundOffset();
void resetMovement();

void drawGround();
void drawBooth();
void drawWater();
void drawActiveObject();
void drawDuck();
void drawTarget();
void setMaterial(const GLfloat ambient[4], const GLfloat diffuse[4], const GLfloat specular[4], GLfloat shininess);

GLuint buildGroundProgram();
GLuint compileShader(GLenum type, const char *src);

int main(int argc, char **argv)
{
glutInit(&argc, argv);
glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
glutInitWindowSize(vWidth, vHeight);
glutInitWindowPosition(200, 30);
glutCreateWindow("Carnival Target Shoot");

initOpenGL(vWidth, vHeight);

glutDisplayFunc(display);
glutReshapeFunc(reshape);
glutKeyboardFunc(keyboard);

glutTimerFunc(16, animationHandler, 0);

glutMainLoop();
        return 0;
}

void initOpenGL(int w, int h)
{
        GLenum glewErr = glewInit();
        if (glewErr != GLEW_OK)
        {
                std::fprintf(stderr, "GLEW initialization failed: %s\n", glewGetErrorString(glewErr));
                std::exit(EXIT_FAILURE);
        }

        glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
        glLightfv(GL_LIGHT1, GL_AMBIENT, light_ambient);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, light_diffuse);
        glLightfv(GL_LIGHT1, GL_SPECULAR, light_specular);

        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_LIGHT1);

        glEnable(GL_DEPTH_TEST);
        glShadeModel(GL_SMOOTH);
        glClearColor(0.58f, 0.74f, 0.92f, 1.0f);
        glClearDepth(1.0f);
        glEnable(GL_NORMALIZE);
        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        Vector3 origin = Vector3(-30.0f, -0.02f, 30.0f);
        Vector3 dir1v = Vector3(1.0f, 0.0f, 0.0f);
        Vector3 dir2v = Vector3(0.0f, 0.0f, -1.0f);
        groundMesh = new QuadMesh(meshSize, 60.0f);
        groundMesh->InitMesh(meshSize, origin, 60.0, 60.0, dir1v, dir2v);

        Vector3 ambient = Vector3(0.02f, 0.05f, 0.02f);
        Vector3 diffuse = Vector3(groundBaseColor.x, groundBaseColor.y, groundBaseColor.z);
        Vector3 specular = Vector3(0.1f, 0.1f, 0.1f);
groundMesh->SetMaterial(ambient, diffuse, specular, 4.0);

groundProgram = buildGroundProgram();
groundColorLocation = glGetUniformLocation(groundProgram, "uBaseColor");
groundMesh->CreateMeshVBO(meshSize, 0, 1);

targetQuadric = gluNewQuadric();
if (targetQuadric)
{
gluQuadricNormals(targetQuadric, GLU_SMOOTH);
}

reshape(w, h);
lastFrameTime = glutGet(GLUT_ELAPSED_TIME);
updateWaterSurface();
objectPosX = waterLeftX;
sceneState.movement = MovementPhase::MoveAcross;
objectVelocityY = 0.0f;
waitTimer = 0.0f;
objectPosY = computeWaterHeight(objectPosX) + getObjectFloatOffset();
}

void display(void)
{
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
glLoadIdentity();
applyCamera();

glLightfv(GL_LIGHT0, GL_POSITION, light_position0);
glLightfv(GL_LIGHT1, GL_POSITION, light_position1);

drawGround();
drawBooth();
drawWater();
drawActiveObject();

glutSwapBuffers();
}

void reshape(int w, int h)
{
        glViewport(0, 0, (GLsizei)w, (GLsizei)h);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(60.0, (GLdouble)w / h, 0.2, 200.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
}

void keyboard(unsigned char key, int x, int y)
{
switch (key)
{
case 27:
std::exit(0);
break;
case 'w':
case 'W':
case '1':
sceneState.water = (sceneState.water == WaterState::Wavy) ? WaterState::Flat : WaterState::Wavy;
updateWaterSurface();
if (sceneState.movement == MovementPhase::MoveAcross)
{
objectPosY = computeWaterHeight(objectPosX) + getObjectFloatOffset();
}
break;
case 'd':
case 'D':
case '2':
sceneState.object = (sceneState.object == ObjectState::Duck) ? ObjectState::TargetOnly : ObjectState::Duck;
if (sceneState.movement == MovementPhase::MoveAcross)
{
objectPosY = computeWaterHeight(objectPosX) + getObjectFloatOffset();
}
else if (sceneState.movement == MovementPhase::Waiting)
{
objectPosY = groundLevel + getObjectGroundOffset();
}
break;
case 'c':
case 'C':
case '4':
sceneState.camera = (sceneState.camera == CameraState::Front) ? CameraState::Perspective : CameraState::Front;
break;
case 'r':
case 'R':
case '3':
resetMovement();
break;
default:
break;
}

glutPostRedisplay();
}

void animationHandler(int param)
{
        int current = glutGet(GLUT_ELAPSED_TIME);
        float dt = (current - lastFrameTime) * 0.001f;
        lastFrameTime = current;
        if (dt < 0.0f)
                dt = 0.0f;

        updateAnimation(dt);

        glutPostRedisplay();
        glutTimerFunc(16, animationHandler, 0);
}

void updateAnimation(float dt)
{
waterTime += dt;
updateWaterSurface();

switch (sceneState.movement)
{
case MovementPhase::MoveAcross:
objectPosX += objectSpeed * dt;
if (objectPosX >= waterRightX)
{
objectPosX = waterRightX;
sceneState.movement = MovementPhase::Falling;
objectVelocityY = 0.0f;
}
objectPosY = computeWaterHeight(objectPosX) + getObjectFloatOffset();
break;
case MovementPhase::Falling:
objectVelocityY -= gravity * dt;
objectPosY += objectVelocityY * dt;
if (objectPosY <= groundLevel + getObjectGroundOffset())
{
objectPosY = groundLevel + getObjectGroundOffset();
objectVelocityY = 0.0f;
sceneState.movement = MovementPhase::Waiting;
waitTimer = waitDuration;
}
break;
case MovementPhase::Waiting:
if (waitTimer > 0.0f)
{
waitTimer -= dt;
if (waitTimer <= 0.0f)
{
objectPosX = waterLeftX;
objectVelocityY = 0.0f;
sceneState.movement = MovementPhase::MoveAcross;
objectPosY = computeWaterHeight(objectPosX) + getObjectFloatOffset();
waitTimer = 0.0f;
}
}
break;
}
}

void applyCamera()
{
switch (sceneState.camera)
{
case CameraState::Front:
gluLookAt(0.0f, 6.0f, 24.0f, 0.0f, 4.0f, 0.0f, 0.0f, 1.0f, 0.0f);
break;
case CameraState::Perspective:
gluLookAt(-16.0f, 9.0f, 18.0f, 0.0f, 4.0f, 0.0f, 0.0f, 1.0f, 0.0f);
break;
}
}

void updateWaterSurface()
{
float step = (waterRightX - waterLeftX) / static_cast<float>(waterResolution);
for (int i = 0; i <= waterResolution; ++i)
{
float x = waterLeftX + step * i;
waterHeights[i] = computeWaterHeight(x);
}
}

float computeWaterHeight(float x)
{
float clampedX = x;
if (clampedX < waterLeftX)
clampedX = waterLeftX;
if (clampedX > waterRightX)
clampedX = waterRightX;
if (sceneState.water == WaterState::Flat)
{
return waterBaseHeight;
}
float normalized = (clampedX - waterLeftX) / (waterRightX - waterLeftX);
float phase = static_cast<float>(M_PI) * 2.0f * (waterWaveCycles * normalized) + waterWaveSpeed * waterTime;
return waterBaseHeight + waterWaveAmplitude * std::sin(phase);
}

float getObjectFloatOffset()
{
return (sceneState.object == ObjectState::Duck) ? 0.8f : 0.45f;
}

float getObjectGroundOffset()
{
return (sceneState.object == ObjectState::Duck) ? 0.9f : 0.35f;
}

void resetMovement()
{
sceneState.movement = MovementPhase::MoveAcross;
objectPosX = waterLeftX;
objectVelocityY = 0.0f;
waitTimer = 0.0f;
objectPosY = computeWaterHeight(objectPosX) + getObjectFloatOffset();
}

void drawGround()
{
        if (!groundMesh)
                return;

        glPushMatrix();
        glDisable(GL_LIGHTING);
        glUseProgram(groundProgram);
        if (groundColorLocation >= 0)
        {
                glUniform3f(groundColorLocation, groundBaseColor.x, groundBaseColor.y, groundBaseColor.z);
        }
        groundMesh->DrawMeshVBO(meshSize);
        glUseProgram(0);
        glEnable(GL_LIGHTING);
        glPopMatrix();
}

void drawBooth()
{
const GLfloat boothAmbient[] = { 0.18f, 0.18f, 0.18f, 1.0f };
const GLfloat boothDiffuse[] = { 0.55f, 0.55f, 0.58f, 1.0f };
const GLfloat boothSpecular[] = { 0.2f, 0.2f, 0.22f, 1.0f };
const GLfloat trimAmbient[] = { 0.12f, 0.12f, 0.15f, 1.0f };
const GLfloat trimDiffuse[] = { 0.35f, 0.35f, 0.4f, 1.0f };
const GLfloat trimSpecular[] = { 0.3f, 0.3f, 0.35f, 1.0f };
const GLfloat panelAmbient[] = { 0.08f, 0.08f, 0.1f, 1.0f };
const GLfloat panelDiffuse[] = { 0.3f, 0.3f, 0.35f, 1.0f };
const GLfloat panelSpecular[] = { 0.15f, 0.15f, 0.2f, 1.0f };

setMaterial(boothAmbient, boothDiffuse, boothSpecular, 20.0f);

glPushMatrix();
glTranslatef(0.0f, 0.2f, 0.0f);
glScalef(boothWidth, 0.4f, boothDepth);
glutSolidCube(1.0f);
glPopMatrix();

glPushMatrix();
glTranslatef(-boothWidth / 2.0f + 0.3f, boothHeight / 2.0f + 0.4f, 0.0f);
glScalef(0.6f, boothHeight, boothDepth);
glutSolidCube(1.0f);
glPopMatrix();

glPushMatrix();
glTranslatef(boothWidth / 2.0f - 0.3f, boothHeight / 2.0f + 0.4f, 0.0f);
glScalef(0.6f, boothHeight, boothDepth);
glutSolidCube(1.0f);
glPopMatrix();

glPushMatrix();
glTranslatef(0.0f, boothHeight / 2.0f + 0.4f, -boothDepth / 2.0f + 0.3f);
glScalef(boothWidth, boothHeight, 0.6f);
glutSolidCube(1.0f);
glPopMatrix();

setMaterial(trimAmbient, trimDiffuse, trimSpecular, 30.0f);
glPushMatrix();
glTranslatef(0.0f, boothHeight + 0.8f, 0.0f);
glScalef(boothWidth + 0.8f, 0.6f, boothDepth + 0.8f);
glutSolidCube(1.0f);
glPopMatrix();

setMaterial(panelAmbient, panelDiffuse, panelSpecular, 12.0f);
glPushMatrix();
glTranslatef(0.0f, boothHeight / 2.0f + 0.6f, -boothDepth / 2.0f + 0.1f);
glScalef(boothWidth - 1.2f, boothHeight - 1.4f, 0.2f);
glutSolidCube(1.0f);
glPopMatrix();
}

void drawWater()
{
float bottomY = waterBaseHeight - waterThickness;

const GLfloat waterAmbient[] = { 0.02f, 0.07f, 0.12f, 1.0f };
const GLfloat waterDiffuse[] = { 0.25f, 0.45f, 0.85f, 1.0f };
const GLfloat waterSpecular[] = { 0.4f, 0.4f, 0.5f, 1.0f };

setMaterial(waterAmbient, waterDiffuse, waterSpecular, 30.0f);

glBegin(GL_QUADS);
glNormal3f(0.0f, 0.0f, 1.0f);
glVertex3f(waterLeftX, bottomY, waterFrontZ);
glVertex3f(waterRightX, bottomY, waterFrontZ);
glVertex3f(waterRightX, waterBaseHeight, waterFrontZ);
glVertex3f(waterLeftX, waterBaseHeight, waterFrontZ);

glNormal3f(0.0f, 0.0f, -1.0f);
glVertex3f(waterRightX, bottomY, waterBackZ);
glVertex3f(waterLeftX, bottomY, waterBackZ);
glVertex3f(waterLeftX, waterBaseHeight, waterBackZ);
glVertex3f(waterRightX, waterBaseHeight, waterBackZ);

glNormal3f(1.0f, 0.0f, 0.0f);
glVertex3f(waterRightX, bottomY, waterFrontZ);
glVertex3f(waterRightX, bottomY, waterBackZ);
glVertex3f(waterRightX, waterBaseHeight, waterBackZ);
glVertex3f(waterRightX, waterBaseHeight, waterFrontZ);

glNormal3f(-1.0f, 0.0f, 0.0f);
glVertex3f(waterLeftX, bottomY, waterBackZ);
glVertex3f(waterLeftX, bottomY, waterFrontZ);
glVertex3f(waterLeftX, waterBaseHeight, waterFrontZ);
glVertex3f(waterLeftX, waterBaseHeight, waterBackZ);

glNormal3f(0.0f, -1.0f, 0.0f);
glVertex3f(waterLeftX, bottomY, waterBackZ);
glVertex3f(waterRightX, bottomY, waterBackZ);
glVertex3f(waterRightX, bottomY, waterFrontZ);
glVertex3f(waterLeftX, bottomY, waterFrontZ);
glEnd();

glDisable(GL_LIGHTING);
glColor3f(0.55f, 0.78f, 0.95f);
float step = (waterRightX - waterLeftX) / static_cast<float>(waterResolution);
for (int i = 0; i < waterResolution; ++i)
{
float x0 = waterLeftX + step * i;
float x1 = x0 + step;
float y0 = waterHeights[i];
float y1 = waterHeights[i + 1];

glBegin(GL_TRIANGLE_STRIP);
glVertex3f(x0, y0, waterFrontZ);
glVertex3f(x0, y0, waterBackZ);
glVertex3f(x1, y1, waterFrontZ);
glVertex3f(x1, y1, waterBackZ);
glEnd();
}
glEnable(GL_LIGHTING);
}

void drawActiveObject()
{
glPushMatrix();
glTranslatef(objectPosX, objectPosY, objectZPosition);
if (sceneState.object == ObjectState::Duck)
{
drawDuck();
}
else
{
drawTarget();
}
glPopMatrix();
}

void drawDuck()
{
const GLfloat bodyAmbient[] = { 0.28f, 0.2f, 0.05f, 1.0f };
const GLfloat bodyDiffuse[] = { 0.95f, 0.8f, 0.2f, 1.0f };
const GLfloat bodySpecular[] = { 0.5f, 0.45f, 0.28f, 1.0f };
const GLfloat wingAmbient[] = { 0.26f, 0.18f, 0.05f, 1.0f };
const GLfloat wingDiffuse[] = { 0.85f, 0.7f, 0.2f, 1.0f };
const GLfloat wingSpecular[] = { 0.35f, 0.35f, 0.2f, 1.0f };
const GLfloat beakAmbient[] = { 0.4f, 0.2f, 0.02f, 1.0f };
const GLfloat beakDiffuse[] = { 0.95f, 0.5f, 0.05f, 1.0f };
const GLfloat beakSpecular[] = { 0.6f, 0.4f, 0.2f, 1.0f };
const GLfloat eyeAmbient[] = { 0.05f, 0.05f, 0.05f, 1.0f };
const GLfloat eyeDiffuse[] = { 0.1f, 0.1f, 0.1f, 1.0f };
const GLfloat eyeSpecular[] = { 0.5f, 0.5f, 0.5f, 1.0f };

setMaterial(bodyAmbient, bodyDiffuse, bodySpecular, 38.0f);
glPushMatrix();
glScalef(1.4f, 1.0f, 2.0f);
glutSolidSphere(0.6f, 32, 32);
glPopMatrix();

setMaterial(wingAmbient, wingDiffuse, wingSpecular, 22.0f);
for (int side = -1; side <= 1; side += 2)
{
glPushMatrix();
glTranslatef(side * 0.9f, 0.0f, -0.2f);
glRotatef(side * 20.0f, 0.0f, 0.0f, 1.0f);
glScalef(0.6f, 0.2f, 1.0f);
glutSolidSphere(0.6f, 20, 20);
glPopMatrix();
}

glPushMatrix();
glTranslatef(0.0f, -0.25f, -1.1f);
glRotatef(35.0f, 1.0f, 0.0f, 0.0f);
glScalef(0.6f, 0.2f, 1.0f);
glutSolidCube(1.0f);
glPopMatrix();

glPushMatrix();
glTranslatef(0.0f, 0.8f, 0.8f);
glRotatef(-8.0f, 1.0f, 0.0f, 0.0f);
glScalef(0.7f, 0.7f, 0.7f);
glutSolidSphere(0.45f, 24, 24);

setMaterial(beakAmbient, beakDiffuse, beakSpecular, 25.0f);
glPushMatrix();
glTranslatef(0.0f, -0.1f, 0.55f);
glutSolidCone(0.18f, 0.5f, 20, 20);
glPopMatrix();

setMaterial(eyeAmbient, eyeDiffuse, eyeSpecular, 80.0f);
glPushMatrix();
glTranslatef(0.2f, 0.15f, 0.28f);
glScalef(0.12f, 0.12f, 0.12f);
glutSolidSphere(0.5f, 12, 12);
glPopMatrix();
glPushMatrix();
glTranslatef(-0.2f, 0.15f, 0.28f);
glScalef(0.12f, 0.12f, 0.12f);
glutSolidSphere(0.5f, 12, 12);
glPopMatrix();
glPopMatrix();

glPushMatrix();
glTranslatef(0.0f, 0.05f, 1.2f);
glScalef(0.75f, 0.75f, 0.75f);
drawTarget();
glPopMatrix();
}

void drawTarget()
{
if (!targetQuadric)
return;

const GLfloat rimAmbient[] = { 0.12f, 0.12f, 0.14f, 1.0f };
const GLfloat rimDiffuse[] = { 0.5f, 0.5f, 0.55f, 1.0f };
const GLfloat rimSpecular[] = { 0.9f, 0.9f, 0.95f, 1.0f };
const GLfloat whiteAmbient[] = { 0.6f, 0.6f, 0.6f, 1.0f };
const GLfloat whiteDiffuse[] = { 0.95f, 0.95f, 0.95f, 1.0f };
const GLfloat whiteSpecular[] = { 0.3f, 0.3f, 0.3f, 1.0f };
const GLfloat redAmbient[] = { 0.35f, 0.0f, 0.0f, 1.0f };
const GLfloat redDiffuse[] = { 0.9f, 0.1f, 0.1f, 1.0f };
const GLfloat redSpecular[] = { 0.4f, 0.2f, 0.2f, 1.0f };

float depth = 0.25f;

setMaterial(rimAmbient, rimDiffuse, rimSpecular, 60.0f);
glPushMatrix();
glTranslatef(0.0f, 0.0f, -depth * 0.5f);
gluCylinder(targetQuadric, 0.7f, 0.7f, depth, 32, 1);

setMaterial(whiteAmbient, whiteDiffuse, whiteSpecular, 35.0f);
gluDisk(targetQuadric, 0.0f, 0.7f, 32, 1);
glPushMatrix();
glTranslatef(0.0f, 0.0f, depth);
gluDisk(targetQuadric, 0.0f, 0.7f, 32, 1);
glPopMatrix();

setMaterial(redAmbient, redDiffuse, redSpecular, 35.0f);
gluDisk(targetQuadric, 0.0f, 0.45f, 32, 1);
glPushMatrix();
glTranslatef(0.0f, 0.0f, depth);
gluDisk(targetQuadric, 0.0f, 0.45f, 32, 1);
glPopMatrix();

setMaterial(whiteAmbient, whiteDiffuse, whiteSpecular, 35.0f);
gluDisk(targetQuadric, 0.0f, 0.22f, 32, 1);
glPushMatrix();
glTranslatef(0.0f, 0.0f, depth);
gluDisk(targetQuadric, 0.0f, 0.22f, 32, 1);
glPopMatrix();
glPopMatrix();
}

void setMaterial(const GLfloat ambient[4], const GLfloat diffuse[4], const GLfloat specular[4], GLfloat shininess)
{
        GLfloat shininessArray[] = { shininess };
        glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
        glMaterialfv(GL_FRONT, GL_SPECULAR, specular);
        glMaterialfv(GL_FRONT, GL_SHININESS, shininessArray);
}

GLuint buildGroundProgram()
{
        const char *vertexSrc =
                "#version 120\n"
                "attribute vec3 position;\n"
                "attribute vec3 normal;\n"
                "varying float vLight;\n"
                "void main()\n"
                "{\n"
                "    vec3 lightDir = normalize(vec3(0.3, 1.0, 0.5));\n"
                "    vLight = max(dot(normalize(normal), lightDir), 0.0);\n"
                "    gl_Position = gl_ModelViewProjectionMatrix * vec4(position, 1.0);\n"
                "}\n";

        const char *fragmentSrc =
                "#version 120\n"
                "varying float vLight;\n"
                "uniform vec3 uBaseColor;\n"
                "void main()\n"
                "{\n"
                "    vec3 color = uBaseColor * (0.35 + 0.65 * vLight);\n"
                "    gl_FragColor = vec4(color, 1.0);\n"
                "}\n";

        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSrc);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glBindAttribLocation(program, 0, "position");
        glBindAttribLocation(program, 1, "normal");
        glLinkProgram(program);

        GLint linked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked)
        {
                GLint logLength = 0;
                glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
                if (logLength > 1)
                {
                        std::vector<char> log(logLength);
                        glGetProgramInfoLog(program, logLength, NULL, &log[0]);
                        std::fprintf(stderr, "Program link error: %s\n", &log[0]);
                }
                glDeleteProgram(program);
                program = 0;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return program;
}

GLuint compileShader(GLenum type, const char *src)
{
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
                GLint logLength = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
                if (logLength > 1)
                {
                        std::vector<char> log(logLength);
                        glGetShaderInfoLog(shader, logLength, NULL, &log[0]);
                        std::fprintf(stderr, "Shader compile error: %s\n", &log[0]);
                }
                glDeleteShader(shader);
                return 0;
        }
        return shader;
}
