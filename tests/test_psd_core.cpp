#include "PsdCore.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

static void be16(std::vector<uint8_t>& v, uint16_t x){v.push_back(static_cast<uint8_t>(x>>8));v.push_back(static_cast<uint8_t>(x));}
static void be32(std::vector<uint8_t>& v, uint32_t x){v.push_back(static_cast<uint8_t>(x>>24));v.push_back(static_cast<uint8_t>(x>>16));v.push_back(static_cast<uint8_t>(x>>8));v.push_back(static_cast<uint8_t>(x));}
static void be64(std::vector<uint8_t>& v, uint64_t x){for(int i=7;i>=0;--i)v.push_back(static_cast<uint8_t>(x>>(i*8)));}
static void header(std::vector<uint8_t>& v, uint16_t version, uint16_t channels, uint32_t w,uint32_t h,uint16_t mode){
  v.insert(v.end(), {'8','B','P','S'}); be16(v,version); v.insert(v.end(),6,0); be16(v,channels); be32(v,h); be32(v,w); be16(v,8); be16(v,mode);
  be32(v,0); // color mode data
}
static void emptyResourcesAndLayers(std::vector<uint8_t>& v, uint16_t version){be32(v,0); if(version==1) be32(v,0); else be64(v,0);}

static std::vector<uint8_t> raw_rgb(){
  std::vector<uint8_t> v; header(v,1,3,2,2,3); emptyResourcesAndLayers(v,1); be16(v,0);
  v.insert(v.end(), {255,0,0,255}); v.insert(v.end(), {0,255,0,255}); v.insert(v.end(), {0,0,255,255});
  return v;
}

static std::vector<uint8_t> rle_gray_psd(){
  std::vector<uint8_t> v; header(v,1,1,4,2,1); emptyResourcesAndLayers(v,1); be16(v,1);
  be16(v,5); be16(v,5); v.insert(v.end(), {3,10,20,30,40, 3,50,60,70,80}); return v;
}

static std::vector<uint8_t> rle_gray_psb(){
  std::vector<uint8_t> v; header(v,2,1,4,2,1); emptyResourcesAndLayers(v,2); be16(v,1);
  be32(v,5); be32(v,5); v.insert(v.end(), {3,11,21,31,41, 3,51,61,71,81}); return v;
}

static std::vector<uint8_t> with_thumb(){
  std::vector<uint8_t> v; header(v,1,3,1,1,3);
  std::vector<uint8_t> r; r.insert(r.end(), {'8','B','I','M'}); be16(r,1036); r.push_back(0); r.push_back(0);
  std::vector<uint8_t> d; be32(d,1); be32(d,64); be32(d,32); be32(d,192); be32(d,4); be32(d,4); be16(d,24); be16(d,1);
  d.insert(d.end(), {0xFF,0xD8,0xFF,0xD9}); be32(r,static_cast<uint32_t>(d.size())); r.insert(r.end(),d.begin(),d.end()); if(d.size()&1) r.push_back(0);
  be32(v,static_cast<uint32_t>(r.size())); v.insert(v.end(),r.begin(),r.end()); be32(v,0); be16(v,0); v.insert(v.end(), {0,0,0}); return v;
}

int main(){
  {
    auto v=raw_rgb(); psdx::MemoryStream s(v.data(),v.size()); psdx::ImageBGRA img; std::string d;
    auto st=psdx::DecodeCompositeThumbnail(s,256,img,&d); assert(st==psdx::DecodeStatus::Ok); assert(img.width==2&&img.height==2);
    auto p=img.pixels.data(); assert(p[2]==255&&p[1]==0&&p[0]==0); p+=4; assert(p[2]==0&&p[1]==255&&p[0]==0); p+=4; assert(p[2]==0&&p[1]==0&&p[0]==255);
  }
  {
    auto v=rle_gray_psd(); psdx::MemoryStream s(v.data(),v.size()); psdx::ImageBGRA img; std::string d;
    auto st=psdx::DecodeCompositeThumbnail(s,256,img,&d); assert(st==psdx::DecodeStatus::Ok); assert(img.pixels[0]==10&&img.pixels[1]==10&&img.pixels[2]==10);
    psdx::MemoryStream s2(v.data(),v.size()); auto info=psdx::Inspect(s2); assert(info.status==psdx::DecodeStatus::Ok&&info.compositeCompression==1&&info.header.version==1);
  }
  {
    auto v=rle_gray_psb(); psdx::MemoryStream s(v.data(),v.size()); psdx::ImageBGRA img; std::string d;
    auto st=psdx::DecodeCompositeThumbnail(s,256,img,&d); assert(st==psdx::DecodeStatus::Ok); assert(img.pixels[0]==11&&img.pixels[1]==11&&img.pixels[2]==11);
    psdx::MemoryStream s2(v.data(),v.size()); auto info=psdx::Inspect(s2); assert(info.status==psdx::DecodeStatus::Ok&&info.compositeCompression==1&&info.header.version==2);
  }
  {
    auto v=with_thumb(); psdx::MemoryStream s(v.data(),v.size()); std::vector<uint8_t> jpg; uint32_t w=0,h=0; std::string d;
    auto st=psdx::ExtractEmbeddedJpeg(s,jpg,w,h,&d); assert(st==psdx::DecodeStatus::Ok); assert(w==64&&h==32&&jpg.size()==4&&jpg[0]==0xFF&&jpg[1]==0xD8);
  }
  std::cout << "PSD/PSB core tests: OK\n";
}
