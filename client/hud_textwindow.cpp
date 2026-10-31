#include "hud.h"           // primeiro: define CHudBase e inclui hud_textwindow.h
#include "hud_textwindow.h"
#include "utils.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "gl_local.h"
#include "hud_spriteutil.h"

// RTN F10: JANELA DE DOCUMENTO (estilo Paranoia 2).
// Recebe "TextWindow" (string = nome do arquivo) do trigger_textwindow,
// abre texts/<nome>.txt, parseia o HEAD (tamanho/imagem/botao) e exibe a
// janela em HUD 2D: fundo escurecido + imagem do documento (TGA com alpha)
// + botao fechar. O jogo pausa com a janela aberta (showpause 0 esconde o
// icone "Paused" da engine, que desenharia POR CIMA do HUD).
// Fechar: ESC/ENTER (HUD_Key_Event) ou clique no botao X.

DECLARE_MESSAGE( m_TextWindow, TextWindow );

#define RTN_TW_BUTTON_SIZE	24	// tamanho do botao X (pixels)

// Resolve o valor de "background"/"imgbutton" do <HEAD> do documento para um
// sprite. O autor do documento pode ter escrito um caminho de .tga (como no
// Paranoia 2) ou de .spr; este jogo so desenha .spr. Ordem de tentativa:
//   1) o proprio valor, se ja termina em .spr
//   2) valor + ".spr", se nao tem extensao (ex.: "sprites/vgui/backpane12")
//   3) sprites/rtn_tw_<nome base>.spr - o .spr gerado de um .tga com
//        python3 tools/tga2spr.py texts/images/foo.tga game_dir/sprites/rtn_tw_foo.spr
// Devolve 0 e avisa no console se nenhum existir (documento abre sem a imagem).
static SpriteHandle RTN_TW_ResolveSprite( const char *pszImage )
{
	if( !pszImage || !pszImage[0] )
		return 0;

	char szPath[128];
	const char *pExt = strrchr( pszImage, '.' );
	const char *pSlash = strrchr( pszImage, '/' );
	bool hasExt = pExt && ( !pSlash || pExt > pSlash );

	if( hasExt && !Q_stricmp( pExt, ".spr" ))
	{
		SpriteHandle h = RTN_LoadOptionalSprite( pszImage );
		if( h ) return h;
	}
	else if( !hasExt )
	{
		Q_snprintf( szPath, sizeof( szPath ), "%s.spr", pszImage );
		SpriteHandle h = RTN_LoadOptionalSprite( szPath );
		if( h ) return h;
	}

	// nome base sem diretorio e sem extensao
	const char *pBase = pSlash ? pSlash + 1 : pszImage;
	char szBase[64];
	Q_strncpy( szBase, pBase, sizeof( szBase ));
	char *pDot = strrchr( szBase, '.' );
	if( pDot ) *pDot = 0;

	Q_snprintf( szPath, sizeof( szPath ), "sprites/rtn_tw_%s.spr", szBase );
	SpriteHandle h = RTN_LoadOptionalSprite( szPath );
	if( !h )
		gEngfuncs.Con_Printf( "RTN TextWindow: sem sprite para '%s' (esperado %s - gere com tools/tga2spr.py)\n", pszImage, szPath );
	return h;
}

int CHudTextWindow::Init( void )
{
	gHUD.AddHudElem( this );
	HOOK_MESSAGE( TextWindow );
	m_iFlags |= HUD_ACTIVE;
	m_bOpen = false;
	m_bWasAttack = false;
	m_hPanelSpr = m_hButtonSpr = 0;
	return 1;
}

int CHudTextWindow::VidInit( void )
{
	m_bOpen = false;
	m_hPanelSpr = m_hButtonSpr = 0;	// handles de .spr nao sobrevivem a troca de mapa
	return 1;
}

void CHudTextWindow::Reset( void )
{
	if( m_bOpen )
		Close();
}

int CHudTextWindow::MsgFunc_TextWindow( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	const char *szFile = READ_STRING();
	END_READ();

	Open( szFile );
	return 1;
}

