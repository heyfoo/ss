#define _USE_MATH_DEFINES
#include <algorithm>
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
const float boothDepth = 10.0f;
const float boothHeight = 12.0f;

const float waterWidth = 14.0f;
const float waterDepth = 4.0f;
const float waterSurfaceY = 5.8f;
const float waterBottomY = 4.0f;
const float waterCenterZ = 1.5f;

const float waterLeftX = -waterWidth * 0.5f;
const float waterRightX = waterWidth * 0.5f;
const float waterBackZ = waterCenterZ - waterDepth * 0.5f;
const float waterFrontZ = waterCenterZ + waterDepth * 0.5f;

const float pathLeftX = waterLeftX + 0.6f;
const float pathRightX = waterRightX - 0.6f;
const float pathZ = waterCenterZ;

const float objectFloatOffset = 1.0f;
const float objectGroundRestY = 1.1f;

const float moveSpeed = 4.6f;
const float gravityAccel = 28.0f;
const float groundPauseDuration = 1.6f;

const float waveAmplitude = 0.65f;
const float waveSpeed = 1.3f;
const float primaryWaveFrequency = 2.0f * static_cast<float>(M_PI);
const float secondaryWaveFrequency = 1.1f * static_cast<float>(M_PI);

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

enum class CameraState
{
Front,
Perspective
};

enum class MovementPhase
{
MoveAcross,
Falling,
GroundPause
};

WaterState waterState = WaterState::Wavy;
ObjectState objectState = ObjectState::Duck;
CameraState cameraState = CameraState::Front;
MovementPhase movementPhase = MovementPhase::MoveAcross;

float wavePhase = 0.0f;
float objectPosX = pathLeftX;
float objectPosY = waterSurfaceY + objectFloatOffset;
float objectPosZ = pathZ;
float verticalVelocity = 0.0f;
float groundPauseTimer = 0.0f;

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

float cameraAzimuth = 0.0f;
float cameraElevation = 18.0f;
float cameraRadius = 34.0f;
float cameraTargetRadius = 34.0f;

const float minRadius = 20.0f;
const float maxRadius = 46.0f;
const float minElevation = 10.0f;
const float maxElevation = 55.0f;
const float orbitSensitivity = 0.25f;
const float elevationSensitivity = 0.2f;
const float zoomSensitivity = 0.2f;

bool leftButtonDown = false;
bool rightButtonDown = false;
int lastMouseX = 0;
int lastMouseY = 0;

int lastFrameTime = 0;

GLUquadric *targetQuadric = NULL;

void initOpenGL(int w, int h);
void display(void);
void reshape(int w, int h);
void keyboard(unsigned char key, int x, int y);
void mouse(int button, int state, int x, int y);
void mouseMotionHandler(int xMouse, int yMouse);
void animationHandler(int param);

void updateAnimation(float dt);
void resetObjectToStart();
float getWaterSurfaceHeight(float x, float z);
Vector3 getWaterNormal(float x, float z);
void applyCameraPreset(CameraState state);

void drawGround();
void drawBooth();
void drawWater();
void drawActiveObject();
void drawDuck();
void drawStandaloneTarget();
void drawTargetLayer(float innerRadius, float outerRadius);
void setMaterial(const GLfloat ambient[4], const GLfloat diffuse[4], const GLfloat specular[4], GLfloat shininess);

GLuint buildGroundProgram();
GLuint compileShader(GLenum type, const char *src);

int main(int argc, char **argv)
{
glutInit(&argc, argv);
glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
glutInitWindowSize(vWidth, vHeight);
glutInitWindowPosition(200, 30);
glutCreateWindow("Shooting Gallery");

initOpenGL(vWidth, vHeight);

glutDisplayFunc(display);
glutReshapeFunc(reshape);
glutMouseFunc(mouse);
glutMotionFunc(mouseMotionHandler);
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

Vector3 ambient = Vector3(0.05f, 0.16f, 0.05f);
Vector3 diffuse = Vector3(groundBaseColor.x, groundBaseColor.y, groundBaseColor.z);
Vector3 specular = Vector3(0.12f, 0.12f, 0.12f);
groundMesh->SetMaterial(ambient, diffuse, specular, 6.0);

groundProgram = buildGroundProgram();
groundColorLocation = glGetUniformLocation(groundProgram, "uBaseColor");
groundMesh->CreateMeshVBO(meshSize, 0, 1);

targetQuadric = gluNewQuadric();
if (targetQuadric)
{
gluQuadricNormals(targetQuadric, GLU_SMOOTH);
}

applyCameraPreset(cameraState);

reshape(w, h);
lastFrameTime = glutGet(GLUT_ELAPSED_TIME);
updateAnimation(0.0f);
}

