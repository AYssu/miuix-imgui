// OkLab / OkHSV equations adapted from Björn Ottosson's 2021 reference.
// MIT license: LICENSES/Ok-color-MIT.txt. C++ adaptation and boundary handling.
#include "skui/skui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>

namespace skui {
namespace {
struct V {double x,y,z;};
double Linear(double x) {return x<=.04045 ? x/12.92 : std::pow((x+.055)/1.055,2.4);}
double Gamma(double x) {return x<=.0031308 ? 12.92*x : 1.055*std::pow(x,1/2.4)-.055;}
V Lab(V c) {
    double l=std::cbrt(.4122214708*c.x+.5363325363*c.y+.0514459929*c.z);
    double m=std::cbrt(.2119034982*c.x+.6806995451*c.y+.1073969566*c.z);
    double s=std::cbrt(.0883024619*c.x+.2817188376*c.y+.6299787005*c.z);
    return {.2104542553*l+.7936177850*m-.0040720468*s,
        1.9779984951*l-2.4285922050*m+.4505937099*s,
        .0259040371*l+.7827717662*m-.8086757660*s};
}
V Rgb(V c) {
    double l=c.x+.3963377774*c.y+.2158037573*c.z,m=c.x-.1055613458*c.y-.0638541728*c.z,s=c.x-.0894841775*c.y-1.2914855480*c.z;
    l=l*l*l;m=m*m*m;s=s*s*s;
    return {4.0767416621*l-3.3077115913*m+.2309699292*s,
        -1.2684380046*l+2.6097574011*m-.3413193965*s,
        -.0041960863*l-.7034186147*m+1.7076147010*s};
}
double Toe(double x) {double k=1.206/1.03,t=k*x-.206;return .5*(t+std::sqrt(t*t+.12*k*x));}
double InvToe(double x) {return (x*x+.206*x)/((1.206/1.03)*(x+.03));}
V ST(double a,double b) {
    // Find the sRGB cusp numerically instead of the reference polynomial/Halley
    // approximation. This runs only for the edited color, not for each pixel.
    double lo=0,hi=4;
    for(int i=0;i<40;++i) {
        double s=(lo+hi)/2;auto c=Rgb({1,s*a,s*b});
        if(std::min({c.x,c.y,c.z})>=0) lo=s;else hi=s;
    }
    auto c=Rgb({1,lo*a,lo*b});double L=std::cbrt(1/std::max({c.x,c.y,c.z})),C=L*lo;
    return {C/L,C/(1-L),0};
}
double Wrap(double h) {return h-std::floor(h);}
V OkToRgb(V c) {
    if(c.z<=0) return {0,0,0};
    double a=std::cos(2*IM_PI*c.x),b=std::sin(2*IM_PI*c.x);auto st=ST(a,b);
    double S=st.x,T=st.y,k=1-.5/S,den=.5+T-T*k*c.y;
    double Lv=1-c.y*.5/den,Cv=c.y*T*.5/den;
    double L=c.z*Lv,C=c.z*Cv,Lvt=InvToe(Lv),Cvt=Cv*Lvt/Lv;
    double next=InvToe(L);C*=next/L;L=next;
    auto scale=Rgb({Lvt,a*Cvt,b*Cvt});double factor=std::cbrt(1/std::max({scale.x,scale.y,scale.z,1e-12}));
    return Rgb({L*factor,C*factor*a,C*factor*b});
}
V RgbToOk(V lab) {
    double C=std::hypot(lab.y,lab.z),L=lab.x;
    if(C<1e-6 || L<1e-9) return {0,0,Toe(std::max(0.,L))};
    double a=lab.y/C,b=lab.z/C,h=Wrap(std::atan2(b,a)/(2*IM_PI));auto st=ST(a,b);
    double T=st.y,k=1-.5/st.x,t=T/(C+L*T),Lv=t*L,Cv=t*C;
    double Lvt=InvToe(Lv),Cvt=Cv*Lvt/Lv;
    auto rgb=Rgb({Lvt,a*Cvt,b*Cvt});double factor=std::cbrt(1/std::max({rgb.x,rgb.y,rgb.z,1e-12}));
    L=Toe(L/factor);
    return {h,(.5+T)*Cv/(T*.5+T*k*Cv),L/Lv};
}
}
ImVec4 ToColorSpace(ImVec4 c,ColorSpace space) {
    if(space==ColorSpace::HSV) {ImGui::ColorConvertRGBtoHSV(c.x,c.y,c.z,c.x,c.y,c.z);return c;}
    auto l=Lab({Linear(c.x),Linear(c.y),Linear(c.z)});V value=l;
    if(space==ColorSpace::OkLCH) value={l.x,std::hypot(l.y,l.z),Wrap(std::atan2(l.z,l.y)/(2*IM_PI))};
    if(space==ColorSpace::OkHSV) value=RgbToOk(l);
    return ImVec4(value.x,value.y,value.z,c.w);
}
ImVec4 FromColorSpace(ImVec4 c,ColorSpace space) {
    if(space==ColorSpace::HSV) {ImGui::ColorConvertHSVtoRGB(c.x,c.y,c.z,c.x,c.y,c.z);return c;}
    V lab{c.x,c.y,c.z},rgb;
    if(space==ColorSpace::OkLCH) lab={c.x,c.y*std::cos(c.z*2*IM_PI),c.y*std::sin(c.z*2*IM_PI)};
    rgb=space==ColorSpace::OkHSV ? OkToRgb({c.x,c.y,c.z}) : Rgb(lab);
    return ImVec4(std::clamp(Gamma(rgb.x),0.,1.),std::clamp(Gamma(rgb.y),0.,1.),std::clamp(Gamma(rgb.z),0.,1.),c.w);
}
} // namespace skui
