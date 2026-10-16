#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <GL/freeglut.h>

struct vertice {
	double x, y, z;
};
struct coord_textura {
	double u, v;
};
// Um canto de face do .obj: indice do vertice, da coordenada de textura e da
// normal. Vale -1 quando o arquivo nao informa aquele dado.
struct indice_face {
	int v, vt, vn;
};
using lista_vertices = std::vector<vertice>;
using lista_texturas = std::vector<coord_textura>;
using face = std::array<indice_face, 3>; // toda face vira triangulo na leitura
using lista_faces = std::vector<face>;

struct Objeto {
	std::string nome;
	GLuint lista;            // display list com os triangulos do modelo
	vertice cor;
	int textura;             // indice em texturas_bmp, ou -1 para nenhuma
	vertice posicao;         // translacao, aplicada com glTranslated
	vertice escala;          // escala, aplicada com glScaled
	vertice rotacao;         // rotacao em graus, aplicada com glRotated
	vertice posicao_inicial;
	vertice centro;          // centro da caixa envolvente, nas coordenadas do arquivo
	double normalizacao;     // fator que faz o modelo caber num cubo de lado 2
	vertice meia_caixa;      // metade da caixa envolvente, ja normalizada
	vertice caixa_min, caixa_max; // caixa envolvente, nas coordenadas do arquivo
	lista_vertices vertices;
	lista_vertices normais;
	lista_texturas texturas;
	lista_faces faces;
};

struct Luz {
	GLenum id;
	std::string nome;
	GLfloat posicao[4];
	GLfloat ambiente[4];
	GLfloat difusa[4];
	GLfloat especular[4];
	bool ligada;
};

struct Textura {
	std::string nome;
	GLuint id;
};

bool carregar_obj(const std::string& caminho, Objeto& objeto);
indice_face ler_indice_face(const std::string& token, const Objeto& objeto);
int converter_indice(const std::string& texto, int quantidade);
bool calcular_normais_faltantes(Objeto& objeto);
void limites_eixo(std::vector<double> valores, double& minimo, double& maximo);
void normalizar(Objeto& objeto);
bool fora_da_caixa(const Objeto& objeto, const vertice& v);
int criar_lista(Objeto& objeto);
void carregar_cena(const std::vector<std::string>& arquivos);
void inicializar_opengl();
void configurar_luzes();
void posicionar_luzes();
void alternar_luz(int indice);
void carregar_texturas(const std::vector<std::string>& arquivos);
GLuint carregar_bmp(const std::string& caminho);
int ler_inteiro(const unsigned char* bytes, int posicao);
void desenhar(const Objeto& objeto, bool selecionado);
void desenhar_caixa(vertice meia_caixa);
void desenhar_luzes();
void desenhar_hud();
void desenhar_texto(int x, int y, const std::string& texto);
void movimentar(Objeto& objeto, double distancia_x, double distancia_y, double distancia_z);
void escalar(Objeto& objeto, double escala_x, double escala_y, double escala_z);
void rotacionar(Objeto& objeto, double angulo_x, double angulo_y, double angulo_z);
void reiniciar(Objeto& objeto);
void display();
void reshape(int largura, int altura);
void redraw(int value);
void keyboard(unsigned char key, int x, int y);
void keyboard_special(int key, int x, int y);
void mouse(int botao, int estado, int x, int y);
void mouse_movimento(int x, int y);
void mouse_roda(int roda, int direcao, int x, int y);
void imprimir_controles();

// Modelos carregados quando o programa e aberto sem argumentos. Os caminhos
// sao relativos a pasta do projeto, que e a pasta de trabalho padrao do
// Visual Studio ao executar com Ctrl + F5.
const std::vector<std::string> ARQUIVOS_OBJ = {
	"data/mba1.obj",
	"data/porsche.obj",
	"data/radar.obj",
	"data/teddy.obj",
	"data/elepham.obj",
};

// Texturas .bmp (24 bits, sem compressao). A tecla T troca a textura do
// objeto selecionado entre estas.
const std::vector<std::string> TEXTURAS_BMP = {
	"data/texturas/canLabel.bmp",
	"data/texturas/canTop.bmp",
	"data/texturas/cray2.bmp",
	"data/texturas/grass.bmp",
	"data/texturas/launch.bmp",
	"data/texturas/nightSky.bmp",
	"data/texturas/sky.bmp",
	"data/texturas/trees.bmp",
};

// Cores dos objetos, usadas em sequencia conforme vao sendo carregados.
const vertice CORES[] = {
	{ 0.85, 0.35, 0.35 },
	{ 0.35, 0.65, 0.90 },
	{ 0.50, 0.80, 0.40 },
	{ 0.90, 0.75, 0.30 },
	{ 0.75, 0.50, 0.85 },
};
const double ESPACAMENTO = 2.8; // distancia entre os objetos na cena
const int COLUNAS = 3;          // objetos por linha da grade
const vertice ROTACAO_INICIAL = { 20, -35, 0 }; // levemente girado, para ver em 3D

