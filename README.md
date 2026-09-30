# Breakout

Jogo estilo Breakout em C para a placa Altera DE10-Standard (processador ARM Cortex-A9), com saída de vídeo VGA e controle pelos botões da placa.
Este projeto foi realizado para a disciplina de Sistemas Embarcados da UFSCar, em dupla. O tutorial em PDF deste repositório foi feito individualmente.

<img width="486" height="270" alt="image" src="https://github.com/user-attachments/assets/9f4bbbc6-3859-4f52-af7c-230c2859d6e1" />

## Lógica do jogo

O jogador controla uma barra na parte de baixo da tela e rebate uma bola para destruir os blocos no topo. Cada bloco destruído vale 10 pontos.

- **Controles:** KEY0 move a barra para a direita (e inicia o jogo no menu), KEY1 para a esquerda e KEY2 reinicia e volta ao menu.
- **Fases:** a fase 1 tem a grade completa de blocos e vai até 500 pontos. A fase 2 tem os blocos em formato de triângulo e exige mais 300 pontos.
- **Power-ups:** alguns blocos soltam itens que caem na tela. Eles podem duplicar as bolas, aumentar ou diminuir a barra, ou deixar o jogo mais lento por um tempo.
- **Vitória e derrota:** o jogador vence ao completar as duas fases e perde quando todas as bolas caem.
- **Organização do código:** uma máquina de estados controla menu, fase 1, fase 2, game over e vitória. O vídeo é desenhado direto na memória da placa (pixel buffer) e os botões são lidos por endereços de memória. As bolas e os power-ups ficam em arrays dinâmicos, porque a quantidade muda durante a partida.

## Arquivos

- `main.c`: lógica do jogo
- `sprites.h`: telas e sprites do jogo
- `address_map_arm.h`: endereços dos periféricos da placa
- `Tutorial_Bricks.pdf`: tutorial completo, da instalação das ferramentas à explicação do código
- `video.mp4`: vídeo do jogo em funcionamento

## Referências

O `address_map_arm.h` e as funções de vídeo usadas no `main.c` (`video_text`, `video_box`, `resample_rgb` e `get_data_bits`) vêm do exemplo *Video* do Intel FPGA Monitor Program (Intel FPGA University Program). A lógica do jogo foi desenvolvida sobre essa base.

## Como rodar

Requisitos: placa DE10-Standard ligada, cabo USB-Blaster, monitor VGA, Intel Quartus Prime Lite 23.1 e Intel FPGA Monitor Program, instalados nos caminhos padrão (o compilar.bat depende deles).

Devido a limitações/compatibilidade do Monitor Program, a opção Compile & Load pode não funcionar corretamente em algumas configurações. Por isso, recomenda-se compilar com o compilar.bat e carregar o arquivo main.srec. Segue o passo a passo:

1. Coloque o compilar.bat na mesma pasta de main.c, sprites.h e address_map_arm.h. O arquivo principal precisa se chamar main.c.
2. Dê dois cliques no compilar.bat. Ele gera o main.axf e o main.srec na mesma pasta.
3. No Monitor Program, crie um projeto com arquitetura ARM Cortex-A9 e sistema DE10-Standard Computer.
4. Em Program Type, escolha AXF, ELF or SREC file e adicione o main.srec.
5. Use Actions > Load e depois Actions > Continue (ou F3).

O passo a passo completo, com imagens, está presente no Tutorial_Bricks.pdf.
