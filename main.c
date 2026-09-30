#include "address_map_arm.h"
#include <stdbool.h>
#include <stdlib.h>
#include "sprites.h"

// -------- FUNÇÕES DO CÓDIGO EXEMPLO-----------------

/* function prototypes */
void video_text(int, int, char *);
void video_box(int, int, int, int, short);
int  resample_rgb(int, int);
int  get_data_bits(int);

#define STANDARD_X 320
#define STANDARD_Y 240
#define BACKGROUND_BLUE 0x2D3241
/* global variables */
int screen_x;
int screen_y;
int res_offset;
int col_offset;
int db;

// ---------------------------------------------------

/* Variaveis do jogo */

#define LARGURA_BLOCO 29
#define ALTURA_BLOCO 10

#define LARGURA_PLAYER_BASE 40
#define LARGURA_PLAYER_EXPANDIDA 55
#define LARGURA_PLAYER_REDUZIDA 30
#define ALTURA_PLAYER  8
#define Y_PLAYER       195
#define VEL_PLAYER     2
#define COR_PLAYER     0xFFFFFF
#define DURACAO_EXPANSAO 3000
#define DURACAO_RELOGIO 3000  // mesmo periodo do contador dos outros poderes
#define MULTIPLICADOR_DELAY_RELOGIO 2 // 2x o delay normal = jogo mais devagar

short player_x;
short LARGURA_PLAYER = LARGURA_PLAYER_BASE;
int contador_expansao = 0;
short tipo_efeito_player = 0; // 1 = expandido, 2 = reduzido
int contador_relogio = 0; // contador proprio do poder relogio (independente do contador_expansao)

typedef struct {
    short Type;
    short Pos_X;
    short Pos_Y;
    bool IsAlive;
    short Contador;
    int Cor;
}Bloco;

Bloco blocos[5][10];

typedef struct{
    short Type_mov;
    short Pos_X;
    short Pos_Y;
}Bola;

typedef struct {
    short Pos_X;
    short Pos_Y;
    bool  Ativo;
    short Type; // 0 = duplicar bola, 1 = aumentar player, 2 = diminuir player, 3 = relogio (deixa o jogo mais devagar)
} Poder;

typedef struct {
    short Pox_X;
    short Pos_Y;
    short Type;
}Poderes;

// Array dinamico de bolas
Bola *bolas = NULL;
int num_bolas = 0;

// Array dinamico de poderes
Poder *poderes = NULL;
int num_poderes = 0;

// Type do bloco indica qual poder ele solta ao ser destruido:
// 0 = bloco normal (nenhum poder), 1 = duplicar bola, 2 = aumentar player,
// 3 = diminuir player, 4 = relogio (deixa o jogo mais devagar)
#define BLOCO_TIPO_NORMAL          0
#define BLOCO_TIPO_DUPLICAR_BOLA   1
#define BLOCO_TIPO_AUMENTAR_PLAYER 2
#define BLOCO_TIPO_DIMINUIR_PLAYER 3
#define BLOCO_TIPO_RELOGIO         4

// -------- MAQUINA DE ESTADOS DO JOGO -----------------
// MENU: tela inicial esperando KEY0
// LEVEL1: fase 1, grade de blocos padrao (cheia)
// LEVEL2: fase 2, blocos no formato de triangulo retangulo
// GAMEOVER: todas as bolas cairam
// YOUWIN: score atingiu o alvo da fase (libera a proxima fase ou vitoria final)
typedef enum {
    ESTADO_MENU,
    ESTADO_LEVEL1,
    ESTADO_LEVEL2,
    ESTADO_GAMEOVER,
    ESTADO_YOUWIN
} EstadoJogo;

#define SCORE_ALVO_LEVEL1 500
#define SCORE_ALVO_LEVEL2 300

// ---------------------------------------------------

// Do stackoverflow.
// Muda dinâmicamente a cor do bloco dependendo da fila
int AtualizarCor(short Pos_Y){
    short G = Pos_Y + 20;
    short B = Pos_Y + 30;
    return ((Pos_Y & 0xff) << 16) + ((G & 0xff) << 8) + (B & 0xff);
}

// Monta a matriz de blocos da fase 1 (grade cheia), exatamente como no
// jogo original: todos os blocos 5x10 nascem vivos.
void EstadoInicial(){
    //Lógica dos blocos, vão ser 5*10 blocos
    short x =10; short y = 10;
    int i = 0;
    int j = 0;
    for(i=0; i<5; i++){
        for(j=0; j<10; j++){
            blocos[i][j].Type = 0;
            blocos[i][j].Pos_X = x + (j * (LARGURA_BLOCO+1));
            blocos[i][j].Pos_Y = y + (i * (ALTURA_BLOCO +1));
            blocos[i][j].IsAlive = true;
            blocos[i][j].Cor = AtualizarCor(blocos[i][j].Pos_Y);
            //desenhar bloco
            video_box(blocos[i][j].Pos_X, blocos[i][j].Pos_Y, blocos[i][j].Pos_X + LARGURA_BLOCO - 1, blocos[i][j].Pos_Y + ALTURA_BLOCO - 1, blocos[i][j].Cor);
        }
    }

    // Marca os blocos especiais que soltam poder ao serem destruidos
    blocos[2][4].Type = BLOCO_TIPO_DUPLICAR_BOLA;
    blocos[2][5].Type = BLOCO_TIPO_AUMENTAR_PLAYER;
    blocos[2][6].Type = BLOCO_TIPO_DIMINUIR_PLAYER;
    blocos[2][7].Type = BLOCO_TIPO_RELOGIO;
}