// Tres luzes pontuais, cada uma com componente ambiente, difusa e especular.
// A posicao e dada em coordenadas do mundo (w = 1 indica luz pontual).
Luz luzes[3] = {
	{ GL_LIGHT0, "Luz 1 (branca)",
		{ 3.5f, 2.6f, 3.0f, 1.0f },
		{ 0.10f, 0.10f, 0.10f, 1.0f }, { 0.85f, 0.85f, 0.85f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, true },
	{ GL_LIGHT1, "Luz 2 (laranja)",
		{ -3.6f, 0.0f, 3.0f, 1.0f },
		{ 0.08f, 0.04f, 0.00f, 1.0f }, { 0.90f, 0.50f, 0.15f, 1.0f }, { 0.90f, 0.60f, 0.30f, 1.0f }, true },
	{ GL_LIGHT2, "Luz 3 (azul)",
		{ 1.5f, -1.8f, 3.0f, 1.0f },
		{ 0.00f, 0.02f, 0.08f, 1.0f }, { 0.25f, 0.45f, 1.00f, 1.0f }, { 0.50f, 0.70f, 1.00f, 1.0f }, true },
};

std::vector<Objeto> objetos;
int selecionado = 0;
std::vector<Textura> texturas_bmp;
bool modo_malha = false;
int largura_janela = 960;
int altura_janela = 640;
int delay = 10;

// Estado do arrasto do mouse: botao esquerdo rotaciona, direito movimenta
int botao_arrastando = -1;
int mouse_ultimo_x = 0;
int mouse_ultimo_y = 0;

int main(int argc, char** argv) {

	glutInit(&argc, argv);

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(largura_janela, altura_janela);

	glutCreateWindow("Trabalho M2 - 3D e arquivos .obj");
	inicializar_opengl();

	// Os .obj tambem podem ser passados na linha de comando; sem argumentos,
	// usa a lista ARQUIVOS_OBJ.
	std::vector<std::string> arquivos(argv + 1, argv + argc);
	if (arquivos.empty()) arquivos = ARQUIVOS_OBJ;

	carregar_texturas(TEXTURAS_BMP);
	carregar_cena(arquivos);
	if (objetos.empty()) {
		std::cout << "Nenhum arquivo .obj foi carregado.\n";
		std::cout << "Coloque os modelos na pasta data/ do projeto (lista ARQUIVOS_OBJ)\n";
		std::cout << "ou passe os caminhos dos .obj na linha de comando.\n";
		return 1;
	}

	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(keyboard_special);
	glutMouseFunc(mouse);
	glutMotionFunc(mouse_movimento);
	glutMouseWheelFunc(mouse_roda);
	glutTimerFunc(10, redraw, 0);

	imprimir_controles();

	glutMainLoop();

	return 0;
}


void display(void) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glLoadIdentity();
	gluLookAt(0, 0.5, 7.5, // camera
		0, 0, 0,           // ponto para onde ela olha
		0, 1, 0);          // vetor "para cima"

	// A posicao da luz e transformada pela matriz atual, por isso e definida a
	// cada quadro, logo depois da camera: assim as luzes ficam fixas no mundo.
	posicionar_luzes();
	desenhar_luzes();

	for (size_t i = 0; i < objetos.size(); i++)
		desenhar(objetos[i], (int)i == selecionado);

	desenhar_hud();

	glutSwapBuffers();
}