void display(void)
{
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
glLoadIdentity();

const float degToRad = static_cast<float>(M_PI) / 180.0f;
float azRad = cameraAzimuth * degToRad;
float elRad = cameraElevation * degToRad;
float cosEl = std::cos(elRad);
float eyeX = cameraRadius * std::sin(azRad) * cosEl;
float eyeY = cameraRadius * std::sin(elRad);
float eyeZ = cameraRadius * std::cos(azRad) * cosEl;

gluLookAt(eyeX, eyeY, eyeZ, 0.0f, 4.5f, 0.0f, 0.0f, 1.0f, 0.0f);

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
case '1':
case 'w':
case 'W':
waterState = (waterState == WaterState::Wavy) ? WaterState::Flat : WaterState::Wavy;
if (movementPhase == MovementPhase::MoveAcross)
{
objectPosY = getWaterSurfaceHeight(objectPosX, pathZ) + objectFloatOffset;
}
break;
case '2':
case 'd':
case 'D':
objectState = (objectState == ObjectState::Duck) ? ObjectState::TargetOnly : ObjectState::Duck;
break;
case '3':
case 'c':
case 'C':
cameraState = (cameraState == CameraState::Front) ? CameraState::Perspective : CameraState::Front;
applyCameraPreset(cameraState);
break;
case 'r':
case 'R':
resetObjectToStart();
break;
default:
break;
}

glutPostRedisplay();
}

void mouse(int button, int state, int x, int y)
{
if (button == GLUT_LEFT_BUTTON)
{
leftButtonDown = (state == GLUT_DOWN);
lastMouseX = x;
lastMouseY = y;
}
else if (button == GLUT_RIGHT_BUTTON)
{
rightButtonDown = (state == GLUT_DOWN);
lastMouseX = x;
lastMouseY = y;
}

glutPostRedisplay();
}

void mouseMotionHandler(int xMouse, int yMouse)
{
int dx = xMouse - lastMouseX;
int dy = yMouse - lastMouseY;

if (leftButtonDown)
{
cameraAzimuth += dx * orbitSensitivity;
cameraElevation -= dy * elevationSensitivity;
if (cameraAzimuth > 70.0f)
cameraAzimuth = 70.0f;
if (cameraAzimuth < -70.0f)
cameraAzimuth = -70.0f;
if (cameraElevation > maxElevation)
cameraElevation = maxElevation;
if (cameraElevation < minElevation)
cameraElevation = minElevation;
}

if (rightButtonDown)
{
cameraTargetRadius += dy * zoomSensitivity;
if (cameraTargetRadius > maxRadius)
cameraTargetRadius = maxRadius;
if (cameraTargetRadius < minRadius)
cameraTargetRadius = minRadius;
}

lastMouseX = xMouse;
lastMouseY = yMouse;

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
const float twoPi = 2.0f * static_cast<float>(M_PI);
wavePhase += dt * waveSpeed;
if (wavePhase > twoPi)
{
wavePhase = std::fmod(wavePhase, twoPi);
}

float smoothing = std::min(1.0f, dt * 5.0f);
cameraRadius += (cameraTargetRadius - cameraRadius) * smoothing;

switch (movementPhase)
{
case MovementPhase::MoveAcross:
{
objectPosX += moveSpeed * dt;
if (objectPosX >= pathRightX)
{
objectPosX = pathRightX;
movementPhase = MovementPhase::Falling;
verticalVelocity = 0.0f;
}
objectPosY = getWaterSurfaceHeight(objectPosX, pathZ) + objectFloatOffset;
break;
}
case MovementPhase::Falling:
{
verticalVelocity += gravityAccel * dt;
objectPosY -= verticalVelocity * dt;
if (objectPosY <= objectGroundRestY)
{
objectPosY = objectGroundRestY;
movementPhase = MovementPhase::GroundPause;
groundPauseTimer = 0.0f;
}
break;
}
case MovementPhase::GroundPause:
{
groundPauseTimer += dt;
if (groundPauseTimer >= groundPauseDuration)
{
resetObjectToStart();
}
break;
}
}
}

void resetObjectToStart()
{
movementPhase = MovementPhase::MoveAcross;
objectPosX = pathLeftX;
objectPosZ = pathZ;
verticalVelocity = 0.0f;
groundPauseTimer = 0.0f;
objectPosY = getWaterSurfaceHeight(objectPosX, pathZ) + objectFloatOffset;
}

