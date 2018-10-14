/******************************************************************************
 * Projekt - Zaklady pocitacove grafiky - IZG
 * spanel@fit.vutbr.cz
 *
 * $Id: student.c 337 2014-02-25 06:52:49Z spanel $
 */

#include "student.h"
#include "transform.h"
#include "bmp.h"

#include <memory.h>
#define _USE_MATH_DEFINES
#include <math.h>


/******************************************************************************
 * Globalni promenne a konstanty
 */

/* rozmer textury */
const int       TEXTURE_SIZE    = 512;

/* pocet policek sachovnice */
const int       NUM_OF_TILES    = 16;

/* barva poli */
const S_RGBA    BLACK_TILE      = { 75, 75, 75 };
const S_RGBA    WHITE_TILE      = { 255, 255, 255 };

// Uzivatelska definice barev
const int		WHITE			= 1;
const int		BLACK			= 2;

// Rozmer jednooh policka, vypocet: TEXTURE_SIZE / NUM_OF_TILES
const int		ROZMER_POLICKA  = 32;


/*****************************************************************************
 * Funkce vytvori vas renderer a nainicializuje jej
 */

S_Renderer * studrenCreate()
{
	int sloupec;
	int barva_policka;
	int i;

    S_StudentRenderer * renderer = (S_StudentRenderer *)malloc(sizeof(S_StudentRenderer));
    IZG_CHECK(renderer, "Cannot allocate enough memory");

    /* inicializace default rendereru */
    renInit(&renderer->base);

    /* nastaveni ukazatelu na upravene funkce */
	renderer->base.releaseFunc = studrenRelease;
	renderer->base.projectTriangleFunc = studrenProjectTriangle;

    /* inicializace nove pridanych casti */
	// Alokace pameti pro texturu.
	renderer->textura_pole = (S_RGBA *) malloc ((TEXTURE_SIZE * TEXTURE_SIZE) * sizeof(S_RGBA *));


	// Inicializace textury
	barva_policka = WHITE;
	i = 1;

	for(sloupec = 0; sloupec < (TEXTURE_SIZE * TEXTURE_SIZE); sloupec++){
		if(sloupec != (TEXTURE_SIZE * ROZMER_POLICKA * i)){ // Pokud nejsme na konci radku
			// Zmena barvy po vykresleni 32 pixelu => sirka jednoho policka
			if((sloupec != 0) && ((sloupec % ROZMER_POLICKA) == 0)){ 
				if(barva_policka == WHITE)
					barva_policka = BLACK;
				else
					barva_policka = WHITE;
			}
		}

		else // Pokud jsme na konci radku
			i++;

		// Prirazeni barvy pro jeden pixel textury
		if(barva_policka == WHITE)
			renderer->textura_pole [sloupec] = WHITE_TILE;
		else
			renderer->textura_pole [sloupec] = BLACK_TILE;
	}

    return (S_Renderer *)renderer;
}

/*****************************************************************************
 * Funkce korektne zrusi renderer a uvolni pamet
 */

void studrenRelease(S_Renderer **ppRenderer)
{
    S_StudentRenderer * renderer;

    if( ppRenderer && *ppRenderer )
    {
        renderer = (S_StudentRenderer *)(*ppRenderer);

        /* pripadne uvolneni pameti */
        free(renderer->textura_pole);

        /* fce default rendereru */
        renRelease(ppRenderer);
    }
}

/******************************************************************************
 * Nova fce pro rasterizaci trojuhelniku do frame bufferu
 * s podporou texturovani a interpolaci texturovacich souøadnic
 * Pozn.: neni nutné øešit perspektivní korekci textury
 * v1, v2, v3 - ukazatele na vrcholy trojuhelniku ve 3D pred projekci
 * n1, n2, n3 - ukazatele na normaly ve vrcholech ve 3D pred projekci
 * t1, t2, t3 - ukazatele na texturovaci souradnice vrcholu
 * x1, y1, ... - vrcholy trojuhelniku po projekci do roviny obrazovky
 */