void reshape(int largura, int altura) {
	if (altura == 0) altura = 1;
	largura_janela = largura;
	altura_janela = altura;

	glViewport(0, 0, largura, altura);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0, double(largura) / double(altura), 0.1, 100.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void keyboard(unsigned char key, int x, int y) {
	Objeto& objeto = objetos[selecionado];

	switch (key) {
	case 27:
		exit(0);
		break;

	// Troca o objeto que recebe as transformacoes
	case 9: // TAB
		selecionado = (selecionado + 1) % (int)objetos.size();
		break;

	// Liga / desliga cada uma das luzes
	case '1':
	case '2':
	case '3':
		alternar_luz(key - '1');
		break;

	// Troca a textura do objeto: nenhuma, cada .bmp carregado, nenhuma de novo
	case 't':
	case 'T':
		objeto.textura++;
		if (objeto.textura >= (int)texturas_bmp.size()) objeto.textura = -1;
		break;

	// Alterna entre faces preenchidas e somente as arestas dos triangulos
	case 'm':
	case 'M':
		modo_malha = !modo_malha;
		glPolygonMode(GL_FRONT_AND_BACK, modo_malha ? GL_LINE : GL_FILL);
		break;

	// Translacao no plano XY
	case 'w':
		movimentar(objeto, 0, 0.1, 0);
		break;
	case 's':
		movimentar(objeto, 0, -0.1, 0);
		break;
	case 'a':
		movimentar(objeto, -0.1, 0, 0);
		break;
	case 'd':
		movimentar(objeto, 0.1, 0, 0);
		break;

	// Translacao em profundidade
	case 'q':
		movimentar(objeto, 0, 0, 0.1);
		break;
	case 'e':
		movimentar(objeto, 0, 0, -0.1);
		break;

	// Escala uniforme
	case '+':
		escalar(objeto, 1.1, 1.1, 1.1);
		break;
	case '-':
		escalar(objeto, 1 / 1.1, 1 / 1.1, 1 / 1.1);
		break;

	// Escala em um eixo de cada vez
	case 'x':
		escalar(objeto, 1.1, 1, 1);
		break;
	case 'X':
		escalar(objeto, 1 / 1.1, 1, 1);
		break;
	case 'y':
		escalar(objeto, 1, 1.1, 1);
		break;
	case 'Y':
		escalar(objeto, 1, 1 / 1.1, 1);
		break;
	case 'z':
		escalar(objeto, 1, 1, 1.1);
		break;
	case 'Z':
		escalar(objeto, 1, 1, 1 / 1.1);
		break;

	// Rotacao no eixo Z
	case 'j':
		rotacionar(objeto, 0, 0, 5);
		break;
	case 'l':
		rotacionar(objeto, 0, 0, -5);
		break;

	case 'r':
		reiniciar(objeto);
		break;
	}
}

void keyboard_special(int key, int x, int y) {
	Objeto& objeto = objetos[selecionado];

	switch (key) {
	case GLUT_KEY_UP:
		rotacionar(objeto, -5, 0, 0);
		break;

	case GLUT_KEY_DOWN:
		rotacionar(objeto, 5, 0, 0);
		break;

	case GLUT_KEY_LEFT:
		rotacionar(objeto, 0, -5, 0);
		break;

	case GLUT_KEY_RIGHT:
		rotacionar(objeto, 0, 5, 0);
		break;
	}
}

void mouse(int botao, int estado, int x, int y) {
	if (botao == GLUT_LEFT_BUTTON || botao == GLUT_RIGHT_BUTTON) {
		botao_arrastando = (estado == GLUT_DOWN) ? botao : -1;
		mouse_ultimo_x = x;
		mouse_ultimo_y = y;
	}
}

// Arrastar com o botao esquerdo rotaciona o objeto (horizontal gira em torno
// do eixo Y, vertical em torno do X); com o botao direito, movimenta no plano XY.
void mouse_movimento(int x, int y) {
	if (botao_arrastando < 0) return;

	int delta_x = x - mouse_ultimo_x;
	int delta_y = y - mouse_ultimo_y;

	if (botao_arrastando == GLUT_LEFT_BUTTON)
		rotacionar(objetos[selecionado], delta_y * 0.5, delta_x * 0.5, 0);
	else
		movimentar(objetos[selecionado], delta_x * 0.01, -delta_y * 0.01, 0);

	mouse_ultimo_x = x;
	mouse_ultimo_y = y;
}

void mouse_roda(int roda, int direcao, int x, int y) {
	if (direcao > 0)
		escalar(objetos[selecionado], 1.1, 1.1, 1.1);
	else
		escalar(objetos[selecionado], 1 / 1.1, 1 / 1.1, 1 / 1.1);
}

void redraw(int value) {
	glutPostRedisplay();
	glutTimerFunc(delay, redraw, 0);
}

// Leitura do .obj, linha a linha. Alem dos vertices (v) e das faces (f), le as
// normais (vn) e as coordenadas de textura (vt). As faces podem vir como
// "v", "v/vt", "v//vn" ou "v/vt/vn" e ter mais de 3 cantos; nesse caso sao
// divididas em triangulos.
bool carregar_obj(const std::string& caminho, Objeto& objeto) {
	std::ifstream arquivo(caminho);
	if (!arquivo.is_open()) {
		std::cout << "arquivo nao encontrado: " << caminho << "\n";
		return false;
	}

	std::string linha;
	while (std::getline(arquivo, linha)) {
		std::istringstream dados(linha);
		std::string tipo;
		dados >> tipo;

		if (tipo == "v") {
			vertice v = { 0, 0, 0 };
			dados >> v.x >> v.y >> v.z;
			objeto.vertices.push_back(v);
		}

		else if (tipo == "vn") {
			vertice n = { 0, 0, 0 };
			dados >> n.x >> n.y >> n.z;
			objeto.normais.push_back(n);
		}

		else if (tipo == "vt") {
			coord_textura t = { 0, 0 };
			dados >> t.u >> t.v;
			objeto.texturas.push_back(t);
		}

		else if (tipo == "f") {
			std::vector<indice_face> cantos;
			std::string token;
			bool valida = true;
			while (dados >> token) {
				indice_face canto = ler_indice_face(token, objeto);
				if (canto.v < 0) valida = false;
				cantos.push_back(canto);
			}
			if (!valida) continue; // face com indice de vertice invalido e descartada

			// Poligono com N cantos vira N - 2 triangulos, em leque a partir
			// do primeiro canto: (0, 1, 2), (0, 2, 3), (0, 3, 4)...
			for (size_t i = 1; i + 1 < cantos.size(); i++)
				objeto.faces.push_back({ cantos[0], cantos[i], cantos[i + 1] });
		}

		// Os demais tipos (comentarios "#", grupos "o" e "g", materiais
		// "mtllib" e "usemtl", suavizacao "s") nao sao usados.
	}
	arquivo.close();

	if (objeto.vertices.empty() || objeto.faces.empty()) {
		std::cout << "arquivo sem vertices ou faces: " << caminho << "\n";
		return false;
	}

	size_t normais_do_arquivo = objeto.normais.size();
	bool normais_calculadas = calcular_normais_faltantes(objeto);
	normalizar(objeto);
	int ignorados = criar_lista(objeto);

	std::cout << objeto.nome << ": "
		<< objeto.vertices.size() << " vertices, "
		<< normais_do_arquivo << " normais, "
		<< objeto.texturas.size() << " coordenadas de textura, "
		<< objeto.faces.size() << " triangulos\n";
	if (normais_calculadas)
		std::cout << "  (faltavam normais no arquivo: calculadas a partir das faces)\n";
	if (objeto.texturas.empty())
		std::cout << "  (sem coordenadas de textura no arquivo: geradas por projecao planar)\n";
	if (ignorados > 0)
		std::cout << "  (" << ignorados << " triangulos isolados, longe do modelo, nao foram desenhados)\n";

	return true;
}

// Separa um canto de face "v/vt/vn" nos tres indices. vt e vn podem faltar.
indice_face ler_indice_face(const std::string& token, const Objeto& objeto) {
	std::string v = token, vt, vn;

	size_t barra1 = token.find("/");
	if (barra1 != std::string::npos) {
		v = token.substr(0, barra1);
		size_t barra2 = token.find("/", barra1 + 1);
		if (barra2 == std::string::npos) {
			vt = token.substr(barra1 + 1);
		}
		else {
			vt = token.substr(barra1 + 1, barra2 - barra1 - 1);
			vn = token.substr(barra2 + 1);
		}
	}

	indice_face canto;
	canto.v = converter_indice(v, (int)objeto.vertices.size());
	canto.vt = converter_indice(vt, (int)objeto.texturas.size());
	canto.vn = converter_indice(vn, (int)objeto.normais.size());
	return canto;
}

// No .obj os indices comecam em 1, e um indice negativo conta a partir do fim
// da lista (-1 e o ultimo lido). Devolve o indice do vetor ou -1 se nao houver.
int converter_indice(const std::string& texto, int quantidade) {
	if (texto.empty()) return -1;

	int indice;
	try {
		indice = std::stoi(texto);
	}
	catch (...) {
		return -1;
	}

	indice = (indice < 0) ? quantidade + indice : indice - 1;
	if (indice < 0 || indice >= quantidade) return -1;
	return indice;
}

// Quando o arquivo nao traz a normal de algum canto, calcula uma normal por
// vertice somando as normais das faces que usam aquele vertice. Isso deixa o
// sombreamento suave, como se as normais tivessem vindo no arquivo.
bool calcular_normais_faltantes(Objeto& objeto) {
	bool faltando = false;
	for (const face& f : objeto.faces)
		for (const indice_face& canto : f)
			if (canto.vn < 0) faltando = true;
	if (!faltando) return false;

	// As normais calculadas ficam no fim da lista: a do vertice i esta em base + i
	size_t base = objeto.normais.size();
	objeto.normais.resize(base + objeto.vertices.size(), { 0, 0, 0 });

	for (face& f : objeto.faces) {
		const vertice& a = objeto.vertices[f[0].v];
		const vertice& b = objeto.vertices[f[1].v];
		const vertice& c = objeto.vertices[f[2].v];

		// Produto vetorial (b - a) x (c - a): perpendicular a face, com tamanho
		// proporcional a area dela (faces maiores pesam mais na media)
		vertice u = { b.x - a.x, b.y - a.y, b.z - a.z };
		vertice w = { c.x - a.x, c.y - a.y, c.z - a.z };
		vertice n = {
			u.y * w.z - u.z * w.y,
			u.z * w.x - u.x * w.z,
			u.x * w.y - u.y * w.x
		};

		for (indice_face& canto : f) {
			if (canto.vn >= 0) continue;
			canto.vn = (int)(base + canto.v);
			objeto.normais[canto.vn].x += n.x;
			objeto.normais[canto.vn].y += n.y;
			objeto.normais[canto.vn].z += n.z;
		}
	}

	for (size_t i = base; i < objeto.normais.size(); i++) {
		vertice& n = objeto.normais[i];
		double tamanho = sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
		if (tamanho > 0) {
			n.x /= tamanho;
			n.y /= tamanho;
			n.z /= tamanho;
		}
	}

	return true;
}

// Limites de um eixo ignorando valores isolados muito longe dos demais. Pega
// a faixa onde estao 98% dos valores (do 1% ao 99%) e aceita ate meia faixa
// alem dela; o que passar disso e considerado geometria isolada.
void limites_eixo(std::vector<double> valores, double& minimo, double& maximo) {
	std::sort(valores.begin(), valores.end());
	size_t n = valores.size();
	double p1 = valores[n / 100];
	double p99 = valores[n * 99 / 100];
	double margem = (p99 - p1) / 2.0;

	minimo = p1;
	maximo = p99;
	for (double valor : valores) {
		if (valor >= p1 - margem && valor < minimo) minimo = valor;
		if (valor <= p99 + margem && valor > maximo) maximo = valor;
	}
}

// Cada arquivo usa uma escala diferente (um modelo pode ter 2 unidades e
// outro 500). Calcula a caixa envolvente para centralizar o modelo na origem
// e reduzi-lo a um cubo de lado 2; a transformacao e aplicada em desenhar().
// A caixa ignora vertices isolados: o elepham.obj, por exemplo, tem um plano
// de chao de 117 mil unidades embaixo de um elefante de mil, que de outra
// forma deixaria o elefante do tamanho de um ponto.
void normalizar(Objeto& objeto) {
	std::vector<double> xs, ys, zs;
	for (const vertice& v : objeto.vertices) {
		xs.push_back(v.x);
		ys.push_back(v.y);
		zs.push_back(v.z);
	}

	vertice minimo, maximo;
	limites_eixo(xs, minimo.x, maximo.x);
	limites_eixo(ys, minimo.y, maximo.y);
	limites_eixo(zs, minimo.z, maximo.z);

	objeto.caixa_min = minimo;
	objeto.caixa_max = maximo;
	objeto.centro = {
		(minimo.x + maximo.x) / 2.0,
		(minimo.y + maximo.y) / 2.0,
		(minimo.z + maximo.z) / 2.0
	};

	double maior_lado = fmax(maximo.x - minimo.x, fmax(maximo.y - minimo.y, maximo.z - minimo.z));
	objeto.normalizacao = (maior_lado > 0) ? 2.0 / maior_lado : 1.0;

	objeto.meia_caixa = {
		(maximo.x - minimo.x) / 2.0 * objeto.normalizacao,
		(maximo.y - minimo.y) / 2.0 * objeto.normalizacao,
		(maximo.z - minimo.z) / 2.0 * objeto.normalizacao
	};
}

bool fora_da_caixa(const Objeto& objeto, const vertice& v) {
	return v.x < objeto.caixa_min.x || v.x > objeto.caixa_max.x
		|| v.y < objeto.caixa_min.y || v.y > objeto.caixa_max.y
		|| v.z < objeto.caixa_min.z || v.z > objeto.caixa_max.z;
}

// Grava os triangulos numa display list. Cada vertice e enviado com a sua
// normal (usada no calculo da iluminacao) e a sua coordenada de textura.
// Triangulos com os 3 vertices fora da caixa envolvente (geometria isolada,
// como o plano de chao do elepham.obj) nao sao desenhados; devolve quantos.
int criar_lista(Objeto& objeto) {
	int ignorados = 0;

	objeto.lista = glGenLists(1);
	glNewList(objeto.lista, GL_COMPILE);
	glBegin(GL_TRIANGLES);

	for (const face& f : objeto.faces) {
		if (fora_da_caixa(objeto, objeto.vertices[f[0].v])
			&& fora_da_caixa(objeto, objeto.vertices[f[1].v])
			&& fora_da_caixa(objeto, objeto.vertices[f[2].v])) {
			ignorados++;
			continue;
		}

		for (const indice_face& canto : f) {
			const vertice& n = objeto.normais[canto.vn];
			glNormal3d(n.x, n.y, n.z);

			const vertice& v = objeto.vertices[canto.v];

			if (canto.vt >= 0) {
				const coord_textura& t = objeto.texturas[canto.vt];
				glTexCoord2d(t.u, t.v);
			}
			else {
				// Sem vt no arquivo: projeta a textura de frente sobre o modelo
				// (plano XY), usando a caixa envolvente para levar x e y a 0..1
				glTexCoord2d((v.x - objeto.centro.x) * objeto.normalizacao / 2.0 + 0.5,
					(v.y - objeto.centro.y) * objeto.normalizacao / 2.0 + 0.5);
			}

			glVertex3d(v.x, v.y, v.z);
		}
	}

	glEnd();
	glEndList();

	return ignorados;
}

// Carrega cada arquivo e distribui os objetos lado a lado no eixo X.
void carregar_cena(const std::vector<std::string>& arquivos) {
	for (const std::string& caminho : arquivos) {
		Objeto objeto;
		objeto.nome = caminho.substr(caminho.find_last_of("/\\") + 1);
		if (!carregar_obj(caminho, objeto)) continue;

		objeto.cor = CORES[objetos.size() % (sizeof(CORES) / sizeof(CORES[0]))];
		objeto.textura = -1;
		objetos.push_back(objeto);
	}

	// Grade com COLUNAS objetos por linha, centralizada na origem
	int total = (int)objetos.size();
	int linhas = (total + COLUNAS - 1) / COLUNAS;
	for (int i = 0; i < total; i++) {
		int linha = i / COLUNAS;
		int coluna = i % COLUNAS;
		int nesta_linha = (linha == linhas - 1) ? total - linha * COLUNAS : COLUNAS;

		double x = (coluna - (nesta_linha - 1) / 2.0) * ESPACAMENTO;
		double y = ((linhas - 1) / 2.0 - linha) * ESPACAMENTO;
		objetos[i].posicao_inicial = { x, y, 0 };
		reiniciar(objetos[i]);
	}
}

void inicializar_opengl() {
	glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glShadeModel(GL_SMOOTH);

	// glScale tambem estica as normais; GL_NORMALIZE as devolve ao tamanho 1,
	// senao a iluminacao fica errada quando o objeto e escalado.
	glEnable(GL_NORMALIZE);

	// A cor do objeto (glColor) vira a componente ambiente e difusa do
	// material. O brilho especular e o mesmo para todos.
	GLfloat especular_material[] = { 0.6f, 0.6f, 0.6f, 1.0f };
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, especular_material);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 40.0f);

	// GL_MODULATE multiplica a textura pela cor ja iluminada, entao as luzes
	// continuam valendo com a textura ligada.
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	configurar_luzes();
}

