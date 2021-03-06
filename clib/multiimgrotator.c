
#include <float.h>
#include <GL/glew.h>
#include <limits.h>
#include <math.h>

#include "multiimgrotator.h"
#include "vmath.h"

static int last_id = -1;

struct imageinfo {
    int id;
    size_t w, h;
    double size_x, size_y;
    double center_x, center_y, center_z;
    double offset_x, offset_y, offset_z;
    double rotation_x, rotation_y, rotation_z;
    struct imageinfo *next, *prev;

    int points_cached;
    double _p1x, _p1y, _p1z, _p2x, _p2y, _p2z;
    double _p3x, _p3y, _p3z, _p4x, _p4y, _p4z;

    int vbooutdated;
    int vboset;
    GLuint VAObufId;
    GLuint VBObufId;
    GLuint IBObufId;

    char *data;
    int textureset;
    GLuint texture; 
};
static struct imageinfo *images = NULL;

void multiimgrotator_SetImageData(struct imageinfo *iinfo,
        char *data) {
    if (iinfo->data == NULL) {
        iinfo->data = malloc(iinfo->w * iinfo->h * 1);
        if (!iinfo->data)
            return;
    }
    memcpy(iinfo->data, data, iinfo->w * iinfo->h * 1);
    if (iinfo->textureset) {
        glDeleteTextures(1, &iinfo->texture);
    }
    iinfo->textureset = 0; 
}

static void multiimgrotator_ComputeImageCornerPositions(
        struct imageinfo *iinfo,
        double *c1x, double *c1y, double *c1z,
        double *c2x, double *c2y, double *c2z,
        double *c3x, double *c3y, double *c3z,
        double *c4x, double *c4y, double *c4z) {
    // Compute point 1:
    double p1x = iinfo->size_y * 0.5;
    double p1y = 0.0;
    double p1z = iinfo->size_x * 0.5;
    vmath_rotatePos(p1x, p1y, p1z,
        iinfo->rotation_x, iinfo->rotation_y, iinfo->rotation_z,
        &p1x, &p1y, &p1z);
    p1x += iinfo->offset_x;
    p1y += iinfo->offset_y;
    p1z += iinfo->offset_z;

    // Compute point 2:
    double p2x = -iinfo->size_x * 0.5;
    double p2y = 0;
    double p2z = iinfo->size_y * 0.5;
    vmath_rotatePos(p2x, p2y, p2z,
        iinfo->rotation_x, iinfo->rotation_y, iinfo->rotation_z,
        &p2x, &p2y, &p2z);
    p2x += iinfo->offset_x;
    p2y += iinfo->offset_y;
    p2z += iinfo->offset_z;

    // Compute point 3:
    double p3x = -iinfo->size_x * 0.5;
    double p3y = 0;
    double p3z = -iinfo->size_y * 0.5;
    vmath_rotatePos(p1x, p1y, p1z,
        iinfo->rotation_x, iinfo->rotation_y, iinfo->rotation_z,
        &p3x, &p3y, &p3z);
    p3x += iinfo->offset_x;
    p3y += iinfo->offset_y;
    p3z += iinfo->offset_z;

    // Compute point 4:
    double p4x = iinfo->size_x * 0.5;
    double p4y = 0;
    double p4z = -iinfo->size_y * 0.5;
    vmath_rotatePos(p1x, p1y, p1z,
        iinfo->rotation_x, iinfo->rotation_y, iinfo->rotation_z,
        &p4x, &p4y, &p4z);
    p4x += iinfo->offset_x;
    p4y += iinfo->offset_y;
    p4z += iinfo->offset_z;

    // Return result:
    *c1x = p1x;
    *c1y = p1y;
    *c1z = p1z;
    *c2x = p2x;
    *c2y = p2y;
    *c2z = p2z;
    *c3x = p3x;
    *c3y = p3y;
    *c3z = p3z;
    *c4x = p4x;
    *c4y = p4y;
    *c4z = p4z;
}

static void multiimgrotator_ComputePointCache(
        struct imageinfo *iinfo) {
    if (iinfo->points_cached)
        return;

    iinfo->points_cached = 1;
    multiimgrotator_ComputeImageCornerPositions(
        iinfo,
        &iinfo->_p1x, &iinfo->_p1y, &iinfo->_p1z,
        &iinfo->_p2x, &iinfo->_p2y, &iinfo->_p2z,
        &iinfo->_p3x, &iinfo->_p3y, &iinfo->_p3z,
        &iinfo->_p4x, &iinfo->_p4y, &iinfo->_p4z);
}

