/*
hud_textwindow.h - document window (Paranoia 2 style, RTN F10)
Copyright (C) 2026 Hermes e Hidrocarboneto

Shows a document window in HUD 2D: darkened background + the document
image (texts/images/*.tga) + a close button (X). The game is paused
while the window is open (the "showpause" icon is hidden).

The document file (texts/<name>.txt) syntax (Paranoia 2 compatible):
  <HEAD xsize=400 ysize=400 background="texts/images/doc.tga" imgbutton="texts/images/close.tga" scrollpos="50 70 300 220">
  <TITLE align=left>Title
  <TEXT font="Default Text" uspace=5 lspace=10 color="0 0 0 0">Body text
*/

#ifndef HUD_TEXTWINDOW_H
#define HUD_TEXTWINDOW_H

#include "hud.h"
#include "texture_handle.h"  // TextureHandle (membros)

class CHudTextWindow : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	void Reset( void );
	int Draw( float flTime );
	int MsgFunc_TextWindow( const char *pszName, int iSize, void *pbuf );

	void Open( const char *pszFileName );
	void Close( void );
	bool IsOpen( void ) { return m_bOpen; }

private:
	void ParseDocument( void );

	bool m_bOpen;
	bool m_bWasAttack;		// edge detect do clique do mouse
	char m_szFileName[64];		// nome do documento (sem extensao)
	char m_szPanelImage[64];	// imagem do painel (background do HEAD)
	char m_szButtonImage[64];	// imagem do botao fechar (imgbutton do HEAD)
	int m_iXSize, m_iYSize;		// tamanho da janela (xsize/ysize do HEAD)
	int m_iScrollX, m_iScrollY, m_iScrollW, m_iScrollH; // scrollpos (area da imagem)
	TextureHandle m_hPanelTex;	// textura do painel (carregada no Open - NAO a cada frame)
	TextureHandle m_hButtonTex;	// textura do botao fechar (idem)
};

#endif // HUD_TEXTWINDOW_H
