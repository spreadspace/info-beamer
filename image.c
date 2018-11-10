/* See Copyright Notice in LICENSE.txt */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <GL/glew.h>
#include <GL/gl.h>
#include <IL/il.h>
#include <IL/ilu.h>
#include <lauxlib.h>
#include <lualib.h>

#include "framebuffer.h"
#include "misc.h"
#include "image.h"
#include "shader.h"

typedef struct {
    GLuint tex;
    GLuint fbo;
    int width;
    int height;
    int flipped;
} image_t;

LUA_TYPE_DECL(image)

/* Instance methods */

static int image_state(lua_State *L) {
    image_t *image = checked_image(L, 1);
    lua_pushliteral(L, "loaded");
    lua_pushnumber(L, image->width);
    lua_pushnumber(L, image->height);
    return 3;
}

static int image_size(lua_State *L) {
    image_t *image = checked_image(L, 1);
    lua_pushnumber(L, image->width);
    lua_pushnumber(L, image->height);
    return 2;
}

static int image_draw(lua_State *L) {
    image_t *image = checked_image(L, 1);
    GLfloat x1 = luaL_checknumber(L, 2);
    GLfloat y1 = luaL_checknumber(L, 3);
    GLfloat x2 = luaL_checknumber(L, 4);
    GLfloat y2 = luaL_checknumber(L, 5);
    GLfloat alpha = luaL_optnumber(L, 6, 1.0);
    GLfloat sx1 = luaL_optnumber(L, 7, 0);
    GLfloat sy1 = luaL_optnumber(L, 8, 0);
    GLfloat sx2 = luaL_optnumber(L, 9, 1);
    GLfloat sy2 = luaL_optnumber(L, 10, 1);

    glBindTexture(GL_TEXTURE_2D, image->tex);
    shader_set_gl_color(1.0, 1.0, 1.0, alpha);

    glBegin(GL_QUADS); 
    if (image->flipped) {
        glTexCoord2f(sx1, sy2); glVertex3f(x1, y1, 0);
        glTexCoord2f(sx2, sy2); glVertex3f(x2, y1, 0);
        glTexCoord2f(sx2, sy1); glVertex3f(x2, y2, 0);
        glTexCoord2f(sx1, sy1); glVertex3f(x1, y2, 0);
    } else {
        glTexCoord2f(sx1, sy1); glVertex3f(x1, y1, 0);
        glTexCoord2f(sx2, sy1); glVertex3f(x2, y1, 0);
        glTexCoord2f(sx2, sy2); glVertex3f(x2, y2, 0);
        glTexCoord2f(sx1, sy2); glVertex3f(x1, y2, 0);
    }
    glEnd();

    return 0;
}

typedef struct {
    float x, y;
} vertex2_t;

typedef struct {
    float x, y, z, w;
} vertex4_t;

static vertex2_t getvert2(lua_State *L, int idx, int i)
{
    vertex2_t ret;
    i *= 2;
    ++i;
    lua_rawgeti(L, idx, i  );
    lua_rawgeti(L, idx, i+1);
    ret.x = lua_tonumber(L, -2);
    ret.y = lua_tonumber(L, -1);
    lua_pop(L, 2);
    return ret;
}

static vertex4_t getvert4(lua_State *L, int idx, int i)
{
    vertex4_t ret;
    i *= 4;
    ++i;
    lua_rawgeti(L, idx, i  );
    lua_rawgeti(L, idx, i+1);
    lua_rawgeti(L, idx, i+2);
    lua_rawgeti(L, idx, i+3);
    ret.x = lua_tonumber(L, -4);
    ret.y = lua_tonumber(L, -3);
    ret.z = lua_tonumber(L, -2);
    ret.w = lua_tonumber(L, -1);
    lua_pop(L, 4);
    return ret;
}

static int getdrawtype(lua_State *L, int idx)
{
    switch(*luaL_checkstring(L, idx)) {
        case 'q': return GL_QUADS;
        case 't': return GL_TRIANGLES;
        case 'T': return GL_TRIANGLE_STRIP;
        case 'p': return GL_POINTS;
        case 'l': return GL_LINES;
        case 'L': return GL_LINE_STRIP;
        case 'f': return GL_TRIANGLE_FAN;
        case 'Q': return GL_QUAD_STRIP;
        case 'P': return GL_POLYGON;
    }
    luaL_error(L, "one of qtTplLfQp expected");
    return 0; /* not reached */
}

