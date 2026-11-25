#include "preferences.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <vector>
#include <fcntl.h>
#include <unistd.h>

namespace gallery {
namespace {
constexpr std::size_t MaxBytes = 4096;
bool Valid(const Preferences &p) {
    auto range=[](float v,float lo,float hi){return std::isfinite(v) && v>=lo && v<=hi;};
    return range(p.theme.accent.x,0,1) && range(p.theme.accent.y,0,1) && range(p.theme.accent.z,0,1) && range(p.theme.accent.w,0,1)
        && range(p.theme.motion_response,.05f,4) && range(p.theme.motion_damping,0,4)
        && range(p.theme.motion_duration_scale,.05f,10) && range(p.theme.tokens.text_scale,.5f,2)
        && range(p.theme.tokens.control_radius,0,100) && range(p.theme.tokens.card_radius,0,100)
        && range(p.position.x,0,1) && range(p.position.y,0,1)
        && range(p.size.x,.1f,1) && range(p.size.y,.1f,1)
        && p.position.x+p.size.x<=1.00001f && p.position.y+p.size.y<=1.00001f;
}
template<class... T> bool Read(const std::string &text,T &...values) {
    std::istringstream stream(text); stream.imbue(std::locale::classic());
    if (!(stream >> ... >> values)) return false;
    stream >> std::ws; return stream.eof();
}
}
bool DecodePreferences(std::string_view text,Preferences &output) {
    if (text.size()>MaxBytes || text.find('\0')!=std::string_view::npos) return false;
    Preferences parsed; // Missing optional fields have defaults, not stale live state.
    std::istringstream stream{std::string(text)};
    std::string line; std::set<std::string> keys;
    int version=-1;
    while (std::getline(stream,line)) {
        if (!line.empty() && line.back()=='\r') line.pop_back();
        if (line.empty() || line[0]=='#') continue;
        auto split=line.find('=');
        if (split==std::string::npos || !split || split>64 || line.size()>256) return false;
        auto key=line.substr(0,split), value=line.substr(split+1);
        if (!keys.insert(key).second || keys.size()>64) return false;
        auto boolean=[&](bool &v) {int n=-1; if (!Read(value,n) || (n!=0 && n!=1)) return false; v=n!=0; return true;};
        bool valid=true;
        if (key=="version") valid=Read(value,version) && (version==0 || version==1);
        else if (key=="dark") valid=boolean(parsed.theme.dark);
        else if (key=="animations") valid=boolean(parsed.theme.animations);
        else if (key=="haptics") valid=boolean(parsed.theme.haptics);
        else if (key=="wide") valid=boolean(parsed.wide);
        else if (key=="sidebar") valid=boolean(parsed.sidebar_expanded);
        else if (key=="accent") valid=Read(value,parsed.theme.accent.x,parsed.theme.accent.y,parsed.theme.accent.z,parsed.theme.accent.w);
        else if (key=="motion") valid=Read(value,parsed.theme.motion_response,parsed.theme.motion_damping,parsed.theme.motion_duration_scale);
        else if (key=="text_scale") valid=Read(value,parsed.theme.tokens.text_scale);
        else if (key=="radii") valid=Read(value,parsed.theme.tokens.control_radius,parsed.theme.tokens.card_radius);
        else if (key=="position") valid=Read(value,parsed.position.x,parsed.position.y);
        else if (key=="size") valid=Read(value,parsed.size.x,parsed.size.y);
        // Unknown optional fields are ignored; future schema versions aren't.
        if (!valid) return false;
    }
    if (version<0 || !Valid(parsed)) return false;
    output=parsed; return true;
}
std::string EncodePreferences(const Preferences &p) {
    if (!Valid(p)) return {};
    std::ostringstream s; s.imbue(std::locale::classic());
    s << std::setprecision(std::numeric_limits<float>::max_digits10)
      << "version=1\ndark=" << p.theme.dark << "\nanimations=" << p.theme.animations
      << "\nhaptics=" << p.theme.haptics << "\nwide=" << p.wide << "\nsidebar=" << p.sidebar_expanded
      << "\naccent=" << p.theme.accent.x << ' ' << p.theme.accent.y << ' ' << p.theme.accent.z << ' ' << p.theme.accent.w
      << "\nmotion=" << p.theme.motion_response << ' ' << p.theme.motion_damping << ' ' << p.theme.motion_duration_scale
      << "\ntext_scale=" << p.theme.tokens.text_scale
      << "\nradii=" << p.theme.tokens.control_radius << ' ' << p.theme.tokens.card_radius
      << "\nposition=" << p.position.x << ' ' << p.position.y << "\nsize=" << p.size.x << ' ' << p.size.y << '\n';
    return s.str();
}
LoadResult LoadPreferences(const std::string &path,Preferences &output) {
    if (path.empty() || path.find('\0')!=std::string::npos) return LoadResult::IoError;
    int fd=::open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NONBLOCK);
    if (fd<0) return errno==ENOENT ? LoadResult::Missing : LoadResult::IoError;
    // Bounded reads also handle files that grow while being read.
    char buffer[MaxBytes+1]; std::size_t length=0;
    while (length<sizeof(buffer)) {
        auto n=::read(fd,buffer+length,sizeof(buffer)-length);
        if (n<0 && errno==EINTR) continue;
        if (n<0) {::close(fd); return LoadResult::IoError;}
        if (!n) break;
        length+=static_cast<std::size_t>(n);
    }
    ::close(fd);
    return DecodePreferences(std::string_view(buffer,length),output) ? LoadResult::Loaded : LoadResult::Invalid;
}
bool SavePreferences(const std::string &path,const Preferences &value) {
    auto text=EncodePreferences(value);
    if (path.empty() || path.find('\0')!=std::string::npos || text.empty()) return false;
    auto temporary=path+".tmp.XXXXXX";
    std::vector<char> name(temporary.begin(),temporary.end()); name.push_back('\0');
    int fd=::mkstemp(name.data());
    if (fd<0) return false;
    bool success=true;
    const char *p=text.data(); auto remaining=text.size();
    while (remaining) {
        auto n=::write(fd,p,remaining);
        if (n<0 && errno==EINTR) continue;
        if (n<=0) {success=false; break;}
        p+=n; remaining-=static_cast<std::size_t>(n);
    }
    if (success && ::fsync(fd)!=0) success=false;
    if (::close(fd)!=0) success=false;
    if (success && ::rename(name.data(),path.c_str())!=0) success=false;
    if (!success) ::unlink(name.data());
    return success;
}
} // namespace gallery
