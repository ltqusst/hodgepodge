#include <cstdio>
#include <string.h>
#include <vector>
#include <set>
#include <algorithm>

//=====================================================================
struct cube_edge_link{
    int face0;
    int edge0;
    
    int face1;
    int edge1;
};

struct cube
{
    static int peer_off[6][4*3];
    void init_peer_off(){
        static const cube_edge_link el[]={
            {0,0,  5,2},
            {0,1,  3,0},
            {0,2,  2,0},
            {0,3,  1,0},
            
            {4,0,  2,2},
            {4,1,  3,2},
            {4,2,  5,0},
            {4,3,  1,2},
            
            {1,1,  2,3},
            {2,1,  3,3},
            {3,1,  5,1},
            {1,3,  5,3},
        };
        #define cntof(x) sizeof(x)/sizeof(x[0])
        for(int i=0;i<cntof(el);i++){
            const cube_edge_link & e = el[i];
            for(int k=0;k<3;k++) peer_off[e.face0][e.edge0*3+k] = &(get(e.face1, e.edge1, 3-k)) - s;
            for(int k=0;k<3;k++) peer_off[e.face1][e.edge1*3+k] = &(get(e.face0, e.edge0, 3-k)) - s;
        }
    }
    // 0 1 2
    // 7   3
    // 6 5 4
    char s[6*8];
    char& get(int face) { return s[face*8]; }
    char& get(int face, int edge, int id){ return s[face*8 + ((edge*2 + id)%8)]; }

    void reset()
    {
        for(int face=0;face<6;face++)
        {
            char * p = &get(face);
            for(int k=0;k<8;k++) p[k] = face;//the color id
        }
    }
    
    void rotate_face(int f)
    {
        char temp[12*2];
        memcpy(temp,     &get(f), 8);
        memcpy(temp + 8, &get(f), 2);
        memcpy(&get(f),  temp + 2,8);
        
        int k;
        for(k=0;k<12;k++) temp[k] = s[peer_off[f][k]];
        for(;k<12+3;k++) temp[k] = s[peer_off[f][k-12]];
        for(k=0;k<12;k++) s[peer_off[f][k]] = temp[k+3];
    }
};

bool operator==(const cube & a, const cube & b)
{
	return memcmp(a.s, b.s, sizeof(a.s)) == 0;
}
bool operator<(const cube & a, const cube & b)
{
	return memcmp(a.s, b.s, sizeof(a.s)) < 0;
}
int cube::peer_off[6][4*3]={0};

int main()
{
	cube root;
	root.reset();
	root.init_peer_off();

	std::set<cube> node_fixed;
	std::vector<cube> node_act[2];
	int actid = 0;
	int depth = 0;
	node_act[actid].push_back(root);

	while(node_act[actid].size() > 0)
	{
		auto & na_cur = node_act[actid];
		auto & na_next = node_act[1-actid];

		na_next.clear();

		printf("depth:%2d acting:%8d fixed:%8d\n",
				depth, na_cur.size(), node_fixed.size());

		for(int k=0;k<na_cur.size();k++)
		{
			cube &c = na_cur[k];

			//derived it
			node_fixed.insert(c);

			for(int i=0;i<6;i++)
			{
				cube n = c;
				n.rotate_face(i);

				if(node_fixed.find(n) == node_fixed.end()){
					node_fixed.insert(n);
					na_next.push_back(n);
				}
			}

			if((k & 0xF) == 0)
				printf("%.2f%%    \r", (float)k*100/node_act[actid].size());
		}
		actid = 1 - actid;
		depth ++;

	}
}

#if 0
//=====================================================================
#include <GL/glut.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <cmath>
#include <assert.h>

#ifndef IMAGE_LOADER_H_INCLUDED
#define IMAGE_LOADER_H_INCLUDED

//Represents an image

class Image {
    public:
        Image(char* ps, int w, int h);
        ~Image();

