/*
 * Minigame de obstáculos para visual novel (raylib)
 *
 * Controles:
 *   Menu: W/S ou setas para navegar, ENTER ou ESPAÇO para confirmar, ESC para sair
 *   Jogo: ESPAÇO, W ou seta para cima para pular, ESC para voltar ao menu
 *   Fim:  ENTER para jogar de novo, ESC para voltar ao menu
 *
 * Compilação:
 *   Linux:   gcc minigame.c -o minigame -lraylib -lm -ldl -lpthread -lGL -lrt -lX11
 *   Windows: gcc minigame.c -o minigame.exe -lraylib -lopengl32 -lgdi32 -lwinmm
 *   macOS:   gcc minigame.c -o minigame -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
 */

#include "raylib.h"
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define LARGURA          800
#define ALTURA           450
#define CHAO_Y           360

#define JOG_X            100.0f
#define JOG_LARGURA      40.0f
#define JOG_ALTURA       50.0f

#define GRAVIDADE        2200.0f
#define FORCA_PULO       780.0f
#define VEL_INICIAL      400.0f
#define VEL_MAXIMA       920.0f
#define ACELERACAO       20.0f

#define MAX_OBSTACULOS   8
#define BONUS_OBSTACULO  25.0f
#define PONTOS_POR_SEG   10.0f

#define NUM_OPCOES       2
#define ARQUIVO_RECORDE  "recorde.dat"

#define PONTOS_TRANSICAO    3000.0f

#define PUZZLE_AREA_X       40
#define PUZZLE_AREA_Y       90
#define PUZZLE_AREA_LARGURA 720
#define PUZZLE_AREA_ALTURA  300
#define PUZZLE_VEL          220.0f
#define BALA_VEL            520.0f
#define MAX_BLOCOS          6
#define MAX_BALAS           5
#define BALAS_POR_FASE      3
#define TOTAL_FASES         3

typedef enum { TELA_MENU, TELA_JOGO, TELA_FIM, TELA_PUZZLE, TELA_VITORIA } Tela;
typedef enum { RESULTADO_NADA, RESULTADO_COLISAO, RESULTADO_TRANSICAO } ResultadoCorrida;
typedef enum { PUZZLE_NADA, PUZZLE_SAIU } ResultadoPuzzle;

typedef struct {
    Rectangle corpo;
    float velY;
    bool noChao;
} Jogador;

typedef struct {
    Rectangle corpo;
    bool ativo;
    bool contado;
} Obstaculo;

typedef struct {
    Jogador jogador;
    Obstaculo obstaculos[MAX_OBSTACULOS];
    float velocidade;
    float tempoSpawn;
    float pontos;
    float deslocamentoChao;
} Jogo;

/* ---------- Fase de puzzle (desbloqueada aos 10000 pontos) ---------- */

typedef struct {
    Rectangle corpo;
    Vector2 direcaoOlhar; /* última direção de movimento, usada para mirar */
} JogadorPuzzle;

typedef struct {
    Rectangle corpo;
    bool quebravel; /* true = destrutível a tiro, false = parede fixa */
    bool ativo;
} Bloco;

typedef struct {
    Rectangle corpo;
    Vector2 direcao;
    bool ativo;
} Bala;

typedef struct {
    JogadorPuzzle jogador;
    Bloco blocos[MAX_BLOCOS];
    int numBlocos;
    Bala balas[MAX_BALAS];
    int balasRestantes;
    Rectangle saida;
} Puzzle;

/* ---------- Recorde ---------- */

static int CarregarRecorde(void)
{
    int recorde = 0;
    FILE *arquivo = fopen(ARQUIVO_RECORDE, "rb");
    if (arquivo != NULL) {
        if (fread(&recorde, sizeof(int), 1, arquivo) != 1) recorde = 0;
        fclose(arquivo);
    }
    return recorde;
}

static void SalvarRecorde(int recorde)
{
    FILE *arquivo = fopen(ARQUIVO_RECORDE, "wb");
    if (arquivo != NULL) {
        fwrite(&recorde, sizeof(int), 1, arquivo);
        fclose(arquivo);
    }
}

/* ---------- Lógica ---------- */

static void ReiniciarJogo(Jogo *j)
{
    j->jogador.corpo = (Rectangle){ JOG_X, CHAO_Y - JOG_ALTURA, JOG_LARGURA, JOG_ALTURA };
    j->jogador.velY = 0.0f;
    j->jogador.noChao = true;

    for (int i = 0; i < MAX_OBSTACULOS; i++) j->obstaculos[i].ativo = false;

    j->velocidade = VEL_INICIAL;
    j->tempoSpawn = 1.2f;
    j->pontos = 0.0f;
    j->deslocamentoChao = 0.0f;
}