static inline void boundaryupdate(double *x_min, double *x_max,
        double *y_min, double *y_max,
        double *z_min, double *z_max,
        double px, double py, double pz) {
    if (*x_min > px)
        *x_min = px;
    if (*x_max < px)
        *x_max = px;
    if (*y_min > py)
        *y_min = py;
    if (*y_max < py)
        *y_max = py;
    if (*z_min > pz)
        *z_min = pz;
    if (*z_max < pz)
        *z_max = pz;
}

/// Computes the boundaries of the world as axis-aligned
/// bounding box that contains all rotated images.
void multiimgrotator_WorldBoundaries(
        double *x_min_output, double *x_max_output,
        double *y_min_output, double *y_max_output,
        double* z_min_output, double *z_max_output) {
    double x_min = DBL_MAX;
    double x_max = DBL_MIN;
    double y_min = DBL_MAX;
    double y_max = DBL_MIN;
    double z_min = DBL_MAX;
    double z_max = DBL_MIN;

    // Loop through images and compute boundaries:
    int atleastoneimage = 0;
    struct imageinfo *iinfo = images;
    while (iinfo != NULL) {
        atleastoneimage = 1;

        multiimgrotator_ComputePointCache(iinfo);
        boundaryupdate(&x_min, &x_max, &y_min, &y_max,
            &z_min, &z_max,
            iinfo->_p1x, iinfo->_p1y, iinfo->_p1z);
        boundaryupdate(&x_min, &x_max, &y_min, &y_max,
            &z_min, &z_max,
            iinfo->_p2x, iinfo->_p2y, iinfo->_p2z);
        boundaryupdate(&x_min, &x_max, &y_min, &y_max,
            &z_min, &z_max,
            iinfo->_p3x, iinfo->_p3y, iinfo->_p3z);
        boundaryupdate(&x_min, &x_max, &y_min, &y_max,
            &z_min, &z_max,
            iinfo->_p4x, iinfo->_p4y, iinfo->_p4z);

        iinfo = iinfo->next;
    }

    // If we don't have an image, we didn't compute any proper min/max values.
    // Just give default dimensions then:
    if (!atleastoneimage) {
        *x_min_output = -0.5;
        *x_max_output = 0.5;
        *y_min_output = -0.5;
        *y_max_output = 0.5;
        *z_min_output = -0.5;
        *z_max_output = 0.5;
        return;
    }

    // Output the final values:
    *x_min_output = x_min;
    *x_min_output = x_min;
    *y_min_output = y_min;
    *y_min_output = y_min;
    *z_min_output = z_min;
    *z_min_output = z_min;
}

void multiimgrotator_FreeImage(struct imageinfo *iinfo) {
    if (iinfo->vboset) {
        // Remove old buffers:
        glDeleteBuffers(1, &iinfo->VBObufId);
        glDeleteBuffers(1, &iinfo->IBObufId);
        glDeleteVertexArrays(1, &iinfo->VAObufId);
    }
    free(iinfo);
}

void multiimgrotator_UpdateVBO(struct imageinfo *iinfo) {
    if (!iinfo->vbooutdated && iinfo->vboset)
        return;

    if (iinfo->vboset) {
        // Remove old buffers:
        glDeleteBuffers(1, &iinfo->VBObufId);
        glDeleteBuffers(1, &iinfo->IBObufId);
        glDeleteVertexArrays(1, &iinfo->VAObufId);
    }

    // Obtain world boundaries:
    double x_min, x_max;
    double y_min, y_max;
    double z_min, z_max;
    multiimgrotator_WorldBoundaries(
        &x_min, &x_max, &y_min, &y_max,
        &z_min, &z_max);
    double world_size_x = (x_max - x_min);
    double world_size_z = (z_max - z_min);

    // Get positions in world space:
    multiimgrotator_ComputePointCache(iinfo);

    // Vertex positions (for topdown 2D points) and UV:
    GLfloat vertexPositions[16];
    vertexPositions[0] = -0.5 + (iinfo->_p1z - z_min) * world_size_z;
    vertexPositions[1] = -0.5 + (iinfo->_p1x - x_min) * world_size_x;
    vertexPositions[2] = 0.0; // UV left
    vertexPositions[3] = 1.0; // UV bottom
    vertexPositions[4] = -0.5 + (iinfo->_p2z - z_min) * world_size_z;
    vertexPositions[5] = -0.5 + (iinfo->_p2x - x_min) * world_size_x;
    vertexPositions[6] = 1.0; // UV right
    vertexPositions[7] = 1.0; // UV bottom
    vertexPositions[8] = -0.5 + (iinfo->_p3z - z_min) * world_size_z;
    vertexPositions[9] = -0.5 + (iinfo->_p3x - x_min) * world_size_x;
    vertexPositions[10] = 1.0; // UV right
    vertexPositions[11] = 0.0; // UV top
    vertexPositions[12] = -0.5 + (iinfo->_p4z - z_min) * world_size_z;
    vertexPositions[13] = -0.5 + (iinfo->_p4x - x_min) * world_size_x;
    vertexPositions[14] = 0.0; // UV left
    vertexPositions[15] = 0.0; // UV left

    // Index numbers for polygons:
    GLuint indices[] = { 0, 1, 2, 3 };

    iinfo->vbooutdated = 0;
    iinfo->vboset = 1;
    glGenVertexArrays(1, &iinfo->VAObufId);
    glBindVertexArray(iinfo->VAObufId);
    glGenBuffers(1, &iinfo->VBObufId);
    glBindBuffer(GL_ARRAY_BUFFER, iinfo->VBObufId);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertexPositions), vertexPositions,
        GL_STATIC_DRAW);
    glGenBuffers(1, &iinfo->IBObufId);
    glBindBuffer(GL_ARRAY_BUFFER, iinfo->IBObufId);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
        GL_STATIC_DRAW);
}