// Monta a matriz de blocos da fase 2: mesma matriz 5x10 da fase 1, mas
// so fica vivo o triangulo retangulo (angulo reto no canto inferior
// esquerdo), crescendo 2 colunas vivas por linha (linha 0 -> 2 colunas,
// linha 4 -> 10 colunas). Blocos fora do triangulo nascem mortos e nao
// sao desenhados. Redundante de proposito com EstadoInicial(), so pra
// fase 2 ter sua propria logica isolada e facil de mexer sem afetar a fase 1.
void EstadoInicialLevel2(){
    //Lógica dos blocos, vão ser 5*10 blocos (mesma grade da fase 1)
    short x =10; short y = 10;
    int i = 0;
    int j = 0;
    for(i=0; i<5; i++){
        short colunas_vivas_na_linha = 2 * (i + 1); // linha 0 -> 2, linha 4 -> 10
        for(j=0; j<10; j++){
            blocos[i][j].Type = 0;
            blocos[i][j].Pos_X = x + (j * (LARGURA_BLOCO+1));
            blocos[i][j].Pos_Y = y + (i * (ALTURA_BLOCO +1));
            blocos[i][j].IsAlive = (j < colunas_vivas_na_linha);
            blocos[i][j].Cor = AtualizarCor(blocos[i][j].Pos_Y);
            //desenhar bloco (so desenha se estiver vivo, formando o triangulo)
            if (blocos[i][j].IsAlive){
                video_box(blocos[i][j].Pos_X, blocos[i][j].Pos_Y, blocos[i][j].Pos_X + LARGURA_BLOCO - 1, blocos[i][j].Pos_Y + ALTURA_BLOCO - 1, blocos[i][j].Cor);
            }
        }
    }

    // Marca os blocos especiais que soltam poder ao serem destruidos
    // (apenas se o bloco realmente nasceu vivo dentro do triangulo)
    if (blocos[2][4].IsAlive) blocos[2][4].Type = BLOCO_TIPO_DUPLICAR_BOLA;
    if (blocos[2][5].IsAlive) blocos[2][5].Type = BLOCO_TIPO_AUMENTAR_PLAYER;
    if (blocos[2][6].IsAlive) blocos[2][6].Type = BLOCO_TIPO_DIMINUIR_PLAYER;
    if (blocos[2][7].IsAlive) blocos[2][7].Type = BLOCO_TIPO_RELOGIO;
}

int score = 0;

//Realiza a atualização do score
void AtualizarScore(){
    char texto[5];
    texto[0] = '0' + (score / 1000) % 10;
    texto[1] = '0' + (score / 100)  % 10;
    texto[2] = '0' + (score / 10)   % 10;
    texto[3] = '0' + (score)        % 10;
    texto[4] = '\0';
    video_text(20, 55, texto);
}

//Aplica um delay, para melhor jogatina
void delay(int delay){
    volatile int i = 0;
    for(i = 0; i < delay; i++);
}

// Inicializa o array dinamico com uma unica bola
void inicializar_bolas() {
    num_bolas = 1;
    bolas = (Bola *)malloc(sizeof(Bola));
    bolas[0].Pos_X = 155;
    bolas[0].Pos_Y = Y_PLAYER - 15;
    bolas[0].Type_mov = 0;
}

// Retorna o tipo de movimento espelhado horizontalmente, para em caso de duplicação da bola, ter 2 bolas em direções espelhadas
short espelho_horizontal(short tipo) {
    if (tipo == 2){ 
        return 3; 
    }
    if (tipo == 3){ 
        return 2; 
    }
    if (tipo == 4){ 
        return 5; 
    }
    if (tipo == 5){ 
        return 4; 
    }
    return tipo; // 0 e 1 nao mudam (movimento vertical puro)
}

// Duplica todas as bolas antigos, cada nova bola sai na mesma posicao em direcao oposta
void duplicar_bolas() {
    bolas = (Bola *)realloc(bolas, sizeof(Bola) * num_bolas * 2);
    int i = 0;
    for (i = 0; i < num_bolas; i++) {
        bolas[num_bolas + i].Pos_X    = bolas[i].Pos_X;
        bolas[num_bolas + i].Pos_Y    = bolas[i].Pos_Y;
        bolas[num_bolas + i].Type_mov = espelho_horizontal(bolas[i].Type_mov);
    }
    num_bolas *= 2;
}

// Adiciona um novo poder ao array dinamico na posicao indicada
void adicionar_poder(short x, short y, short tipo) {
    num_poderes++;
    poderes = (Poder *)realloc(poderes, sizeof(Poder) * num_poderes);
    poderes[num_poderes - 1].Pos_X = x;
    poderes[num_poderes - 1].Pos_Y = y;
    poderes[num_poderes - 1].Ativo = true;
    poderes[num_poderes - 1].Type  = tipo;
}

// Redesenha qualquer bloco vivo cuja area se sobreponha ao retangulo indicado.
// Usado para corrigir o bug do poder apagando pedaco de bloco por baixo dele:
// como os poderes sao apagados com um video_box "cego" na cor de fundo, se um
// bloco ainda vivo estiver por baixo dessa area, ele fica com um "furo".
// Chamando isso logo apos apagar a area do poder, qualquer bloco vivo atingido
// e redesenhado por completo, corrigindo o furo.
void redesenhar_blocos(int x1, int y1, int x2, int y2, int db_local) {
    int i = 0;
    int j = 0;
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 10; j++) {
            if (!blocos[i][j].IsAlive) {
                continue;
            }
            int bx1 = blocos[i][j].Pos_X;
            int by1 = blocos[i][j].Pos_Y;
            int bx2 = bx1 + LARGURA_BLOCO - 1;
            int by2 = by1 + ALTURA_BLOCO - 1;
            // Verifica sobreposicao entre o retangulo apagado e o bloco
            if (x2 >= bx1 && x1 <= bx2 && y2 >= by1 && y1 <= by2) {
                video_box(bx1, by1, bx2, by2, blocos[i][j].Cor);
            }
        }
    }
}

// Remove um poder do array compactando os elementos restantes
void remover_poder(int idx, short background_color) {
    // apaga da tela
    video_box(poderes[idx].Pos_X, poderes[idx].Pos_Y,
              poderes[idx].Pos_X + 7, poderes[idx].Pos_Y + 7,
              background_color);
    // compacta o array removendo o elemento idx
    int i = 0;
    for (i = idx; i < num_poderes - 1; i++) {
        poderes[i] = poderes[i + 1];
    }
    num_poderes--;
    if (num_poderes > 0) {
        poderes = (Poder *)realloc(poderes, sizeof(Poder) * num_poderes);
    }
    else {
        free(poderes);
        poderes = NULL;
    }
}