        /* An array of the form (R1, G1, B1, R2, G2, B2, ...) indicating the
         * color of each pixel in image.  Color components range from 0 to 255.
         * The array starts the bottom-left pixel, then moves right to the end
         * of the row, then moves up to the next column, and so on.  This is the
         * format in which OpenGL likes images.
         */
        char* pixels;
        int width;
        int height;
};

//Reads a bitmap image from file.
Image* loadBMP(const char* filename);
#endif

using namespace std;

Image::Image(char* ps, int w, int h) : pixels(ps), width(w), height(h) {}
Image::~Image() {delete[] pixels;}

namespace {
    //Converts a four-character array to an integer, using little-endian form
    int toInt(const char* bytes) {
        return (int)(((unsigned char)bytes[3] << 24) |
                     ((unsigned char)bytes[2] << 16) |
                     ((unsigned char)bytes[1] << 8) |
                     (unsigned char)bytes[0]);
    }

    //Converts a two-character array to a short, using little-endian form
    short toShort(const char* bytes) {
        return (short)(((unsigned char)bytes[1] << 8) |
                       (unsigned char)bytes[0]);
    }

    //Reads the next four bytes as an integer, using little-endian form
    int readInt(ifstream &input) {
        char buffer[4];
        input.read(buffer, 4);
        return toInt(buffer);
    }

    //Reads the next two bytes as a short, using little-endian form
    short readShort(ifstream &input) {
        char buffer[2];
        input.read(buffer, 2);
        return toShort(buffer);
    }

    //Just like auto_ptr, but for arrays
    template<class T>
    class auto_array {
        private:
            T* array;
            mutable bool isReleased;
        public:
            explicit auto_array(T* array_ = NULL) :
                array(array_), isReleased(false) {
            }
            auto_array(const auto_array<T> &aarray) {
                array = aarray.array;
                isReleased = aarray.isReleased;
                aarray.isReleased = true;
            }
            ~auto_array() {
               if (!isReleased && array != NULL) {delete[] array;}
            }

            void operator=(const auto_array<T> &aarray) {
                if (!isReleased && array != NULL) {
                    delete[] array;
                }
                array = aarray.array;
                isReleased = aarray.isReleased;
                aarray.isReleased = true;
            }

            T* release() {isReleased = true; return array;}
            
            void reset(T* array_ = NULL) {
                if (!isReleased && array != NULL) delete[] array;
                array = array_;
            }
            
            T* get() const {return array;}
            T* operator->() const {return array;}
            T &operator*() const {return *array;}
            T* operator+(int i) { return array + i;}
            T &operator[](int i) { return array[i];}
    };
}

 

Image* loadBMP(const char* filename) {
    ifstream input;
    char filepath[256];
    sprintf(filepath, "../hodgepodge/data/%s",filename);
    input.open(filepath, ifstream::binary);
    assert(!input.fail() || !"Could not find file");
    char buffer[2];
    input.read(buffer, 2);
    assert(buffer[0] == 'B' && buffer[1] == 'M' || (printf("Not a bitmap file: %s  %c,%c", filepath, buffer[0],buffer[1]) , 0) );
    input.ignore(8);
    int dataOffset = readInt(input);
   
    //Read the header
    int headerSize = readInt(input);
    int width;
    int height;
    switch(headerSize) {
        case 40:
            //V3
            width = readInt(input);
            height = readInt(input);
            input.ignore(2);
            assert(readShort(input) == 24 || !"Image is not 24 bits per pixel");
            assert(readShort(input) == 0 || !"Image is compressed");
            break;
        case 12:
            //OS/2 V1
            width = readShort(input);
            height = readShort(input);
            input.ignore(2);
            assert(readShort(input) == 24 || !"Image is not 24 bits per pixel");
            break;
        case 64:
            //OS/2 V2
            assert(!"Can't load OS/2 V2 bitmaps");
            break;
        case 108:
            //Windows V4
            assert(!"Can't load Windows V4 bitmaps");
            break;
        case 124:
            //Windows V5
            assert(!"Can't load Windows V5 bitmaps");
            break;
        default:
            assert(!"Unknown bitmap format");
    }
    
    //Read the data
    int bytesPerRow = ((width * 3 + 3) / 4) * 4 - (width * 3 % 4);
    int size = bytesPerRow * height;
    auto_array<char> pixels(new char[size]);
    input.seekg(dataOffset, ios_base::beg);
    input.read(pixels.get(), size);
    
    //Get the data into the right format
    auto_array<char> pixels2(new char[width * height * 3]);
    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            for(int c = 0; c < 3; c++) {
                pixels2[3 * (width * y + x) + c] =
                    pixels[bytesPerRow * y + 3 * x + (2 - c)];
            }
        }
    }
    
    input.close();
    return new Image(pixels2.release(), width, height);
}

 