float getWaterSurfaceHeight(float x, float z)
{
float height = waterSurfaceY;
if (waterState == WaterState::Wavy)
{
float xRatio = (x - waterLeftX) / (waterRightX - waterLeftX);
float zRatio = (z - waterBackZ) / (waterFrontZ - waterBackZ);
float primary = std::sin(primaryWaveFrequency * xRatio + wavePhase);
float secondary = std::sin(secondaryWaveFrequency * zRatio + wavePhase * 0.6f);
height += waveAmplitude * (0.7f * primary + 0.3f * secondary);
}
return height;
}

Vector3 getWaterNormal(float x, float z)
{
if (waterState == WaterState::Flat)
{
return Vector3(0.0f, 1.0f, 0.0f);
}

const float eps = 0.1f;
float hL = getWaterSurfaceHeight(x - eps, z);
float hR = getWaterSurfaceHeight(x + eps, z);
float hB = getWaterSurfaceHeight(x, z - eps);
float hF = getWaterSurfaceHeight(x, z + eps);

Vector3 tangentX(2.0f * eps, hR - hL, 0.0f);
Vector3 tangentZ(0.0f, hF - hB, 2.0f * eps);
Vector3 normal = tangentZ.cross(tangentX);
normal.normalize();
return normal;
}

void applyCameraPreset(CameraState state)
{
if (state == CameraState::Front)
{
cameraAzimuth = 0.0f;
cameraElevation = 18.0f;
cameraRadius = cameraTargetRadius = 34.0f;
}
else
{
cameraAzimuth = -32.0f;
cameraElevation = 24.0f;
cameraRadius = cameraTargetRadius = 36.0f;
}
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
const GLfloat boothAmbient[] = { 0.22f, 0.22f, 0.25f, 1.0f };
const GLfloat boothDiffuse[] = { 0.62f, 0.62f, 0.66f, 1.0f };
const GLfloat boothSpecular[] = { 0.35f, 0.35f, 0.4f, 1.0f };
const GLfloat trimAmbient[] = { 0.18f, 0.08f, 0.08f, 1.0f };
const GLfloat trimDiffuse[] = { 0.7f, 0.25f, 0.25f, 1.0f };
const GLfloat trimSpecular[] = { 0.3f, 0.2f, 0.2f, 1.0f };
const GLfloat beamAmbient[] = { 0.12f, 0.12f, 0.14f, 1.0f };
const GLfloat beamDiffuse[] = { 0.4f, 0.4f, 0.45f, 1.0f };
const GLfloat beamSpecular[] = { 0.2f, 0.2f, 0.25f, 1.0f };

setMaterial(boothAmbient, boothDiffuse, boothSpecular, 48.0f);

// floor platform
glPushMatrix();
glTranslatef(0.0f, 0.6f, 0.0f);
glScalef(boothWidth, 1.2f, boothDepth);
glutSolidCube(1.0f);
glPopMatrix();

// ceiling
glPushMatrix();
glTranslatef(0.0f, boothHeight - 0.6f, 0.0f);
glScalef(boothWidth, 1.2f, boothDepth);
glutSolidCube(1.0f);
glPopMatrix();

// side walls
glPushMatrix();
glTranslatef(-boothWidth * 0.5f + 0.4f, boothHeight * 0.5f, 0.0f);
glScalef(0.8f, boothHeight - 1.2f, boothDepth);
glutSolidCube(1.0f);
glPopMatrix();

glPushMatrix();
glTranslatef(boothWidth * 0.5f - 0.4f, boothHeight * 0.5f, 0.0f);
glScalef(0.8f, boothHeight - 1.2f, boothDepth);
glutSolidCube(1.0f);
glPopMatrix();

// back wall
glPushMatrix();
glTranslatef(0.0f, boothHeight * 0.5f, -boothDepth * 0.5f + 0.4f);
glScalef(boothWidth - 0.8f, boothHeight - 1.2f, 0.8f);
glutSolidCube(1.0f);
glPopMatrix();

// roof beams framing the opening
setMaterial(beamAmbient, beamDiffuse, beamSpecular, 24.0f);
glPushMatrix();
glTranslatef(0.0f, boothHeight - 1.4f, boothDepth * 0.5f - 0.6f);
glScalef(boothWidth - 1.2f, 0.8f, 0.8f);
glutSolidCube(1.0f);
glPopMatrix();

glPushMatrix();
glTranslatef(0.0f, 2.4f, boothDepth * 0.5f - 0.6f);
glScalef(boothWidth - 1.2f, 0.6f, 0.8f);
glutSolidCube(1.0f);
glPopMatrix();

setMaterial(trimAmbient, trimDiffuse, trimSpecular, 32.0f);
glPushMatrix();
glTranslatef(0.0f, boothHeight * 0.5f, boothDepth * 0.5f - 0.2f);
glScalef(boothWidth, boothHeight - 1.0f, 0.4f);
glutSolidCube(1.0f);
glPopMatrix();
}