// Atualiza posicao de todos os poderes, verifica colisao com player e saida de tela
void atualizar_poderes(short background_color) {
    int i = 0;
    for (i = num_poderes - 1; i >= 0; i--) {
        // apaga posicao antiga
        video_box(poderes[i].Pos_X, poderes[i].Pos_Y,
                  poderes[i].Pos_X + 7, poderes[i].Pos_Y + 7,
                  background_color);
        // elimina o bug de apagar pedaco de bloco que estava por baixo do poder
        redesenhar_blocos(poderes[i].Pos_X, poderes[i].Pos_Y,
                                    poderes[i].Pos_X + 7, poderes[i].Pos_Y + 7, db);

        // move para baixo
        poderes[i].Pos_Y += 1;

        // saiu da tela sem ser coletado: remove
        if (poderes[i].Pos_Y > 210) {
            remover_poder(i, background_color);
            continue;
        }

        // colidiu com o player: aplica efeito do poder e remove
        if ((poderes[i].Pos_Y + 7 >= Y_PLAYER) &&
            (poderes[i].Pos_Y <= Y_PLAYER + ALTURA_PLAYER) &&
            (poderes[i].Pos_X + 7 >= player_x) &&
            (poderes[i].Pos_X <= player_x + LARGURA_PLAYER)) {
            if (poderes[i].Type == 0) {
                duplicar_bolas();
            }
            if (poderes[i].Type == 1) {
                LARGURA_PLAYER = LARGURA_PLAYER_EXPANDIDA;
                contador_expansao = DURACAO_EXPANSAO;
                tipo_efeito_player = 1;
                video_box(player_x, Y_PLAYER, player_x + LARGURA_PLAYER - 1, Y_PLAYER + ALTURA_PLAYER - 1, resample_rgb(db, COR_PLAYER));
            }
            if (poderes[i].Type == 2) {
                // apaga o player no tamanho atual antes de reduzir
                video_box(player_x, Y_PLAYER, player_x + LARGURA_PLAYER - 1, Y_PLAYER + ALTURA_PLAYER - 1, background_color);
                LARGURA_PLAYER = LARGURA_PLAYER_REDUZIDA;
                contador_expansao = DURACAO_EXPANSAO;
                tipo_efeito_player = 2;
                video_box(player_x, Y_PLAYER, player_x + LARGURA_PLAYER - 1, Y_PLAYER + ALTURA_PLAYER - 1, resample_rgb(db, COR_PLAYER));
            }
            if (poderes[i].Type == 3) {
                // ativa o relogio com seu proprio contador, independente do contador_expansao
                contador_relogio = DURACAO_RELOGIO;
            }
            remover_poder(i, background_color);
            continue;
        }

        // desenha sprite na nova posicao, de acordo com o Type
        if (poderes[i].Type == 0) {
            draw_sprite(poderes[i].Pos_X, poderes[i].Pos_Y, sprite_poder, 8, 8);
        }
        if (poderes[i].Type == 1) {
            draw_sprite(poderes[i].Pos_X, poderes[i].Pos_Y, sprite_poder_largura, 8, 8);
        }
        if (poderes[i].Type == 2) {
            draw_sprite(poderes[i].Pos_X, poderes[i].Pos_Y, sprite_poder_reduzir, 8, 8);
        }
        if (poderes[i].Type == 3) {
            draw_sprite(poderes[i].Pos_X, poderes[i].Pos_Y, sprite_poder_relogio, 8, 8);
        }
    }
}

// Espera o usuario soltar a tecla indicada antes de prosseguir.
// menu e o inicio do jogo enquanto o botao continua pressionado.
void aguardar_soltar_tecla(volatile int *KEY_ptr, int mascara) {
    while (*KEY_ptr & mascara);
}

void TelaInicial(){
    volatile int *KEY_ptr = (int *)KEY_BASE;
    draw_background(0, 0, palmeiras, 320, 240);
    char text_top_row[40]    = "Jogo breakout\0";
    char text_bottom_row[40] = "Aperte Key0 para start\0";
    video_text(35, 29, text_top_row);
    video_text(32, 30, text_bottom_row);
    while(1){
        int botoes = *KEY_ptr;
        if (botoes & 0x1) {
            return; // KEY0 = direita
        }
        if (botoes & 0x4) {
            // KEY2 = reset tambem sai do menu (evita travar no menu);
            // espera soltar antes de sair, evitando piscar entre estados
            aguardar_soltar_tecla(KEY_ptr, 0x4);
            return;
        }
    }
}

// Limpa a tela de texto (linhas usadas pelas telas de menu/gameover/vitoria)
// Tambem limpa o texto do score (SCORE + placar), que fica numa camada de
// caracteres separada da tela de pixels e por isso nao some sozinho quando
// o fundo e redesenhado. Assim o score so aparece durante o jogo.
void LimparTextoTela(){
    char text_top_row[40]    = "             \0";
    char text_bottom_row[40] = "                      \0";
    char text_score_clear[6] = "     \0";
    char text_pont_clear[5]  = "    \0";
    video_text(35, 29, text_top_row);
    video_text(32, 30, text_bottom_row);
    video_text(5, 55, text_score_clear);
    video_text(20, 55, text_pont_clear);
}

/*
Mata as bolas anteriores e os poderes, seta uma bola na posição original
seta o jogador no centro, e monta a fase indicada (LEVEL1 ou LEVEL2)
*/
void Reset(EstadoJogo fase){
    score = 0;
    AtualizarScore();

    video_box(10, 10, 309, 209, resample_rgb(db, BACKGROUND_BLUE));

    player_x = 135;
    LARGURA_PLAYER = LARGURA_PLAYER_BASE;

    contador_expansao = 0;
    contador_relogio = 0;

    if (bolas) free(bolas);
    bolas = NULL;

    if (poderes) free(poderes);
    poderes = NULL;
    num_poderes = 0;

    // Escolhe qual matriz de blocos montar de acordo com a fase
    if (fase == ESTADO_LEVEL2) {
        EstadoInicialLevel2();
    }
    else {
        EstadoInicial();
    }
    inicializar_bolas();

    video_box(player_x, Y_PLAYER, player_x + LARGURA_PLAYER - 1,
              Y_PLAYER + ALTURA_PLAYER - 1, resample_rgb(db, COR_PLAYER));
}

// -------- FUNÇÕES DO CÓDIGO EXEMPLO-----------------

/*******************************************************************************
    Partes dessas linhas de código foram aproveitadas do exemplo fornecido pelo programa
    Não esquecer: O quadrado de fundo tem uma resolução de 600*400
    Tem um padding de 20px

 ******************************************************************************/