using namespace std;

float _angle = 0;
float i;
int k=0;
const float BOX_SIZE = 2.0f;

GLuint white_textureId;
GLuint red_textureId;
GLuint blue_textureId;
GLuint green_textureId;
GLuint yellow_textureId;
GLuint orange_textureId;

GLuint all_textureId[6];

GLuint _displayListId_smallcube[6]; //The OpenGL id of the display list
//Makes the image into a texture, and returns the id of the texture

GLuint loadTexture(Image* image) 
{
    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGB,
                 image->width, image->height,
                 0,
                 GL_RGB,
                 GL_UNSIGNED_BYTE,
                 image->pixels);
    return textureId;
}


void handleKeypress(unsigned char key,int x,int y)
{
    switch(key)
    {
        case 'q':
            exit(0);
    }
}

void handleResize(int w,int h)
{
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0,(double)w/(double)h,1.0,200);
    gluLookAt(0.0f,5.5f, 15.0f,
              0.0f,0.0f,0.0f,
              0.0f,1.0f,0.0f);
}

void draw_smallcube(int current_colorId)
{
    static bool bInit = false;
    if(!bInit){
        Image* image = loadBMP("white.bmp");
        all_textureId[0] = white_textureId = loadTexture(image);
        delete image;

        Image* image1 = loadBMP("red.bmp");
        all_textureId[1] = red_textureId = loadTexture(image1);
        delete image1;

        Image* image2 = loadBMP("blue.bmp");
        all_textureId[2] = blue_textureId = loadTexture(image2);
        delete image2;

        Image* image3 = loadBMP("orange.bmp");
        all_textureId[3] = orange_textureId = loadTexture(image3);
        delete image3;

        Image* image4 = loadBMP("green.bmp");
        all_textureId[4] = green_textureId = loadTexture(image4);
        delete image4;

        Image* image5 = loadBMP("yellow.bmp");
        all_textureId[5] = yellow_textureId = loadTexture(image5);
        delete image5;
        bInit = true;
    }
    
    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, all_textureId[current_colorId]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBegin(GL_QUADS);
        //Top face
        //glNormal3f(0.0, 1.0f, 0.0f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-BOX_SIZE / 2, BOX_SIZE / 2, -BOX_SIZE / 2);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(-BOX_SIZE / 2, BOX_SIZE / 2, BOX_SIZE / 2);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(BOX_SIZE / 2, BOX_SIZE / 2, BOX_SIZE / 2);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(BOX_SIZE / 2, BOX_SIZE / 2, -BOX_SIZE / 2);
    glEnd();
    glDisable(GL_TEXTURE_2D);

 
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, all_textureId[current_colorId]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBegin(GL_QUADS);
        //Bottom face
        //glNormal3f(0.0, -1.0f, 0.0f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-BOX_SIZE / 2, -BOX_SIZE / 2, -BOX_SIZE / 2);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(BOX_SIZE / 2, -BOX_SIZE / 2, -BOX_SIZE / 2);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(BOX_SIZE / 2, -BOX_SIZE / 2, BOX_SIZE / 2);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(-BOX_SIZE / 2, -BOX_SIZE / 2, BOX_SIZE / 2);
    glEnd();

 
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, all_textureId[current_colorId]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBegin(GL_QUADS);
        //Left face
        //glNormal3f(-1.0, 0.0f, 0.0f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-BOX_SIZE / 2, -BOX_SIZE / 2, -BOX_SIZE / 2);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(-BOX_SIZE / 2, -BOX_SIZE / 2, BOX_SIZE / 2);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(-BOX_SIZE / 2, BOX_SIZE / 2, BOX_SIZE / 2);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(-BOX_SIZE / 2, BOX_SIZE / 2, -BOX_SIZE / 2);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, all_textureId[current_colorId]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBegin(GL_QUADS);
        //Right face
        //glNormal3f(1.0, 0.0f, 0.0f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(BOX_SIZE / 2, -BOX_SIZE / 2, -BOX_SIZE / 2);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(BOX_SIZE / 2, BOX_SIZE / 2, -BOX_SIZE / 2);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(BOX_SIZE / 2, BOX_SIZE / 2, BOX_SIZE / 2);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(BOX_SIZE / 2, -BOX_SIZE / 2, BOX_SIZE / 2);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, all_textureId[current_colorId]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBegin(GL_QUADS);
        //Front face
        //glNormal3f(0.0, 0.0f, 1.0f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-BOX_SIZE / 2, -BOX_SIZE / 2, BOX_SIZE / 2);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(BOX_SIZE / 2, -BOX_SIZE / 2, BOX_SIZE / 2);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(BOX_SIZE / 2, BOX_SIZE / 2, BOX_SIZE / 2);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(-BOX_SIZE / 2, BOX_SIZE / 2, BOX_SIZE / 2);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, all_textureId[current_colorId]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBegin(GL_QUADS);
        //Back face
        //glNormal3f(0.0, 0.0f, -1.0f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-BOX_SIZE / 2, -BOX_SIZE / 2, -BOX_SIZE / 2);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(-BOX_SIZE / 2, BOX_SIZE / 2, -BOX_SIZE / 2);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(BOX_SIZE / 2, BOX_SIZE / 2, -BOX_SIZE / 2);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(BOX_SIZE / 2, -BOX_SIZE / 2, -BOX_SIZE / 2);
    glEnd();
    //glDisable(GL_TEXTURE_2D);
    //glutSwapBuffers();
}

