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
const float trackHeight = 9.0f;
const float trackFrontZ = 6.0f;

const float pathLeftX = -8.0f;
const float pathRightX = 8.0f;
const float arcRadius = 3.0f;
const float backZ = trackFrontZ - arcRadius;
const float backY = trackHeight - arcRadius;

enum class TargetPhase
{
        MoveFront,
        RotateDown,
        MoveBack,
        RotateUp
};

TargetPhase targetPhase = TargetPhase::MoveFront;

float frontProgress = 0.0f;
float rotateProgress = 0.0f;
float backProgress = 0.0f;

float targetPosX = pathLeftX;
float targetPosY = trackHeight;
float targetPosZ = trackFrontZ;
float basePitch = 0.0f;
float flipAngle = 0.0f;
float flipTargetAngle = 0.0f;

const float frontSpeed = 5.5f;
const float backSpeed = 4.5f;
const float rotationDuration = 1.4f;
const float flipDuration = 1.2f;
const float flipSpeed = 90.0f / flipDuration;
const float sineAmplitude = 0.8f;
const float sineFrequency = 2.0f * static_cast<float>(M_PI);
const float waveSpeed = 1.5f;

float wavePhase = 0.0f;

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
float cameraElevation = 22.0f;
float cameraRadius = 30.0f;
float cameraTargetRadius = 30.0f;

