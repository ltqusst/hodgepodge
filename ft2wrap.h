
#ifndef _FREETYPE2LIB_H_
#define _FREETYPE2LIB_H_

#include "opencv2/opencv.hpp"

#include <fcntl.h>           /* Definition of AT_* constants */
#include <unistd.h>

#include <mutex>
#include "ft2build.h"
#include FT_FREETYPE_H

using namespace cv;
using namespace std;

//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

class ft2wrap
{
public:
    void init(const char * fontfile = NULL)
    {
        const char * default_font = "/usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc";
        const char * default_font_ubuntu = "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc";
        
        if(fontfile == NULL){
            if ((access(default_font, F_OK)) == 0)
                fontfile = default_font;
            else
            if ((access(default_font_ubuntu, F_OK)) == 0)
                fontfile = default_font_ubuntu;
            else{
                fprintf(stderr,"Can't load font\n");
                exit(-1);
            }
        }

        FT_Init_FreeType(&library);
        
        FT_New_Face(library, fontfile, 0, &face);
        
        if (face == 0) {
            fprintf(stderr, "font %s load failed\n", fontfile);
            exit(1);
        }
        cur_size = 24;
        FT_Set_Pixel_Sizes(face, cur_size, 0);
        FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    }

    float printString(Mat& img,std::wstring str,const int penx,const int peny,Scalar color, int size)
    {
        std::unique_lock<std::mutex> lk(_m);
        FT_Bool       use_kerning=0;
        FT_UInt       previous=0;
        use_kerning = FT_HAS_KERNING( face );
        float dy_max = 0;
        float posx = penx;
        float dx=0;
        FT_Error err;
        setSize(size);
        for(size_t k=0;k<str.length();k++)
        {
            FT_UInt glyph_index = FT_Get_Char_Index( face, str.c_str()[k] );

            if(glyph_index == 0) {
                fprintf(stderr,"Undefined glyph for char index: %d", str.c_str()[k]);
                continue; //undefined glyph
            }

            FT_GlyphSlot  slot = face->glyph;  // a small shortcut
            if(k>0){dx=slot->advance.x/64;  }
            err = FT_Load_Glyph( face, glyph_index, FT_LOAD_DEFAULT );
            if(err) continue;

            err = FT_Render_Glyph (slot,FT_RENDER_MODE_NORMAL);
            if(err) continue;

            if ( use_kerning && previous && glyph_index )
            {
                FT_Vector  delta = {0};
                err = FT_Get_Kerning( face, previous, glyph_index, FT_KERNING_DEFAULT, &delta );
                if(err == 0)
                    posx += (delta.x/64);
            }
            posx += (dx);
            my_draw_bitmap(img,&slot->bitmap,posx + slot->bitmap_left, peny - slot->bitmap_top,color);
            previous = glyph_index;

            if(dy_max < slot->bitmap.rows) dy_max = slot->bitmap.rows;
        //printf("%d,%d, %f, %f, %f\n", slot->bitmap.rows, slot->bitmap.width, (float)slot->advance.x, (float)slot->advance.y, (float)slot->metrics.vertAdvance);
        }
        return dy_max;//prev_yadv;
    }
    
#if 0
    void printText(Mat& img, std::wstring str, int x, int y, Scalar color, int size)
    {
        float posy=0;
        float dy = 0;
        setSize(size);

        for(size_t pos=str.find_first_of(L'\n');pos!=wstring::npos;pos=str.find_first_of(L'\n'))
        {
            std::wstring substr=str.substr(0,pos);
            str.erase(0,pos+1);
            posy+= (dy = printString(img,substr,x,y+posy, color, size));
        }
        printString(img,str,x,y+posy,color, size);
    }
#endif

    void selfTest(unsigned int c0=0x4E00, unsigned int c1=0x9FA5)
    {
        //4E00-9FA5 is range of CJK(Hanzi)
        const int W = 960;
        const int H = 480;
        cv::Mat nvaShowImg = cv::Mat::zeros(H, W, CV_8UC3);
        wchar_t wstr[256] = {0};
        int i;
        for(;;)
        {
            int len = rand() % 256;
            int sz = 8 + (rand() % 63);
            for(i=0;i<len;i++) wstr[i] = c0 + (rand() % (c1-c0));
            wstr[i] = 0;
            
            int x0 = -len*sz,x1 = W;
            int y0 = -sz, y1 = H+sz;
            int x = x0 + rand() % (x1-x0);
            int y = y0 + rand() % (y1-y0);
            
            int r = rand() & 0xFF;
            int g = rand() & 0xFF;
            int b = rand() & 0xFF;
            printString(nvaShowImg, wstr, x, y, cv::Scalar(r, g, b), sz);
            
            cv::imshow("nvaShowImg", nvaShowImg);
            
            if((waitKey(2) & 0xFF) == 'q') break;
        }
    }
    
private:
    void setSize(int size)
    {
        if (cur_size != size) {
            cur_size = size;
            FT_Set_Pixel_Sizes(face, cur_size, 0);
        }
    }

    void my_draw_bitmap(Mat& img,FT_Bitmap* bitmap,const int x,const int y, Scalar color)
    {
        int c[] = {(int)color[0], (int)color[1], (int)color[2]};
        int x0 = x, x1 = x + bitmap->width;
        int y0 = y, y1 = y + bitmap->rows;
        
        if(x0 < 0) x0 = 0;
        if(y0 < 0) y0 = 0;
        if(x1 > img.cols) x1 = img.cols;
        if(y1 > img.rows) y1 = img.rows;
        
        int i0 = y0 - y, i1 = y1 - y;
        int j0 = x0 - x, j1 = x1 - x;
        
        for(int i = i0;i < i1;i++)
        {
            unsigned char * alpha = &(bitmap->buffer[i*bitmap->pitch]);
            for(int j = j0;j < j1;j++)
            {
                unsigned char val = alpha[j];
                if(val!=0)
                {
                    Vec3b &src = img.at<Vec3b>(i+y,j+x);
                    src[0] = (src[0] * (255-val) + c[0] * val)/256;
                    src[1] = (src[1] * (255-val) + c[1] * val)/256;
                    src[2] = (src[2] * (255-val) + c[2] * val)/256;
                }
            }
        }
        //printf("i:(%d,%d), j:(%d,%d)\n", i0+y, i1+y, j0+x, j1+x);
    }

    FT_Library  library;
    FT_Face     face;
    std::mutex  _m;
    int         cur_size;
};


#endif