int main(void) {
    volatile int * video_resolution = (int *)(PIXEL_BUF_CTRL_BASE + 0x8);
    screen_x                        = *video_resolution & 0xFFFF;
    screen_y                        = (*video_resolution >> 16) & 0xFFFF;

    volatile int * rgb_status = (int *)(RGB_RESAMPLER_BASE);
    db = get_data_bits(*rgb_status & 0x3F);

    /* check if resolution is smaller than the standard 320 x 240 */
    res_offset = (screen_x == 160) ? 1 : 0;

    /* check if number of data bits is less than the standard 16-bits */
    col_offset = (db == 8) ? 1 : 0;

    volatile int *KEY_ptr = (int *)KEY_BASE;

    /* update color */
    short background_color = resample_rgb(db, BACKGROUND_BLUE); //faz a compressão se for necessário

    char text_top_row[40]    = "";
    char text_bottom_row[40] = "";
    char text_score[40]    = "SCORE\0";
    char text_pontoacao[40] = "0000\0";

    // Estado inicial da maquina de estados: comeca no MENU
    EstadoJogo estado = ESTADO_MENU;

    // Loop principal da maquina de estados do jogo
    while (1) {
        switch (estado) {

            // ---------------- MENU ----------------
            case ESTADO_MENU: {
                TelaInicial();
                LimparTextoTela();
                estado = ESTADO_LEVEL1;
                break;
            }

            // ---------------- LEVEL1 (fase padrao, grade cheia) ----------------
            case ESTADO_LEVEL1: {
                video_text(5, 55, text_score);
                video_text(20, 55, text_pontoacao);
                video_box(0, 0, STANDARD_X - 1, STANDARD_Y - 1, 0); // fundo preto
                video_box(10, 10, 310 - 1, 210 - 1, background_color);

                Reset(ESTADO_LEVEL1);

                bool fase_ativa = true;
                while (fase_ativa) {
                    int botoes = *KEY_ptr;

                    // Apaga player só se vai mover
                    short novo_x = player_x;

                    if (botoes & 0x1) {
                        novo_x += VEL_PLAYER; // KEY0 = direita
                    }
                    if (botoes & 0x2) {
                        novo_x -= VEL_PLAYER; // KEY1 = esquerda
                    }

                    //RESET: volta para o MENU e restaura tudo
                    if (botoes & 0x04) {
                        // espera soltar o botao antes de trocar de estado,
                        // evita ficar piscando entre jogo e menu
                        aguardar_soltar_tecla(KEY_ptr, 0x04);
                        LimparTextoTela();
                        estado = ESTADO_MENU;
                        fase_ativa = false;
                        break;
                    }

                    if (novo_x < 10){
                        novo_x = 10;
                    }
                    if (novo_x > 310 - LARGURA_PLAYER){
                        novo_x = 310 - LARGURA_PLAYER;
                    }

                    // Só redesenha se moveu
                    if (novo_x != player_x) {
                        // Apaga só a fatia que ficou pra trás
                        if (novo_x > player_x) {
                            // moveu direita: apaga a esquerda
                            video_box(player_x, Y_PLAYER, novo_x - 1, Y_PLAYER + ALTURA_PLAYER - 1, resample_rgb(db, BACKGROUND_BLUE));
                        }
                        else {
                            // moveu esquerda: apaga a direita
                            video_box(novo_x + LARGURA_PLAYER, Y_PLAYER, player_x + LARGURA_PLAYER - 1, Y_PLAYER + ALTURA_PLAYER - 1, resample_rgb(db, BACKGROUND_BLUE));
                        }
                        player_x = novo_x;
                        video_box(player_x, Y_PLAYER, player_x + LARGURA_PLAYER - 1, Y_PLAYER + ALTURA_PLAYER - 1, resample_rgb(db, COR_PLAYER));
                    }

                    bool *bola_morreu = (bool *)malloc(sizeof(bool) * num_bolas);
                    int bb = 0;
                    for (bb = 0; bb < num_bolas; bb++) {
                        bola_morreu[bb] = false;
                    }

                    int b = 0;
                    // Loop sobre todas as bolas ativas
                    for (b = 0; b < num_bolas; b++) {

                        // Apaga a bola na posicao antiga (tamanho quadrado de 4x4 pixels)
                        video_box(bolas[b].Pos_X, bolas[b].Pos_Y, bolas[b].Pos_X + 3, bolas[b].Pos_Y + 3, resample_rgb(db, BACKGROUND_BLUE));

                        // Movimentacao da bola dependendo do estado atual do Type_mov
                        if (bolas[b].Type_mov == 0){ 
                            bolas[b].Pos_Y += 1; 
                        }
                        if (bolas[b].Type_mov == 1){ 
                            bolas[b].Pos_Y -= 1; 
                        }
                        if (bolas[b].Type_mov == 2){ 
                            bolas[b].Pos_X -= 1; 
                            bolas[b].Pos_Y -= 1; 
                        }
                        if (bolas[b].Type_mov == 3){ 
                            bolas[b].Pos_X += 1; 
                            bolas[b].Pos_Y -= 1; 
                        }
                        if (bolas[b].Type_mov == 4){ 
                            bolas[b].Pos_X -= 1; 
                            bolas[b].Pos_Y += 1; 
                        }
                        if (bolas[b].Type_mov == 5){ 
                            bolas[b].Pos_X += 1; 
                            bolas[b].Pos_Y += 1; 
                        }

                        // Colisao simples com as paredes do cenario
                        if (bolas[b].Pos_X < 10) {
                            bolas[b].Pos_X = 10;
                            if (bolas[b].Type_mov == 2) { bolas[b].Type_mov = 3; }
                            if (bolas[b].Type_mov == 4) { bolas[b].Type_mov = 5; }
                        }
                        if (bolas[b].Pos_X > 306){
                            bolas[b].Pos_X = 306;
                            if (bolas[b].Type_mov == 3){ 
                                bolas[b].Type_mov = 2; 
                            }
                            if (bolas[b].Type_mov == 5){ 
                                bolas[b].Type_mov = 4; 
                            }
                        }
                        if (bolas[b].Pos_Y < 10){
                            bolas[b].Pos_Y = 10;
                            if (bolas[b].Type_mov == 1){ 
                                bolas[b].Type_mov = 0; 
                            }
                            if (bolas[b].Type_mov == 2){ 
                                bolas[b].Type_mov = 4; 
                            }
                            if (bolas[b].Type_mov == 3){ 
                                bolas[b].Type_mov = 5; 
                            }
                        }

                        // Se cair no fundo do mapa, apenas marca para remocao depois
                        if (bolas[b].Pos_Y > 205) {
                            bola_morreu[b] = true;
                            continue;
                        }

                        // Colisao com o player dividido em 3 partes (esquerda, meio, direita)
                        if ((bolas[b].Pos_Y + 3 >= Y_PLAYER - 1) && (bolas[b].Pos_Y <= Y_PLAYER + ALTURA_PLAYER - 1)) {
                            if ((bolas[b].Pos_X + 3 >= player_x) && (bolas[b].Pos_X <= player_x + LARGURA_PLAYER)) {
                                if (bolas[b].Pos_X < player_x + 14) {
                                    bolas[b].Type_mov = 2;
                                }
                                else if (bolas[b].Pos_X <= player_x + 26) {
                                    bolas[b].Type_mov = 1;
                                }
                                else {
                                    bolas[b].Type_mov = 3;
                                }
                            }
                        }

                        // Mapeamento matematico em Hash direto na matriz de blocos sem usar loops
                        short teste_x = bolas[b].Pos_X;
                        short teste_y = bolas[b].Pos_Y;

                        if (bolas[b].Type_mov == 0){ 
                            teste_x = bolas[b].Pos_X + 1; 
                            teste_y = bolas[b].Pos_Y + 3; 
                        }
                        if (bolas[b].Type_mov == 1){ 
                            teste_x = bolas[b].Pos_X + 1; 
                            teste_y = bolas[b].Pos_Y;     
                        }
                        if (bolas[b].Type_mov == 2){ 
                            teste_x = bolas[b].Pos_X;     
                            teste_y = bolas[b].Pos_Y;     
                        }
                        if (bolas[b].Type_mov == 3){ 
                            teste_x = bolas[b].Pos_X + 3; 
                            teste_y = bolas[b].Pos_Y;     
                        }
                        if (bolas[b].Type_mov == 4){ 
                            teste_x = bolas[b].Pos_X;     
                            teste_y = bolas[b].Pos_Y + 3; 
                        }
                        if (bolas[b].Type_mov == 5){ 
                            teste_x = bolas[b].Pos_X + 3; 
                            teste_y = bolas[b].Pos_Y + 3; 
                        }

                        int col_bloco = (teste_x - 10) / 30;
                        int row_bloco = (teste_y - 10) / 11;

                        if ((row_bloco >= 0) && (row_bloco < 5)) {
                            if ((col_bloco >= 0) && (col_bloco < 10)) {
                                // Só considera a colisão se a bola estiver perto o suficiente do bloco
                                bool perto_o_suficiente =
                                    (bolas[b].Pos_X + 3 >= blocos[row_bloco][col_bloco].Pos_X) &&
                                    (bolas[b].Pos_X     <= blocos[row_bloco][col_bloco].Pos_X + LARGURA_BLOCO - 1) &&
                                    (bolas[b].Pos_Y + 3 >= blocos[row_bloco][col_bloco].Pos_Y) &&
                                    (bolas[b].Pos_Y     <= blocos[row_bloco][col_bloco].Pos_Y + ALTURA_BLOCO - 1);

                                if (perto_o_suficiente && (blocos[row_bloco][col_bloco].IsAlive == true)) {
                                    blocos[row_bloco][col_bloco].IsAlive = false;
                                    video_box(blocos[row_bloco][col_bloco].Pos_X, blocos[row_bloco][col_bloco].Pos_Y, blocos[row_bloco][col_bloco].Pos_X + LARGURA_BLOCO - 1, blocos[row_bloco][col_bloco].Pos_Y + ALTURA_BLOCO - 1, background_color);
                                    score += 10;
                                    AtualizarScore();

                                    // Verifica se o bloco destruido solta poder, de acordo com o seu Type
                                    if (blocos[row_bloco][col_bloco].Type == BLOCO_TIPO_DUPLICAR_BOLA) {
                                        adicionar_poder(blocos[row_bloco][col_bloco].Pos_X + LARGURA_BLOCO / 2,
                                                        blocos[row_bloco][col_bloco].Pos_Y, 0);
                                    }
                                    if (blocos[row_bloco][col_bloco].Type == BLOCO_TIPO_AUMENTAR_PLAYER) {
                                        adicionar_poder(blocos[row_bloco][col_bloco].Pos_X + LARGURA_BLOCO / 2,
                                                        blocos[row_bloco][col_bloco].Pos_Y, 1);
                                    }
                                    if (blocos[row_bloco][col_bloco].Type == BLOCO_TIPO_DIMINUIR_PLAYER) {
                                        adicionar_poder(blocos[row_bloco][col_bloco].Pos_X + LARGURA_BLOCO / 2,
                                                        blocos[row_bloco][col_bloco].Pos_Y, 2);
                                    }
                                    if (blocos[row_bloco][col_bloco].Type == BLOCO_TIPO_RELOGIO) {
                                        adicionar_poder(blocos[row_bloco][col_bloco].Pos_X + LARGURA_BLOCO / 2,
                                                        blocos[row_bloco][col_bloco].Pos_Y, 3);
                                    }

                                    // Rebate a bola ao destruir bloco
                                    if (bolas[b].Type_mov == 0){ 
                                        bolas[b].Type_mov = 1; 
                                    }
                                    else if (bolas[b].Type_mov == 1){ 
                                        bolas[b].Type_mov = 0; 
                                    }
                                    else if (bolas[b].Type_mov == 2){ 
                                        bolas[b].Type_mov = 4; 
                                    }
                                    else if (bolas[b].Type_mov == 3){ 
                                        bolas[b].Type_mov = 5; 
                                    }
                                    else if (bolas[b].Type_mov == 4){ 
                                        bolas[b].Type_mov = 2; 
                                    }
                                    else if (bolas[b].Type_mov == 5){ 
                                        bolas[b].Type_mov = 3; 
                                    }
                                }
                            }
                        }

                        // Desenha a bola na nova posicao
                        video_box(bolas[b].Pos_X, bolas[b].Pos_Y, bolas[b].Pos_X + 3, bolas[b].Pos_Y + 3, resample_rgb(db, COR_PLAYER));
                    }

                    // Agora que o loop de logica das bolas terminou por completo,
                    // compacta o array UMA UNICA VEZ removendo as bolas marcadas.
                    // Isso evita qualquer inconsistencia de indices/num_bolas
                    // durante o processamento das outras bolas ainda vivas.
                    {
                        int write_idx = 0;
                        int read_idx = 0;
                        for (read_idx = 0; read_idx < num_bolas; read_idx++) {
                            if (!bola_morreu[read_idx]) {
                                if (write_idx != read_idx) {
                                    bolas[write_idx] = bolas[read_idx];
                                }
                                write_idx++;
                            }
                        }
                        num_bolas = write_idx;
                    }
                    free(bola_morreu);

                    if (num_bolas == 0) {
                        free(bolas);
                        bolas = NULL;
                        // Nenhuma bola restante: game over
                        estado = ESTADO_GAMEOVER;
                        fase_ativa = false;
                    }

                    if (!fase_ativa) {
                        break;
                    }

                    // Atualiza todos os poderes (movimento, colisao com player, saida de tela)
                    atualizar_poderes(background_color);

                    // Controla a duracao do efeito ativo na largura do player (expandido ou reduzido)
                    if (contador_expansao > 0){
                        contador_expansao--;
                        if (contador_expansao == 0){
                            // apaga o player no tamanho atual e redesenha no tamanho normal
                            video_box(player_x, Y_PLAYER, player_x + LARGURA_PLAYER - 1, Y_PLAYER + ALTURA_PLAYER - 1, resample_rgb(db, BACKGROUND_BLUE));
                            LARGURA_PLAYER = LARGURA_PLAYER_BASE;
                            tipo_efeito_player = 0;
                            if (player_x > 310 - LARGURA_PLAYER) {
                                player_x = 310 - LARGURA_PLAYER;
                            }
                            video_box(player_x, Y_PLAYER, player_x + LARGURA_PLAYER - 1, Y_PLAYER + ALTURA_PLAYER - 1, resample_rgb(db, COR_PLAYER));
                        }
                    }

                    // Passou do score alvo da fase 1: avanca para a fase 2
                    if (score >= SCORE_ALVO_LEVEL1) {
                        estado = ESTADO_LEVEL2;
                        fase_ativa = false;
                        break;
                    }

                    // Controla a duracao do efeito do relogio (contador proprio, independente do contador_expansao)
                    if (contador_relogio > 0) {
                        contador_relogio--;
                    }

                    // Se o relogio estiver ativo, multiplica o delay para deixar o jogo mais devagar
                    if (contador_relogio > 0) {
                        delay(50000 * MULTIPLICADOR_DELAY_RELOGIO);
                    }
                    else {
                        delay(40000); // delay para controlar a velocidade do jogo
                    }
                }
                break;
            }

            // ---------------- LEVEL2 (blocos em triangulo retangulo) ----------------
            case ESTADO_LEVEL2: {
                video_text(5, 55, text_score);
                video_text(20, 55, text_pontoacao);
                video_box(0, 0, STANDARD_X - 1, STANDARD_Y - 1, 0); // fundo preto
                video_box(10, 10, 310 - 1, 210 - 1, background_color);

                // Mantem o score acumulado da fase 1, so reseta bolas/poderes/player
                // e monta a matriz de blocos no formato triangular
                int score_ao_entrar = score;
                Reset(ESTADO_LEVEL2);
                score = score_ao_entrar;
                AtualizarScore();

                bool fase_ativa = true;
                while (fase_ativa) {
                    int botoes = *KEY_ptr;

                    short novo_x = player_x;

                    if (botoes & 0x1) {
                        novo_x += VEL_PLAYER; // KEY0 = direita
                    }
                    if (botoes & 0x2) {
                        novo_x -= VEL_PLAYER; // KEY1 = esquerda
                    }

                    //RESET: volta para o MENU e restaura tudo
                    if (botoes & 0x04) {
                        // espera soltar o botao antes de trocar de estado,
                        // evita ficar piscando entre jogo e menu
                        aguardar_soltar_tecla(KEY_ptr, 0x04);
                        LimparTextoTela();
                        estado = ESTADO_MENU;
                        fase_ativa = false;
                        break;
                    }

                    if (novo_x < 10){
                        novo_x = 10;
                    }
                    if (novo_x > 310 - LARGURA_PLAYER){
                        novo_x = 310 - LARGURA_PLAYER;
                    }

                    if (novo_x != player_x) {
                        if (novo_x > player_x) {
                            video_box(player_x, Y_PLAYER, novo_x - 1, Y_PLAYER + ALTURA_PLAYER - 1, resample_rgb(db, BACKGROUND_BLUE));
                        }
                        else {
                            video_box(novo_x + LARGURA_PLAYER, Y_PLAYER, player_x + LARGURA_PLAYER - 1, Y_PLAYER + ALTURA_PLAYER - 1, resample_rgb(db, BACKGROUND_BLUE));
                        }
                        player_x = novo_x;
                        video_box(player_x, Y_PLAYER, player_x + LARGURA_PLAYER - 1, Y_PLAYER + ALTURA_PLAYER - 1, resample_rgb(db, COR_PLAYER));
                    }
                    bool *bola_morreu = (bool *)malloc(sizeof(bool) * num_bolas);
                    int bb = 0;
                    for (bb = 0; bb < num_bolas; bb++) {
                        bola_morreu[bb] = false;
                    }

                    int b = 0;
                    for (b = 0; b < num_bolas; b++) {

                        video_box(bolas[b].Pos_X, bolas[b].Pos_Y, bolas[b].Pos_X + 3, bolas[b].Pos_Y + 3, resample_rgb(db, BACKGROUND_BLUE));

                        if (bolas[b].Type_mov == 0){ 
                            bolas[b].Pos_Y += 1; 
                        }
                        if (bolas[b].Type_mov == 1){ 
                            bolas[b].Pos_Y -= 1; 
                        }
                        if (bolas[b].Type_mov == 2){ 
                            bolas[b].Pos_X -= 1; 
                            bolas[b].Pos_Y -= 1; 
                        }
                        if (bolas[b].Type_mov == 3){ 
                            bolas[b].Pos_X += 1; 
                            bolas[b].Pos_Y -= 1; 
                        }
                        if (bolas[b].Type_mov == 4){ 
                            bolas[b].Pos_X -= 1; 
                            bolas[b].Pos_Y += 1; 
                        }
                        if (bolas[b].Type_mov == 5){ 
                            bolas[b].Pos_X += 1; 
                            bolas[b].Pos_Y += 1; 
                        }

                        if (bolas[b].Pos_X < 10) {
                            bolas[b].Pos_X = 10;
                            if (bolas[b].Type_mov == 2) { bolas[b].Type_mov = 3; }
                            if (bolas[b].Type_mov == 4) { bolas[b].Type_mov = 5; }
                        }
                        if (bolas[b].Pos_X > 306){
                            bolas[b].Pos_X = 306;
                            if (bolas[b].Type_mov == 3){ 
                                bolas[b].Type_mov = 2; 
                            }
                            if (bolas[b].Type_mov == 5){ 
                                bolas[b].Type_mov = 4; 
                            }
                        }
                        if (bolas[b].Pos_Y < 10){
                            bolas[b].Pos_Y = 10;
                            if (bolas[b].Type_mov == 1){ 
                                bolas[b].Type_mov = 0; 
                            }
                            if (bolas[b].Type_mov == 2){ 
                                bolas[b].Type_mov = 4; 
                            }
                            if (bolas[b].Type_mov == 3){ 
                                bolas[b].Type_mov = 5; 
                            }
                        }

                        // Se cair no fundo do mapa, apenas MARCA para remocao depois
                        if (bolas[b].Pos_Y > 205) {
                            bola_morreu[b] = true;
                            continue;
                        }

                        if ((bolas[b].Pos_Y + 3 >= Y_PLAYER - 1) && (bolas[b].Pos_Y <= Y_PLAYER + ALTURA_PLAYER - 1)) {
                            if ((bolas[b].Pos_X + 3 >= player_x) && (bolas[b].Pos_X <= player_x + LARGURA_PLAYER)) {
                                if (bolas[b].Pos_X < player_x + 14) {
                                    bolas[b].Type_mov = 2;
                                }
                                else if (bolas[b].Pos_X <= player_x + 26) {
                                    bolas[b].Type_mov = 1;
                                }
                                else {
                                    bolas[b].Type_mov = 3;
                                }
                            }
                        }

                        short teste_x = bolas[b].Pos_X;
                        short teste_y = bolas[b].Pos_Y;

                        if (bolas[b].Type_mov == 0){ 
                            teste_x = bolas[b].Pos_X + 1; 
                            teste_y = bolas[b].Pos_Y + 3; 
                        }
                        if (bolas[b].Type_mov == 1){ 
                            teste_x = bolas[b].Pos_X + 1; 
                            teste_y = bolas[b].Pos_Y;     
                        }
                        if (bolas[b].Type_mov == 2){ 
                            teste_x = bolas[b].Pos_X;     
                            teste_y = bolas[b].Pos_Y;     
                        }
                        if (bolas[b].Type_mov == 3){ 
                            teste_x = bolas[b].Pos_X + 3; 
                            teste_y = bolas[b].Pos_Y;     
                        }
                        if (bolas[b].Type_mov == 4){ 
                            teste_x = bolas[b].Pos_X;     
                            teste_y = bolas[b].Pos_Y + 3; 
                        }
                        if (bolas[b].Type_mov == 5){ 
                            teste_x = bolas[b].Pos_X + 3; 
                            teste_y = bolas[b].Pos_Y + 3; 
                        }

                        int col_bloco = (teste_x - 10) / 30;
                        int row_bloco = (teste_y - 10) / 11;

                        if ((row_bloco >= 0) && (row_bloco < 5)) {
                            if ((col_bloco >= 0) && (col_bloco < 10)) {
                                bool perto_o_suficiente =
                                    (bolas[b].Pos_X + 3 >= blocos[row_bloco][col_bloco].Pos_X) &&
                                    (bolas[b].Pos_X     <= blocos[row_bloco][col_bloco].Pos_X + LARGURA_BLOCO - 1) &&
                                    (bolas[b].Pos_Y + 3 >= blocos[row_bloco][col_bloco].Pos_Y) &&
                                    (bolas[b].Pos_Y     <= blocos[row_bloco][col_bloco].Pos_Y + ALTURA_BLOCO - 1);

                                if (perto_o_suficiente && (blocos[row_bloco][col_bloco].IsAlive == true)) {
                                    blocos[row_bloco][col_bloco].IsAlive = false;
                                    video_box(blocos[row_bloco][col_bloco].Pos_X, blocos[row_bloco][col_bloco].Pos_Y, blocos[row_bloco][col_bloco].Pos_X + LARGURA_BLOCO - 1, blocos[row_bloco][col_bloco].Pos_Y + ALTURA_BLOCO - 1, background_color);
                                    score += 10;
                                    AtualizarScore();

                                    if (blocos[row_bloco][col_bloco].Type == BLOCO_TIPO_DUPLICAR_BOLA) {
                                        adicionar_poder(blocos[row_bloco][col_bloco].Pos_X + LARGURA_BLOCO / 2,
                                                        blocos[row_bloco][col_bloco].Pos_Y, 0);
                                    }
                                    if (blocos[row_bloco][col_bloco].Type == BLOCO_TIPO_AUMENTAR_PLAYER) {
                                        adicionar_poder(blocos[row_bloco][col_bloco].Pos_X + LARGURA_BLOCO / 2,
                                                        blocos[row_bloco][col_bloco].Pos_Y, 1);
                                    }
                                    if (blocos[row_bloco][col_bloco].Type == BLOCO_TIPO_DIMINUIR_PLAYER) {
                                        adicionar_poder(blocos[row_bloco][col_bloco].Pos_X + LARGURA_BLOCO / 2,
                                                        blocos[row_bloco][col_bloco].Pos_Y, 2);
                                    }
                                    if (blocos[row_bloco][col_bloco].Type == BLOCO_TIPO_RELOGIO) {
                                        adicionar_poder(blocos[row_bloco][col_bloco].Pos_X + LARGURA_BLOCO / 2,
                                                        blocos[row_bloco][col_bloco].Pos_Y, 3);
                                    }

                                    if (bolas[b].Type_mov == 0){ 
                                        bolas[b].Type_mov = 1; 
                                    }
                                    else if (bolas[b].Type_mov == 1){ 
                                        bolas[b].Type_mov = 0; 
                                    }
                                    else if (bolas[b].Type_mov == 2){ 
                                        bolas[b].Type_mov = 4; 
                                    }
                                    else if (bolas[b].Type_mov == 3){ 
                                        bolas[b].Type_mov = 5; 
                                    }
                                    else if (bolas[b].Type_mov == 4){ 
                                        bolas[b].Type_mov = 2; 
                                    }
                                    else if (bolas[b].Type_mov == 5){ 
                                        bolas[b].Type_mov = 3; 
                                    }
                                }
                            }
                        }

                        video_box(bolas[b].Pos_X, bolas[b].Pos_Y, bolas[b].Pos_X + 3, bolas[b].Pos_Y + 3, resample_rgb(db, COR_PLAYER));
                    }

                    // Compacta o array de bolas uma unica vez, fora do loop de logica
                    {
                        int write_idx = 0;
                        int read_idx = 0;
                        for (read_idx = 0; read_idx < num_bolas; read_idx++) {
                            if (!bola_morreu[read_idx]) {
                                if (write_idx != read_idx) {
                                    bolas[write_idx] = bolas[read_idx];
                                }
                                write_idx++;
                            }
                        }
                        num_bolas = write_idx;
                    }
                    free(bola_morreu);

                    if (num_bolas == 0) {
                        free(bolas);
                        bolas = NULL;
                        estado = ESTADO_GAMEOVER;
                        fase_ativa = false;
                    }

                    if (!fase_ativa) {
                        break;
                    }

                    atualizar_poderes(background_color);

                    if (contador_expansao > 0){
                        contador_expansao--;
                        if (contador_expansao == 0){
                            video_box(player_x, Y_PLAYER, player_x + LARGURA_PLAYER - 1, Y_PLAYER + ALTURA_PLAYER - 1, resample_rgb(db, BACKGROUND_BLUE));
                            LARGURA_PLAYER = LARGURA_PLAYER_BASE;
                            tipo_efeito_player = 0;
                            if (player_x > 310 - LARGURA_PLAYER) {
                                player_x = 310 - LARGURA_PLAYER;
                            }
                            video_box(player_x, Y_PLAYER, player_x + LARGURA_PLAYER - 1, Y_PLAYER + ALTURA_PLAYER - 1, resample_rgb(db, COR_PLAYER));
                        }
                    }

                    // Passou do score alvo da fase 2 (a partir do que ja tinha): vitoria final
                    if (score >= score_ao_entrar + SCORE_ALVO_LEVEL2) {
                        estado = ESTADO_YOUWIN;
                        fase_ativa = false;
                        break;
                    }

                    if (contador_relogio > 0) {
                        contador_relogio--;
                    }

                    if (contador_relogio > 0) {
                        delay(50000 * MULTIPLICADOR_DELAY_RELOGIO);
                    }
                    else {
                        delay(40000);
                    }
                }
                break;
            }

            // ---------------- GAME OVER ----------------
            case ESTADO_GAMEOVER: {
                draw_background(0, 0, gameover, 320, 240);
                LimparTextoTela();
                while (1) {
                    int botoes = *KEY_ptr;
                    if (botoes & 0x04) { // KEY2 = reset volta pro menu
                        aguardar_soltar_tecla(KEY_ptr, 0x04);
                        LimparTextoTela();
                        estado = ESTADO_MENU;
                        break;
                    }
                }
                break;
            }

            // ---------------- YOU WIN ----------------
            case ESTADO_YOUWIN: {
                draw_background(0, 0, ganhou, 320, 240);
                LimparTextoTela();
                while (1) {
                    int botoes = *KEY_ptr;
                    if (botoes & 0x04) { // KEY2 = reset volta pro menu
                        aguardar_soltar_tecla(KEY_ptr, 0x04);
                        LimparTextoTela();
                        estado = ESTADO_MENU;
                        break;
                    }
                }
                break;
            }
        }
    }
}