static void CriarObstaculo(Jogo *j)
{
    for (int i = 0; i < MAX_OBSTACULOS; i++) {
        if (!j->obstaculos[i].ativo) {
            float largura = (float)GetRandomValue(24, 50);
            float altura = (float)GetRandomValue(40, 90);
            j->obstaculos[i].corpo = (Rectangle){ LARGURA + 20.0f, CHAO_Y - altura, largura, altura };
            j->obstaculos[i].ativo = true;
            j->obstaculos[i].contado = false;
            return;
        }
    }
}

/* Retorna o resultado do frame: colisão, transição para o modo puzzle, ou nada. */
static ResultadoCorrida AtualizarJogo(Jogo *j, float dt)
{
    Jogador *p = &j->jogador;

    bool pediuPulo = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
    if (pediuPulo && p->noChao) {
        p->velY = -FORCA_PULO;
        p->noChao = false;
    }

    p->velY += GRAVIDADE * dt;
    p->corpo.y += p->velY * dt;
    if (p->corpo.y >= CHAO_Y - p->corpo.height) {
        p->corpo.y = CHAO_Y - p->corpo.height;
        p->velY = 0.0f;
        p->noChao = true;
    }

    j->velocidade += ACELERACAO * dt;
    if (j->velocidade > VEL_MAXIMA) j->velocidade = VEL_MAXIMA;

    j->pontos += PONTOS_POR_SEG * dt;
    j->deslocamentoChao += j->velocidade * dt;
    if (j->deslocamentoChao >= 60.0f) j->deslocamentoChao -= 60.0f;

    j->tempoSpawn -= dt;
    if (j->tempoSpawn <= 0.0f) {
        CriarObstaculo(j);
        j->tempoSpawn = 1.0f + (float)GetRandomValue(0, 100) / 100.0f;
    }

    Rectangle hitbox = { p->corpo.x + 6, p->corpo.y + 6, p->corpo.width - 12, p->corpo.height - 8 };

    for (int i = 0; i < MAX_OBSTACULOS; i++) {
        Obstaculo *o = &j->obstaculos[i];
        if (!o->ativo) continue;

        o->corpo.x -= j->velocidade * dt;

        if (o->corpo.x + o->corpo.width < 0.0f) {
            o->ativo = false;
            continue;
        }

        if (!o->contado && o->corpo.x + o->corpo.width < p->corpo.x) {
            o->contado = true;
            j->pontos += BONUS_OBSTACULO;
        }

        if (CheckCollisionRecs(hitbox, o->corpo)) return RESULTADO_COLISAO;
    }

    if (j->pontos >= PONTOS_TRANSICAO) return RESULTADO_TRANSICAO;

    return RESULTADO_NADA;
}

/* ---------- Desenho ---------- */

static void TextoCentral(const char *texto, int y, int tamanho, Color cor)
{
    DrawText(texto, (LARGURA - MeasureText(texto, tamanho)) / 2, y, tamanho, cor);
}

static void DesenharCenario(float deslocamento)
{
    ClearBackground((Color){ 24, 24, 38, 255 });
    DrawRectangle(0, CHAO_Y, LARGURA, ALTURA - CHAO_Y, (Color){ 46, 46, 70, 255 });
    DrawRectangle(0, CHAO_Y, LARGURA, 3, RAYWHITE);

    int inicio = -(int)deslocamento;
    for (int x = inicio; x < LARGURA; x += 60) {
        DrawRectangle(x, CHAO_Y + 25, 24, 4, (Color){ 90, 90, 120, 255 });
    }
}

static void DesenharObjetos(const Jogo *j)
{
    for (int i = 0; i < MAX_OBSTACULOS; i++) {
        const Obstaculo *o = &j->obstaculos[i];
        if (!o->ativo) continue;
        DrawRectangleRec(o->corpo, (Color){ 230, 80, 80, 255 });
        DrawRectangleLinesEx(o->corpo, 2, MAROON);
    }

    const Rectangle *c = &j->jogador.corpo;
    DrawRectangleRec(*c, SKYBLUE);
    DrawRectangleLinesEx(*c, 2, BLUE);
    DrawRectangle((int)c->x + 24, (int)c->y + 10, 10, 10, WHITE);
    DrawRectangle((int)c->x + 29, (int)c->y + 13, 5, 5, BLACK);
}

