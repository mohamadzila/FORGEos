#define NOMINMAX
#include <windows.h>
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
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"ws2_32.lib")

namespace forge {
using u32=std::uint32_t; using u8=std::uint8_t;

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
  void save()const{std::filesystem::path t=path; t+=L".tmp";std::ofstream f(t,std::ios::binary);f.write("FOS1",4);u32 n=(u32)files.size();f.write((char*)&n,4);for(auto&x:files){u32 a=(u32)x.first.size(),b=(u32)x.second.size();f.write((char*)&a,4);f.write((char*)&b,4);f.write(x.first.data(),a);f.write(x.second.data(),b);}f.close();MoveFileExW(t.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);}
  bool load(){files.clear();std::ifstream f(path,std::ios::binary);if(!f){put("/Documents/Welcome.fdoc","Welcome to ForgeOS.");put("/Config/theme","dark");save();return true;}char m[4]{};f.read(m,4);if(std::string(m,4)!="FOS1")return false;u32 n=0;f.read((char*)&n,4);for(u32 i=0;i<n;i++){u32 a=0,b=0;f.read((char*)&a,4);f.read((char*)&b,4);if(a>4096||b>1<<20)return false;std::string p(a,0),d(b,0);f.read(p.data(),a);f.read(d.data(),b);files.push_back({p,d});}return true;}
};

enum App{WELCOME,TERM,FILES,BROWSER,CALC,SETTINGS,MONITOR,EDIT};
struct Window{App app;RECT r;std::string title,text,input;int id;};

