#include "hud.h"           // primeiro: define CHudBase e inclui hud_textwindow.h
#include "hud_textwindow.h"
#include "utils.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "texture_handle.h"
#include "gl_local.h"      // GL_Bind

// tri.cpp - quad ortogonal 2D com a textura atual bindada
extern void OrthoQuad( int x1, int y1, int x2, int y2 );

// RTN F10: JANELA DE DOCUMENTO (estilo Paranoia 2).
// Recebe "TextWindow" (string = nome do arquivo) do trigger_textwindow,
// abre texts/<nome>.txt, parseia o HEAD (tamanho/imagem/botao) e exibe a
// janela em HUD 2D: fundo escurecido + imagem do documento (TGA com alpha)
// + botao fechar. O jogo pausa com a janela aberta (showpause 0 esconde o
// icone "Paused" da engine, que desenharia POR CIMA do HUD).
// Fechar: ESC/ENTER (HUD_Key_Event) ou clique no botao X.

DECLARE_MESSAGE( m_TextWindow, TextWindow );

#define RTN_TW_BUTTON_SIZE	24	// tamanho do botao X (pixels)

int CHudTextWindow::Init( void )
{
	gHUD.AddHudElem( this );
	HOOK_MESSAGE( TextWindow );
	m_iFlags |= HUD_ACTIVE;
	m_bOpen = false;
	m_bWasAttack = false;
	return 1;
}

int CHudTextWindow::VidInit( void )
{
	m_bOpen = false;
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

	Q_strncpy( m_szFileName, pszFileName, sizeof( m_szFileName ));
	ParseDocument();

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
	char *pText = (char *)pFile;
	char *pHead = strstr( pText, "<HEAD" );
	if( pHead )
	{
		char *p = pHead;
		while( p && *p && *p != '>' )
		{
			char szParam[32], szValue[64];
			szParam[0] = szValue[0] = 0;

			// procura o proximo parametro (letra seguida de =)
			p = strstr( p, "=" );
			if( !p ) break;
			p--;

			// volta ate o inicio do nome do parametro
			char *pName = p;
			while( pName > pHead && *(pName-1) != ' ' && *(pName-1) != '<' )
				pName--;

			int iNameLen = p - pName;
			if( iNameLen > 0 && iNameLen < (int)sizeof( szParam ))
			{
				Q_strncpy( szParam, pName, iNameLen + 1 );

				// valor: entre aspas ou ate o espaco
				p++; // pula o '='
				while( *p == ' ' ) p++;
				if( *p == '"' )
				{
					p++;
					char *pEnd = strstr( p, "\"" );
					if( !pEnd ) break;
					int iValLen = pEnd - p;
					if( iValLen < (int)sizeof( szValue ))
					{
						Q_strncpy( szValue, p, iValLen + 1 );
						p = pEnd + 1;
					}
					else break;
				}
				else
				{
					char *pEnd = p;
					while( *pEnd && *pEnd != ' ' && *pEnd != '>' ) pEnd++;
					int iValLen = pEnd - p;
					if( iValLen < (int)sizeof( szValue ))
					{
						Q_strncpy( szValue, p, iValLen + 1 );
						p = pEnd;
					}
					else break;
				}

				// aplica o parametro
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
					int n = sscanf( szValue, "%d %d %d %d", &vals[0], &vals[1], &vals[2], &vals[3] );
					if( n == 4 )
					{
						m_iScrollX = vals[0]; m_iScrollY = vals[1];
						m_iScrollW = vals[2]; m_iScrollH = vals[3];
					}
				}
			}
			else break;
		}
	}

	gEngfuncs.COM_FreeFile( pFile );
}

int CHudTextWindow::Draw( float flTime )
{
	if( !m_bOpen )
		return 0;

	extern cvar_t *rtn_hud_style;
	if( !rtn_hud_style || rtn_hud_style->value < 1.0f )
		return 0;

	int cx = ScreenWidth / 2;
	int cy = ScreenHeight / 2;
	int w = m_iXSize;
	int h = m_iYSize;
	int x = cx - w / 2;
	int y = cy - h / 2;

	// fundo escurecido (quad preto translucido em tela cheia)
	gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
	gEngfuncs.pTriAPI->Color4f( 0.0f, 0.0f, 0.0f, 0.6f );
	GL_Bind( 0, FIND_TEXTURE( "*white" ));
	OrthoQuad( 0, 0, ScreenWidth, ScreenHeight );

	// painel do documento (a imagem - texts/images/*.tga ou o sprite)
	TextureHandle hTex = LOAD_TEXTURE( m_szPanelImage, NULL, 0, TF_CLAMP | TF_IMAGE );
	if( hTex.Initialized( ))
	{
		gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
		gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
		GL_Bind( 0, hTex );
		OrthoQuad( x, y, x + w, y + h );
	}

	// botao fechar (X) - no canto superior direito do painel
	if( m_szButtonImage[0] )
	{
		TextureHandle hBtn = LOAD_TEXTURE( m_szButtonImage, NULL, 0, TF_CLAMP | TF_IMAGE );
		if( hBtn.Initialized( ))
		{
			int btnX = x + w - RTN_TW_BUTTON_SIZE - 6;
			int btnY = y + 6;
			gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
			gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
			GL_Bind( 0, hBtn );
			OrthoQuad( btnX, btnY, btnX + RTN_TW_BUTTON_SIZE, btnY + RTN_TW_BUTTON_SIZE );

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
	}

	return 1;
}
