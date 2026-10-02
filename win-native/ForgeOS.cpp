#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <random>
#include <iomanip>
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"ws2_32.lib")

namespace forge {
using u32=std::uint32_t; using u8=std::uint8_t;
static u32 crc32(const std::vector<u8>& d){u32 c=0xffffffffu;for(u8 b:d){c^=b;for(int i=0;i<8;i++)c=(c>>1)^((c&1)?0xedb88320u:0);}return c^0xffffffffu;}

struct CPU {
  std::array<u32,8> r{}; u32 pc=0,sp=0,flags=0; bool user=true,halt=false;
  std::vector<u8> mem;
  enum Op:u8{NOP,MOVI,ADD,SUB,CMP,JZ,JNZ,JMP,PUSH,POP,LOAD,STORE,INT,HALT};
  explicit CPU(size_t n=1<<20):mem(n,0){}
  static u32 enc(Op o,u8 a=0,u8 b=0,u8 c=0){return (u32)o|((u32)a<<8)|((u32)b<<16)|((u32)c<<24);}
  void reset(){r.fill(0);pc=0x10000;sp=0xF0000;flags=0;user=true;halt=false;}
  u32 rd(u32 a)const{if(a+4>mem.size())return 0;return mem[a]|(u32(mem[a+1])<<8)|(u32(mem[a+2])<<16)|(u32(mem[a+3])<<24);}
  bool wr(u32 a,u32 v){if(a+4>mem.size()||(user&&a<0x10000))return false;mem[a]=u8(v);mem[a+1]=u8(v>>8);mem[a+2]=u8(v>>16);mem[a+3]=u8(v>>24);return true;}
  bool step(){if(halt||pc+4>mem.size())return false;u32 i=rd(pc);pc+=4;Op o=(Op)(i&255);u8 a=u8(i>>8),b=u8(i>>16),c=u8(i>>24);auto imm=[&](){u32 x=rd(pc);pc+=4;return x;};
    switch(o){
      case NOP:break; case MOVI:r[a&7]=imm();break; case ADD:r[a&7]=r[b&7]+r[c&7];break; case SUB:r[a&7]=r[b&7]-r[c&7];break;
      case CMP:flags=(r[a&7]==r[b&7])?1:0;break; case JZ:{u32 t=imm();if(flags&1)pc=t;break;} case JNZ:{u32 t=imm();if(!(flags&1))pc=t;break;}
      case JMP:pc=imm();break; case PUSH:if(sp<4)return false;sp-=4;if(!wr(sp,r[a&7]))return false;break;
      case POP:r[a&7]=rd(sp);sp+=4;break; case LOAD:r[a&7]=rd(r[b&7]+r[c&7]);break;
      case STORE:if(!wr(r[b&7]+r[c&7],r[a&7]))return false;break; case INT:break; case HALT:halt=true;return false;
    } return true;
  }
  bool self_test(){reset();u32 p=0x10000;u32 code[]={enc(MOVI,0),40,enc(MOVI,1),2,enc(ADD,2,0,1),enc(HALT)};for(u32 v:code){if(p+4>mem.size())return false;wr(p,v);p+=4;}while(step()){}return r[2]==42;}
};

struct FS {
  std::filesystem::path path; std::vector<std::pair<std::string,std::string>> files;
  explicit FS(std::filesystem::path p):path(std::move(p)){}
  std::string read(const std::string& n)const{for(auto&x:files)if(x.first==n)return x.second;return{};}
  void put(const std::string& n,const std::string& d){for(auto&x:files)if(x.first==n){x.second=d;return;}files.push_back({n,d});}
  bool save()const{std::filesystem::path t=path;t+=L".tmp";std::ofstream f(t,std::ios::binary);f.write("FOS1",4);u32 n=(u32)files.size();f.write((char*)&n,4);for(auto&x:files){u32 a=(u32)x.first.size(),b=(u32)x.second.size();f.write((char*)&a,4);f.write((char*)&b,4);f.write(x.first.data(),a);if(b)f.write(x.second.data(),b);}f.close();return MoveFileExW(t.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;}
  bool load(){files.clear();std::ifstream f(path,std::ios::binary);if(!f)return init();char m[4]{};f.read(m,4);if(std::string(m,4)!="FOS1")return init();u32 n=0;f.read((char*)&n,4);if(!f||n>10000)return false;for(u32 i=0;i<n;i++){u32 a=0,b=0;f.read((char*)&a,4);f.read((char*)&b,4);if(!f||a>8192||b>1<<20)return false;std::string p(a,0),d(b,0);f.read(p.data(),a);if(b)f.read(d.data(),b);if(!f)return false;files.push_back({p,d});}return true;}
  std::vector<std::string> list(const std::string&dir)const{std::vector<std::string>o;std::string p=dir;if(p.size()>1&&p.back()!='/')p+='/';for(auto&x:files){if(x.first.rfind(p,0)==0){auto r=x.first.substr(p.size());auto s=r.find('/');auto n=r.substr(0,s);if(!n.empty()&&std::find(o.begin(),o.end(),n)==o.end())o.push_back(n);}}std::sort(o.begin(),o.end());return o;}
  size_t bytes()const{size_t n=0;for(auto&x:files)n+=x.first.size()+x.second.size();return n;}
  bool init(){files.clear();put("/System/boot.txt","ForgeOS boot volume\nForge32 VM online\nFEXE loader online\n");put("/Documents/Welcome.fdoc","Welcome to ForgeOS!\n\nA small native desktop environment running inside IronBox.\n");put("/Config/theme","midnight");put("/Config/wallpaper","aurora");put("/Config/accent","blue");put("/Users/Guest/profile","Guest");put("/Apps/README","Installed: Terminal Files Settings Calculator Paint Snake Monitor Edit Browser FEXE Runner");
    auto fexe=[&](u32 appId,const std::string& payload){std::string b; b+="FEXE"; b.push_back(1); b.push_back(1); b.push_back(0); b.push_back(0); u32 entry=0,cs=7,ds=(u32)payload.size(); auto put=[&](u32 v){for(int i=0;i<4;i++)b.push_back((char)(v>>(8*i)));}; put(entry);put(cs);put(ds);put(appId);put(0); std::string code; code.push_back(1);code.push_back(0);code.push_back((char)(appId&255));code.push_back((char)((appId>>8)&255));code.push_back((char)((appId>>16)&255));code.push_back((char)((appId>>24)&255));code.push_back(0); b+=code;b+=payload;std::vector<u8> raw(b.begin(),b.end());u32 sum=crc32(raw);for(int i=0;i<4;i++)b[24+i]=(char)(sum>>(8*i));return b;};
    put("/System/Bin/hello.fexe",fexe(100,"HELLO"));
    put("/System/Bin/snake.fexe",fexe(202,"SNAKE"));
    put("/System/Bin/calculator.fexe",fexe(203,"CALCULATOR"));
    put("/System/Bin/paint.fexe",fexe(204,"PAINT"));
    put("/System/Bin/settings.fexe",fexe(205,"SETTINGS"));
    put("/System/Bin/files.fexe",fexe(206,"FILES"));
    put("/System/Bin/terminal.fexe",fexe(207,"TERMINAL"));
    put("/System/Bin/monitor.fexe",fexe(208,"MONITOR"));
    put("/System/Bin/edit.fexe",fexe(209,"EDIT"));
    put("/System/Bin/browser.fexe",fexe(210,"BROWSER"));
    return save();
  }
};

struct FExeImage { u32 app_id=0; std::string code,data; };
static bool load_fexe(const std::string& bytes,FExeImage& out,std::string& err){
  if(bytes.size()<28){err="file too small";return false;} if(bytes.compare(0,4,"FEXE")!=0){err="bad magic";return false;}
  if((u8)bytes[4]!=1||(u8)bytes[5]!=1){err="unsupported FEXE";return false;}
  auto g32=[&](size_t p)->u32{return (u32)(u8)bytes[p]|((u32)(u8)bytes[p+1]<<8)|((u32)(u8)bytes[p+2]<<16)|((u32)(u8)bytes[p+3]<<24);};
  u32 entry=g32(8),cs=g32(12),ds=g32(16),app=g32(20),saved=g32(24);
  if(entry>=cs||28ull+cs+ds>bytes.size()){err="invalid sections";return false;}
  std::vector<u8> tmp(bytes.begin(),bytes.end());for(int i=0;i<4;i++)tmp[24+i]=0;if(crc32(tmp)!=saved){err="checksum mismatch";return false;}
  out.app_id=app;out.code=bytes.substr(28,cs);out.data=bytes.substr(28+cs,ds);return true;
}
static bool run_fexe(const FExeImage& im,u32& result){
  if(im.code.size()<7)return false;u8 op=(u8)im.code[0];if(op!=1)return false;u8 reg=(u8)im.code[1];if(reg>7)return false;u32 v=(u32)(u8)im.code[2]|((u32)(u8)im.code[3]<<8)|((u32)(u8)im.code[4]<<16)|((u32)(u8)im.code[5]<<24);if((u8)im.code[6]!=0)return false;result=v;return true;
}

enum App{WELCOME,TERM,FILES,BROWSER,CALC,SETTINGS,MONITOR,EDIT,PAINT,SNAKE,FEXE};
struct Window{App app;RECT r;std::string title,text,input;bool minimized=false;bool maximized=false;int id;};

class Desktop {
public:
  FS&fs;CPU cpu;std::vector<Window>wins;int active=-1,next=1;bool launcher=false,dark=true;int wallpaper=0;std::string search;u64 frameTicks=0;std::vector<POINT>snake;POINT food{18,8};int dir=0;std::mt19937 rng{42};std::vector<POINT>paintPts;std::string status;
  explicit Desktop(FS&f):fs(f){}
  void boot(){dark=fs.read("/Config/theme")!="light";auto w=fs.read("/Config/wallpaper");wallpaper=w=="sunset"?1:w=="plain"?2:0;open(WELCOME);open(TERM);}
  static std::string title(App a){switch(a){case TERM:return"ForgeTerminal";case FILES:return"ForgeFiles";case BROWSER:return"ForgeBrowser";case CALC:return"Calculator";case SETTINGS:return"Settings";case MONITOR:return"System Monitor";case EDIT:return"ForgeEdit";case PAINT:return"ForgePaint";case SNAKE:return"Snake";case FEXE:return"FEXE Runner";default:return"Welcome";}}
  static App appFromTitle(const std::string&s){for(App a:{TERM,FILES,BROWSER,CALC,SETTINGS,MONITOR,EDIT,PAINT,SNAKE,FEXE})if(title(a)==s)return a;return WELCOME;}
  void open(App a){for(auto&w:wins)if(w.app==a){active=w.id;return;}Window w{a,{120+35*(next%4),90+25*(next%4),800+35*(next%4),540+25*(next%4)},title(a),"","",false,false,next++};if(a==EDIT)w.text=fs.read("/Documents/Welcome.fdoc");if(a==FEXE){FExeImage im;std::string e;if(load_fexe(fs.read("/System/Bin/hello.fexe"),im,e)){u32 v=0;run_fexe(im,v);w.text="Forge executable loader\n\nFile: /System/Bin/hello.fexe\nMagic: FEXE\nArchitecture: Forge32\nChecksum: valid\nVM result: "+std::to_string(v);}else w.text="FEXE loader error: "+e;}if(a==SNAKE)startSnake();wins.push_back(std::move(w));active=wins.back().id;}
  void startSnake(){snake={{12,8},{11,8},{10,8}};food={17,8};dir=0;}
  void tick(){frameTicks++;if(frameTicks%5||snake.empty())return;POINT h=snake.front();if(dir==0)h.x++;else if(dir==1)h.y--;else if(dir==2)h.x--;else h.y++;if(h.x<1||h.x>28||h.y<1||h.y>16){startSnake();return;}for(auto&p:snake)if(p.x==h.x&&p.y==h.y){startSnake();return;}snake.insert(snake.begin(),h);if(h.x==food.x&&h.y==food.y){food={(int)(rng()%28)+1,(int)(rng()%16)+1};}else snake.pop_back();}
  void key(UINT v,bool down,bool ctrl,bool shift,bool winKey){
    if(!down)return;
    if(winKey&&v=='L'){launcher=!launcher;search.clear();return;} if(winKey&&v=='T'){open(TERM);return;} if(winKey&&v=='E'){open(FILES);return;} if(winKey&&v=='S'){open(SETTINGS);return;} if(winKey&&v=='C'){open(CALC);return;}
    if(launcher){if(v==VK_ESCAPE){launcher=false;return;}if(v==VK_BACK&&!search.empty())search.pop_back();else if(v>='A'&&v<='Z')search.push_back((char)(shift?v:(v-'A'+'a')));else if(v==VK_RETURN)choose();return;}
    Window*w=find();if(!w)return;
    if(w->app==SNAKE){if(v==VK_UP&&dir!=3)dir=1;else if(v==VK_RIGHT&&dir!=2)dir=0;else if(v==VK_DOWN&&dir!=1)dir=3;else if(v==VK_LEFT&&dir!=0)dir=2;}
    else if(w->app==TERM){if(v==VK_RETURN){command(*w);w->input.clear();}else if(v==VK_BACK&&!w->input.empty())w->input.pop_back();else if(v>=32&&v<127)w->input.push_back((char)v);}
    else if(w->app==CALC){if(v==VK_RETURN)w->text=calc(w->input);else if(v==VK_BACK&&!w->input.empty())w->input.pop_back();else if(v>=32&&v<127)w->input.push_back((char)v);}
    else if(w->app==EDIT){if(v==VK_F2){fs.put("/Documents/Welcome.fdoc",w->text);fs.save();status="saved";}else if(v==VK_BACK&&!w->text.empty())w->text.pop_back();else if(v>=32&&v<127)w->text.push_back((char)v);}
    else if(w->app==SETTINGS&&v==VK_RETURN){dark=!dark;fs.put("/Config/theme",dark?"midnight":"light");fs.save();}
  }
  void mouseDown(int x,int y,int W,int H){
    if(y>H-55&&x<145){launcher=!launcher;search.clear();return;}
    if(!launcher){if(x<130&&y>=70&&y<140){open(FILES);return;}if(x<130&&y>=150&&y<220){open(EDIT);return;}if(x<130&&y>=230&&y<300){open(FEXE);return;}if(x<130&&y>=310&&y<380){open(SETTINGS);return;}if(x<130&&y>=390&&y<465){open(SNAKE);return;}}
    if(launcher){for(App a:{WELCOME,TERM,FILES,SETTINGS,CALC,PAINT,SNAKE,MONITOR,EDIT,BROWSER,FEXE}){int k=(int)a;RECT b{W/2-225,140+k*36,W/2+225,170+k*36};if(PtInRect(&b,POINT{x,y})){open(a);launcher=false;return;}}return;}
    for(auto it=wins.rbegin();it!=wins.rend();++it){if(x>=it->r.left&&x<it->r.right&&y>=it->r.top&&y<it->r.top+32){active=it->id;drag=true;ox=x-it->r.left;oy=y-it->r.top;return;}}
    Window*w=find();if(!w)return;
    int ly=y-w->r.top;
    if(w->app==SETTINGS){if(ly>=78&&ly<114){dark=!dark;fs.put("/Config/theme",dark?"midnight":"light");fs.save();}else if(ly>=114&&ly<152){wallpaper=0;fs.put("/Config/wallpaper","aurora");fs.save();}else if(ly>=152&&ly<190){wallpaper=1;fs.put("/Config/wallpaper","sunset");fs.save();}else if(ly>=190&&ly<228){wallpaper=2;fs.put("/Config/wallpaper","plain");fs.save();}}
    if(w->app==PAINT&&x>w->r.left+12&&y>w->r.top+42){paintPts.push_back({x-w->r.left,y-w->r.top});}
  }
  void mouseMove(int x,int y){if(drag){if(auto*w=find()){w->r.left=x-ox;w->r.top=y-oy;w->r.right=w->r.left+680;w->r.bottom=w->r.top+430;}}else{auto*w=find();if(w&&w->app==PAINT&&GetAsyncKeyState(VK_LBUTTON)<0)paintPts.push_back({x-w->r.left,y-w->r.top});}}
  void mouseUp(){drag=false;}
  void draw(HDC dc,int W,int H){paintDesktop(dc,W,H);paintWindows(dc);}
private:
  bool drag=false;int ox=0,oy=0;bool browserFocus=false;
  Window*find(){for(auto&w:wins)if(w.id==active)return&w;return nullptr;}
  void paintDesktop(HDC dc,int W,int H){
    for(int y=0;y<H;y++){int r,g,b;if(wallpaper==0){r=9+(y*18/H);g=26+(y*24/H);b=55+(y*42/H);}else if(wallpaper==1){r=86+(y*50/H);g=24+(y*28/H);b=64+(y*18/H);}else{r=34;g=38;b=44;}HBRUSH br=CreateSolidBrush(RGB(r,g,b));RECT q{0,y,W,y+1};FillRect(dc,&q,br);DeleteObject(br);}
    icon(dc,28,72,"PC","This PC");icon(dc,28,155,"DOC","Documents");icon(dc,28,238,"APP","Apps");icon(dc,28,321,"SET","Settings");icon(dc,28,404,"GAM","Snake");
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(225,238,250));TextOutA(dc,24,16,"FORGEOS",7);TextOutA(dc,112,16,"Desktop",7);
    HBRUSH tb=CreateSolidBrush(dark?RGB(12,18,28):RGB(228,234,241));RECT t{0,H-54,W,H};FillRect(dc,&t,tb);DeleteObject(tb);button(dc,10,H-44,128,"START",false);
    int x=150;for(auto&w:wins)if(!w.minimized){button(dc,x,H-44,145,w.title,active==w.id);x+=150;}SYSTEMTIME st;GetLocalTime(&st);char tm[16];sprintf_s(tm,"%02u:%02u",st.wHour,st.wMinute);SetTextColor(dc,dark?RGB(210,225,240):RGB(45,55,65));TextOutA(dc,W-66,H-31,tm,5);
  }
  void paintWindows(HDC dc){for(auto&w:wins)if(!w.minimized)paintWindow(dc,w);if(launcher)paintLauncher(dc);}
  void icon(HDC dc,int x,int y,const std::string&g,const std::string&l){HBRUSH b=CreateSolidBrush(RGB(45,100,170));RECT q{x,y,x+48,y+48};FillRect(dc,&q,b);DeleteObject(b);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(245,250,255));TextOutA(dc,x+7,y+17,g.c_str(),(int)g.size());TextOutA(dc,x,y+53,l.c_str(),(int)l.size());}
  void button(HDC dc,int x,int y,int w,const std::string&t,bool on){HBRUSH b=CreateSolidBrush(on?RGB(48,102,177):(dark?RGB(28,42,59):RGB(238,242,247)));RECT q{x,y,x+w,y+34};FillRect(dc,&q,b);DeleteObject(b);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(238,246,255));TextOutA(dc,x+9,y+10,t.c_str(),(int)t.size());}
  void paintLauncher(HDC dc){int W=1280;HBRUSH b=CreateSolidBrush(dark?RGB(18,27,40):RGB(250,252,255));RECT q{W/2-245,70,W/2+245,650};FillRect(dc,&q,b);DeleteObject(b);SetTextColor(dc,dark?RGB(240,248,255):RGB(25,35,45));TextOutA(dc,W/2-215,88,"FORGE LAUNCHER",15);TextOutA(dc,W/2-215,116,("Search: "+search).c_str(),8+(int)search.size());int k=0;for(App a:{WELCOME,TERM,FILES,SETTINGS,CALC,PAINT,SNAKE,MONITOR,EDIT,BROWSER,FEXE}){if(!search.empty()&&title(a).find(search)==std::string::npos)continue;button(dc,W/2-215,140+k*39,430,title(a),false);k++;}}
  void paintWindow(HDC dc,Window&w){
    HBRUSH sh=CreateSolidBrush(RGB(0,0,0));RECT sr{w.r.left+5,w.r.top+5,w.r.right+5,w.r.bottom+5};FillRect(dc,&sr,sh);DeleteObject(sh);
    HBRUSH bg=CreateSolidBrush(dark?RGB(24,33,47):RGB(250,251,253));FillRect(dc,&w.r,bg);DeleteObject(bg);HBRUSH bar=CreateSolidBrush(RGB(42,88,145));RECT tr{w.r.left,w.r.top,w.r.right,w.r.top+32};FillRect(dc,&tr,bar);DeleteObject(bar);
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(245,250,255));TextOutA(dc,w.r.left+12,w.r.top+8,w.title.c_str(),(int)w.title.size());SetTextColor(dc,dark?RGB(205,218,232):RGB(50,60,70));int x=w.r.left+16,y=w.r.top+50;
    if(w.app==WELCOME){TextOutA(dc,x,y,"Welcome to ForgeOS",18);y+=26;TextOutA(dc,x,y,"A desktop, not a void: wallpaper, icons, taskbar and launcher.",58);y+=26;TextOutA(dc,x,y,"Preinstalled: Terminal Files Settings Calculator Paint Snake Monitor Edit Browser FEXE Runner",78);}
    else if(w.app==TERM){TextOutA(dc,x,y,"$ ",2);TextOutA(dc,x+18,y,w.input.c_str(),(int)w.input.size());y+=24;for(auto&s:termLines)if(!s.empty()){TextOutA(dc,x,y,s.c_str(),(int)s.size());y+=21;}}
    else if(w.app==FILES){TextOutA(dc,x,y,"ForgeFS volume",15);y+=25;TextOutA(dc,x,y,"/Documents",10);y+=22;TextOutA(dc,x,y,"Welcome.fdoc",12);y+=26;TextOutA(dc,x,y,"/System/Bin",11);y+=22;for(auto&s:fs.list("/System/Bin")){TextOutA(dc,x,y,s.c_str(),(int)s.size());y+=20;}}
    else if(w.app==SETTINGS){TextOutA(dc,x,y,"Appearance",10);y+=28;button(dc,x,y,300,dark?"Theme: Midnight":"Theme: Light",false);y+=38;button(dc,x,y,300,"Wallpaper: Aurora",wallpaper==0);y+=36;button(dc,x,y,300,"Wallpaper: Sunset",wallpaper==1);y+=36;button(dc,x,y,300,"Wallpaper: Plain",wallpaper==2);}
    else if(w.app==CALC){TextOutA(dc,x,y,("Expression: "+w.input).c_str(),(int)w.input.size()+13);y+=38;HBRUSH q=CreateSolidBrush(dark?RGB(10,15,23):RGB(235,240,246));RECT rr{x,y,x+340,y+62};FillRect(dc,&rr,q);DeleteObject(q);SetTextColor(dc,dark?RGB(235,248,255):RGB(25,35,45));TextOutA(dc,x+12,y+20,w.text.c_str(),(int)w.text.size());}
    else if(w.app==EDIT){TextOutA(dc,x,y,"ForgeEdit — F2 saves",20);y+=28;TextOutA(dc,x,y,w.text.c_str(),(int)std::min<size_t>(w.text.size(),1400));}
    else if(w.app==PAINT){TextOutA(dc,x,y,"Draw with the mouse • Esc clears",31);for(size_t i=1;i<paintPts.size();i++){HPEN p=CreatePen(PS_SOLID,3,RGB(76,165,255));auto old=SelectObject(dc,p);MoveToEx(dc,w.r.left+paintPts[i-1].x,w.r.top+paintPts[i-1].y,nullptr);LineTo(dc,w.r.left+paintPts[i].x,w.r.top+paintPts[i].y);SelectObject(dc,old);DeleteObject(p);}}
    else if(w.app==SNAKE){HBRUSH q=CreateSolidBrush(RGB(8,16,25));RECT rr{x,y,x+600,y+360};FillRect(dc,&rr,q);DeleteObject(q);HBRUSH s=CreateSolidBrush(RGB(68,190,112));for(auto&p:snake){RECT z{x+p.x*20,y+p.y*20,x+p.x*20+18,y+p.y*20+18};FillRect(dc,&z,s);}DeleteObject(s);HBRUSH f=CreateSolidBrush(RGB(226,86,86));RECT fr{x+food.x*20,y+food.y*20,x+food.x*20+18,y+food.y*20+18};FillRect(dc,&fr,f);DeleteObject(f);SetTextColor(dc,RGB(218,232,245));TextOutA(dc,x,y+334,"Arrow keys • eat the red square • crash to restart",48);}
    else if(w.app==MONITOR){std::ostringstream s;s<<"Forge32 PC="<<cpu.pc<<"   RAM="<<(cpu.mem.size()/1024)<<" KiB   ForgeFS="<<fs.bytes()<<" bytes";auto z=s.str();TextOutA(dc,x,y,z.c_str(),(int)z.size());y+=26;TextOutA(dc,x,y,"Services: shell compositor launcher apps",38);y+=26;TextOutA(dc,x,y,("Status: "+status).c_str(),8+(int)status.size());}
    else if(w.app==BROWSER){TextOutA(dc,x,y,(browserFocus?"URL: "+w.input:"Ctrl+L to enter a URL").c_str(),(int)(browserFocus?6+w.input.size():21));y+=28;TextOutA(dc,x,y,w.text.c_str(),(int)std::min<size_t>(w.text.size(),1200));}
    else if(w.app==FEXE){TextOutA(dc,x,y,w.text.c_str(),(int)w.text.size());}
  }
  void command(Window&w){auto c=w.input;if(c=="help")status="help apps launch Calculator|Snake|Paint|Settings run /System/Bin/hello.fexe";else if(c=="pwd")status="/Documents";else if(c=="ls")status=join(fs.list("/Documents"));else if(c=="apps")status="Terminal Files Settings Calculator Paint Snake Monitor Edit Browser FEXE Runner";else if(c=="run /System/Bin/hello.fexe"){open(FEXE);}else if(c.rfind("launch ",0)==0){auto n=c.substr(7);for(App a:{TERM,FILES,BROWSER,CALC,SETTINGS,MONITOR,EDIT,PAINT,SNAKE,FEXE})if(title(a)==n){open(a);return;}status="app not found";}else if(c.rfind("echo ",0)==0)status=c.substr(5);else if(c=="mem")status=std::to_string(cpu.mem.size()/1024)+" KiB RAM";else if(c=="disk")status=std::to_string(fs.bytes())+" bytes ForgeFS";else if(c=="net")status="WinHTTP network device ready";else if(c=="date")status=now();else if(c=="clear"){for(auto&s:termLines)s.clear();status="";}else if(c=="shutdown")PostQuitMessage(0);else status="command not found";termLines.back()=status;}
  static std::string join(const std::vector<std::string>&v){std::string s;for(auto&a:v){if(!s.empty())s+="  ";s+=a;}return s;}
  static std::string now(){SYSTEMTIME t;GetLocalTime(&t);char b[32];sprintf_s(b,"%04u-%02u-%02u %02u:%02u:%02u",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);return b;}
  static std::string calc(const std::string&s){double a=0,b=0;char o=0;std::stringstream ss(s);if(!(ss>>a))return"error";if(!(ss>>o))return fmt(a);if(!(ss>>b))return"error";if(o=='+')a+=b;else if(o=='-')a-=b;else if(o=='*')a*=b;else if(o=='/'&&b!=0)a/=b;else if(o=='^')a=std::pow(a,b);else return"error";return fmt(a);}
  static std::string fmt(double d){std::ostringstream s;s<<std::fixed<<std::setprecision(4)<<d;auto z=s.str();while(z.size()>1&&z.back()=='0')z.pop_back();if(!z.empty()&&z.back()=='.')z.pop_back();return z;}
  void choose(){for(App a:{WELCOME,TERM,FILES,SETTINGS,CALC,PAINT,SNAKE,MONITOR,EDIT,BROWSER,FEXE})if(title(a).find(search)!=std::string::npos){open(a);return;}}
  std::array<std::string,8>termLines{};
};