void studrenDrawTriangle(S_Renderer *pRenderer,
                         S_Coords *v1, S_Coords *v2, S_Coords *v3,
                         S_Coords *n1, S_Coords *n2, S_Coords *n3,
                         S_Coords *t1, S_Coords *t2, S_Coords *t3,
                         int x1, int y1,
                         int x2, int y2,
                         int x3, int y3
                         )
{
    int         minx, miny, maxx, maxy;
    int         a1, a2, a3, b1, b2, b3, c1, c2, c3;
    int         s1, s2, s3;
    int         x, y, e1, e2, e3;
    double      alpha, beta, w1, w2, w3, z, u, v, green, blue, red;
    S_RGBA      col1, col2, col3, color, barva_textury;


    IZG_ASSERT(pRenderer && v1 && v2 && v3 && n1 && n2 && n3);

    /* vypocet barev ve vrcholech */
    col1 = pRenderer->calcReflectanceFunc(pRenderer, v1, n1);
    col2 = pRenderer->calcReflectanceFunc(pRenderer, v2, n2);
    col3 = pRenderer->calcReflectanceFunc(pRenderer, v3, n3);

    /* obalka trojuhleniku */
    minx = MIN(x1, MIN(x2, x3));
    maxx = MAX(x1, MAX(x2, x3));
    miny = MIN(y1, MIN(y2, y3));
    maxy = MAX(y1, MAX(y2, y3));

    /* oriznuti podle rozmeru okna */
    miny = MAX(miny, 0);
    maxy = MIN(maxy, pRenderer->frame_h - 1);
    minx = MAX(minx, 0);
    maxx = MIN(maxx, pRenderer->frame_w - 1);

    /* Pineduv alg. rasterizace troj.
       hranova fce je obecna rovnice primky Ax + By + C = 0
       primku prochazejici body (x1, y1) a (x2, y2) urcime jako
       (y1 - y2)x + (x2 - x1)y + x1y2 - x2y1 = 0 */

    /* normala primek - vektor kolmy k vektoru mezi dvema vrcholy, tedy (-dy, dx) */
    a1 = y1 - y2;
    a2 = y2 - y3;
    a3 = y3 - y1;
    b1 = x2 - x1;
    b2 = x3 - x2;
    b3 = x1 - x3;

    /* koeficient C */
    c1 = x1 * y2 - x2 * y1;
    c2 = x2 * y3 - x3 * y2;
    c3 = x3 * y1 - x1 * y3;

    /* vypocet hranove fce (vzdalenost od primky) pro protejsi body */
    s1 = a1 * x3 + b1 * y3 + c1;
    s2 = a2 * x1 + b2 * y1 + c2;
    s3 = a3 * x2 + b3 * y2 + c3;

    /* normalizace, aby vzdalenost od primky byla kladna uvnitr trojuhelniku */
    if( s1 < 0 )
    {
        a1 *= -1;
        b1 *= -1;
        c1 *= -1;
    }
    if( s2 < 0 )
    {
        a2 *= -1;
        b2 *= -1;
        c2 *= -1;
    }
    if( s3 < 0 )
    {
        a3 *= -1;
        b3 *= -1;
        c3 *= -1;
    }

    /* koeficienty pro barycentricke souradnice */
    alpha = 1.0 / ABS(s2);
    beta = 1.0 / ABS(s3);
    /*gamma = 1.0 / ABS(s1);*/

    /* vyplnovani... */
    for( y = miny; y <= maxy; ++y )
    {
        /* inicilizace hranove fce v bode (minx, y) */
        e1 = a1 * minx + b1 * y + c1;
        e2 = a2 * minx + b2 * y + c2;
        e3 = a3 * minx + b3 * y + c3;

        for( x = minx; x <= maxx; ++x )
        {
            if( e1 >= 0 && e2 >= 0 && e3 >= 0 )
            {
                /* interpolace pomoci barycentrickych souradnic
                   e1, e2, e3 je aktualni vzdalenost bodu (x, y) od primek */
                w1 = alpha * e2;
                w2 = beta * e3;
                w3 = 1.0 - w1 - w2;

                /* interpolace z-souradnice */
                z = w1 * v1->z + w2 * v2->z + w3 * v3->z;

				u = w1 * t1->x + w2 * t2->x + w3 * t3->x; 
				v = w1 * t1->y + w2 * t2->y + w3 * t3->y; 
               
				barva_textury = studrenTextureValue((S_StudentRenderer *) pRenderer, u, v);
				
				green = (double) barva_textury.green / (double) 255;
				blue = (double) barva_textury.blue / (double) 255;
				red = (double) barva_textury.red / (double) 255;

                /* interpolace barvy */
                color.red = ROUND2BYTE(w1 * col1.red + w2 * col2.red + w3 * col3.red);
                color.green = ROUND2BYTE(w1 * col1.green + w2 * col2.green + w3 * col3.green);
                color.blue = ROUND2BYTE(w1 * col1.blue + w2 * col2.blue + w3 * col3.blue);
                color.alpha = 255;

				color.red *= red;
				color.blue *= blue;
				color.green *= green;

                /* vykresleni bodu */
                if( z < DEPTH(pRenderer, x, y) )
                {
                    PIXEL(pRenderer, x, y) = color;
                    DEPTH(pRenderer, x, y) = z;
                }
            }

            /* hranova fce o pixel vedle */
            e1 += a1;
            e2 += a2;
            e3 += a3;
        }


    }
}