/*******************************************************************************
 * Subroutine to send a string of text to the video monitor
 ******************************************************************************/
void video_text(int x, int y, char * text_ptr) {
    int             offset;
    volatile char * character_buffer =
        (char *)FPGA_CHAR_BASE; // video character buffer

    /* assume that the text string fits on one line */
    offset = (y << 7) + x;
    while (*(text_ptr)) {
        *(character_buffer + offset) =
            *(text_ptr); // write to the character buffer
        ++text_ptr;
        ++offset;
    }
}

/*******************************************************************************
 * Draw a filled rectangle on the video monitor
 * Takes in points assuming 320x240 resolution and adjusts based on differences
 * in resolution and color bits.
 ******************************************************************************/
void video_box(int x1, int y1, int x2, int y2, short pixel_color) {
    int pixel_buf_ptr = *(int *)PIXEL_BUF_CTRL_BASE;
    int pixel_ptr, row, col;
    int x_factor = 0x1 << (res_offset + col_offset);
    int y_factor = 0x1 << (res_offset);
    x1           = x1 / x_factor;
    x2           = x2 / x_factor;
    y1           = y1 / y_factor;
    y2           = y2 / y_factor;

    /* assume that the box coordinates are valid */
    for (row = y1; row <= y2; row++) {
        for (col = x1; col <= x2; ++col) {
            pixel_ptr = pixel_buf_ptr +
                        (row << (10 - res_offset - col_offset)) + (col << 1);
            *(short *)pixel_ptr = pixel_color; // set pixel color
        }
    }
}

