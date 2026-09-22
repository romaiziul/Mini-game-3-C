/*
 * Minigame de obstáculos para visual novel (raylib)
 *
 * Controles:
 *   Menu: W/S ou setas para navegar, ENTER ou ESPAÇO para confirmar, ESC para sair
 *   Jogo: ESPAÇO, W ou seta para cima para pular, ESC para voltar ao menu
 *   Fim:  ENTER para jogar de novo, ESC para voltar ao menu
 *
 * Compilação:
 *   Linux:   gcc minigame-1.c -o minigame -lraylib -lm -ldl -lpthread -lGL -lrt -lX11
 *   Windows: gcc minigame-1.c -o minigame.exe -lraylib -lglfw3 -lopengl32 -lgdi32 -lwinmm
 *   macOS:   gcc minigame-1.c -o minigame -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
 */

#include <raylib.h>
#include <stdbool.h>

#include <stdio.h>
#include <time.h>

#define LARGURA          800
#define ALTURA           450
#define CHAO_Y           360

#define JOG_X            100.0f
#define JOG_LARGURA      40.0f
#define JOG_ALTURA       50.0f

#define GRAVIDADE        2200.0f
#define FORCA_PULO       780.0f
#define VEL_INICIAL      300.0f
#define VEL_MAXIMA       720.0f
#define ACELERACAO       10.0f

#define MAX_OBSTACULOS   8
#define BONUS_OBSTACULO  25.0f
#define PONTOS_POR_SEG   10.0f

#define NUM_OPCOES       2
#define ARQUIVO_RECORDE  "recorde.dat"

typedef enum { TELA_MENU, TELA_JOGO, TELA_FIM } Tela;

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

/* Retorna true quando o jogador colide com um obstáculo. */
static bool AtualizarJogo(Jogo *j, float dt)
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

        if (CheckCollisionRecs(hitbox, o->corpo)) return true;
    }

    return false;
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

        case TELA_JOGO:
            if (IsKeyPressed(KEY_ESCAPE)) {
                tela = TELA_MENU;
                break;
            }
            if (AtualizarJogo(&jogo, dt)) {
                pontuacaoFinal = (int)jogo.pontos;
                novoRecorde = pontuacaoFinal > recorde;
                if (novoRecorde) {
                    recorde = pontuacaoFinal;
                    SalvarRecorde(recorde);
                }
                tela = TELA_FIM;
            }
            break;

        case TELA_FIM:
            if (IsKeyPressed(KEY_ENTER)) {
                ReiniciarJogo(&jogo);
                tela = TELA_JOGO;
            }
            if (IsKeyPressed(KEY_ESCAPE)) tela = TELA_MENU;
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
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