void drawWater()
{
const GLfloat waterAmbient[] = { 0.0f, 0.08f, 0.18f, 1.0f };
const GLfloat waterDiffuse[] = { 0.2f, 0.45f, 0.8f, 1.0f };
const GLfloat waterSpecular[] = { 0.5f, 0.6f, 0.7f, 1.0f };

setMaterial(waterAmbient, waterDiffuse, waterSpecular, 48.0f);

// draw the water volume without the animated top
glPushMatrix();
float volumeHeight = (waterSurfaceY - waterBottomY) - 0.06f;
float waterCenterY = waterBottomY + 0.5f * volumeHeight;
glTranslatef(0.0f, waterCenterY, waterCenterZ);
glScalef(waterWidth, volumeHeight, waterDepth);
glutSolidCube(1.0f);
glPopMatrix();

// animated surface
int segmentsX = 48;
int segmentsZ = 10;
float stepX = (waterRightX - waterLeftX) / segmentsX;
float stepZ = (waterFrontZ - waterBackZ) / segmentsZ;

for (int z = 0; z < segmentsZ; ++z)
{
float z0 = waterBackZ + stepZ * z;
float z1 = z0 + stepZ;
glBegin(GL_TRIANGLE_STRIP);
for (int x = 0; x <= segmentsX; ++x)
{
float worldX = waterLeftX + stepX * x;

Vector3 normal0 = getWaterNormal(worldX, z0);
float h0 = getWaterSurfaceHeight(worldX, z0);
glNormal3f(normal0.x, normal0.y, normal0.z);
glVertex3f(worldX, h0, z0);

Vector3 normal1 = getWaterNormal(worldX, z1);
float h1 = getWaterSurfaceHeight(worldX, z1);
glNormal3f(normal1.x, normal1.y, normal1.z);
glVertex3f(worldX, h1, z1);
}
glEnd();
}
}

void drawActiveObject()
{
const GLfloat railAmbient[] = { 0.12f, 0.12f, 0.12f, 1.0f };
const GLfloat railDiffuse[] = { 0.35f, 0.35f, 0.38f, 1.0f };
const GLfloat railSpecular[] = { 0.5f, 0.5f, 0.55f, 1.0f };
setMaterial(railAmbient, railDiffuse, railSpecular, 56.0f);

// front support rail
glPushMatrix();
glTranslatef(0.0f, waterSurfaceY + 0.15f, waterFrontZ + 0.4f);
glScalef(waterWidth + 2.0f, 0.3f, 0.6f);
glutSolidCube(1.0f);
glPopMatrix();

glPushMatrix();
glTranslatef(0.0f, objectPosY, objectPosZ);
if (objectState == ObjectState::Duck)
{
drawDuck();
}
else
{
drawStandaloneTarget();
}
glPopMatrix();
}

