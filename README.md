# Trabalho M2 - 3D e Arquivos .obj (Computação Gráfica)

Continuação do projeto do **Trabalho M1**: em vez do cubo em wireframe, o programa agora **lê modelos
3D de arquivos `.obj`** (vértices, normais, coordenadas de textura e faces) e desenha os modelos com
**faces preenchidas, iluminação e texturas `.bmp`**. A cena tem **3 luzes** que podem ser ligadas e
desligadas, e cada objeto pode ser **transladado, rotacionado e escalado** pelo teclado e pelo mouse.

Diferente do M1, aqui as transformações e a projeção usam as funções do próprio OpenGL
(`glTranslated`, `glRotated`, `glScaled` e `gluPerspective`), como o enunciado permite.

---

## Requisitos do trabalho e onde eles estão resolvidos

| Requisito | Como foi atendido | Onde |
| --- | --- | --- |
| Eventos de mouse e/ou teclado para rotação, escala e translação | Teclado, teclas especiais, arraste com os botões esquerdo e direito e roda do mouse. Atuam sobre o objeto selecionado (`TAB` troca) | `keyboard()`, `keyboard_special()`, `mouse()`, `mouse_movimento()`, `mouse_roda()` |
| Pode usar `glRotate`, `glTranslate` e `glScale` | O estado de cada objeto (posição, rotação, escala) é aplicado com `glTranslated`, `glRotated` e `glScaled` | `desenhar()` |
| Expandir a leitura do `.obj` para carregar também normais e texturas | Além de `v` e `f`, lê `vn` e `vt`, e as faces nos formatos `v`, `v/vt`, `v//vn` e `v/vt/vn`. As imagens de textura `.bmp` são lidas e aplicadas usando essas coordenadas | `carregar_obj()`, `ler_indice_face()`, `carregar_bmp()` |
| Carregar a forma como faces (triângulos), não só arestas | Display list com `GL_TRIANGLES`; faces com 4 ou mais cantos são divididas em triângulos | `carregar_obj()`, `criar_lista()` |
| Vértices com posição **e normal** para a iluminação | Cada vértice é enviado com `glNormal3d` + `glVertex3d` e sombreamento suave (`GL_SMOOTH`) | `criar_lista()` |
| Pelo menos 3 luzes com componente difusa, especular e ambiente | `GL_LIGHT0`, `GL_LIGHT1` e `GL_LIGHT2`, cada uma com `GL_AMBIENT`, `GL_DIFFUSE` e `GL_SPECULAR` | `luzes[]`, `configurar_luzes()` |
| Luzes podem ser ativadas e desativadas | Teclas `1`, `2` e `3` chamam `glEnable` / `glDisable` da luz | `alternar_luz()` |

Todo o código está em [`ComputacaoGrafica/ComputacaoGrafica.cpp`](ComputacaoGrafica/ComputacaoGrafica.cpp).

---

## Modelos e texturas

Os arquivos ficam em `ComputacaoGrafica/data/`. Juntos, os 5 modelos exercitam todos os formatos
que o leitor aceita:

| Modelo | O que o arquivo traz | Observação |
| --- | --- | --- |
| `mba1.obj` | `v`, `vt`, `vn`, faces `v/vt/vn` | 84 mil triângulos |
| `porsche.obj` | `v`, `vn`, faces `v//vn` | Faces de 4 cantos, divididas em triângulos |
| `radar.obj` | `v`, `vt`, `vn` | Índices **negativos** (`f -2711/-3032/-2711 ...`) |
| `teddy.obj` | só `v` e `f` | Normais calculadas a partir das faces |
| `elepham.obj` | `v`, `vn`, faces `v//vn` | Tem um plano de chão gigante (veja *Normalização*) |

Texturas em `ComputacaoGrafica/data/texturas/`: `canLabel`, `canTop`, `cray2`, `grass`, `launch`,
`nightSky`, `sky` e `trees` (`.bmp` de 24 bits).

As listas `ARQUIVOS_OBJ` e `TEXTURAS_BMP`, no começo do código, dizem o que é carregado. Os caminhos
são relativos à pasta do projeto, que é a pasta de trabalho do Visual Studio ao executar com
`Ctrl + F5`. Também dá para passar outros `.obj` pela linha de comando:

```
ComputacaoGrafica.exe data/teddy.obj data/porsche.obj
```

Os objetos são organizados numa grade de 3 por linha, cada um com uma cor.

---

## Estrutura de dados