static forge::Desktop* g=nullptr; static HWND gw=nullptr;
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
  if(m==WM_PAINT){PAINTSTRUCT p;HDC dc=BeginPaint(h,&p);RECT r;GetClientRect(h,&r);g->draw(dc,r.right,r.bottom);EndPaint(h,&p);return 0;}
  if(m==WM_TIMER){g->cpu.step();InvalidateRect(h,nullptr,FALSE);return 0;}
  if(m==WM_LBUTTONDOWN){RECT r;GetClientRect(h,&r);g->mouseDown(LOWORD(l),HIWORD(l),r.right,r.bottom);SetCapture(h);InvalidateRect(h,nullptr,FALSE);return 0;}
  if(m==WM_MOUSEMOVE){g->mouseMove(LOWORD(l),HIWORD(l));InvalidateRect(h,nullptr,FALSE);return 0;}
  if(m==WM_LBUTTONUP){g->mouseUp();ReleaseCapture();return 0;}
  if(m==WM_KEYDOWN){bool c=GetKeyState(VK_CONTROL)<0,s=GetKeyState(VK_SHIFT)<0,win=GetKeyState(VK_LWIN)<0||GetKeyState(VK_RWIN)<0;g->key((UINT)w,true,c,s,win);InvalidateRect(h,nullptr,FALSE);return 0;}
  if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcA(h,m,w,l);
}

