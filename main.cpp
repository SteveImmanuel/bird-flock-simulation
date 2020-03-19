/* Particles in a box */

#define MAX_NUM_PARTICLES 1000
#define INITIAL_NUM_PARTICLES 100
#define INITIAL_POINT_SIZE 5.0
#define INITIAL_SPEED 0.01
#define NUM_COLORS 8
#define PERCEPTION 0.2
#define FALSE 0
#define TRUE 1

#include <GL/glew.h>

#include "include/Angel.h"

typedef Angel::vec4 point4;
typedef Angel::vec4 color4;

point4 vertices[8] = {point4(-1.1, -1.1, 1.1, 1.0),  point4(-1.1, 1.1, 1.1, 1.0),
                      point4(1.1, 1.1, 1.1, 1.0),    point4(1.1, -1.1, 1.1, 1.0),
                      point4(-1.1, -1.1, -1.1, 1.0), point4(-1.1, 1.1, -1.1, 1.0),
                      point4(1.1, 1.1, -1.1, 1.0),   point4(1.1, -1.1, -1.1, 1.0)};

// RGBA colors
color4 vertex_colors[8] = {
    color4(0.0, 0.0, 0.0, 1.0),  // black
    color4(1.0, 0.0, 0.0, 1.0),  // red
    color4(1.0, 1.0, 0.0, 1.0),  // yellow
    color4(0.0, 1.0, 0.0, 1.0),  // green
    color4(0.0, 0.0, 1.0, 1.0),  // blue
    color4(1.0, 0.0, 1.0, 1.0),  // magenta
    color4(0.0, 1.0, 1.0, 1.0),  // cyan
    color4(1.0, 1.0, 1.0, 1.0)   // white
};

point4 points[MAX_NUM_PARTICLES + 24];
color4 point_colors[MAX_NUM_PARTICLES + 24];

GLuint program;
mat4 model_view;
mat4 projection;

GLuint buffer;
GLuint loc, loc2;
GLint model_view_loc, projection_loc;

vec4 clamp(vec4 velocity);

/* particle struct */

typedef struct particle {
    int color;
    point4 position;
    vec4 velocity;
    vec4 acceleration;
    float mass;
} particle;

particle particles[MAX_NUM_PARTICLES]; /* particle system */

/* initial state of particle system */

int present_time;
int last_time;
int num_particles = INITIAL_NUM_PARTICLES;
float point_size = INITIAL_POINT_SIZE;
float speed = INITIAL_SPEED;
bool gravity = FALSE;                           /* gravity off */
bool elastic = FALSE;                           /* restitution off */
float coef = 1.0;                               /* perfectly elastic collisions */
float d2[MAX_NUM_PARTICLES][MAX_NUM_PARTICLES]; /* array for interparticle distances */

//----------------------------------------------------------------------------

static int Index = 0;

void quad(int a, int b, int c, int d) {
    point_colors[Index] = vertex_colors[0];
    points[Index] = vertices[a];
    Index++;
    point_colors[Index] = vertex_colors[0];
    points[Index] = vertices[b];
    Index++;
    point_colors[Index] = vertex_colors[0];
    points[Index] = vertices[c];
    Index++;
    point_colors[Index] = vertex_colors[0];
    points[Index] = vertices[d];
    Index++;
}

//----------------------------------------------------------------------------

void colorcube() {
    quad(1, 0, 3, 2);
    quad(2, 3, 7, 6);
    quad(3, 0, 4, 7);
    quad(6, 5, 1, 2);
    quad(4, 5, 6, 7);
    quad(5, 4, 0, 1);
}

//----------------------------------------------------------------------------
void initParticle() {
    colorcube();
    /* set up particles with random locations and velocities */
    for (int i = 0; i < num_particles; i++) {
        particles[i].mass = 1.0;
        particles[i].color = i % NUM_COLORS;
        for (int j = 0; j < 3; j++) {
            particles[i].position[j] = 2.0 * ((float)rand() / RAND_MAX) - 1.0;
            particles[i].velocity[j] = speed * 2.0 * ((float)rand() / RAND_MAX) - 1.0;
            particles[i].acceleration[j] = 0;
        }
        particles[i].velocity = clamp(particles[i].velocity);
        particles[i].position[3] = 1.0;
    }
    glPointSize(point_size);
}

void init(void) {
    // Create a vertex array object
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Create and initialize a buffer object
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(points) + sizeof(point_colors), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(points), points);
    glBufferSubData(GL_ARRAY_BUFFER, sizeof(points), sizeof(point_colors), point_colors);

    // Load shaders and use the resulting shader program
    GLuint program = InitShader("shader/vshader91.glsl", "shader/fshader91.glsl");
    glUseProgram(program);

    // set up vertex arrays
    GLuint vPosition = glGetAttribLocation(program, "vPosition");
    glEnableVertexAttribArray(vPosition);
    glVertexAttribPointer(vPosition, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));

    GLuint vColor = glGetAttribLocation(program, "vColor");
    glEnableVertexAttribArray(vColor);
    glVertexAttribPointer(vColor, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(sizeof(points)));

    model_view_loc = glGetUniformLocation(program, "ModelView");
    projection_loc = glGetUniformLocation(program, "Projection");

    glClearColor(0.5, 0.5, 0.5, 1.0);
}

//----------------------------------------------------------------------------

vec4 forces(int i) {
    vec4 force;
    vec4 avg_speed;
    int total_neighbor = 0;

    force.z = 1.0;
    // gravity
    if (gravity) force.y = -1.0;

    for (int k = 0; k < num_particles; k++) {
        if (k != i) {
            // repulsive
            force += 0.001 * (particles[i].position - particles[k].position) / (0.001 + d2[i][k]);
            if (length(particles[i].position - particles[k].position) <= PERCEPTION) {
                avg_speed += particles[k].velocity;
                total_neighbor += 1;
            }
        }
    }

    return (force);
}

