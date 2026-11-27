/*
    Copyright 2006 Pierre Lagouge, Pierre-Yves Ricau

    This file is part of 2H4U.

    2H4U is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    2H4U is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with 2H4U; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#ifndef Menu
#define Menu

#include "sdl_includes.h"

#include "highscore.h"

// Menu row index of Display. On the Xbox it only shows the video mode
// (Platform_DisplayLabel) and the cursor skips it.
#define MENU_ITEM_DISPLAY 3

// Text of menu row i (0 = Play). lang is the language file's lines.
const char* MenuItemText(char lang[MAX_LANG][256], int i);

// Renders text with font, or, if it is wider than maxW, with a smaller copy
// of the menu font so it fits the button.
SDL_Surface* RenderItemText(TTF_Font* font, const char* text, SDL_Color color, int maxW);

class CItem
{
private:
    SDL_Surface * m_pSelected;
    SDL_Rect m_pPosition;
public:

    CItem();
    void initItem(SDL_Surface*,int,int);
    void deleteItem();
    void drawItem(SDL_Surface*,bool);
};


class CMenu
{
private:

    int m_iSelectedItem; //ici entre 0 et 6
    int m_iNbItems;
    SDL_Surface * m_pFond;
    SDL_Surface * m_pScores;
    SDL_Surface * m_pChangeMusique;
    SDL_Surface * m_pChangeSons;
    SDL_Surface * m_pChangeAffichage;
    CItem * m_pItems;
    SDL_Surface* scoreTxt[5];

public:
    CMenu();
    void initMenu(char [MAX_LANG][256]);
    void deleteMenu();

    void drawMenu(SDL_Surface*,bool,bool,bool);

    int getSelectedItem();
    int getNbItems();

    void selectNextItem();
    void selectPreviousItem();
    void makeScores(CHighscore);
    void drawScores(SDL_Surface*);

};



#endif
