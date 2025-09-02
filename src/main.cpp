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

// --- Logging simple ---
static inline void log_info(const std::string& msg){
    std::cout << "[INFO] " << msg << "\n";
}
static inline void log_warn(const std::string& msg){
    std::cout << "[WARN] " << msg << "\n";
}
static inline void log_err(const std::string& msg){
    std::cerr << "[ERR ] " << msg << "\n";
}

struct Args {
    int N=800, width=960, height=600, threads=0;
    std::string mode="omp"; // "seq" | "omp"
    double benchmarkSeconds=0.0;
    unsigned long long seed=42;
};

Args parseArgs(int argc, char** argv){
    Args a; 
    for(int i=1;i<argc;i++) {
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
    // estado actual
    float x, y;
    // parámetros Lissajous
    float Cx, Cy;     // centro
    float A, B;       // amplitudes
    float wx, wy;     // frecuencias
    float phx, phy;   // fases
    float t;          // tiempo local
    // color actual
    unsigned char r, g, b;
};

// utilitario: HSV -> RGB (H[0..1], S[0..1], V[0..1])
static inline void hsv_to_rgb(float H, float S, float V, unsigned char& R, unsigned char& G, unsigned char& B) {
    float h = fmodf(H, 1.0f) * 6.0f; if (h < 0) h += 6.0f;
    int i = (int)h;
    float f = h - i;
    float p = V * (1.0f - S);
    float q = V * (1.0f - S * f);
    float t = V * (1.0f - S * (1.0f - f));
    float rf=0,gf=0,bf=0;
    switch(i % 6){
        case 0: rf=V; gf=t; bf=p; break;
        case 1: rf=q; gf=V; bf=p; break;
        case 2: rf=p; gf=V; bf=t; break;
        case 3: rf=p; gf=q; bf=V; break;
        case 4: rf=t; gf=p; bf=V; break;
        case 5: rf=V; gf=p; bf=q; break;
    }
    R=(unsigned char)(rf*255); G=(unsigned char)(gf*255); B=(unsigned char)(bf*255);
}

std::vector<Particle> initState(int N, int W, int H, unsigned long long seed){
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<float> rCx(0.2f*W, 0.8f*W);
    std::uniform_real_distribution<float> rCy(0.2f*H, 0.8f*H);
    std::uniform_real_distribution<float> rA (0.15f*W, 0.4f*W);
    std::uniform_real_distribution<float> rB (0.15f*H, 0.4f*H);
    std::uniform_real_distribution<float> rwx(0.6f, 2.0f);
    std::uniform_real_distribution<float> rwy(0.6f, 2.0f);
    std::uniform_real_distribution<float> rph(0.0f, 6.2831853f); // 0..2π
    std::uniform_real_distribution<float> rto(0.0f, 100.0f);

    std::vector<Particle> v(N);
    for(int i=0;i<N;i++){
        v[i].Cx = rCx(rng);
        v[i].Cy = rCy(rng);
        v[i].A  = rA(rng);
        v[i].B  = rB(rng);
        v[i].wx = rwx(rng);
        v[i].wy = rwy(rng);
        v[i].phx= rph(rng);
        v[i].phy= rph(rng);
        v[i].t  = rto(rng); // desfasar tiempos
        // posición inicial
        v[i].x = v[i].Cx + v[i].A * std::sin(v[i].wx * v[i].t + v[i].phx);
        v[i].y = v[i].Cy + v[i].B * std::sin(v[i].wy * v[i].t + v[i].phy);
        // color inicial por fase
        float hue = std::fmod(v[i].wx*v[i].t + v[i].phx, 6.2831853f) / 6.2831853f; // 0..1
        hsv_to_rgb(hue, 0.85f, 1.0f, v[i].r, v[i].g, v[i].b);
    }
    return v;
}

// ======== LÓGICA DE UPDATE (sustituye por tu idea) ========
inline void updateOne(Particle& p, float dt, int W, int H){
    // tiempo local
    p.t += dt;

    // “respirar” centros muy suavemente (opcional)
    float drift = 0.15f; // píxeles/seg a lo sumo
    p.Cx += drift * std::sin(0.25f * p.t);
    p.Cy += drift * std::cos(0.22f * p.t);

    // trayectoria Lissajous
    p.x = p.Cx + p.A * std::sin(p.wx * p.t + p.phx);
    p.y = p.Cy + p.B * std::sin(p.wy * p.t + p.phy);

    // si centro se sale mucho del viewport, recentrar suave
    if (p.Cx < 0) p.Cx = 0;
    if (p.Cx > W) p.Cx = (float)W;
    if (p.Cy < 0) p.Cy = 0;
    if (p.Cy > H) p.Cy = (float)H;

    // color por fase x (HSV → RGB)
    float hue = std::fmod(p.wx*p.t + p.phx, 6.2831853f) / 6.2831853f;
    hsv_to_rgb(hue, 0.85f, 1.0f, p.r, p.g, p.b);
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
    // SDL_SetRenderDrawColor(R, p.r, p.g, p.b, 255);
    // SDL_RenderDrawPoint(R, (int)p.x, (int)p.y);
    const int radius = 3; // ajusta a tu gusto
    SDL_SetRenderDrawColor(R, p.r, p.g, p.b, 255);
    // círculo relleno: dibuja scanlines horizontales
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)std::sqrt(radius*radius - dy*dy);
        int x1 = (int)p.x - dx;
        int x2 = (int)p.x + dx;
        int y  = (int)p.y + dy;
        SDL_RenderDrawLine(R, x1, y, x2, y);
    }
}