//----------------------------------------------------------------------------

void collision(int n)

/* tests for collisions against cube and reflect particles if necessary */
{
    int i;
    for (i = 0; i < 3; i++) {
        if (particles[n].position[i] >= 1.0) {
            particles[n].velocity[i] = -coef * particles[n].velocity[i];
            particles[n].position[i] = 1.0 - coef * (particles[n].position[i] - 1.0);
        }
        if (particles[n].position[i] <= -1.0) {
            particles[n].velocity[i] = -coef * particles[n].velocity[i];
            particles[n].position[i] = -1.0 - coef * (particles[n].position[i] + 1.0);
        }
    }
}

vec4 clamp(vec4 velocity) {
    if (length(velocity) > speed) {
        return normalize(velocity) * speed;
    }
    return velocity;
}

vec4 getDirection(int i) {
    vec4 steering = 0;
    vec4 avg_speed;
    vec4 center_mass;
    int total_neighbor = 0;

    for (int k = 0; k < num_particles; k++) {
        if (length(particles[i].position - particles[k].position) <= PERCEPTION) {
            //apply separation
            steering += 0.01 * (particles[i].position - particles[k].position) / (0.01 + d2[i][k]);
            
            avg_speed += particles[k].velocity;
            center_mass += particles[k].position;
            total_neighbor += 1;
        }
    }

    if (total_neighbor > 1) {
        // apply alignment
        avg_speed /= total_neighbor;
        avg_speed = normalize(avg_speed);
        steering += avg_speed - particles[i].velocity;

        // apply cohesion
        center_mass /= total_neighbor;
        vec4 vector_to_center_mass = center_mass - particles[i].position;
        vector_to_center_mass = normalize(vector_to_center_mass) * 0.1;
        steering += vector_to_center_mass - particles[i].velocity;

        steering = normalize(steering);
    }

    return steering;
}

//----------------------------------------------------------------------------
void updatePosition() {
    for (int i = 0; i < num_particles; i++) {
        particles[i].acceleration += getDirection(i);
        particles[i].position += particles[i].velocity;
        particles[i].velocity += particles[i].acceleration;
        particles[i].velocity = clamp(particles[i].velocity);

        particles[i].acceleration = 0;
        collision(i);
    }
}

void updateDistance() {
    for (int i = 0; i < num_particles; i++) {
        for (int k = 0; k < i; k++) {
            d2[i][k] = 0.0;
            for (int j = 0; j < 3; j++) {
                d2[i][k] += (particles[i].position[j] - particles[k].position[j]) *
                            (particles[i].position[j] - particles[k].position[j]);
            }
            d2[k][i] = d2[i][k];
        }
    }
}

void idle(void) {
    present_time = glutGet(GLUT_ELAPSED_TIME);
    updateDistance();
    updatePosition();
    glutPostRedisplay();
}

//----------------------------------------------------------------------------

void menu(int option) {
    switch (option) {
        case 1:
            num_particles *= 2;
            break;

        case 2:
            num_particles /= 2;
            break;

        case 3:
            speed *= 2.0;
            break;

        case 4:
            speed /= 2.0;
            break;

        case 5:
            point_size *= 2.0;
            break;

        case 6:
            point_size /= 2.0;
            if (point_size < 1.0) point_size = 1.0;
            break;

        case 7:
            gravity = !gravity;
            break;

        case 8:
            elastic = !elastic;
            coef = elastic ? 0.9 : 1.0;
            break;

        case 9:
            exit(EXIT_SUCCESS);
            break;
    }

    initParticle();
    glutPostRedisplay();
}

//----------------------------------------------------------------------------

void display(void) {
    glClear(GL_COLOR_BUFFER_BIT);

    for (int i = 0; i < num_particles; i++) {
        point_colors[i + 24] = vertex_colors[particles[i].color];
        points[i + 24] = particles[i].position;
    }

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(points), points);
    glBufferSubData(GL_ARRAY_BUFFER, sizeof(points), sizeof(point_colors), point_colors);
    glDrawArrays(GL_POINTS, 24, num_particles);
    for (int i = 0; i < 6; i++) glDrawArrays(GL_LINE_LOOP, i * 4, 4);
    glutSwapBuffers();
}

//----------------------------------------------------------------------------

void reshape(int width, int height) {
    glViewport(0, 0, width, height);

    point4 eye = vec4(1.5, 1.0, 1.0, 1.0);
    point4 at = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 up = vec4(0.0, 1.0, 0.0, 1.0);

    mat4 projection = Ortho(-2.0, 2.0, -2.0, 2.0, -4.0, 4.0);
    mat4 model_view = LookAt(eye, at, up);

    glUniformMatrix4fv(model_view_loc, 1, GL_TRUE, model_view);
    glUniformMatrix4fv(projection_loc, 1, GL_TRUE, projection);
}

//----------------------------------------------------------------------------

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 800);
    glutCreateWindow("Bird Flock Simulation by Steve Immanuel");
    glutCreateMenu(menu);
    glutAddMenuEntry("more particles", 1);
    glutAddMenuEntry("fewer particles", 2);
    glutAddMenuEntry("faster", 3);
    glutAddMenuEntry("slower", 4);
    glutAddMenuEntry("larger particles", 5);
    glutAddMenuEntry("smaller particles", 6);
    glutAddMenuEntry("toggle gravity", 7);
    glutAddMenuEntry("toggle restitution", 8);
    glutAddMenuEntry("quit", 9);
    glutAttachMenu(GLUT_RIGHT_BUTTON);
    glewInit();
    initParticle();
    init();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(idle);
    glutMainLoop();
    return 0;
}