void drawDuck()
{
const GLfloat bodyAmbient[] = { 0.28f, 0.2f, 0.05f, 1.0f };
const GLfloat bodyDiffuse[] = { 0.95f, 0.78f, 0.18f, 1.0f };
const GLfloat bodySpecular[] = { 0.5f, 0.5f, 0.3f, 1.0f };
const GLfloat wingAmbient[] = { 0.26f, 0.18f, 0.05f, 1.0f };
const GLfloat wingDiffuse[] = { 0.85f, 0.7f, 0.2f, 1.0f };
const GLfloat wingSpecular[] = { 0.35f, 0.35f, 0.2f, 1.0f };
const GLfloat beakAmbient[] = { 0.4f, 0.2f, 0.02f, 1.0f };
const GLfloat beakDiffuse[] = { 0.95f, 0.5f, 0.05f, 1.0f };
const GLfloat beakSpecular[] = { 0.6f, 0.4f, 0.2f, 1.0f };
const GLfloat eyeAmbient[] = { 0.05f, 0.05f, 0.05f, 1.0f };
const GLfloat eyeDiffuse[] = { 0.1f, 0.1f, 0.1f, 1.0f };
const GLfloat eyeSpecular[] = { 0.5f, 0.5f, 0.5f, 1.0f };
const GLfloat whiteAmbient[] = { 0.6f, 0.6f, 0.6f, 1.0f };
const GLfloat whiteDiffuse[] = { 0.9f, 0.9f, 0.9f, 1.0f };
const GLfloat whiteSpecular[] = { 0.3f, 0.3f, 0.3f, 1.0f };
const GLfloat redAmbient[] = { 0.4f, 0.0f, 0.0f, 1.0f };
const GLfloat redDiffuse[] = { 0.9f, 0.1f, 0.1f, 1.0f };
const GLfloat redSpecular[] = { 0.5f, 0.2f, 0.2f, 1.0f };

float bodyLength = 2.6f;
float bodyHeight = 1.8f;
float bodyWidth = 1.6f;

setMaterial(bodyAmbient, bodyDiffuse, bodySpecular, 40.0f);
glPushMatrix();
glScalef(bodyWidth, bodyHeight, bodyLength);
glutSolidSphere(0.5f, 32, 32);
glPopMatrix();

setMaterial(wingAmbient, wingDiffuse, wingSpecular, 28.0f);
for (int side = -1; side <= 1; side += 2)
{
glPushMatrix();
glTranslatef(side * bodyWidth * 0.55f, 0.05f, -0.2f);
glRotatef(side * 25.0f, 0.0f, 0.0f, 1.0f);
glScalef(bodyWidth * 0.5f, bodyHeight * 0.7f, bodyLength * 0.35f);
glutSolidCube(1.0f);
glPopMatrix();
}

glPushMatrix();
glTranslatef(0.0f, -0.3f, -bodyLength * 0.45f);
glRotatef(25.0f, 1.0f, 0.0f, 0.0f);
glScalef(bodyWidth * 0.45f, 0.2f, bodyLength * 0.6f);
glutSolidCube(1.0f);
glPopMatrix();

setMaterial(bodyAmbient, bodyDiffuse, bodySpecular, 40.0f);
glPushMatrix();
glTranslatef(0.0f, bodyHeight * 0.65f, bodyLength * 0.2f);
glPushMatrix();
glScalef(0.9f, 0.9f, 0.9f);
glutSolidSphere(0.5f, 24, 24);
glPopMatrix();

setMaterial(beakAmbient, beakDiffuse, beakSpecular, 25.0f);
glPushMatrix();
glTranslatef(0.0f, -0.05f, 0.55f);
glutSolidCone(0.22f, 0.6f, 20, 20);
glPopMatrix();

setMaterial(eyeAmbient, eyeDiffuse, eyeSpecular, 80.0f);
glPushMatrix();
glTranslatef(0.22f, 0.15f, 0.35f);
glScalef(0.12f, 0.12f, 0.12f);
glutSolidSphere(0.5f, 12, 12);
glPopMatrix();

glPushMatrix();
glTranslatef(-0.22f, 0.15f, 0.35f);
glScalef(0.12f, 0.12f, 0.12f);
glutSolidSphere(0.5f, 12, 12);
glPopMatrix();

glPushMatrix();
glTranslatef(0.0f, -0.35f, 0.55f);
setMaterial(whiteAmbient, whiteDiffuse, whiteSpecular, 30.0f);
drawTargetLayer(0.0f, 0.7f);
setMaterial(redAmbient, redDiffuse, redSpecular, 30.0f);
drawTargetLayer(0.35f, 0.55f);
setMaterial(whiteAmbient, whiteDiffuse, whiteSpecular, 30.0f);
drawTargetLayer(0.0f, 0.22f);
glPopMatrix();

glPopMatrix();
}

void drawStandaloneTarget()
{
    const GLfloat whiteAmbient[] = { 0.6f, 0.6f, 0.6f, 1.0f };
    const GLfloat whiteDiffuse[] = { 0.9f, 0.9f, 0.9f, 1.0f };
    const GLfloat whiteSpecular[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    const GLfloat redAmbient[] = { 0.4f, 0.0f, 0.0f, 1.0f };
    const GLfloat redDiffuse[] = { 0.9f, 0.1f, 0.1f, 1.0f };
    const GLfloat redSpecular[] = { 0.5f, 0.2f, 0.2f, 1.0f };

    glPushMatrix();
    setMaterial(whiteAmbient, whiteDiffuse, whiteSpecular, 30.0f);
    drawTargetLayer(0.0f, 0.75f);
    setMaterial(redAmbient, redDiffuse, redSpecular, 30.0f);
    drawTargetLayer(0.4f, 0.6f);
    setMaterial(whiteAmbient, whiteDiffuse, whiteSpecular, 30.0f);
    drawTargetLayer(0.0f, 0.25f);
    glPopMatrix();
}

void drawTargetLayer(float innerRadius, float outerRadius)
{
    if (!targetQuadric)
        return;

    gluDisk(targetQuadric, innerRadius, outerRadius, 32, 1);
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