void configurar_luzes() {
	// Uma luz ambiente global bem fraca, para o objeto nao sumir por completo
	// quando as tres luzes estiverem desligadas.
	GLfloat ambiente_global[] = { 0.05f, 0.05f, 0.05f, 1.0f };
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambiente_global);
	// Calcula o reflexo especular a partir da posicao real da camera
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

	glEnable(GL_LIGHTING);
	for (const Luz& luz : luzes) {
		glLightfv(luz.id, GL_AMBIENT, luz.ambiente);
		glLightfv(luz.id, GL_DIFFUSE, luz.difusa);
		glLightfv(luz.id, GL_SPECULAR, luz.especular);
		if (luz.ligada) glEnable(luz.id);
		else glDisable(luz.id);
	}
}

void posicionar_luzes() {
	for (const Luz& luz : luzes)
		glLightfv(luz.id, GL_POSITION, luz.posicao);
}

void alternar_luz(int indice) {
	Luz& luz = luzes[indice];
	luz.ligada = !luz.ligada;
	if (luz.ligada) glEnable(luz.id);
	else glDisable(luz.id);

	std::cout << luz.nome << (luz.ligada ? " ligada\n" : " desligada\n");
}

void carregar_texturas(const std::vector<std::string>& arquivos) {
	for (const std::string& caminho : arquivos) {
		GLuint id = carregar_bmp(caminho);
		if (id == 0) continue;

		Textura textura;
		textura.nome = caminho.substr(caminho.find_last_of("/\\") + 1);
		textura.id = id;
		texturas_bmp.push_back(textura);
	}
	std::cout << texturas_bmp.size() << " texturas .bmp carregadas\n";
}

