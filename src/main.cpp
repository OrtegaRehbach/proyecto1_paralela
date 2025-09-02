#include <SDL.h>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <iostream>
#include <cmath>
#include <algorithm>

#ifdef _OPENMP
  #include <omp.h>
#else
  inline int omp_get_max_threads(){return 1;}
  inline void omp_set_num_threads(int){}
#endif

struct Args {
  int N=800, width=960, height=600, threads=0;
  std::string mode="omp"; // "seq" | "omp"
  double benchmarkSeconds=0.0;
  unsigned long long seed=42;
};
Args parseArgs(int argc, char** argv){
  Args a; 
  for(int i=1;i<argc;i++){
    std::string s=argv[i];
    auto nextI=[&](int& dst){ if(i+1<argc) dst=std::stoi(argv[++i]); };
    auto nextD=[&](double& dst){ if(i+1<argc) dst=std::stod(argv[++i]); };
    auto nextS=[&](std::string& dst){ if(i+1<argc) dst=argv[++i]; };
    auto nextU=[&](unsigned long long& dst){ if(i+1<argc) dst=std::stoull(argv[++i]); };

    if(s=="-n"||s=="--N") nextI(a.N);
    else if(s=="--width") nextI(a.width);
    else if(s=="--height") nextI(a.height);
    else if(s=="--threads") nextI(a.threads);
    else if(s=="--mode") nextS(a.mode);
    else if(s=="--benchmark") nextD(a.benchmarkSeconds);
    else if(s=="--seed") nextU(a.seed);
    else if(s=="-h"||s=="--help"){
      std::cout<<"Usage: saver [-n N] [--mode seq|omp] [--threads T] "
                  "[--width W] [--height H] [--benchmark S] [--seed X]\n";
      std::exit(0);
    }
  }
  return a;
}

// ======== MODELO DE DATOS (adáptalo a tu idea) ========
struct Particle {
  float x,y,vx,vy,phase;
  unsigned char r,g,b;
};

std::vector<Particle> initState(int N, int W, int H, unsigned long long seed){
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<float> rx(0.f,(float)W);
  std::uniform_real_distribution<float> ry(0.f,(float)H);
  std::uniform_real_distribution<float> rv(-120.f,120.f);
  std::uniform_real_distribution<float> ph(0.f, 6.2831853f);
  std::uniform_int_distribution<int>   rc(0,255);

  std::vector<Particle> v(N);
  for(int i=0;i<N;i++){
    v[i].x=rx(rng); v[i].y=ry(rng);
    v[i].vx=rv(rng); v[i].vy=rv(rng);
    v[i].phase=ph(rng);
    v[i].r=(unsigned char)rc(rng);
    v[i].g=(unsigned char)rc(rng);
    v[i].b=(unsigned char)rc(rng);
  }
  return v;
}

// ======== LÓGICA DE UPDATE (sustituye por tu idea) ========
inline void updateOne(Particle& p, float dt, int W, int H){
  // Ejemplo temporal: trayectoria Lissajous aplicada como "perturbación"
  // Cambia por tus reglas: boids, metaballs (solo mover centros), etc.
  p.phase += dt;
  float ax = 40.f * std::sin(2.0f*p.phase);
  float ay = 40.f * std::sin(3.0f*p.phase + 1.7f);

  p.x += (p.vx + ax)*dt;
  p.y += (p.vy + ay)*dt;

  if(p.x<0){p.x=0; p.vx=-p.vx;}
  if(p.x>W){p.x=(float)W; p.vx=-p.vx;}
  if(p.y<0){p.y=0; p.vy=-p.vy;}
  if(p.y>H){p.y=(float)H; p.vy=-p.vy;}
}

void updateSequential(std::vector<Particle>& P, float dt, int W, int H){
  for(auto& p:P) updateOne(p,dt,W,H);
}

void updateParallel(std::vector<Particle>& P, float dt, int W, int H){
  #pragma omp parallel for schedule(static)
  for(int i=0;i<(int)P.size();++i) updateOne(P[i],dt,W,H);
}

// ======== RENDER (puedes dibujar puntos, líneas, trails en una texture) ========
inline void drawOne(SDL_Renderer* R, const Particle& p){
  SDL_SetRenderDrawColor(R, p.r, p.g, p.b, 255);
  SDL_RenderDrawPoint(R, (int)p.x, (int)p.y);
}

// ======== BENCHMARK SIN VENTANA ========
void simulateOnly(std::vector<Particle>& P, const Args& A){
  using clk=std::chrono::high_resolution_clock;
  const float dt = 1.0f/240.0f;
  auto t0=clk::now();
  double elapsed=0; unsigned long long steps=0;
  while(elapsed < (A.benchmarkSeconds>0?A.benchmarkSeconds:3.0)){
    if(A.mode=="seq") updateSequential(P,dt,A.width,A.height);
    else              updateParallel(P,dt,A.width,A.height);
    steps++;
    elapsed=std::chrono::duration<double>(clk::now()-t0).count();
  }
  std::cout<<"[BENCH] mode="<<A.mode
           <<" N="<<P.size()
           <<" threads="<<(A.threads>0?A.threads:omp_get_max_threads())
           <<" time="<<elapsed<<"s"
           <<" steps="<<steps
           <<" UPS="<<(steps/elapsed)<<"\n";
}

int main(int argc, char** argv){
  Args A = parseArgs(argc, argv);
#ifdef _OPENMP
  if(A.threads>0) omp_set_num_threads(A.threads);
#endif

  auto state = initState(A.N, A.width, A.height, A.seed);

  if (A.benchmarkSeconds > 0.0) {
    simulateOnly(state, A);
    return 0;
  }

  if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0){
    std::cerr<<"SDL_Init error: "<<SDL_GetError()<<"\n"; return 1;
  }
  SDL_Window* win = SDL_CreateWindow("SDL+OpenMP Screensaver",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      A.width, A.height, SDL_WINDOW_RESIZABLE);
  if(!win){ std::cerr<<"CreateWindow: "<<SDL_GetError()<<"\n"; return 1; }

  SDL_Renderer* renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
  if(!renderer){ std::cerr<<"CreateRenderer: "<<SDL_GetError()<<"\n"; return 1; }

  bool running=true; Uint64 prev=SDL_GetPerformanceCounter();
  const double freq=(double)SDL_GetPerformanceFrequency();

  while(running){
    SDL_Event e;
    while(SDL_PollEvent(&e)){
      if(e.type==SDL_QUIT) running=false;
      if(e.type==SDL_KEYDOWN && e.key.keysym.sym==SDLK_ESCAPE) running=false;
    }

    Uint64 now=SDL_GetPerformanceCounter();
    float dt = float((now - prev) / freq);
    prev=now;
    dt = std::min(dt, 1.0f/30.0f); // clamp

    int W,H; SDL_GetRendererOutputSize(renderer,&W,&H);
    if(A.mode=="seq") updateSequential(state,dt,W,H);
    else              updateParallel(state,dt,W,H);

    SDL_SetRenderDrawColor(renderer, 10,10,18,255);
    SDL_RenderClear(renderer);
    for(const auto& p: state) drawOne(renderer,p);
    // HUD
    SDL_RenderPresent(renderer);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