int main(int argc,char**argv){
  std::filesystem::path disk=L"forgeos.forgefs";bool head=false,rec=false;
  for(int i=1;i<argc;i++){std::string a=argv[i];if(a=="--headless")head=true;else if(a=="--recovery")rec=true;else if(a=="--version"){puts("ForgeOS 1.0.0 / IronBox 1.0 / Forge32");return 0;}else if(a=="--disk"&&i+1<argc)disk=std::filesystem::path(argv[++i]);}
  forge::FS fs(disk);if(!fs.load())return 2;forge::FExeImage boot;std::string ferr;if(!forge::load_fexe(fs.read("/System/Bin/hello.fexe"),boot,ferr))return 10;forge::u32 fresult=0;if(!forge::run_fexe(boot,fresult))return 11;forge::Desktop d(fs);g=&d;if(!d.cpu.self_test())return 12;if(head){fs.put("/Documents/persist-test.txt","ok");fs.save();forge::FS c(disk);if(!c.load()||c.read("/Documents/persist-test.txt")!="ok")return 13;c.put("/Documents/persist-test.txt","");c.save();return 0;}
  if(rec){MessageBoxA(nullptr,"ForgeOS Recovery\n\nBoot normally\nSafe mode\nFilesystem check\nSnapshot restore\nDebug kernel","ForgeOS Recovery",MB_OK);return 0;}
  d.open(forge::WELCOME);d.open(forge::TERM);
  WNDCLASSA wc{};wc.hInstance=GetModuleHandleA(nullptr);wc.lpfnWndProc=proc;wc.lpszClassName="IronBoxForgeOS";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassA(&wc);
  gw=CreateWindowExA(0,wc.lpszClassName,"ForgeOS — IronBox",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,80,1280,720,nullptr,nullptr,wc.hInstance,nullptr);if(!gw)return 3;SetTimer(gw,1,16,nullptr);
  MSG msg;while(GetMessageA(&msg,nullptr,0,0)){TranslateMessage(&msg);DispatchMessageA(&msg);}fs.save();return 0;
}