// Leitura de um .bmp de 24 bits sem compressao. O arquivo comeca com um
// cabecalho de 54 bytes (14 do arquivo + 40 da imagem) e depois vem os pixels.
GLuint carregar_bmp(const std::string& caminho) {
	std::ifstream arquivo(caminho, std::ios::binary);
	if (!arquivo.is_open()) {
		std::cout << "textura nao encontrada: " << caminho << "\n";
		return 0;
	}

	unsigned char cabecalho[54];
	arquivo.read((char*)cabecalho, 54);
	if (!arquivo || cabecalho[0] != 'B' || cabecalho[1] != 'M') {
		std::cout << "arquivo nao e um .bmp: " << caminho << "\n";
		return 0;
	}

	int inicio_pixels = ler_inteiro(cabecalho, 10);
	int largura = ler_inteiro(cabecalho, 18);
	int altura = ler_inteiro(cabecalho, 22);
	int bits_por_pixel = cabecalho[28] | (cabecalho[29] << 8);
	int compressao = ler_inteiro(cabecalho, 30);
	if (bits_por_pixel != 24 || compressao != 0 || largura <= 0 || altura == 0) {
		std::cout << "so .bmp de 24 bits sem compressao e suportado: " << caminho << "\n";
		return 0;
	}

	// Altura positiva: as linhas estao de baixo para cima, como o OpenGL espera.
	// Altura negativa: de cima para baixo, entao e preciso inverter.
	bool de_cima_para_baixo = altura < 0;
	if (de_cima_para_baixo) altura = -altura;

	// Cada linha do arquivo e completada com zeros ate um multiplo de 4 bytes
	int bytes_por_linha = (largura * 3 + 3) / 4 * 4;
	std::vector<unsigned char> linha(bytes_por_linha);
	std::vector<GLubyte> pixels(largura * altura * 3);

	arquivo.seekg(inicio_pixels);
	for (int y = 0; y < altura; y++) {
		arquivo.read((char*)linha.data(), bytes_por_linha);
		int destino = de_cima_para_baixo ? altura - 1 - y : y;

		// O BMP guarda cada pixel na ordem azul, verde, vermelho
		for (int x = 0; x < largura; x++) {
			pixels[(destino * largura + x) * 3 + 0] = linha[x * 3 + 2];
			pixels[(destino * largura + x) * 3 + 1] = linha[x * 3 + 1];
			pixels[(destino * largura + x) * 3 + 2] = linha[x * 3 + 0];
		}
	}
	if (!arquivo) {
		std::cout << "arquivo .bmp incompleto: " << caminho << "\n";
		return 0;
	}

	GLuint id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	// Gera as versoes reduzidas da imagem (mipmaps) e aceita qualquer tamanho
	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, largura, altura, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

	return id;
}