/******************************************************************************
 * Vykresli i-ty trojuhelnik modelu pomoci nove fce studrenDrawTriangle()
 * Pred vykreslenim aplikuje na vrcholy a normaly trojuhelniku
 * aktualne nastavene transformacni matice!
 * i - index trojuhelniku
 */

void studrenProjectTriangle(S_Renderer *pRenderer, S_Model *pModel, int i)
{
    S_Coords    aa, bb, cc;             /* souradnice vrcholu po transformaci */
    S_Coords    naa, nbb, ncc;          /* normaly ve vrcholech po transformaci */
	S_Coords	*dd, *ee, *ff;
    S_Coords    nn;                     /* normala trojuhelniku po transformaci */
    int         u1, v1, u2, v2, u3, v3; /* souradnice vrcholu po projekci do roviny obrazovky */
    S_Triangle  * triangle;

    IZG_ASSERT(pRenderer && pModel && i >= 0 && i < trivecSize(pModel->triangles));

    /* z modelu si vytahneme trojuhelnik */
    triangle = trivecGetPtr(pModel->triangles, i);

    /* transformace vrcholu matici model */
    trTransformVertex(&aa, cvecGetPtr(pModel->vertices, triangle->v[0]));
    trTransformVertex(&bb, cvecGetPtr(pModel->vertices, triangle->v[1]));
    trTransformVertex(&cc, cvecGetPtr(pModel->vertices, triangle->v[2]));

    /* promitneme vrcholy trojuhelniku na obrazovku */
    trProjectVertex(&u1, &v1, &aa);
    trProjectVertex(&u2, &v2, &bb);
    trProjectVertex(&u3, &v3, &cc);

    /* pro osvetlovaci model transformujeme take normaly ve vrcholech */
    trTransformVector(&naa, cvecGetPtr(pModel->normals, triangle->v[0]));
    trTransformVector(&nbb, cvecGetPtr(pModel->normals, triangle->v[1]));
    trTransformVector(&ncc, cvecGetPtr(pModel->normals, triangle->v[2]));

    /* normalizace normal */
    coordsNormalize(&naa);
    coordsNormalize(&nbb);
    coordsNormalize(&ncc);

    /* transformace normaly trojuhelniku matici model */
    trTransformVector(&nn, cvecGetPtr(pModel->trinormals, triangle->n));
    
    /* normalizace normaly */
    coordsNormalize(&nn);

    /* je troj. privraceny ke kamere, tudiz viditelny? */
    if( !renCalcVisibility(pRenderer, &aa, &nn) )
    {
        /* odvracene troj. vubec nekreslime */
        return;
    }

	dd = cvecGetPtr(pModel->texcoords, triangle->v[0]);
	ee = cvecGetPtr(pModel->texcoords, triangle->v[1]);
	ff = cvecGetPtr(pModel->texcoords, triangle->v[2]);


    /* rasterizace trojuhelniku */
    studrenDrawTriangle(pRenderer,
						&aa, &bb, &cc,
						&naa, &nbb, &ncc,
						dd, ee, ff,
						u1, v1, u2, v2, u3, v3
					   );
}

/******************************************************************************
 * Vrací hodnotu v aktuálnì nastavené textuøe na zadaných
 * texturovacích souøadnicích u, v
 * Pro urèení hodnoty používá bilineární interpolaci
 * u, v - texturovací souøadnice v intervalu 0..1, který odpovídá šíøce/výšce textury
 */