int multiimgrotator_AddImage(size_t w, size_t h) {
    // Get new id:
    int id = last_id + 1;
    last_id++;

    // Add image to list:
    struct imageinfo *iinfo = malloc(sizeof(*iinfo));
    memset(iinfo, 0, sizeof(*iinfo));
    iinfo->size_x = 1.0;
    iinfo->size_y = 1.0;
    iinfo->w = w;
    iinfo->h = h;
    iinfo->id = id;
    images->prev = iinfo;
    images->next = NULL;
    iinfo->next = images;
    images = iinfo;
    return iinfo->id;
}

void multiimgrotator_ScaleImage(int id, double size_x, double size_y) {
    struct imageinfo *iinfo = images;
    while (iinfo != NULL) {
        if (iinfo->id == id) {
            if (size_x < 0.00001) {
                size_x = 0.00001;
            }
            if (size_y < 0.00001) {
                size_y = 0.00001;
            }
            double old_scale_x = iinfo->size_x;
            double old_scale_y = iinfo->size_y;
            if (fabs(old_scale_x - iinfo->size_x) > 0.001 ||
                    fabs(old_scale_y - iinfo->size_y)) {
                iinfo->size_x = size_x;
                iinfo->size_y = size_y;
                iinfo->points_cached = 0;
                iinfo->vbooutdated = 1;
            }
            return;
        }
        iinfo = iinfo->next;
    }
}

void multiimgrotator_TranslateImage(int id,
        double center_x, double center_y, double center_z,
        double offset_x, double offset_y, double offset_z,
        double rotation_euler_x,
        double rotation_euler_y,
        double rotation_euler_z) {
    struct imageinfo *iinfo = images;
    while (iinfo != NULL) {
        if (iinfo->id == id) {
            iinfo->center_x = center_x;
            iinfo->center_y = center_y;
            iinfo->center_z = center_z;
            iinfo->offset_x = offset_x;
            iinfo->offset_y = offset_y;
            iinfo->offset_z = offset_z;
            iinfo->rotation_x = rotation_euler_x;
            iinfo->rotation_y = rotation_euler_y;
            iinfo->rotation_z = rotation_euler_z;
            iinfo->vbooutdated = 1;
            iinfo->points_cached = 0;
            return;
        }
        iinfo = iinfo->next;
    } 
}

void multiimgrotator_RemoveImage(int id) {
    struct imageinfo *iinfo = images;
    while (iinfo != NULL) { 
        if (iinfo->id == id) {
            if (iinfo->next != NULL) {
                iinfo->next->prev = iinfo->prev;
            }
            if (iinfo->prev != NULL) {
                iinfo->prev->next = iinfo->next;
            } else {
                images = iinfo->next;
            }
            multiimgrotator_FreeImage(iinfo);
            return;
        }
        iinfo = iinfo->next;
    }
}

static void multiimgrotator_PrintShaderError(GLint shaderId) {
    int max_log_length;
    glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &max_log_length);
    if (max_log_length > 4096)
        max_log_length = 4096;
    int log_length = 0;
    char *info_log_buf = malloc(max_log_length);
    if (!info_log_buf) {
        fprintf(stderr, "clib/multiimgrotator.c: error: "
            "shader log buffer allocation failed");
        fflush(stderr);
        return;
    }
    glGetShaderInfoLog(shaderId, max_log_length,
        &log_length, info_log_buf);
    fprintf(stderr, "clib/multiimgrotator.c: debug: shader log: "
        "%s\n", info_log_buf);
    fflush(stderr);
    free(info_log_buf);
}