static void DesenharHud(const Jogo *j, int recorde)
{
    DrawText(TextFormat("Pontos: %d", (int)j->pontos), 20, 16, 24, RAYWHITE);

    const char *textoRecorde = TextFormat("Recorde: %d", recorde);
    DrawText(textoRecorde, LARGURA - MeasureText(textoRecorde, 24) - 20, 16, 24, GOLD);
}

static void DesenharMenu(int opcao, int recorde)
{
    static const char *opcoes[NUM_OPCOES] = { "Jogar", "Sair" };

    DesenharCenario(0.0f);
    TextoCentral("MINIGAME DE OBSTÁCULOS", 70, 40, RAYWHITE);
    TextoCentral("Desvie de tudo o que aparecer", 125, 20, LIGHTGRAY);

    for (int i = 0; i < NUM_OPCOES; i++) {
        int y = 190 + i * 50;
        int x = (LARGURA - MeasureText(opcoes[i], 30)) / 2;
        Color cor = (i == opcao) ? YELLOW : GRAY;
        DrawText(opcoes[i], x, y, 30, cor);
        if (i == opcao) DrawText(">", x - 30, y, 30, YELLOW);
    }

    TextoCentral(TextFormat("Recorde: %d", recorde), 305, 22, GOLD);
    TextoCentral("Pular: ESPAÇO, W ou seta para cima", 395, 18, LIGHTGRAY);
}

static void DesenharFim(int pontuacao, int recorde, bool novoRecorde)
{
    DrawRectangle(0, 0, LARGURA, ALTURA, Fade(BLACK, 0.6f));
    TextoCentral("FIM DE JOGO", 100, 50, RAYWHITE);
    TextoCentral(TextFormat("Pontos: %d", pontuacao), 180, 30, RAYWHITE);

    if (novoRecorde) {
        TextoCentral("Novo recorde!", 225, 26, GOLD);
    } else {
        TextoCentral(TextFormat("Recorde: %d", recorde), 225, 26, LIGHTGRAY);
    }

    TextoCentral("ENTER: jogar de novo    ESC: menu", 300, 20, LIGHTGRAY);
}

/* ---------- Lógica do puzzle ---------- */

static void AdicionarBloco(Puzzle *p, float x, float y, float w, float h, bool quebravel)
{
    p->blocos[p->numBlocos].corpo = (Rectangle){ x, y, w, h };
    p->blocos[p->numBlocos].quebravel = quebravel;
    p->blocos[p->numBlocos].ativo = true;
    p->numBlocos++;
}

/* Monta a sala de cada fase: paredes fixas (quebravel=false) e blocos
   destrutíveis (quebravel=true) que só cedem a tiro. */
static void CarregarFase(int fase, Puzzle *p)
{
    p->numBlocos = 0;
    for (int i = 0; i < MAX_BALAS; i++) p->balas[i].ativo = false;
    p->balasRestantes = BALAS_POR_FASE;
    p->jogador.direcaoOlhar = (Vector2){ 1.0f, 0.0f };
    p->jogador.corpo = (Rectangle){ 60.0f, 150.0f, 30.0f, 30.0f };
    p->saida = (Rectangle){ 740.0f, 170.0f, 20.0f, 90.0f };

    switch (fase) {
    case 1:
        /* Parede bloqueia a passagem de cima; só há caminho por baixo.
           Um único bloco quebrável guarda o resto do caminho. */
        AdicionarBloco(p, 250, 90, 20, 180, false);
        AdicionarBloco(p, 500, 90, 40, 300, true);
        break;

    case 2:
        /* Zigue-zague: primeiro gap embaixo, depois em cima.
           Dois blocos quebráveis, com uma bala de folga. */
        AdicionarBloco(p, 220, 90, 20, 210, false);
        AdicionarBloco(p, 420, 180, 20, 210, false);
        AdicionarBloco(p, 470, 90, 40, 300, true);
        AdicionarBloco(p, 650, 90, 40, 300, true);
        break;

    case 3:
    default:
        /* Três desvios e três blocos: exatamente 3 balas para 3 alvos,
           sem margem para erro. */
        AdicionarBloco(p, 150, 90, 20, 210, false);
        AdicionarBloco(p, 190, 90, 40, 300, true);
        AdicionarBloco(p, 350, 180, 20, 210, false);
        AdicionarBloco(p, 390, 90, 40, 300, true);
        AdicionarBloco(p, 550, 90, 20, 210, false);
        AdicionarBloco(p, 590, 90, 40, 300, true);
        break;
    }
}

static bool ColideComSolidos(const Puzzle *p, Rectangle alvo)
{
    for (int i = 0; i < p->numBlocos; i++) {
        if (p->blocos[i].ativo && CheckCollisionRecs(alvo, p->blocos[i].corpo)) return true;
    }
    return false;
}