// Os numeros do cabecalho do .bmp tem 4 bytes, do menos para o mais significativo
int ler_inteiro(const unsigned char* bytes, int posicao) {
	return bytes[posicao] | (bytes[posicao + 1] << 8) | (bytes[posicao + 2] << 16) | (bytes[posicao + 3] << 24);
}

// As transformacoes sao feitas com as funcoes do OpenGL. A ordem no codigo e
// a inversa da ordem aplicada ao vertice: primeiro o modelo e centralizado e
// normalizado, depois escalado, rotacionado e por ultimo transladado. Como o
// modelo ja esta centrado na origem, escala e rotacao acontecem em torno do
// centro do objeto.
void desenhar(const Objeto& objeto, bool selecionado) {
	glPushMatrix();

	glTranslated(objeto.posicao.x, objeto.posicao.y, objeto.posicao.z);
	glRotated(objeto.rotacao.x, 1, 0, 0);
	glRotated(objeto.rotacao.y, 0, 1, 0);
	glRotated(objeto.rotacao.z, 0, 0, 1);
	glScaled(objeto.escala.x, objeto.escala.y, objeto.escala.z);

	if (selecionado) desenhar_caixa(objeto.meia_caixa);

	// Com textura, a cor base e branca para a imagem aparecer com as cores
	// originais (ainda iluminada pelas luzes)
	if (objeto.textura >= 0) {
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, texturas_bmp[objeto.textura].id);
		glColor3d(1, 1, 1);
	}
	else {
		glColor3d(objeto.cor.x, objeto.cor.y, objeto.cor.z);
	}
	glScaled(objeto.normalizacao, objeto.normalizacao, objeto.normalizacao);
	glTranslated(-objeto.centro.x, -objeto.centro.y, -objeto.centro.z);
	glCallList(objeto.lista);

	glDisable(GL_TEXTURE_2D);
	glPopMatrix();
}