void CHudTextWindow::Open( const char *pszFileName )
{
	if( !pszFileName || !pszFileName[0] )
		return;

	// se ja estiver aberto, nao acumula
	if( m_bOpen )
		return;

	// RTN F10 fix: normaliza o nome - o user poe no message da entidade
	// 'doc_cadaver' OU 'texts/doc_cadaver.txt'; o Open monta texts/<nome>.txt
	// -> SEM a normalizacao ficava texts/texts/doc_cadaver.txt.txt (nao abria)
	char szName[64];
	Q_strncpy( szName, pszFileName, sizeof( szName ));
	if( !Q_strnicmp( szName, "texts/", 6 ))  // tira o 'texts/' do inicio
		memmove( szName, szName + 6, Q_strlen( szName + 6 ) + 1 );
	int iLen = Q_strlen( szName );
	if( iLen > 4 && !Q_stricmp( szName + iLen - 4, ".txt" ))  // tira o '.txt' do fim
		szName[iLen-4] = 0;

	Q_strncpy( m_szFileName, szName, sizeof( m_szFileName ));
	ParseDocument();

	// Carrega os sprites UMA vez (no Open), nao a cada frame. Textura crua
	// (LOAD_TEXTURE de .tga) gera GL_INVALID_ENUM todo frame neste engine, entao
	// as imagens do documento sao .spr v32 - ver RTN_TW_ResolveSprite().
	m_hPanelSpr = RTN_TW_ResolveSprite( m_szPanelImage );
	m_hButtonSpr = m_szButtonImage[0] ? RTN_TW_ResolveSprite( m_szButtonImage ) : 0;

	// pausa o jogo e esconde o icone "Paused" da engine (showpause 0)
	gEngfuncs.pfnClientCmd( "showpause 0\n" );
	gEngfuncs.pfnClientCmd( "pause\n" );

	m_bOpen = true;
	m_bWasAttack = false;
}

void CHudTextWindow::Close( void )
{
	if( !m_bOpen )
		return;

	m_bOpen = false;

	// handles de .spr sao do engine, nao ha o que liberar - so esquece
	m_hPanelSpr = 0;
	m_hButtonSpr = 0;

	// despausa e restaura o icone de pausa
	gEngfuncs.pfnClientCmd( "showpause 1\n" );
	gEngfuncs.pfnClientCmd( "pause\n" );
}

void CHudTextWindow::ParseDocument( void )
{
	// defaults
	Q_strncpy( m_szPanelImage, "sprites/vgui_backpanel1.spr", sizeof( m_szPanelImage ));
	m_szButtonImage[0] = 0;
	m_iXSize = 400;
	m_iYSize = 400;
	m_iScrollX = 0; m_iScrollY = 0; m_iScrollW = 0; m_iScrollH = 0;

	// le texts/<nome>.txt
	char szPath[128];
	Q_snprintf( szPath, sizeof( szPath ), "texts/%s.txt", m_szFileName );

	int iSize = 0;
	byte *pFile = (byte *)gEngfuncs.COM_LoadFile( szPath, 5, &iSize );
	if( !pFile )
	{
		gEngfuncs.Con_Printf( "RTN TextWindow: unable to load %s\n", szPath );
		return;
	}

	// parseia o <HEAD ...> - extrai xsize/ysize/background/imgbutton/scrollpos
	// RTN F10 fix: parser REESCRITO (metodo robusto nome=valor, com/sem aspas)
	// - o anterior (loop com p--/pName) extraia valores errados em casos de
	// borda (ex: 'sprites/vgui/backpane12' e 'game' no arquivo do user).
	char *pText = (char *)pFile;
	char *pHead = strstr( pText, "<HEAD" );
	if( pHead )
	{
		char *p = pHead + 5;  // depois do "<HEAD"
		while( p && *p && *p != '>' )
		{
			while( *p == ' ' || *p == '	' ) p++;
			if( *p == '>' || !*p ) break;

			// le o nome do parametro (letras/digitos ate o '=')
			char szParam[32];
			int i = 0;
			while( *p && *p != '=' && *p != ' ' && *p != '>' && i < 31 )
				szParam[i++] = *p++;
			szParam[i] = 0;
			if( *p != '=' ) break;  // sem '=' -> formato invalido, para
			p++;  // pula o '='

			// le o valor (entre aspas OU ate o espaco/>)
			char szValue[128];
			int j = 0;
			if( *p == '"' )
			{
				p++;
				while( *p && *p != '"' && j < 127 )
					szValue[j++] = *p++;
				if( *p == '"' ) p++;
			}
			else
			{
				while( *p && *p != ' ' && *p != '>' && j < 127 )
					szValue[j++] = *p++;
			}
			szValue[j] = 0;

			// aplica o parametro (desconhecidos - imgscroll/buttoncolor - ignorados)
			if( !Q_stricmp( szParam, "xsize" ))
				m_iXSize = atoi( szValue );
			else if( !Q_stricmp( szParam, "ysize" ))
				m_iYSize = atoi( szValue );
			else if( !Q_stricmp( szParam, "background" ))
				Q_strncpy( m_szPanelImage, szValue, sizeof( m_szPanelImage ));
			else if( !Q_stricmp( szParam, "imgbutton" ))
				Q_strncpy( m_szButtonImage, szValue, sizeof( m_szButtonImage ));
			else if( !Q_stricmp( szParam, "scrollpos" ))
			{
				// "x y w h"
				int vals[4] = { 0, 0, 0, 0 };
				if( sscanf( szValue, "%d %d %d %d", &vals[0], &vals[1], &vals[2], &vals[3] ) == 4 )
				{
					m_iScrollX = vals[0]; m_iScrollY = vals[1];
					m_iScrollW = vals[2]; m_iScrollH = vals[3];
				}
			}
		}
	}

	gEngfuncs.COM_FreeFile( pFile );
}