static ResultadoPuzzle AtualizarPuzzle(Puzzle *p, float dt)
{
    JogadorPuzzle *j = &p->jogador;
    Vector2 mov = { 0.0f, 0.0f };

    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) mov.x += 1.0f;
    if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) mov.x -= 1.0f;
    if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) mov.y += 1.0f;
    if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) mov.y -= 1.0f;

    if (mov.x != 0.0f || mov.y != 0.0f) {
        float comprimento = sqrtf(mov.x * mov.x + mov.y * mov.y);
        mov.x /= comprimento;
        mov.y /= comprimento;
        j->direcaoOlhar = mov; /* mira sempre na última direção andada */
    }

    Rectangle tentativaX = j->corpo;
    tentativaX.x += mov.x * PUZZLE_VEL * dt;
    if (tentativaX.x >= PUZZLE_AREA_X &&
        tentativaX.x + tentativaX.width <= PUZZLE_AREA_X + PUZZLE_AREA_LARGURA &&
        !ColideComSolidos(p, tentativaX)) {
        j->corpo.x = tentativaX.x;
    }

    Rectangle tentativaY = j->corpo;
    tentativaY.y += mov.y * PUZZLE_VEL * dt;
    if (tentativaY.y >= PUZZLE_AREA_Y &&
        tentativaY.y + tentativaY.height <= PUZZLE_AREA_Y + PUZZLE_AREA_ALTURA &&
        !ColideComSolidos(p, tentativaY)) {
        j->corpo.y = tentativaY.y;
    }

    if (IsKeyPressed(KEY_SPACE) && p->balasRestantes > 0) {
        for (int i = 0; i < MAX_BALAS; i++) {
            if (!p->balas[i].ativo) {
                float cx = j->corpo.x + j->corpo.width / 2.0f - 5.0f;
                float cy = j->corpo.y + j->corpo.height / 2.0f - 5.0f;
                p->balas[i].corpo = (Rectangle){ cx, cy, 10.0f, 10.0f };
                p->balas[i].direcao = j->direcaoOlhar;
                p->balas[i].ativo = true;
                p->balasRestantes--;
                break;
            }
        }
    }

    for (int i = 0; i < MAX_BALAS; i++) {
        Bala *b = &p->balas[i];
        if (!b->ativo) continue;

        b->corpo.x += b->direcao.x * BALA_VEL * dt;
        b->corpo.y += b->direcao.y * BALA_VEL * dt;

        if (b->corpo.x < PUZZLE_AREA_X || b->corpo.x > PUZZLE_AREA_X + PUZZLE_AREA_LARGURA ||
            b->corpo.y < PUZZLE_AREA_Y || b->corpo.y > PUZZLE_AREA_Y + PUZZLE_AREA_ALTURA) {
            b->ativo = false;
            continue;
        }

        for (int k = 0; k < p->numBlocos; k++) {
            Bloco *bloco = &p->blocos[k];
            if (!bloco->ativo) continue;
            if (CheckCollisionRecs(b->corpo, bloco->corpo)) {
                b->ativo = false;
                if (bloco->quebravel) bloco->ativo = false;
                break;
            }
        }
    }

    if (CheckCollisionRecs(j->corpo, p->saida)) return PUZZLE_SAIU;
    return PUZZLE_NADA;
}

/* ---------- Desenho do puzzle ---------- */

static void DesenharPuzzle(const Puzzle *p, int fase)
{
    ClearBackground((Color){ 18, 18, 30, 255 });
    DrawRectangleLines(PUZZLE_AREA_X, PUZZLE_AREA_Y, PUZZLE_AREA_LARGURA, PUZZLE_AREA_ALTURA, RAYWHITE);

    for (int i = 0; i < p->numBlocos; i++) {
        const Bloco *b = &p->blocos[i];
        if (!b->ativo) continue;
        Color cor = b->quebravel ? (Color){ 200, 140, 60, 255 } : (Color){ 90, 90, 110, 255 };
        DrawRectangleRec(b->corpo, cor);
        DrawRectangleLinesEx(b->corpo, 2, b->quebravel ? MAROON : BLACK);
    }

    DrawRectangleRec(p->saida, (Color){ 60, 200, 100, 255 });
    DrawText("SAIDA", (int)p->saida.x - 12, (int)p->saida.y - 22, 16, GREEN);

    for (int i = 0; i < MAX_BALAS; i++) {
        if (p->balas[i].ativo) DrawRectangleRec(p->balas[i].corpo, YELLOW);
    }

    const Rectangle *c = &p->jogador.corpo;
    DrawRectangleRec(*c, SKYBLUE);
    DrawRectangleLinesEx(*c, 2, BLUE);
    float cx = c->x + c->width / 2.0f;
    float cy = c->y + c->height / 2.0f;
    DrawLine((int)cx, (int)cy,
             (int)(cx + p->jogador.direcaoOlhar.x * 20.0f),
             (int)(cy + p->jogador.direcaoOlhar.y * 20.0f), WHITE);

    DrawText(TextFormat("Fase %d/%d", fase, TOTAL_FASES), 20, 16, 24, RAYWHITE);
    DrawText(TextFormat("Balas: %d", p->balasRestantes), LARGURA - 140, 16, 24, YELLOW);
    DrawText("WASD/setas: mover   ESPACO: atirar   R: reiniciar fase   ESC: menu",
             20, ALTURA - 26, 16, LIGHTGRAY);
}