```cpp
struct vertice       { double x, y, z; };
struct coord_textura { double u, v; };
struct indice_face   { int v, vt, vn; };          // -1 quando o arquivo não informa

using face = std::array<indice_face, 3>;          // toda face vira triângulo na leitura

struct Objeto {
    std::string nome;
    GLuint lista;              // display list com os triângulos
    vertice cor;
    int textura;               // índice em texturas_bmp, ou -1 para nenhuma
    vertice posicao, escala, rotacao;
    vertice centro;            // centro da caixa envolvente do modelo
    double normalizacao;       // fator que faz o modelo caber num cubo de lado 2
    lista_vertices vertices;   // v
    lista_vertices normais;    // vn
    lista_texturas texturas;   // vt
    lista_faces faces;         // f
};
```

O `Poligono` do M1 virou `Objeto`. Ele guarda as listas lidas do arquivo e, como no M1, a posição,
a escala e a rotação acumuladas. Cada canto de face guarda **três índices**: vértice, coordenada de
textura e normal.

---

## Como funciona

### Leitura do `.obj`

O arquivo é lido linha a linha, do mesmo jeito que o `loadObj` do exemplo da aula, com os tipos novos:

```
v  x y z        -> vertices
vt u v          -> texturas
vn x y z        -> normais
f  1/1/1 2/2/2 3/3/3 [4/4/4 ...]
```

- Cada canto da face é separado nas barras: `7/3/5` dá vértice 7, textura 3 e normal 5. Os formatos
  `7`, `7/3` e `7//5` também funcionam.
- Os índices do `.obj` começam em **1**, então é feito `- 1`. Índices negativos contam a partir do
  fim da lista (`-1` é o último lido).
- Uma face com N cantos vira N − 2 triângulos, em leque: `(0,1,2)`, `(0,2,3)`, `(0,3,4)`...
- As linhas que o programa não usa (`#`, `o`, `g`, `s`, `mtllib`, `usemtl`) são ignoradas.
- Ao carregar, o console mostra quantos vértices, normais, coordenadas de textura e triângulos
  foram lidos.

**Normais faltando:** se o arquivo não traz a normal de algum canto (caso do `teddy.obj`), ela é
calculada. A normal de cada face é o produto vetorial `(b − a) × (c − a)`, e a normal de cada
vértice é a soma das normais das faces que usam aquele vértice, normalizada. O resultado é um
sombreamento suave.

### Normalização do modelo

Cada `.obj` vem numa escala diferente: o `porsche.obj` mede ~110 unidades e o `elepham.obj` ~1000.
Na carga é calculada a caixa envolvente do modelo. No desenho, o modelo é centralizado na origem e
reduzido para caber num cubo de lado 2. Por isso qualquer arquivo aparece com um tamanho razoável
e, como o modelo fica centrado na origem, a rotação e a escala acontecem em torno do centro dele.

O `elepham.obj` tem, além do elefante, um **plano de chão de ~117 mil unidades** (4 vértices e 2
triângulos no fim do arquivo). Se ele entrasse na caixa, o elefante ficaria do tamanho de um ponto.
Por isso a caixa ignora vértices isolados: em cada eixo pega a faixa onde estão 98% dos vértices
(do 1% ao 99%) e aceita até meia faixa além dela (`limites_eixo()`). Triângulos com os 3 vértices
fora dessa caixa não são desenhados, e o console avisa quantos foram. Nos outros modelos nada é
cortado.

### Desenho e transformações

Os triângulos são gravados uma única vez numa **display list** (`glNewList` / `glCallList`). Cada
vértice vai com a sua normal e a sua coordenada de textura:

```cpp
glNormal3d(n.x, n.y, n.z);
glTexCoord2d(t.u, t.v);
glVertex3d(v.x, v.y, v.z);
```

A cada quadro, `desenhar()` aplica as transformações do objeto antes de chamar a lista:

```cpp
glTranslated(posicao);                // 4º: leva para o lugar na cena
glRotated(rotacao.x, 1, 0, 0);        // 3º: rotação nos eixos X, Y e Z
glRotated(rotacao.y, 0, 1, 0);
glRotated(rotacao.z, 0, 0, 1);
glScaled(escala);                     // 2º: escala do usuário
glScaled(normalizacao);               // 1º: centraliza e normaliza o modelo
glTranslated(-centro);
glCallList(lista);
```

Como o `glScale` também estica as normais, `GL_NORMALIZE` fica ligado para as normais voltarem ao
tamanho 1 e a iluminação continuar correta depois de escalar.

Os objetos começam levemente girados (`ROTACAO_INICIAL`), para dar para ver que são 3D. O objeto
selecionado ganha uma caixa envolvente amarela, desenhada com os mesmos 8 vértices e 12 arestas do
cubo do M1.

### Iluminação

| Luz | Cor | Posição | Tecla |
| --- | --- | --- | --- |
| `GL_LIGHT0` | branca | à direita, acima `(3.5, 2.6, 3.0)` | `1` |
| `GL_LIGHT1` | laranja | à esquerda `(-3.6, 0.0, 3.0)` | `2` |
| `GL_LIGHT2` | azul | abaixo `(1.5, -1.8, 3.0)` | `3` |