// ======== BENCHMARK SIN VENTANA ========
void simulateOnly(std::vector<Particle>& P, const Args& A){
    using clk=std::chrono::high_resolution_clock;
    const float dt = 1.0f/240.0f;
    auto t0=clk::now();
    double elapsed=0; unsigned long long steps=0;
    while(elapsed < (A.benchmarkSeconds>0?A.benchmarkSeconds:3.0)){
        if(A.mode=="seq")
            updateSequential(P,dt,A.width,A.height);
        else
            updateParallel(P,dt,A.width,A.height);
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

    // Loguear información inicial
    log_info("Args: mode=" + A.mode + " N=" + std::to_string(A.N) +
         " size=" + std::to_string(A.width) + "x" + std::to_string(A.height) +
         " threads=" + std::to_string(A.threads) +
         " benchmark=" + std::to_string(A.benchmarkSeconds) + "s" +
         " seed=" + std::to_string(A.seed));

#ifdef _OPENMP
    if(A.threads>0) omp_set_num_threads(A.threads);
    int omp_threads = 1;
    #pragma omp parallel
    {
        #pragma omp single
        omp_threads = omp_get_num_threads();
    }
    log_info("OpenMP habilitado. Hilos activos: " + std::to_string(omp_threads));
#else
    log_warn("OpenMP NO está habilitado (compilado sin _OPENMP).");
#endif

    auto state = initState(A.N, A.width, A.height, A.seed);
    log_info("Particulas inicializadas: N=" + std::to_string(state.size()));

    if (A.benchmarkSeconds > 0.0) {
        log_info("Iniciando BENCHMARK por " + std::to_string(A.benchmarkSeconds) + "s...");
        simulateOnly(state, A);
        log_info("Benchmark finalizado.");
        return 0;
    }
    log_info("Iniciando modo VISUAL (SDL)...");

    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0){
        log_err(std::string("SDL_Init error: ") + SDL_GetError());
        return 1;
    }
    log_info("SDL inicializado OK.");

    SDL_Window* win = SDL_CreateWindow("SDL+OpenMP Screensaver",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        A.width, A.height, SDL_WINDOW_RESIZABLE);
    if(!win){
        log_err(std::string("CreateWindow error: ") + SDL_GetError());
        return 1;
    }
    log_info("Ventana creada: " + std::to_string(A.width) + "x" + std::to_string(A.height));

    SDL_Renderer* renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if(!renderer){
        log_err(std::string("CreateRenderer error: ") + SDL_GetError());
        return 1;
    }
    log_info("Renderer creado (acelerado).");

    bool running=true; 
    Uint64 prev=SDL_GetPerformanceCounter();
    const double freq=(double)SDL_GetPerformanceFrequency();

    // Acumuladores FPS
    double fps_acc = 0.0;
    int    fps_frames = 0;
    double fps_interval = 0.5; // segundos

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

        // Acumular para FPS
        fps_acc += dt;
        fps_frames++;
        if (fps_acc >= fps_interval) {
            double fps = fps_frames / fps_acc;

#ifdef _OPENMP
            int th = (A.threads>0?A.threads:omp_get_max_threads());
#else
            int th = 1;
#endif
            std::string title = "SDL+OpenMP | mode=" + A.mode +
                                " | N=" + std::to_string((int)state.size()) +
                                " | threads=" + std::to_string(th) +
                                " | FPS=" + std::to_string((int)std::round(fps));
            SDL_SetWindowTitle(win, title.c_str());
            log_info("FPS=" + std::to_string((int)std::round(fps)) + "  (N=" + std::to_string((int)state.size()) +
            ", threads=" + std::to_string(th) + ", mode=" + A.mode + ")");
            fps_acc = 0.0;
            fps_frames = 0;
        }   

        int W,H; SDL_GetRendererOutputSize(renderer,&W,&H);
        
        if(A.mode=="seq")
            updateSequential(state,dt,W,H);
        else
            updateParallel(state,dt,W,H);

        SDL_SetRenderDrawColor(renderer, 10,10,18,255);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 10,10,18, 40); // alpha bajo
        SDL_Rect full{0,0,W,H};
        SDL_RenderFillRect(renderer, &full);
        for(const auto& p: state) drawOne(renderer,p);
        // HUD
        SDL_RenderPresent(renderer);
    }

    log_info("Saliendo. Liberando recursos SDL...");
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
