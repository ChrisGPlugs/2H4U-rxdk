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

#include "son.h"


CSon::CSon()
{
#ifndef NO_SOUND_2H4U
    effet = new Mix_Chunk * [MAX_SONS];
    musique = NULL;
#endif
}

//Portage RXDK : le son n'est plus ouvert dans le constructeur (appelé
//avant SDL_Init), mais une fois SDL et la vidéo prêts.
void CSon::ouvrirSon()
{
#ifndef NO_SOUND_2H4U
    if (musique != NULL) return;
    Platform_Log("2H4U: audio open\n");
    if( Mix_OpenAudio( 22050, MIX_DEFAULT_FORMAT, 2, 2048 ) == -1 )
    {
        fprintf(stderr,"erreur init_sdl_mixer\n");

#ifdef _XBOX
		XLaunchNewImage(NULL, NULL);
#else
        exit(0);
#endif
    }
    Platform_Log("2H4U: music load\n");
    musique = Mix_LoadMUS( "D:\\data\\sons\\Musique.mp3" );
    //S'il y a eu une erreur au chargement de la musique
    if( musique == NULL )
    {
        fprintf(stderr,"erreur chargement music ds load_all_files()\n");

#ifdef _XBOX
		XLaunchNewImage(NULL, NULL);
#else        
		exit(0);
#endif
    }
#endif
}
CSon::~CSon()
{
#ifndef NO_SOUND_2H4U
    Mix_FreeMusic( musique );
    delete effet;
    Mix_CloseAudio();
#endif
}

void CSon::initSon(bool musiqueOn, bool sonOn)
{
#ifndef NO_SOUND_2H4U
    m_bSonOn=sonOn;
    m_bMusiqueOn=musiqueOn;

    if (m_bSonOn==true)
    {
        //Chargement des différents effets.
        effet[S_CASSE_BRIQUE] = Mix_LoadWAV( "D:\\data\\sons\\balle_brique.wav" );
        effet[S_REBOND_RAQ] = Mix_LoadWAV( "D:\\data\\sons\\balle_raquette.wav" );
        effet[S_CASSE_LIGNE] = Mix_LoadWAV( "D:\\data\\sons\\detruit_ligne.wav" );
        effet[S_POSE_BRIQUE] = Mix_LoadWAV( "D:\\data\\sons\\pose_brique.wav" );
        effet[S_REBOND_MUR] = Mix_LoadWAV( "D:\\data\\sons\\rebond_mur.wav" );
        effet[S_ROTATION] = Mix_LoadWAV( "D:\\data\\sons\\rotation.wav" );
        effet[S_STRAFE] = Mix_LoadWAV( "D:\\data\\sons\\deplacement.wav" );
        effet[S_NIVEAU] = Mix_LoadWAV( "D:\\data\\sons\\niveau.wav" );
        effet[S_GAME_OVER] = Mix_LoadWAV( "D:\\data\\sons\\game_over.wav" );
        effet[S_PERDU_BALLE] = Mix_LoadWAV( "D:\\data\\sons\\perdu_balle.wav" );
        effet[S_GAGNE_BONUS] = Mix_LoadWAV( "D:\\data\\sons\\bonus.wav" );

        for (int i=0;i<MAX_SONS;i++)
        {
            if (effet[i]==NULL)
            {
                fprintf(stderr,"erreur chargement effets ds initSon() : numéro %d\n",i);

#ifdef _XBOX
		XLaunchNewImage(NULL, NULL);
#else
				exit(0);
#endif
            }
        }

        for (int i=0;i<MAX_SONS;i++)
        {
            effet[i]->volume=70;
        }
    }

    if (m_bMusiqueOn==true) Mix_PlayMusic(musique, -1 );
#endif
}

void CSon::deleteSon()
{
#ifndef NO_SOUND_2H4U
    //Si a un moment on a re l'erreur.
    if (m_bSonOn==true)
    {
        for (int i=0;i<MAX_SONS;i++)
        {
            if (effet[i]!=NULL)
            {
                Mix_FreeChunk(effet[i]);
            }
        }
    }
    if (m_bMusiqueOn==true)
    {
        Mix_HaltMusic();
    }
#endif
}

void CSon::playEffet(int numero)
{
#ifndef NO_SOUND_2H4U
        if (m_bSonOn==true) Mix_PlayChannel( -1, effet[numero], 0 );
#endif
}

void CSon::pauseMusic()
{
#ifndef NO_SOUND_2H4U
    if (m_bMusiqueOn==true)
    {
        if (Mix_PlayingMusic()!=-1) Mix_PauseMusic();
        else fprintf(stderr,"erreur ds pause_music()\n");
    }
#endif
}

void CSon::resumeMusic()
{
#ifndef NO_SOUND_2H4U
        if (m_bMusiqueOn==true) Mix_ResumeMusic();
#endif
}