static GLint vertexPos2DAttrLocation = -1;
static GLint UVattribAttrLocation = -1;
static GLuint drawShadersProgramId = 0;
static GLuint uniformShaderTexParam = -1;
static int draw_initialized = 0;
void multiimgrotator_InitDraw() {
    if (draw_initialized)
        return;

    draw_initialized = 1;

    drawShadersProgramId = glCreateProgram();

    GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    const char *vertexShaderProgram[] = { "version 140\n"
        "in vec2 LVertexPos2D;\n"
        "void main() {\n"
        "    gl_Position = vec4(LVertexPos2D.x, LVertexPos2D.y, 0, 1);\n"
        "}\n" };
    glShaderSource(vertexShaderId, 1, vertexShaderProgram, NULL);
    glCompileShader(vertexShaderId);
    GLint shaderCompiled;
    glGetShaderiv(vertexShaderId, GL_COMPILE_STATUS, &shaderCompiled);
    if (shaderCompiled != GL_TRUE) {
        fprintf(stderr, "clib/multiimgrotator.c: fatal error: "
            "vertex shader compilation failed.\n");
        fflush(stderr);
        multiimgrotator_PrintShaderError(vertexShaderId);
        exit(1);
    }
    glAttachShader(drawShadersProgramId, vertexShaderId);

    GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
    const char *fragmentShaderProgram[] = { "#version 330\n"
        "uniform sampler2D texUnit;\n"
        "in vec2 theCoords;\n"
        "out vec4 outputColour;\n"
        "void main() {\n"
        "   outputColour = texture(texUnit, theCoords);\n"
        "}\n" };
    glShaderSource(fragmentShaderId, 1, fragmentShaderProgram, NULL);
    glCompileShader(fragmentShaderId);
    glGetShaderiv(fragmentShaderId, GL_COMPILE_STATUS, &shaderCompiled);
    if (shaderCompiled != GL_TRUE) {
        fprintf(stderr, "clib/multiimgrotator.c: fatal error: "
            "fragment shader compilation failed.\n");
        fflush(stderr);
        multiimgrotator_PrintShaderError(fragmentShaderId);
        exit(1);
    }
    glAttachShader(drawShadersProgramId, fragmentShaderId);
 
    glLinkProgram(drawShadersProgramId);
    GLint shaderLinked;
    glGetProgramiv(drawShadersProgramId, GL_LINK_STATUS, &shaderLinked);
    if (shaderLinked != GL_TRUE) {
        fprintf(stderr, "clib/multiimgrotator.c: fatal error: "
            "linking shader code failed.");
        exit(1);
    }

    vertexPos2DAttrLocation = glGetAttribLocation(
        drawShadersProgramId, "LVertexPos2D");
    if (vertexPos2DAttrLocation == -1) {
        fprintf(stderr, "clib/multiimgrotator.c: fatal error: "
            "failed to obtain LVertexPos2D attribute");
        exit(1);
    }
    UVattribAttrLocation = glGetAttribLocation(
        drawShadersProgramId, "texCoords"); 
    if (UVattribAttrLocation == -1) {
        fprintf(stderr, "clib/multiimgrotator.c: fatal error: "
            "failed to obtain texCoords attribute");
        exit(1);
    }

    uniformShaderTexParam = glGetUniformLocation(
        drawShadersProgramId, "texUnit");
}

void multiimgrotator_Draw() {
    // Make sure everything is initialized:
    multiimgrotator_InitDraw();

    // Prepare window:
    glClear(GL_COLOR_BUFFER_BIT);

    // Render images with all transformations applied:
    struct imageinfo *iinfo = images;
    while (iinfo != NULL) {
        if (!iinfo->data)
            continue;

        // Make sure VBO is up to date:
        multiimgrotator_UpdateVBO(iinfo);

        // Upload texture if not present:
        if (!iinfo->textureset) {
            iinfo->textureset = 1;
            glGenTextures(1, &iinfo->texture);
            glBindTexture(GL_TEXTURE_2D, iinfo->texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
                iinfo->w, iinfo->h, 0, GL_R8, GL_UNSIGNED_BYTE,
                iinfo->data);
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        // Set texture active and draw:
        glUseProgram(drawShadersProgramId);
        glBindVertexArray(iinfo->VAObufId);
        glEnableVertexArrayAttrib(iinfo->VAObufId, vertexPos2DAttrLocation);
        glEnableVertexArrayAttrib(iinfo->VAObufId, UVattribAttrLocation);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, iinfo->texture);
        glUniform1i(uniformShaderTexParam, 0);
        glDrawArrays(GL_TRIANGLES, 0, 4);
        glDisableVertexAttribArray(vertexPos2DAttrLocation);
        glUseProgram(0);

        iinfo = iinfo->next;
    }
}