class Desktop {
public:
  FS& fs; CPU cpu; std::vector<Window> wins; int active=-1,next=1; bool launcher=false,dark=true; std::string search;
  explicit Desktop(FS&f):fs(f){}
  void open(App a){for(auto&w:wins)if(w.app==a){active=w.id;return;}Window w{a,{100+40*(next%4),80+30*(next%4),760+40*(next%4),520+30*(next%4)},title(a),{}, {},next++};if(a==EDIT)w.text=fs.read("/Documents/Welcome.fdoc");wins.push_back(std::move(w));active=w.id;}
  static std::string title(App a){switch(a){case TERM:return"ForgeTerminal";case FILES:return"ForgeFiles";case BROWSER:return"ForgeBrowser";case CALC:return"Calculator";case SETTINGS:return"Settings";case MONITOR:return"System Monitor";case EDIT:return"ForgeEdit";default:return"Welcome";}}
  void draw(HDC dc,int W,int H){
    HBRUSH bg=CreateSolidBrush(dark?RGB(10,16,26):RGB(235,239,245));RECT q{0,0,W,H};FillRect(dc,&q,bg);DeleteObject(bg);
    HBRUSH top=CreateSolidBrush(RGB(42,92,155));q={0,0,W,54};FillRect(dc,&q,top);DeleteObject(top);
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(245,250,255));TextOutA(dc,22,17,"FORGEOS",7);
    RECT dock{0,H-48,W,H};HBRUSH db=CreateSolidBrush(dark?RGB(18,27,40):RGB(220,226,235));FillRect(dc,&dock,db);DeleteObject(db);
    btn(dc,10,H-40,110,"LAUNCH",launcher);int x=130;for(auto&w:wins){btn(dc,x,H-40,150,w.title,active==w.id);x+=160;}
    std::string clock=now();SetTextColor(dc,dark?RGB(220,230,240):RGB(50,60,70));TextOutA(dc,W-70,H-29,clock.c_str(),(int)clock.size());
    for(auto&w:wins)paintWindow(dc,w);if(launcher)paintLauncher(dc,W,H);
  }
  void click(int x,int y,int W,int H){
    if(y>H-50&&x<120){launcher=!launcher;search.clear();return;}
    if(launcher){std::array<App,8>a{WELCOME,TERM,FILES,BROWSER,CALC,SETTINGS,MONITOR,EDIT};int k=0;for(auto ap:a){std::string t=title(ap);if(search.empty()||t.find(search)!=std::string::npos){RECT b{W/2-190,120+k*42,W/2+190,154+k*42};if(PtInRect(&b,POINT{x,y})){open(ap);launcher=false;return;}k++;}}return;}
    for(auto it=wins.rbegin();it!=wins.rend();++it){if(y>=it->r.top&&y<it->r.top+32&&x>=it->r.left&&x<it->r.right){active=it->id;drag=true;ox=x-it->r.left;oy=y-it->r.top;return;}}
    for(auto&w:wins)if(w.app==SETTINGS&&active==w.id&&x>w.r.left+20&&x<w.r.left+300&&y>w.r.top+80&&y<w.r.top+120){dark=!dark;fs.put("/Config/theme",dark?"dark":"light");fs.save();}
  }
  void move(int x,int y){if(!drag)return;for(auto&w:wins)if(w.id==active){w.r.left=x-ox;w.r.right=w.r.left+680;w.r.top=y-oy;w.r.bottom=w.r.top+430;}}
  void up(){drag=false;}
  void key(UINT v,bool down,bool ctrl,bool shift,bool super){
    if(!down)return;if(super&&v=='L'){launcher=!launcher;return;}if(super&&v=='T'){open(TERM);return;}if(super&&v=='E'){open(FILES);return;}if(super&&v=='B'){open(BROWSER);return;}if(ctrl&&v==VK_SPACE){launcher=!launcher;return;}
    if(launcher){if(v==VK_ESCAPE){launcher=false;return;}if(v==VK_BACK&&!search.empty())search.pop_back();else if(v>='A'&&v<='Z')search.push_back((char)v+(shift?0:32));return;}
    Window*w=nullptr;for(auto&q:wins)if(q.id==active)w=&q;if(!w)return;
    if(w->app==TERM){if(v==VK_RETURN){cmd(*w);w->input.clear();}else if(v==VK_BACK&&!w->input.empty())w->input.pop_back();else if(v>=32&&v<127)w->input.push_back((char)v);}
    else if(w->app==EDIT){if(v==VK_F2){fs.put(w->title=="ForgeEdit"?"/Documents/Welcome.fdoc":"/Documents/Welcome.fdoc",w->text);fs.save();status="saved";}else if(v==VK_BACK&&!w->text.empty())w->text.pop_back();else if(v>=32&&v<127)w->text.push_back((char)v);}
    else if(w->app==CALC){if(v==VK_RETURN){w->text=eval(w->input);}else if(v==VK_BACK&&!w->input.empty())w->input.pop_back();else if(v>=32&&v<127)w->input.push_back((char)v);}
    else if(w->app==BROWSER){if(ctrl&&v=='L'){w->input.clear();browserFocus=true;}else if(browserFocus&&v==VK_RETURN){w->text=fetch(w->input);browserFocus=false;}else if(browserFocus&&v==VK_BACK&&!w->input.empty())w->input.pop_back();else if(browserFocus&&v>=32&&v<127)w->input.push_back((char)v);}
  }
  static std::string now(){SYSTEMTIME t;GetLocalTime(&t);char b[16];sprintf_s(b,"%02u:%02u",t.wHour,t.wMinute);return b;}
  std::string status;