// Caixa envolvente do objeto selecionado: os mesmos 8 vertices e 12 arestas
// do cubo do M1, desenhados com GL_LINES e sem iluminacao.
void desenhar_caixa(vertice m) {
	m = { m.x * 1.05, m.y * 1.05, m.z * 1.05 }; // folga para nao encostar no modelo

	vertice cantos[8] = {
		{ -m.x, -m.y, -m.z }, { m.x, -m.y, -m.z }, { m.x, m.y, -m.z }, { -m.x, m.y, -m.z },
		{ -m.x, -m.y,  m.z }, { m.x, -m.y,  m.z }, { m.x, m.y,  m.z }, { -m.x, m.y,  m.z },
	};
	int arestas[12][2] = {
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
		{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
	};

	glDisable(GL_LIGHTING);
	glColor3d(1.0, 0.85, 0.2);
	glBegin(GL_LINES);
	for (int i = 0; i < 12; i++) {
		vertice origem = cantos[arestas[i][0]];
		vertice destino = cantos[arestas[i][1]];
		glVertex3d(origem.x, origem.y, origem.z);
		glVertex3d(destino.x, destino.y, destino.z);
	}
	glEnd();
	glEnable(GL_LIGHTING);
}

// Uma esfera pequena na posicao de cada luz: na cor da luz quando ligada,
// cinza quando desligada.
void desenhar_luzes() {
	glDisable(GL_LIGHTING);
	for (const Luz& luz : luzes) {
		glPushMatrix();
		glTranslatef(luz.posicao[0], luz.posicao[1], luz.posicao[2]);
		if (luz.ligada) glColor3fv(luz.difusa);
		else glColor3d(0.3, 0.3, 0.3);
		glutSolidSphere(0.1, 16, 16);
		glPopMatrix();
	}
	glEnable(GL_LIGHTING);
}

// Texto na tela com o estado atual. Troca temporariamente para uma projecao
// ortogonal em pixels e depois restaura as matrizes.
void desenhar_hud() {
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, largura_janela, 0, altura_janela);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	int y = altura_janela - 22;
	const Objeto& atual = objetos[selecionado];

	glColor3d(1.0, 0.85, 0.2);
	desenhar_texto(12, y, "[TAB] Objeto: " + atual.nome + " (" + std::to_string(selecionado + 1)
		+ "/" + std::to_string(objetos.size()) + ")");

	for (int i = 0; i < 3; i++) {
		y -= 18;
		if (luzes[i].ligada) glColor3fv(luzes[i].difusa);
		else glColor3d(0.45, 0.45, 0.45);
		desenhar_texto(12, y, "[" + std::to_string(i + 1) + "] " + luzes[i].nome
			+ (luzes[i].ligada ? ": ligada" : ": desligada"));
	}

	y -= 18;
	glColor3d(0.8, 0.8, 0.8);
	desenhar_texto(12, y, "[T] Textura: " + (atual.textura >= 0 ? texturas_bmp[atual.textura].nome : std::string("nenhuma"))
		+ "   [M] Desenho: " + (modo_malha ? "arestas" : "faces"));

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
}

