
#pragma once

#include "myspace/myspace_include.h"
#include "myspace/memory/memory.hpp"
MYSPACE_BEGIN

#ifdef MYSPACE_LINUX

class Iconv
{
public:
    Iconv(const char* from, const char* to)
    :_iconv(iconv_open(to, from))
    {

    }

    ~Iconv()
    {
        iconv_close(_iconv);
    }

    string convert(const string& src)
    {
        if(src.empty()) return "";
        
        char* psrc = (char*)src.c_str();

		char** ppsrc = &psrc;

        size_t srcleft = sizeof(src.size());

        const size_t srclen = srcleft;

        size_t* psrcleft = &srcleft;

        auto dst = new_unique<char[]>(srclen);

		char* pdst = dst.get();

        char** ppdst = &pdst;

        size_t dstleft = srclen;

        const size_t dstlen = dstleft;

        size_t* pdstleft = &dstleft;

        string result;

        for(;;)
        {
            auto n = iconv(_iconv, ppsrc, psrcleft, ppdst, pdstleft);

            if(n < 0)
            {
                if ( errno == EILSEQ ||  errno == EINVAL)
                {
                    *ppsrc++;
                    
                    *psrcleft--;

                    continue;
                }
                else if( errno == E2BIG)
                {
                    result.append(dst.get(), dstlen - dstleft);

                    dstleft = dstlen;

                    pdst = dst.get();

                    continue;
                }
                else
                {
                    break;
                }
            }
            else
            {
                result.append(dst.get(), dstlen - dstleft);

                break;
            }
        }

        return move(result);
    }
private:
    iconv_t     _iconv;
};

#endif


class Codec
{
public:

    static string convertGbkToUtf8(const string& gbk)
    {
#ifdef MYSPACE_LINUX
        return Iconv("GB2312", "UTF-8").convert(gbk);
#else
        return "";
#endif
    }


    static string encodeBase64(const string& src)
    {
        string result;

        uint8_t a3[3] = {0}, a4[4] = {0};

        size_t i = 0;

        result.reserve(EncodedLength(src.size()));

        for(auto c : src)
        {
            a3[i++] = (uint8_t)c;

            if(i == 3)
            {
                i = 0;

                a3_to_a4(a3, a4);

                for(auto j = 0; j < 4; ++j)
                {
                    result.append(1, kBase64Alphabet[a4[j]]);
                }
            }
        }

        if(i != 0)
        {
            for(int j = i; j < 3; ++j)
            {
                a3[j] = 0;
            }

            a3_to_a4(a3, a4);

            for(int j = 0; j < i + 1; ++j)
            {
                result.append(1, kBase64Alphabet[a4[j]]);
            }

            while(i++ < 3)
            {
                result.append(1, '=');
            }
        }

        return move(result);
    }

private:

    static size_t EncodedLength(size_t length) 
    {
        return (length + 2 - ((length + 2) % 3)) / 3 * 4;
    }            

    static void a3_to_a4(unsigned char * a3, unsigned char * a4) 
    {
        a4[0] = (a3[0] & 0xfc) >> 2;
        a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
        a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
        a4[3] = (a3[2] & 0x3f);
    }   

    static constexpr const char* const kBase64Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                        "abcdefghijklmnopqrstuvwxyz"
                                                        "0123456789+/";
};

MYSPACE_END