S_RGBA studrenTextureValue(S_StudentRenderer * pRenderer, double u, double v)
{
	int x1, x2;
	int y1, y2;
	int r1_r, r1_g, r1_b;
	int r2_r, r2_g, r2_b;
	int p_r, p_g, p_b;
	
	u *= 512;
	v *= 512;

	x1 = u;
	x2 = u + 1;
	y1 = v;
	y2 = v + 1;


	// Binarni interpolace
	// Vypocet RGBA hodnot barvy pro R1
	r1_r = (((x2 - u)/(x2 - x1)) * pRenderer->textura_pole[(int) ((x1 * TEXTURE_SIZE) + y1)].red) + 
		   (((u - x1)/(x2 - x1)) * pRenderer->textura_pole[(int) ((x2 * TEXTURE_SIZE) + y1)].red);
	r1_g = (((x2 - u)/(x2 - x1)) * pRenderer->textura_pole[(int) ((x1 * TEXTURE_SIZE) + y1)].green) +
		   (((u - x1)/(x2 - x1)) * pRenderer->textura_pole[(int) ((x2 * TEXTURE_SIZE) + y1)].green);	
	r1_b = (((x2 - u)/(x2 - x1)) * pRenderer->textura_pole[(int) ((x1 * TEXTURE_SIZE) + y1)].blue) + 
		   (((u - x1)/(x2 - x1)) * pRenderer->textura_pole[(int) ((x2 * TEXTURE_SIZE) + y1)].blue);

	// Vypocet RGBA hodnot barvy pro R2
	r2_r = (((x2 - u)/(x2 - x1)) * pRenderer->textura_pole[(int) ((x1 * TEXTURE_SIZE) + y2)].red) + 
		   (((u - x1)/(x2 - x1)) * pRenderer->textura_pole[(int) ((x2 * TEXTURE_SIZE) + y2)].red);
	r2_g = (((x2 - u)/(x2 - x1)) * pRenderer->textura_pole[(int) ((x1 * TEXTURE_SIZE) + y2)].green) + 
		   (((u - x1)/(x2 - x1)) * pRenderer->textura_pole[(int) ((x2 * TEXTURE_SIZE) + y2)].green);
	r2_b = (((x2 - u)/(x2 - x1)) * pRenderer->textura_pole[(int) ((x1 * TEXTURE_SIZE) + y2)].blue) + 
		   (((u - x1)/(x2 - x1)) * pRenderer->textura_pole[(int) ((x2 * TEXTURE_SIZE) + y2)].blue);

	// Vypocet RGBA hodnot barvy pro vysledny bod P
	p_r = (((y2 - v)/(y2 - y1)) * r1_r) + (((v - y1)/(y2 - y1) * r2_r));
	p_g = (((y2 - v)/(y2 - y1)) * r1_g) + (((v - y1)/(y2 - y1) * r2_g));
	p_b = (((y2 - v)/(y2 - y1)) * r1_b) + (((v - y1)/(y2 - y1) * r2_b));

    return makeColor(p_r, p_g, p_b);
}


/******************************************************************************
 ******************************************************************************
 * Callback funkce volana pri startu aplikace
 * Doplnte automaticke vygenerovani a prirazeni texturovacich souradnic
 * vrcholum modelu s vyuzitim mapovani na kouli
 * Muzete predpokladat, ze model je umisten v pocatku souradneho systemu
 * a posunuti neni treba resit
 */

void onInit(S_Renderer *pRenderer, S_Model *pModel)
{
	int i;
	double u, v;
	S_Coords souradnice_vektoru;
	S_Coords *vektor;
	S_Coords tmp_vektor;

    for( i = 0; i < trivecSize(pModel->vertices); ++i )
    {
		vektor = cvecGetPtr(pModel->vertices, i);
		tmp_vektor = makeCoords(vektor->x, vektor->y, vektor->z);
		coordsNormalize(&tmp_vektor);

		u = (0.5) + ((atan2(tmp_vektor.z, tmp_vektor.x)) / (PI * 2)); 
		v = (0.5) - ((asin(tmp_vektor.y)) / PI); 

		souradnice_vektoru = makeCoords(u, v, 0);

		vecSet(pModel->texcoords, i, &souradnice_vektoru);
    }
}


/*****************************************************************************
 *****************************************************************************/