const float minRadius = 18.0f;
const float maxRadius = 46.0f;
const float minElevation = 12.0f;
const float maxElevation = 62.0f;
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
void drawGround();
void drawBooth();
void drawWaterWave();
void drawTarget();
void drawDuck();
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

        Vector3 ambient = Vector3(0.02f, 0.05f, 0.02f);
        Vector3 diffuse = Vector3(groundBaseColor.x, groundBaseColor.y, groundBaseColor.z);
        Vector3 specular = Vector3(0.1f, 0.1f, 0.1f);
        groundMesh->SetMaterial(ambient, diffuse, specular, 4.0);

        groundProgram = buildGroundProgram();
        groundColorLocation = glGetUniformLocation(groundProgram, "uBaseColor");
        groundMesh->CreateMeshVBO(meshSize, 0, 1);

        targetQuadric = gluNewQuadric();

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
        drawWaterWave();
        drawTarget();

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
        case 'f':
        case 'F':
                if (targetPhase == TargetPhase::MoveFront && flipTargetAngle >= 0.0f)
                {
                        flipTargetAngle = -90.0f;
                }
                break;
        case 'r':
        case 'R':
                targetPhase = TargetPhase::MoveFront;
                frontProgress = rotateProgress = backProgress = 0.0f;
                targetPosX = pathLeftX;
                targetPosY = trackHeight;
                targetPosZ = trackFrontZ;
                basePitch = 0.0f;
                flipTargetAngle = 0.0f;
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
        float distance = pathRightX - pathLeftX;

        wavePhase += dt * waveSpeed;
        if (wavePhase > twoPi)
        {
                wavePhase = std::fmod(wavePhase, twoPi);
        }

        if (std::fabs(flipAngle - flipTargetAngle) > 0.01f)
        {
                float direction = (flipTargetAngle > flipAngle) ? 1.0f : -1.0f;
                float step = flipSpeed * dt;
                if (std::fabs(flipTargetAngle - flipAngle) <= step)
                {
                        flipAngle = flipTargetAngle;
                }
                else
                {
                        flipAngle += direction * step;
                }
        }
        else
        {
                flipAngle = flipTargetAngle;
        }

        float smoothing = std::min(1.0f, dt * 5.0f);
        cameraRadius += (cameraTargetRadius - cameraRadius) * smoothing;

        switch (targetPhase)
        {
        case TargetPhase::MoveFront:
        {
                float advance = (frontSpeed * dt) / distance;
                frontProgress += advance;
                if (frontProgress >= 1.0f)
                {
                        frontProgress = 1.0f;
                        targetPosX = pathRightX;
                        targetPosY = trackHeight;
                        targetPosZ = trackFrontZ;
                        basePitch = 0.0f;
                        targetPhase = TargetPhase::RotateDown;
                        rotateProgress = 0.0f;
                }
                else
                {
                        float t = frontProgress;
                        targetPosX = pathLeftX + (pathRightX - pathLeftX) * t;
                        float sineValue = std::sin(wavePhase + t * sineFrequency);
                        targetPosY = trackHeight + sineAmplitude * sineValue;
                        targetPosZ = trackFrontZ;
                        basePitch = 0.0f;
                }
                break;
        }
        case TargetPhase::RotateDown:
        {
                float advance = dt / rotationDuration;
                rotateProgress += advance;
                if (rotateProgress >= 1.0f)
                {
                        rotateProgress = 1.0f;
                }
                float angleDeg = 90.0f + rotateProgress * 90.0f;
                float angleRad = angleDeg * static_cast<float>(M_PI) / 180.0f;
                targetPosX = pathRightX;
                targetPosY = trackHeight + arcRadius * std::cos(angleRad);
                targetPosZ = (trackFrontZ - arcRadius) + arcRadius * std::sin(angleRad);
                basePitch = -(angleDeg - 90.0f);
                if (rotateProgress >= 1.0f)
                {
                        targetPhase = TargetPhase::MoveBack;
                        backProgress = 0.0f;
                        flipTargetAngle = 0.0f;
                }
                break;
        }
        case TargetPhase::MoveBack:
        {
                float advance = (backSpeed * dt) / distance;
                backProgress += advance;
                if (backProgress >= 1.0f)
                {
                        backProgress = 1.0f;
                        targetPosX = pathLeftX;
                        targetPosY = backY;
                        targetPosZ = backZ;
                        targetPhase = TargetPhase::RotateUp;
                        rotateProgress = 0.0f;
                }
                else
                {
                        float t = backProgress;
                        targetPosX = pathRightX - (pathRightX - pathLeftX) * t;
                        targetPosY = backY;
                        targetPosZ = backZ;
                }
                basePitch = -90.0f;
                break;
        }
        case TargetPhase::RotateUp:
        {
                float advance = dt / rotationDuration;
                rotateProgress += advance;
                if (rotateProgress >= 1.0f)
                {
                        rotateProgress = 1.0f;
                }
                float angleDeg = 180.0f - rotateProgress * 90.0f;
                float angleRad = angleDeg * static_cast<float>(M_PI) / 180.0f;
                targetPosX = pathLeftX;
                targetPosY = trackHeight + arcRadius * std::cos(angleRad);
                targetPosZ = (trackFrontZ - arcRadius) + arcRadius * std::sin(angleRad);
                basePitch = -(angleDeg - 90.0f);
                if (rotateProgress >= 1.0f)
                {
                        targetPhase = TargetPhase::MoveFront;
                        frontProgress = 0.0f;
                        basePitch = 0.0f;
                        targetPosY = trackHeight;
                        targetPosZ = trackFrontZ;
                        flipTargetAngle = 0.0f;
                }
                break;
        }
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
        const GLfloat woodAmbient[] = { 0.25f, 0.14f, 0.08f, 1.0f };
        const GLfloat woodDiffuse[] = { 0.55f, 0.3f, 0.12f, 1.0f };
        const GLfloat woodSpecular[] = { 0.2f, 0.2f, 0.2f, 1.0f };
        const GLfloat curtainAmbient[] = { 0.08f, 0.02f, 0.08f, 1.0f };
        const GLfloat curtainDiffuse[] = { 0.6f, 0.15f, 0.55f, 1.0f };
        const GLfloat curtainSpecular[] = { 0.3f, 0.2f, 0.3f, 1.0f };

        setMaterial(woodAmbient, woodDiffuse, woodSpecular, 24.0f);

        glPushMatrix();
        glTranslatef(0.0f, -0.75f, 0.0f);
        glScalef(boothWidth + 4.0f, 1.5f, boothDepth + 4.0f);
        glutSolidCube(1.0f);
        glPopMatrix();

        float postHeight = boothHeight;
        float postThickness = 0.8f;
        for (int i = -1; i <= 1; i += 2)
        {
                for (int j = -1; j <= 1; j += 2)
                {
                        glPushMatrix();
                        glTranslatef((boothWidth / 2.0f) * i, postHeight / 2.0f, (boothDepth / 2.0f) * j);
                        glScalef(postThickness, postHeight, postThickness);
                        glutSolidCube(1.0f);
                        glPopMatrix();
                }
        }

        glPushMatrix();
        glTranslatef(0.0f, boothHeight - 0.4f, boothDepth / 2.0f);
        glScalef(boothWidth, 1.2f, 0.6f);
        glutSolidCube(1.0f);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(0.0f, boothHeight - 0.4f, -boothDepth / 2.0f);
        glScalef(boothWidth, 1.2f, 0.6f);
        glutSolidCube(1.0f);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(0.0f, boothHeight, 0.0f);
        glScalef(boothWidth, 0.6f, boothDepth);
        glutSolidCube(1.0f);
        glPopMatrix();

        setMaterial(curtainAmbient, curtainDiffuse, curtainSpecular, 38.0f);
        glPushMatrix();
        glTranslatef(0.0f, boothHeight / 2.0f, -boothDepth / 2.0f + 0.3f);
        glScalef(boothWidth - 1.8f, boothHeight - 1.2f, 0.6f);
        glutSolidCube(1.0f);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(-boothWidth / 2.2f, boothHeight / 2.5f, boothDepth / 2.0f - 0.4f);
        glScalef(1.2f, boothHeight - 2.5f, 0.4f);
        glutSolidCube(1.0f);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(boothWidth / 2.2f, boothHeight / 2.5f, boothDepth / 2.0f - 0.4f);
        glScalef(1.2f, boothHeight - 2.5f, 0.4f);
        glutSolidCube(1.0f);
        glPopMatrix();
}