void initRendering()
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_COLOR_MATERIAL);
    //glEnable(GL_NORMALIZE);
    //glEnable(GL_COLOR_MATERIAL);
    glClearColor(0.0f,0.0f,0.2f,1.0f);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    //Set up a display list for drawing a cube
    for(int k=0;k<6;k++){
        _displayListId_smallcube[k] = glGenLists(1); //Make room for the display list
        glNewList(_displayListId_smallcube[k], GL_COMPILE); //Begin the display list
        draw_smallcube(k); //Add commands for drawing a black area to the display list
        glEndList(); //End the display list
    }
}

static cube the_cube;

void drawScene()
{
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0,-5,-10);
    glRotatef(-_angle, 2.0f, 2.0f, 0.0f);
    for(int i=0;i<5;i+=2)
    {
        for(int j=0;j<5;j+=2)
        {
            for(int k=0;k<5;k+=2)
            {
                int current_colorId = rand() % 6;
                glPushMatrix();
                glTranslatef(i,j,k);
                glCallList(_displayListId_smallcube[current_colorId]);
                glPopMatrix();
            }
        }
    }
    glutSwapBuffers();
}


void update(int value) 
{
    _angle += 2.0f;
    if (_angle > 360) {
        _angle -= 360;
    }

    glutPostRedisplay();
    glutTimerFunc(10, update, 0);
}

int main(int argc,char** argv)
{
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB|GLUT_DEPTH);
    glutInitWindowSize(800,800);

    glutCreateWindow("basic shape");
    initRendering();
   // glutFullScreen();

    glutDisplayFunc(drawScene);
    glutKeyboardFunc(handleKeypress);
    glutReshapeFunc(handleResize);
    glutTimerFunc(25, update, 0);

    glutMainLoop();
    return 0;
}

#endif