private:
  bool drag=false;int ox=0,oy=0;bool browserFocus=false;
  void btn(HDC dc,int x,int y,int ww,const std::string&t,bool on){HBRUSH b=CreateSolidBrush(on?RGB(55,112,190):(dark?RGB(30,44,62):RGB(240,244,248)));RECT q{x,y,x+ww,y+32};FillRect(dc,&q,b);DeleteObject(b);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(235,245,255));TextOutA(dc,x+10,y+9,t.c_str(),(int)t.size());}
  void paintWindow(HDC dc,Window&w){HBRUSH b=CreateSolidBrush(dark?RGB(22,32,46):RGB(250,251,253));FillRect(dc,&w.r,b);DeleteObject(b);HBRUSH t=CreateSolidBrush(RGB(32,72,122));RECT bar{w.r.left,w.r.top,w.r.right,w.r.top+32};FillRect(dc,&bar,t);DeleteObject(t);SetTextColor(dc,RGB(245,250,255));TextOutA(dc,w.r.left+12,w.r.top+8,w.title.c_str(),(int)w.title.size());SetTextColor(dc,dark?RGB(200,215,230):RGB(50,60,70));int x=w.r.left+14,y=w.r.top+48;
    if(w.app==TERM){TextOutA(dc,x,y,"$ ",2);TextOutA(dc,x+18,y,w.input.c_str(),(int)w.input.size());}
    else if(w.app==FILES){auto v=fs.read("/Documents/Welcome.fdoc");TextOutA(dc,x,y,"/Documents",10);y+=26;TextOutA(dc,x,y,"Welcome.fdoc",12);y+=26;TextOutA(dc,x,y,v.c_str(),(int)v.size());}
    else if(w.app==EDIT){TextOutA(dc,x,y,"ForgeEdit — F2 saves",20);y+=26;TextOutA(dc,x,y,w.text.c_str(),(int)w.text.size());}
    else if(w.app==CALC){TextOutA(dc,x,y,w.input.c_str(),(int)w.input.size());y+=28;TextOutA(dc,x,y,w.text.c_str(),(int)w.text.size());}
    else if(w.app==BROWSER){std::string u=browserFocus?"URL: "+w.input:"Ctrl+L then type URL";TextOutA(dc,x,y,u.c_str(),(int)u.size());y+=28;TextOutA(dc,x,y,w.text.c_str(),(int)w.text.size());}
    else if(w.app==SETTINGS){TextOutA(dc,x,y,("Theme: "+std::string(dark?"Forge Dark":"Forge Light")).c_str(),20);y+=28;TextOutA(dc,x,y,"Click here to toggle",19);}
    else if(w.app==MONITOR){std::ostringstream s;s<<"CPU PC="<<cpu.pc<<" R0="<<cpu.r[0]<<" RAM="<<cpu.mem.size()/1024<<" KiB";auto z=s.str();TextOutA(dc,x,y,z.c_str(),(int)z.size());}
    else{TextOutA(dc,x,y,"Welcome to ForgeOS",18);y+=26;TextOutA(dc,x,y,"An independent computer inside IronBox.",40);}
  }
  void paintLauncher(HDC dc,int W,int){HBRUSH b=CreateSolidBrush(dark?RGB(18,27,40):RGB(250,252,255));RECT q{W/2-220,80,W/2+220,600};FillRect(dc,&q,b);DeleteObject(b);SetTextColor(dc,dark?RGB(240,248,255):RGB(30,40,50));TextOutA(dc,W/2-190,104,"COMMAND PALETTE",15);int k=0;for(App a:{WELCOME,TERM,FILES,BROWSER,CALC,SETTINGS,MONITOR,EDIT}){std::string t=title(a);if(search.empty()||t.find(search)!=std::string::npos){btn(dc,W/2-190,135+k*42,380,t,false);k++;}}}
  void cmd(Window&w){std::string c=w.input;if(c=="help")status="help ls cat pwd mkdir touch ps mem disk net date uname reboot shutdown";else if(c=="pwd")status="/Documents";else if(c=="cat /Documents/Welcome.fdoc")status=fs.read("/Documents/Welcome.fdoc");else if(c.rfind("echo ",0)==0)status=c.substr(5);else if(c=="mem")status=std::to_string(cpu.mem.size()/1024)+" KiB";else if(c=="net")status="NAT network: ready";else if(c=="ps")status="1 terminal  2 desktop  3 browser";else if(c=="reboot")status="restarting guest";else if(c=="shutdown")PostQuitMessage(0);else status="command not found";for(size_t i=0;i+1<termLines.size();++i)termLines[i]=termLines[i+1];termLines.back()=status;}
  static std::string eval(const std::string&s){double a=0,b=0;char op=0;std::stringstream ss(s);if(ss>>a){if(!(ss>>op>>b))return std::to_string(a);if(op=='+')a+=b;else if(op=='-')a-=b;else if(op=='*')a*=b;else if(op=='/'&&b!=0)a/=b;}return std::to_string(a);}
  std::string fetch(const std::string&url){
    std::wstring u;int n=MultiByteToWideChar(CP_UTF8,0,url.data(),(int)url.size(),nullptr,0);u.resize(n);MultiByteToWideChar(CP_UTF8,0,url.data(),(int)url.size(),u.data(),n);
    URL_COMPONENTSW c{};c.dwStructSize=sizeof(c);wchar_t host[256]{},path[2048]{};c.lpszHostName=host;c.dwHostNameLength=255;c.lpszUrlPath=path;c.dwUrlPathLength=2047;
    if(!WinHttpCrackUrl(u.c_str(),0,0,&c))return"Invalid URL";
    HINTERNET s=WinHttpOpen(L"ForgeBrowser/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!s)return"WinHTTP unavailable";
    HINTERNET h=WinHttpConnect(s,c.lpszHostName,c.nPort,0);if(!h){WinHttpCloseHandle(s);return"Connection failed";}
    DWORD fl=c.nScheme==INTERNET_SCHEME_HTTPS?WINHTTP_FLAG_SECURE:0;HINTERNET r=WinHttpOpenRequest(h,L"GET",c.lpszUrlPath[0]?c.lpszUrlPath:L"/",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,fl);
    std::string out;if(r&&WinHttpSendRequest(r,WINHTTP_NO_ADDITIONAL_HEADERS,0,nullptr,0,0,0)&&WinHttpReceiveResponse(r,nullptr)){DWORD a=0;while(WinHttpQueryDataAvailable(r,&a)&&a&&out.size()<12000){std::string z(a,0);DWORD got=0;if(!WinHttpReadData(r,z.data(),a,&got)||!got)break;z.resize(got);out+=z;}}if(r)WinHttpCloseHandle(r);WinHttpCloseHandle(h);WinHttpCloseHandle(s);for(char&ch:out)if(ch=='\r'||ch=='\n')ch=' ';return out.empty()?"(empty response)":out;
  }
  std::array<std::string,8>termLines{};
};