//Desenha um único pixel na tela
void draw_pixel(int x, int y, short color) {
    int pixel_buf_ptr = *(int *)PIXEL_BUF_CTRL_BASE;
    int x_factor = 0x1 << (res_offset + col_offset);
    int y_factor = 0x1 << res_offset;
    x = x / x_factor;
    y = y / y_factor;
    int pixel_ptr = pixel_buf_ptr + (y << (10 - res_offset - col_offset)) + (x << 1);
    *(short *)pixel_ptr = color;
}

//Desenha um sprite a partir de chamadas sucessivas do pixel
void draw_background(int x, int y, int sprite[][320], int w, int h) {
    int i = 0;
    int j = 0;
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            if (sprite[i][j] != -1) {
                draw_pixel(x + j, y + i, resample_rgb(db, sprite[i][j]));
            }
        }
    }
}

//Desenha um sprite a partir de chamadas sucessivas do pixel
void draw_sprite(int x, int y, int sprite[][8], int w, int h) {
    int i = 0;
    int j = 0;
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            if (sprite[i][j] != -1) {
                draw_pixel(x + j, y + i, resample_rgb(db, sprite[i][j]));
            }
        }
    }
}

/*******************************************************************************
 * Resamples 24-bit color to 16-bit or 8-bit color
 *******************************************************************************/
int resample_rgb(int num_bits, int color) {
    if (num_bits == 8) {
        color = (((color >> 16) & 0x000000E0) | ((color >> 11) & 0x0000001C) |
                 ((color >> 6) & 0x00000003));
        color = (color << 8) | color;
    } else if (num_bits == 16) {
        color = (((color >> 8) & 0x0000F800) | ((color >> 5) & 0x000007E0) |
                 ((color >> 3) & 0x0000001F));
    }
    return color;
}

/*******************************************************************************
 * Finds the number of data bits from the mode
 *******************************************************************************/
int get_data_bits(int mode) {
    switch (mode) {
    case 0x0:
        return 1;
    case 0x7:
        return 8;
    case 0x11:
        return 8;
    case 0x12:
        return 9;
    case 0x14:
        return 16;
    case 0x17:
        return 24;
    case 0x19:
        return 30;
    case 0x31:
        return 8;
    case 0x32:
        return 12;
    case 0x37:
        return 32;
    case 0x39:
        return 40;
    }
}