/* image:drawverts(mode, vertlist, texcoordlist, alpha = 1.0) */
static int image_drawverts(lua_State *L) {
    image_t *image = checked_image(L, 1);
    int mode = getdrawtype(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    luaL_checktype(L, 4, LUA_TTABLE);
    GLfloat alpha = luaL_optnumber(L, 5, 1.0);
    int n = lua_objlen(L, 3) / 4;

    glBindTexture(GL_TEXTURE_2D, image->tex);
    shader_set_gl_color(1.0, 1.0, 1.0, alpha);

    glBegin(mode);
    for(int i = 0; i < n; ++i) {
        vertex4_t vert = getvert4(L, 3, i);
        vertex2_t uv = getvert2(L, 4, i);
        glTexCoord2f(uv.x, uv.y);
        glVertex4f(vert.x, vert.y, vert.z, vert.w);
    }
    glEnd();

    return 0;
}

static int image_texid(lua_State *L) {
    image_t *image = checked_image(L, 1);
    lua_pushnumber(L, image->tex);
    return 1;
}

static int image_dispose(lua_State *L) {
    return 0;
}

static const luaL_reg image_methods[] = {
    {"state",   image_state},
    {"draw",    image_draw},
    {"drawverts",image_drawverts},
    {"size",    image_size},
    {"texid",   image_texid},
    {"dispose", image_dispose},
    {0,0}
};

/* Lifecycle */

int image_create(lua_State *L, GLuint tex, GLuint fbo, int width, int height, int flipped) {
    image_t *image = push_image(L);
    image->tex = tex;
    image->fbo = fbo;
    image->width = width;
    image->height = height;
    image->flipped = flipped;
    return 1;
}

int image_from_current_framebuffer(lua_State *L, int x, int y, int width, int height, int mipmap) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    GLint filter = mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, y, width, height);
    if (mipmap)
        glGenerateMipmap(GL_TEXTURE_2D);
    return image_create(L, tex, 0, width, height, 1);
}

int image_from_color(lua_State *L, GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    unsigned char buf[4] = {r * 255, g * 255, b * 255, a * 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf);

    return image_create(L, tex, 0, 1, 1, 0);
}


int image_load(lua_State *L, const char *path, const char *name) {
    ILuint imageID;
    ilGenImages(1, &imageID);
    ilBindImage(imageID);
 
    if (!ilLoadImage(path)) {
        ilDeleteImages(1, &imageID);
        return luaL_error(L, "loading %s failed: %s",
            path, iluErrorString(ilGetError()));
    }
 
    ILinfo ImageInfo;
    iluGetImageInfo(&ImageInfo);

    if (!ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE)) {
        ilDeleteImages(1, &imageID);
        return luaL_error(L, "converting %s failed: %s",
            path, iluErrorString(ilGetError()));
    }

    int width = ilGetInteger(IL_IMAGE_WIDTH);
    int height = ilGetInteger(IL_IMAGE_HEIGHT);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, ilGetInteger(IL_IMAGE_BPP), width, height, 0,
                 ilGetInteger(IL_IMAGE_FORMAT), GL_UNSIGNED_BYTE, ilGetData());
    glGenerateMipmap(GL_TEXTURE_2D);
    ilDeleteImages(1, &imageID);
    return image_create(L, tex, 0, width, height, 0);
}

static int image_gc(lua_State *L) {
    image_t *image = to_image(L, 1);
    if (image->fbo) {
        // If images has attached Framebuffer, put the
        // texture and framebuffer into the recycler.
        // Allocations for new framebuffers can then
        // reuse these => Better performance.
        recycle_framebuffer(image->width, image->height, 
            image->tex, image->fbo);
    } else {
        // No Framebuffer? Just remove the texture.
        glDeleteTextures(1, &image->tex);
    }
    return 0;
}

LUA_TYPE_IMPL(image)