static forge::Desktop* g=nullptr; static HWND gw=nullptr;
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
  if(m==WM_PAINT){PAINTSTRUCT p;HDC dc=BeginPaint(h,&p);RECT r;GetClientRect(h,&r);g->draw(dc,r.right,r.bottom);EndPaint(h,&p);return 0;}
  if(m==WM_TIMER){g->cpu.step();InvalidateRect(h,nullptr,FALSE);return 0;}
  if(m==WM_LBUTTONDOWN){RECT r;GetClientRect(h,&r);g->click(LOWORD(l),HIWORD(l),r.right,r.bottom);SetCapture(h);InvalidateRect(h,nullptr,FALSE);return 0;}
  if(m==WM_MOUSEMOVE){g->move(LOWORD(l),HIWORD(l));InvalidateRect(h,nullptr,FALSE);return 0;}
  if(m==WM_LBUTTONUP){g->up();ReleaseCapture();return 0;}
  if(m==WM_KEYDOWN){bool c=GetKeyState(VK_CONTROL)<0,s=GetKeyState(VK_SHIFT)<0,win=GetKeyState(VK_LWIN)<0||GetKeyState(VK_RWIN)<0;g->key((UINT)w,true,c,s,win);InvalidateRect(h,nullptr,FALSE);return 0;}
  if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcA(h,m,w,l);
}

int main(int argc,char**argv){
  std::filesystem::path disk=L"forgeos.forgefs";bool head=false,rec=false;
  for(int i=1;i<argc;i++){std::string a=argv[i];if(a=="--headless")head=true;else if(a=="--recovery")rec=true;else if(a=="--version"){puts("ForgeOS 1.0.0 / IronBox 1.0 / Forge32");return 0;}else if(a=="--disk"&&i+1<argc)disk=forge::widen(argv[++i]);}
  forge::FS fs(disk);if(!fs.load())return 2;forge::Desktop d(fs);g=&d;if(!d.cpu.self_test())return 10;if(head){fs.put("/Documents/persist-test.txt","ok");fs.save();forge::FS c(disk);if(!c.load()||c.read("/Documents/persist-test.txt")!="ok")return 11;c.put("/Documents/persist-test.txt","");c.save();return 0;}
  if(rec){MessageBoxA(nullptr,"ForgeOS Recovery\n\nBoot normally\nSafe mode\nFilesystem check\nSnapshot restore\nDebug kernel","ForgeOS Recovery",MB_OK);return 0;}
  d.open(forge::WELCOME);d.open(forge::TERM);
  WNDCLASSA wc{};wc.hInstance=GetModuleHandleA(nullptr);wc.lpfnWndProc=proc;wc.lpszClassName="IronBoxForgeOS";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassA(&wc);
  gw=CreateWindowExA(0,wc.lpszClassName,"ForgeOS — IronBox",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,80,1280,720,nullptr,nullptr,wc.hInstance,nullptr);if(!gw)return 3;SetTimer(gw,1,16,nullptr);
  MSG msg;while(GetMessageA(&msg,nullptr,0,0)){TranslateMessage(&msg);DispatchMessageA(&msg);}fs.save();return 0;
}