/* ---------- Programa principal ---------- */

int main(void)
{
    InitWindow(LARGURA, ALTURA, "Minigame de Obstáculos");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    SetRandomSeed((unsigned int)time(NULL));

    Tela tela = TELA_MENU;
    Jogo jogo;
    ReiniciarJogo(&jogo);

    Puzzle puzzle;
    int faseAtual = 0;

    int opcao = 0;
    int recorde = CarregarRecorde();
    int pontuacaoFinal = 0;
    bool novoRecorde = false;
    bool sair = false;

    while (!sair && !WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;

        switch (tela) {
        case TELA_MENU:
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) opcao = (opcao + 1) % NUM_OPCOES;
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) opcao = (opcao + NUM_OPCOES - 1) % NUM_OPCOES;

            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                if (opcao == 0) {
                    ReiniciarJogo(&jogo);
                    tela = TELA_JOGO;
                } else {
                    sair = true;
                }
            }
            if (IsKeyPressed(KEY_ESCAPE)) sair = true;
            break;

        case TELA_JOGO: {
            if (IsKeyPressed(KEY_ESCAPE)) {
                tela = TELA_MENU;
                break;
            }
            ResultadoCorrida resultado = AtualizarJogo(&jogo, dt);
            if (resultado == RESULTADO_COLISAO) {
                pontuacaoFinal = (int)jogo.pontos;
                novoRecorde = pontuacaoFinal > recorde;
                if (novoRecorde) {
                    recorde = pontuacaoFinal;
                    SalvarRecorde(recorde);
                }
                tela = TELA_FIM;
            } else if (resultado == RESULTADO_TRANSICAO) {
                faseAtual = 1;
                CarregarFase(faseAtual, &puzzle);
                tela = TELA_PUZZLE;
            }
            break;
        }

        case TELA_FIM:
            if (IsKeyPressed(KEY_ENTER)) {
                ReiniciarJogo(&jogo);
                tela = TELA_JOGO;
            }
            if (IsKeyPressed(KEY_ESCAPE)) tela = TELA_MENU;
            break;

        case TELA_PUZZLE:
            if (IsKeyPressed(KEY_ESCAPE)) {
                tela = TELA_MENU;
                break;
            }
            if (IsKeyPressed(KEY_R)) {
                CarregarFase(faseAtual, &puzzle);
                break;
            }
            if (AtualizarPuzzle(&puzzle, dt) == PUZZLE_SAIU) {
                faseAtual++;
                if (faseAtual > TOTAL_FASES) {
                    tela = TELA_VITORIA;
                } else {
                    CarregarFase(faseAtual, &puzzle);
                }
            }
            break;

        case TELA_VITORIA:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
                tela = TELA_MENU;
            }
            break;
        }

        BeginDrawing();
        switch (tela) {
        case TELA_MENU:
            DesenharMenu(opcao, recorde);
            break;

        case TELA_JOGO:
            DesenharCenario(jogo.deslocamentoChao);
            DesenharObjetos(&jogo);
            DesenharHud(&jogo, recorde);
            break;

        case TELA_FIM:
            DesenharCenario(jogo.deslocamentoChao);
            DesenharObjetos(&jogo);
            DesenharHud(&jogo, recorde);
            DesenharFim(pontuacaoFinal, recorde, novoRecorde);
            break;

        case TELA_PUZZLE:
            DesenharPuzzle(&puzzle, faseAtual);
            break;

        case TELA_VITORIA:
            ClearBackground((Color){ 18, 18, 30, 255 });
            TextoCentral("VOCE COMPLETOU AS 3 FASES!", 180, 34, GOLD);
            TextoCentral("ENTER ou ESC: voltar ao menu", 240, 20, LIGHTGRAY);
            break;
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