- Cada luz tem as três componentes: **ambiente** (luz espalhada que ilumina tudo por igual),
  **difusa** (depende do ângulo entre a normal e a direção da luz) e **especular** (o brilho, que
  depende também da posição da câmera).
- São luzes pontuais (`w = 1`). A posição é enviada a cada quadro, logo depois da câmera, para elas
  ficarem paradas no mundo.
- Uma esfera pequena marca onde está cada luz: na cor da luz quando ligada, cinza quando desligada.
- A cor do objeto vira o material ambiente e difuso (`GL_COLOR_MATERIAL`). O material especular é
  o mesmo para todos, com brilho 40.
- Há uma luz ambiente global bem fraca (0.05), para o objeto não sumir por completo com as três
  luzes desligadas.

### Texturas

**Leitura do `.bmp`** (`carregar_bmp()`): o arquivo começa com um cabeçalho de 54 bytes, de onde
saem a largura, a altura, os bits por pixel (tem que ser 24) e onde começam os pixels. Depois vêm as
linhas da imagem, que:

- estão **de baixo para cima**, a mesma ordem que o OpenGL espera;
- guardam cada pixel como **azul, verde, vermelho**, então os canais são trocados para RGB;
- são completadas com zeros até um múltiplo de 4 bytes.

A imagem vira uma textura com `gluBuild2DMipmaps` (versões reduzidas para quando o objeto fica
pequeno na tela) e `GL_REPEAT`.

**Aplicação:** `T` troca a textura do objeto selecionado: nenhuma → `canLabel` → `canTop` → ... →
`trees` → nenhuma. O modo `GL_MODULATE` multiplica a textura pela iluminação, então as luzes
continuam valendo. Com textura, a cor base do objeto passa a ser branca, para a imagem aparecer com
as cores originais.

- Modelos **com `vt`** (`mba1`, `radar`) usam as coordenadas do arquivo.
- Modelos **sem `vt`** (`porsche`, `teddy`, `elepham`) recebem coordenadas geradas por **projeção
  planar**: a textura é projetada de frente (plano XY), usando a caixa envolvente para levar `x` e
  `y` ao intervalo 0..1.

---

## Controles

Valem para o **objeto selecionado** (o que está com a caixa amarela).

| Tecla / ação | Operação |
| --- | --- |
| `TAB` | Seleciona o próximo objeto |
| `W` `A` `S` `D` | Translação no plano XY |
| `Q` / `E` | Translação no eixo Z (aproximar / afastar) |
| Arrastar com o botão direito | Translação no plano XY |
| Setas ↑ ↓ ← → | Rotação nos eixos X e Y |
| `J` / `L` | Rotação no eixo Z |
| Arrastar com o botão esquerdo | Rotação nos eixos X e Y |
| `+` / `-` | Escala uniforme |
| Roda do mouse | Escala uniforme |
| `x` `X` `y` `Y` `z` `Z` | Escala em um eixo por vez (minúscula aumenta, maiúscula diminui) |
| `R` | Reinicia o objeto |
| `1` / `2` / `3` | Liga / desliga cada luz |
| `T` | Troca a textura do objeto (nenhuma ou um dos `.bmp`) |
| `M` | Alterna entre faces preenchidas e só as arestas dos triângulos |
| `ESC` | Sair |

O canto superior esquerdo da janela mostra o objeto selecionado, o estado das luzes, a textura do
objeto e o modo de desenho. A lista de controles também é impressa no console.

---

## Como compilar e executar

**Requisitos:** Visual Studio 2022 (toolset v145) com a carga de trabalho *Desenvolvimento para
desktop com C++*. O freeglut já vem versionado no repositório (headers, `.lib` e `.dll`), então não
é preciso instalar nada além disso.

1. Abrir `ComputacaoGrafica.sln` no Visual Studio.
2. Selecionar a configuração **Debug | x64** (ou Release | x64).
3. Compilar e executar (`Ctrl + F5`).

O `mba1.obj` tem 14 MB; em **Release** ele carrega bem mais rápido que em Debug.

O projeto já está configurado com:

- include: `$(ProjectDir)freeglut\include`
- biblioteca: `freeglut.lib`
- `freeglut.dll` copiada junto do executável

---

## Organização do repositório

```
ComputacaoGrafica.sln
ComputacaoGrafica/
  ComputacaoGrafica.cpp      <- todo o trabalho está aqui
  ComputacaoGrafica.vcxproj
  data/                      <- modelos .obj
    texturas/                <- texturas .bmp
  freeglut/                  <- dependência vendorizada (include, lib, bin)
  freeglut.dll
```