int CHudTextWindow::Draw( float flTime )
{
	if( !m_bOpen )
		return 0;

	extern cvar_t *rtn_hud_style;
	(void)rtn_hud_style;  // RTN F10 fix: o documento desenha SEMPRE (o if do
	// rtn_hud_style < 1 impedia a janela no estilo classico - jogo pausava
	// preso na tela preta sem o documento visivel!)

	int cx = ScreenWidth / 2;
	int cy = ScreenHeight / 2;
	int w = m_iXSize;
	int h = m_iYSize;
	int x = cx - w / 2;
	int y = cy - h / 2;

	// RTN F10: fundo preto de tela cheia REMOVIDO a pedido do user - a cena
	// fica totalmente visivel atras do documento (imersao estilo Tarkov).
	// Se faltar contraste p/ ler, da pra voltar com alpha bem baixo (~0.12).

	// painel do documento (a imagem - texts/images/*.tga ou o sprite)
	if( m_hPanelSpr )
		RTN_DrawSpriteStretched( m_hPanelSpr, x, y, x + w, y + h, 1.0f, 1.0f, 1.0f, 1.0f );

	// botao fechar (X) - no canto superior direito do painel
	if( m_hButtonSpr )
	{
		int btnX = x + w - RTN_TW_BUTTON_SIZE - 6;
		int btnY = y + 6;
		RTN_DrawSpriteStretched( m_hButtonSpr, btnX, btnY, btnX + RTN_TW_BUTTON_SIZE, btnY + RTN_TW_BUTTON_SIZE, 1.0f, 1.0f, 1.0f, 1.0f );

		// clique no botao (mouse + IN_ATTACK, edge detect)
		int mx = 0, my = 0;
		gEngfuncs.GetMousePosition( &mx, &my );
		bool bAttack = ( gHUD.m_iKeyBits & IN_ATTACK ) != 0;
		if( bAttack && !m_bWasAttack )
		{
			if( mx >= btnX && mx <= btnX + RTN_TW_BUTTON_SIZE &&
			    my >= btnY && my <= btnY + RTN_TW_BUTTON_SIZE )
			{
				Close();
				m_bWasAttack = bAttack;
				return 1;
			}
		}
		m_bWasAttack = bAttack;
	}

	// RTN F10 fix: restaura o estado GL (o blend desligado p/ nao vazar
	// para os outros elementos do HUD)
	GL_Blend( GL_FALSE );
	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );

	return 1;
}