void drawWaterWave()
{
        const GLfloat waterAmbient[] = { 0.0f, 0.05f, 0.12f, 1.0f };
        const GLfloat waterDiffuse[] = { 0.2f, 0.4f, 0.8f, 1.0f };
        const GLfloat waterSpecular[] = { 0.3f, 0.3f, 0.5f, 1.0f };

        setMaterial(waterAmbient, waterDiffuse, waterSpecular, 48.0f);

        int segments = 48;
        float width = pathRightX - pathLeftX;
        float step = width / segments;
        float startX = -width / 2.0f;

        glPushMatrix();
        glTranslatef(0.0f, trackHeight - 0.6f, trackFrontZ + 0.45f);
        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= segments; ++i)
        {
                        float t = static_cast<float>(i) / segments;
                        float x = startX + step * i;
                        float crest = sineAmplitude * std::sin(wavePhase + t * sineFrequency);
                        float yTop = crest + 0.5f;
                        float yBottom = -0.9f;
                        glNormal3f(0.0f, 0.0f, 1.0f);
                        glVertex3f(x, yTop, 0.0f);
                        glVertex3f(x, yBottom, 0.0f);
        }
        glEnd();
        glPopMatrix();
}

void drawTarget()
{
        const GLfloat metalAmbient[] = { 0.12f, 0.12f, 0.14f, 1.0f };
        const GLfloat metalDiffuse[] = { 0.5f, 0.5f, 0.55f, 1.0f };
        const GLfloat metalSpecular[] = { 0.9f, 0.9f, 0.95f, 1.0f };

        setMaterial(metalAmbient, metalDiffuse, metalSpecular, 64.0f);
        glPushMatrix();
        glTranslatef(0.0f, trackHeight + 0.5f, trackFrontZ);
        glScalef((pathRightX - pathLeftX) + 4.0f, 0.3f, 0.6f);
        glutSolidCube(1.0f);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(targetPosX, targetPosY, targetPosZ);
        glRotatef(basePitch + flipAngle, 1.0f, 0.0f, 0.0f);
        drawDuck();
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
        glTranslatef(0.0f, -0.35f, 0.48f);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        setMaterial(whiteAmbient, whiteDiffuse, whiteSpecular, 30.0f);
        gluDisk(targetQuadric, 0.0f, 0.7f, 24, 1);
        setMaterial(redAmbient, redDiffuse, redSpecular, 30.0f);
        gluDisk(targetQuadric, 0.0f, 0.45f, 24, 1);
        setMaterial(whiteAmbient, whiteDiffuse, whiteSpecular, 30.0f);
        gluDisk(targetQuadric, 0.0f, 0.22f, 24, 1);
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