void desenhar_texto(int x, int y, const std::string& texto) {
	glRasterPos2i(x, y);
	glutBitmapString(GLUT_BITMAP_9_BY_15, (const unsigned char*)texto.c_str());
}

// As funcoes abaixo so atualizam o estado do objeto; a transformacao em si e
// aplicada em desenhar() com glTranslated, glRotated e glScaled.

void movimentar(Objeto& objeto, double distancia_x, double distancia_y, double distancia_z) {
	objeto.posicao.x += distancia_x;
	objeto.posicao.y += distancia_y;
	objeto.posicao.z += distancia_z;
}

void escalar(Objeto& objeto, double escala_x, double escala_y, double escala_z) {
	objeto.escala.x *= escala_x;
	objeto.escala.y *= escala_y;
	objeto.escala.z *= escala_z;
}

void rotacionar(Objeto& objeto, double angulo_x, double angulo_y, double angulo_z) {
	objeto.rotacao.x = fmod(objeto.rotacao.x + angulo_x, 360.0);
	objeto.rotacao.y = fmod(objeto.rotacao.y + angulo_y, 360.0);
	objeto.rotacao.z = fmod(objeto.rotacao.z + angulo_z, 360.0);
}

void reiniciar(Objeto& objeto) {
	objeto.posicao = objeto.posicao_inicial;
	objeto.escala = { 1, 1, 1 };
	objeto.rotacao = ROTACAO_INICIAL;
}

void imprimir_controles() {
	std::cout << "\nControles (valem para o objeto selecionado):\n";
	std::cout << "  TAB            - selecionar o proximo objeto\n";
	std::cout << "  Setas          - rotacionar nos eixos X e Y\n";
	std::cout << "  J / L          - rotacionar no eixo Z\n";
	std::cout << "  Arrastar (esq) - rotacionar nos eixos X e Y\n";
	std::cout << "  W A S D        - mover no plano XY\n";
	std::cout << "  Q / E          - mover no eixo Z (aproximar / afastar)\n";
	std::cout << "  Arrastar (dir) - mover no plano XY\n";
	std::cout << "  + / -          - escala uniforme\n";
	std::cout << "  Roda do mouse  - escala uniforme\n";
	std::cout << "  x X y Y z Z    - escala em um eixo por vez\n";
	std::cout << "  R              - reiniciar o objeto\n";
	std::cout << "  1 / 2 / 3      - ligar / desligar cada luz\n";
	std::cout << "  T              - trocar a textura do objeto (nenhuma ou um dos .bmp)\n";
	std::cout << "  M              - alternar entre faces e arestas\n";
	std::cout << "  ESC            - sair\n\n";
}
